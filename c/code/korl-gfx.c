#include "korl-gfx.h"
#include "korl-log.h"
#include "korl-assetCache.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-vulkan.h"
#include "korl-time.h"
#include "korl-stb-truetype.h"
#define _KORL_GFX_DEBUG_LOG_GLYPH_PAGE_BITMAPS 0
typedef struct _Korl_Gfx_FontGlyphBackedCharacterBoundingBox
{
    u16 x0,y0,x1,y1; // coordinates of bbox in bitmap
    /** For non-outline glyphs, this value will always be == to \c -bearingLeft 
     * of the baked character */
    f32 offsetX;
    /** How much we need to offset the glyph mesh to center it on the baseline 
     * cursor origin. */
    f32 offsetY;
} _Korl_Gfx_FontGlyphBackedCharacterBoundingBox;
/** Basically just an extension of stbtt_bakedchar */
typedef struct _Korl_Gfx_FontGlyphBakedCharacter
{
    _Korl_Gfx_FontGlyphBackedCharacterBoundingBox bbox;
    _Korl_Gfx_FontGlyphBackedCharacterBoundingBox bboxOutline;
    f32 advanceX;   // pre-scaled by glyph scale at a particular pixelHeight value
    f32 bearingLeft;// pre-scaled by glyph scale at a particular pixelHeight value
    int glyphIndex;///@TODO: remove this if we don't actually need it for kerning or anything like that when building the text meshes
    bool isEmpty;// true if stbtt_IsGlyphEmpty for this glyph returned true
} _Korl_Gfx_FontGlyphBakedCharacter;
typedef struct _Korl_Gfx_FontGlyphBitmapPackRow
{
    u16 offsetY;
    u16 offsetX;
    u16 sizeY;// _including_ padding
} _Korl_Gfx_FontGlyphBitmapPackRow;
typedef struct _Korl_Gfx_FontGlyphPage
{
    u16                               packRowsSize;
    u16                               packRowsCapacity;
    _Korl_Gfx_FontGlyphBitmapPackRow* packRows;// emplaced after `data` in memory
    Korl_Vulkan_TextureHandle textureHandle;
    u16 dataSquareSize;
    u8* data;//1-channel, Alpha8 format, with an array size of dataSquareSize*dataSquareSize
} _Korl_Gfx_FontGlyphPage;
typedef struct _Korl_Gfx_FontCache
{
    stbtt_fontinfo fontInfo;
    f32 fontScale;// scale calculated from fontInfo & pixelHeight
    /** From STB TrueType documentation: 
     * you should advance the vertical position by "*ascent - *descent + *lineGap" */
    f32 fontAscent; // adjusted by pixelHeight already
    f32 fontDescent;// adjusted by pixelHeight already
    f32 fontLineGap;// adjusted by pixelHeight already
    int glyphDataCodepointStart;
    int glyphDataCodepointCount;//KORL-ISSUE-000-000-070: gfx/fontCache: Either cache glyphs dynamically instead of a fixed region all at once, or devise some way to configure the pre-cached range.
    f32 pixelHeight;
    f32 pixelOutlineThickness;// if this is 0.f, then an outline has not yet been cached in this fontCache glyphPageList
    //KORL-PERFORMANCE-000-000-000: (minor) convert pointers to contiguous memory into offsets to save some space
    wchar_t* fontAssetName;
    _Korl_Gfx_FontGlyphBakedCharacter* glyphData;
    _Korl_Gfx_FontGlyphPage* glyphPage;
} _Korl_Gfx_FontCache;
typedef struct _Korl_Gfx_Context
{
    /** used to store persistent data, such as Font asset glyph cache/database */
    Korl_Memory_AllocatorHandle allocatorHandle;
    KORL_MEMORY_POOL_DECLARE(_Korl_Gfx_FontCache*, fontCaches, 16);//KORL-FEATURE-000-000-007: dynamic resizing arrays
} _Korl_Gfx_Context;
korl_global_variable _Korl_Gfx_Context _korl_gfx_context;
korl_internal void _korl_gfx_glyphPage_insert(_Korl_Gfx_FontGlyphPage*const glyphPage, const int sizeX, const int sizeY, 
                                              u16* out_x0, u16* out_y0, u16* out_x1, u16* out_y1)
{
    /* find the pack row which fulfills the following conditions:
        - sizeY is the smallest among all pack rows that satisfy the following conditions
        - remaining size on the X axis can contain `w` pixels */
    korl_shared_const u16 PACK_ROW_PADDING = 1;
    u16 packRowIndex = glyphPage->packRowsCapacity;
    u16 packRowSizeY = KORL_U16_MAX;
    for(u16 p = 0; p < glyphPage->packRowsSize; p++)
    {
        if(glyphPage->packRows[p].offsetX > glyphPage->dataSquareSize)
            continue;
        const u16 remainingSpaceX = glyphPage->dataSquareSize 
                                  - glyphPage->packRows[p].offsetX;
        if(    glyphPage->packRows[p].sizeY < sizeY + 2*PACK_ROW_PADDING 
            || remainingSpaceX              < sizeX + 2*PACK_ROW_PADDING)
            continue;
        if(packRowSizeY > glyphPage->packRows[p].sizeY)
        {
            packRowIndex = p;
            packRowSizeY = glyphPage->packRows[p].sizeY;
        }
    }
    /* if we couldn't find a pack row, we need to allocate a new one */
    if(packRowIndex == glyphPage->packRowsCapacity)
    {
        korl_assert(sizeX + 2*PACK_ROW_PADDING <= glyphPage->dataSquareSize);
        if(glyphPage->packRowsSize == 0)
            korl_assert(sizeY + 2*PACK_ROW_PADDING <= glyphPage->dataSquareSize);
        else
            korl_assert(sizeY + 2*PACK_ROW_PADDING + glyphPage->packRows[glyphPage->packRowsSize - 1].sizeY <= glyphPage->dataSquareSize);
        korl_assert(glyphPage->packRowsSize < glyphPage->packRowsCapacity);
        packRowIndex = glyphPage->packRowsSize++;
        glyphPage->packRows[packRowIndex].sizeY   = korl_checkCast_i$_to_u16(sizeY + 2*PACK_ROW_PADDING);
        glyphPage->packRows[packRowIndex].offsetX = PACK_ROW_PADDING;
        if(packRowIndex > 0)
            glyphPage->packRows[packRowIndex].offsetY = glyphPage->packRows[packRowIndex - 1].offsetY 
                                                      + glyphPage->packRows[packRowIndex - 1].sizeY;
        else
            glyphPage->packRows[packRowIndex].offsetY = PACK_ROW_PADDING;
    }
    korl_assert(packRowIndex < glyphPage->packRowsSize);
    /* At this point, we should be guaranteed to have a pack row which 
        can fit our SDF glyph!  This means we can generate all of the 
        metrics related to the baked form of this glyph: */
    *out_x0 = glyphPage->packRows[packRowIndex].offsetX;
    *out_y0 = glyphPage->packRows[packRowIndex].offsetY;
    *out_x1 = korl_checkCast_i$_to_u16(*out_x0 + sizeX);
    *out_y1 = korl_checkCast_i$_to_u16(*out_y0 + sizeY);
    /* update the pack row metrics */
    glyphPage->packRows[packRowIndex].offsetX += korl_checkCast_i$_to_u16(sizeX + PACK_ROW_PADDING);
}
/** Vertex order for each glyph quad:
 * [ bottom-left
 * , bottom-right
 * , top-right
 * , top-left ] 
 * @TODO: adjust this documentation if this changes */
korl_internal void _korl_gfx_textGenerateMesh(Korl_Gfx_Batch*const batch, Korl_AssetCache_Get_Flags assetCacheGetFlags)
{
    _Korl_Gfx_Context*const context = &_korl_gfx_context;
    if(!batch->_assetNameFont || batch->_fontTextureHandle)
        return;
    Korl_AssetCache_AssetData assetDataFont = korl_assetCache_get(batch->_assetNameFont, assetCacheGetFlags);
    if(!assetDataFont.data)
        return;
#if 1// @TODO: delete, or maybe keep this around for debugging later?; testing stb_truetype bitmap rendering API:
    korl_shared_variable bool bitmapTestDone = false;
    if(!bitmapTestDone)
    {
        korl_shared_const int CODEPOINT = 'C';
        korl_shared_const f32 PIXEL_HEIGHT = 24.f;
        bitmapTestDone = true;
        korl_log(INFO, "===== stb_truetype bitmap test =====");
        stbtt_fontinfo fontInfo;
        korl_assert(stbtt_InitFont(&fontInfo, assetDataFont.data, 0/*font offset*/));
        const int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, CODEPOINT);
        const f32 scale = stbtt_ScaleForPixelHeight(&fontInfo, PIXEL_HEIGHT);
        int ix0, iy0, ix1, iy1;
        stbtt_GetGlyphBitmapBox(&fontInfo, glyphIndex, scale, scale, &ix0, &iy0, &ix1, &iy1);
        int width, height, xoff, yoff;
        u8*const bitmap = stbtt_GetGlyphBitmap(&fontInfo, scale, scale, glyphIndex, 
                                               &width, &height, &xoff, &yoff);
        int advanceX;
        int bearingLeft;
        stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advanceX, &bearingLeft);
        korl_log(INFO, "codepoint=%i glyph=%i w=%i h=%i xoff=%i yoff=%i ix0=%i iy0=%i ix1=%i iy1=%i advanceX=%f bearingLeft=%f", 
                 CODEPOINT, glyphIndex, width, height, xoff, yoff, ix0, iy0, ix1, iy1, scale*advanceX, scale*bearingLeft);
        wchar_t bitmapScanlineBuffer[256];
        korl_shared_const wchar_t SHADE_BLOCKS[] = L" ░▒▓█";
        for(int cy = 0; cy < height; cy++)
        {
            for(int cx = 0; cx < width; cx++)
            {
                if(bitmap[cy*width + cx] == 0)
                    bitmapScanlineBuffer[cx] = SHADE_BLOCKS[0];
                else if(bitmap[cy*width + cx] == KORL_U8_MAX)
                    bitmapScanlineBuffer[cx] = SHADE_BLOCKS[4];
                else if(bitmap[cy*width + cx] < 85)
                    bitmapScanlineBuffer[cx] = SHADE_BLOCKS[1];
                else if(bitmap[cy*width + cx] < 170)
                    bitmapScanlineBuffer[cx] = SHADE_BLOCKS[2];
                else
                    bitmapScanlineBuffer[cx] = SHADE_BLOCKS[3];
            }
            bitmapScanlineBuffer[width] = L'\0';
            korl_log(INFO, "%ws", bitmapScanlineBuffer);
        }
        stbtt_FreeBitmap(bitmap, NULL);
    }
#endif
#if 0// @TODO: delete, or maybe keep this around for debugging later?; stb_truetype testing SDF API:
    korl_shared_variable bool sdfTestDone = false;
    if(!sdfTestDone)
    {
        sdfTestDone = true;
        const f32 pixelOutlineThickness = 5.f;
        korl_log(INFO, "===== stb_truetype SDF test =====");
        stbtt_fontinfo fontInfo;
        korl_assert(stbtt_InitFont(&fontInfo, assetDataFont.data, 0/*font offset*/));
        int w, h, offsetX, offsetY;
        const u8 on_edge_value     = 128;// This is rather arbitrary; it seems like we can just select any reasonable # here and it will be enough for most fonts/sizes.  This value seems rather finicky however...  So if we have to modify how any of this glyph outline stuff is drawn, this will likely have to change somehow.
        const int padding          = 1/*this is necessary to keep the SDF glyph outline from bleeding all the way to the bitmap edge*/ + KORL_C_CAST(int, korl_math_ceil(pixelOutlineThickness));
        const f32 pixel_dist_scale = KORL_C_CAST(f32, on_edge_value)/KORL_C_CAST(f32, padding);
        u8* bitmapSdf = stbtt_GetCodepointSDF(&fontInfo, 
                                              stbtt_ScaleForPixelHeight(&fontInfo, /*batch->_textPixelHeight*/16), 
                                              '*', padding, on_edge_value, pixel_dist_scale, 
                                              &w, &h, &offsetX, &offsetY);
        korl_assert(bitmapSdf);
        korl_log(INFO, "w: %d, h: %d, offsetX: %d, offsetY: %d", w, h, offsetX, offsetY);
        wchar_t bitmapScanlineBuffer[256];
        korl_shared_const wchar_t SDF_RENDER[] = L" ░▒▓█";
        for(int cy = 0; cy < h; cy++)
        {
            for(int cx = 0; cx < w; cx++)
            {
                if(bitmapSdf[cy*w + cx] == 0)
                    bitmapScanlineBuffer[cx] = SDF_RENDER[0];
                else if(bitmapSdf[cy*w + cx] == KORL_U8_MAX)
                    bitmapScanlineBuffer[cx] = SDF_RENDER[4];
                else if(bitmapSdf[cy*w + cx] < 85)
                    bitmapScanlineBuffer[cx] = SDF_RENDER[1];
                else if(bitmapSdf[cy*w + cx] < 170)
                    bitmapScanlineBuffer[cx] = SDF_RENDER[2];
                else
                    bitmapScanlineBuffer[cx] = SDF_RENDER[3];
            }
            bitmapScanlineBuffer[w] = L'\0';
            korl_log_noMeta(INFO, "%ws", bitmapScanlineBuffer);
        }
        korl_log_noMeta(INFO, "");
        for(int cy = 0; cy < h; cy++)
        {
            for(int cx = 0; cx < w; cx++)
            {
                // only use SDF pixels that are >= the outline thickness edge value
                if(bitmapSdf[cy*w + cx] >= KORL_C_CAST(f32, on_edge_value) - (pixelOutlineThickness*pixel_dist_scale))
                {
#if 1
                    /* In an effort to keep the outline "smooth" or "anti-aliased", 
                        we simply increase the magnitude of each SDF pixel by 
                        the maximum magnitude difference between the shape edge 
                        & the furthest outline edge value.  This will bring us 
                        closer to a normalized set of outline bitmap pixel values, 
                        such that we should approach KORL_U8_MAX the closer we 
                        get to the shape edge, while simultaneously preventing 
                        harsh edges around the outside of the outline bitmap. */
                    const u16 pixelValue = KORL_MATH_CLAMP(KORL_C_CAST(u8, KORL_C_CAST(f32, bitmapSdf[cy*w + cx]) + (pixelOutlineThickness*pixel_dist_scale)), 0, KORL_U8_MAX);
#else
                    const u16 pixelValue = KORL_MATH_CLAMP(bitmapSdf[cy*w + cx] + on_edge_value, 0, KORL_U8_MAX);
#endif
                    if(pixelValue == 0)
                        bitmapScanlineBuffer[cx] = SDF_RENDER[0];
                    else if(pixelValue == KORL_U8_MAX)
                        bitmapScanlineBuffer[cx] = SDF_RENDER[4];
                    else if(pixelValue < 85)
                        bitmapScanlineBuffer[cx] = SDF_RENDER[1];
                    else if(pixelValue < 170)
                        bitmapScanlineBuffer[cx] = SDF_RENDER[2];
                    else
                        bitmapScanlineBuffer[cx] = SDF_RENDER[3];
                }
                else
                    bitmapScanlineBuffer[cx] = SDF_RENDER[0];
            }
            bitmapScanlineBuffer[w] = L'\0';
            korl_log_noMeta(INFO, "%ws", bitmapScanlineBuffer);
        }
#if 0
        korl_log_noMeta(INFO, "");
        for(int cy = 0; cy < h; cy++)
        {
            for(int cx = 0; cx < w; cx++)
                bitmapScanlineBuffer[cx] = bitmapSdf[cy*w + cx] >= on_edge_value ? L'█' : L' ';
            bitmapScanlineBuffer[w] = L'\0';
            korl_log_noMeta(INFO, "%ws", bitmapScanlineBuffer);
        }
        korl_log_noMeta(INFO, "");
        for(int cy = 0; cy < h; cy++)
        {
            for(int cx = 0; cx < w; cx++)
                bitmapScanlineBuffer[cx] = bitmapSdf[cy*w + cx] >= on_edge_value - ((padding - 0.5f)*pixel_dist_scale) ? L'█' : L' ';
            bitmapScanlineBuffer[w] = L'\0';
            korl_log_noMeta(INFO, "%ws", bitmapScanlineBuffer);
        }
        korl_log_noMeta(INFO, "");
        for(int cy = 0; cy < h; cy++)
        {
            for(int cx = 0; cx < w; cx++)
                bitmapScanlineBuffer[cx] = bitmapSdf[cy*w + cx] >= on_edge_value - ((padding - 1)*pixel_dist_scale) ? L'█' : L' ';
            bitmapScanlineBuffer[w] = L'\0';
            korl_log_noMeta(INFO, "%ws", bitmapScanlineBuffer);
        }
        korl_log_noMeta(INFO, "");
        for(int cy = 0; cy < h; cy++)
        {
            for(int cx = 0; cx < w; cx++)
                bitmapScanlineBuffer[cx] = bitmapSdf[cy*w + cx] >= on_edge_value - (padding*pixel_dist_scale) ? L'█' : L' ';
            bitmapScanlineBuffer[w] = L'\0';
            korl_log_noMeta(INFO, "%ws", bitmapScanlineBuffer);
        }
#endif
        stbtt_FreeSDF(bitmapSdf, NULL);
    }
#endif
    /* check and see if a matching font cache already exists */
    u$ existingFontCacheIndex = 0;
    bool bakeGlyphs        = false;
    bool bakeGlyphOutlines = false;
    for(; existingFontCacheIndex < KORL_MEMORY_POOL_SIZE(context->fontCaches); existingFontCacheIndex++)
        // find the font cache with a matching font asset name AND render 
        //  parameters such as font pixel height, etc... //
        if(    0 == korl_memory_stringCompare(batch->_assetNameFont, context->fontCaches[existingFontCacheIndex]->fontAssetName) 
            && context->fontCaches[existingFontCacheIndex]->pixelHeight == batch->_textPixelHeight 
            && (   context->fontCaches[existingFontCacheIndex]->pixelOutlineThickness == 0.f // the font cache has not yet been cached with outline glyphs
                || context->fontCaches[existingFontCacheIndex]->pixelOutlineThickness == batch->_textPixelOutline))
        {
            bakeGlyphOutlines = context->fontCaches[existingFontCacheIndex]->pixelOutlineThickness != batch->_textPixelOutline;
            break;
        }
    _Korl_Gfx_FontCache* fontCache = NULL;
    if(existingFontCacheIndex < KORL_MEMORY_POOL_SIZE(context->fontCaches))
        fontCache = context->fontCaches[existingFontCacheIndex];
    else// if we could not find a matching fontCache, we need to create one //
    {
        bakeGlyphs        = true;
        bakeGlyphOutlines = batch->_textPixelOutline != 0.f;
        /* add a new font cache address to the context's font cache address pool */
        korl_assert(!KORL_MEMORY_POOL_ISFULL(context->fontCaches));
        _Korl_Gfx_FontCache**const newFontCache = KORL_MEMORY_POOL_ADD(context->fontCaches);
        /* calculate how much memory we need */
        korl_shared_const int GLYPH_CODE_FIRST       = ' ';
        korl_shared_const int GLYPH_CODE_LAST        = '~';// ASCII [32, 126]|[' ', '~'] is 95 glyphs
        korl_shared_const u16 GLYPH_PAGE_SQUARE_SIZE = 512;
        korl_shared_const u16 PACK_ROWS_CAPACITY     = 64;
        const int glyphDataSize = GLYPH_CODE_LAST - GLYPH_CODE_FIRST + 1;
        const u$ assetNameFontBufferSize = korl_memory_stringSize(batch->_assetNameFont) + 1/*null terminator*/;
        const u$ assetNameFontBufferBytes = assetNameFontBufferSize*sizeof(*batch->_assetNameFont);
        const u$ fontCacheRequiredBytes = sizeof(_Korl_Gfx_FontCache)
            + sizeof(_Korl_Gfx_FontGlyphPage)
            + PACK_ROWS_CAPACITY*sizeof(_Korl_Gfx_FontGlyphBitmapPackRow)
            + GLYPH_PAGE_SQUARE_SIZE*GLYPH_PAGE_SQUARE_SIZE// glyph page bitmap (data)
            + assetNameFontBufferBytes
            + glyphDataSize*sizeof(_Korl_Gfx_FontGlyphBakedCharacter)/*glyphData*/;
        /* allocate the required memory from the korl-gfx context allocator */
        *newFontCache = korl_allocate(context->allocatorHandle, fontCacheRequiredBytes);
        korl_assert(*newFontCache);
        fontCache = *newFontCache;
        /* initialize the memory */
#if KORL_DEBUG
        korl_assert(korl_memory_isNull(*newFontCache, fontCacheRequiredBytes));
#endif//KORL_DEBUG
        fontCache->glyphDataCodepointStart = GLYPH_CODE_FIRST;
        fontCache->glyphDataCodepointCount = glyphDataSize;
        fontCache->pixelHeight             = batch->_textPixelHeight;
        fontCache->pixelOutlineThickness   = batch->_textPixelOutline;
        fontCache->fontAssetName           = KORL_C_CAST(wchar_t*, fontCache + 1);
        fontCache->glyphData               = KORL_C_CAST(_Korl_Gfx_FontGlyphBakedCharacter*, KORL_C_CAST(u8*, fontCache->fontAssetName) + assetNameFontBufferBytes);
        fontCache->glyphPage               = KORL_C_CAST(_Korl_Gfx_FontGlyphPage*          , KORL_C_CAST(u8*, fontCache->glyphData)     + glyphDataSize*sizeof(*fontCache->glyphData));
        _Korl_Gfx_FontGlyphPage*const glyphPage = fontCache->glyphPage;
        glyphPage->packRowsCapacity = PACK_ROWS_CAPACITY;
        glyphPage->dataSquareSize   = GLYPH_PAGE_SQUARE_SIZE;
        glyphPage->data             = KORL_C_CAST(u8*, glyphPage + 1);
        glyphPage->packRows         = KORL_C_CAST(_Korl_Gfx_FontGlyphBitmapPackRow*, KORL_C_CAST(u8*, glyphPage->data + GLYPH_PAGE_SQUARE_SIZE*GLYPH_PAGE_SQUARE_SIZE));
        korl_assert(korl_checkCast_u$_to_i$(assetNameFontBufferSize) == korl_memory_stringCopy(batch->_assetNameFont, fontCache->fontAssetName, assetNameFontBufferSize));
        /* initialize the font info using the raw font asset data */
        korl_assert(stbtt_InitFont(&(fontCache->fontInfo), assetDataFont.data, 0/*font offset*/));
        fontCache->fontScale = stbtt_ScaleForPixelHeight(&(fontCache->fontInfo), fontCache->pixelHeight);
        int ascent;
        int descent;
        int lineGap;
        stbtt_GetFontVMetrics(&(fontCache->fontInfo), &ascent, &descent, &lineGap);
        fontCache->fontAscent  = fontCache->fontScale*KORL_C_CAST(f32, ascent );
        fontCache->fontDescent = fontCache->fontScale*KORL_C_CAST(f32, descent);
        fontCache->fontLineGap = fontCache->fontScale*KORL_C_CAST(f32, lineGap);
    }
    if(bakeGlyphs || bakeGlyphOutlines)
    {
        _Korl_Gfx_FontGlyphPage*const glyphPage = fontCache->glyphPage;
        /* iterate over all of our pre-determined glyph codepoints and bake them 
            into the font cache's glyph page data */
        for(int c = 0; c < fontCache->glyphDataCodepointCount; c++)
        {
            const int glyphIndex = stbtt_FindGlyphIndex(&(fontCache->fontInfo), 
                                                        c + fontCache->glyphDataCodepointStart);
            int advanceX;
            int bearingLeft;
            stbtt_GetGlyphHMetrics(&(fontCache->fontInfo), glyphIndex, &advanceX, &bearingLeft);
            _Korl_Gfx_FontGlyphBakedCharacter*const bakedChar = &(fontCache->glyphData[c]);
            korl_memory_zero(bakedChar, sizeof(*bakedChar));
            bakedChar->glyphIndex  = glyphIndex;
            bakedChar->isEmpty     = stbtt_IsGlyphEmpty(&(fontCache->fontInfo), glyphIndex);
            bakedChar->advanceX    = fontCache->fontScale*KORL_C_CAST(f32, advanceX   );
            bakedChar->bearingLeft = fontCache->fontScale*KORL_C_CAST(f32, bearingLeft);
            if(bakedChar->isEmpty)
                continue;
            if(bakeGlyphs)
            {
                int ix0, iy0, ix1, iy1;
                stbtt_GetGlyphBitmapBox(&(fontCache->fontInfo), glyphIndex, fontCache->fontScale, fontCache->fontScale, &ix0, &iy0, &ix1, &iy1);
                const int w = ix1 - ix0;
                const int h = iy1 - iy0;
                _korl_gfx_glyphPage_insert(glyphPage, w, h, 
                                           &bakedChar->bbox.x0, 
                                           &bakedChar->bbox.y0, 
                                           &bakedChar->bbox.x1, 
                                           &bakedChar->bbox.y1);
                bakedChar->bbox.offsetX = bakedChar->bearingLeft;
                bakedChar->bbox.offsetY = -KORL_C_CAST(f32, iy1);
                /* actually write  the glyph bitmap in the glyph page data */
                stbtt_MakeGlyphBitmap(&(fontCache->fontInfo), 
                                      glyphPage->data + (bakedChar->bbox.y0*glyphPage->dataSquareSize + bakedChar->bbox.x0), 
                                      w, h, glyphPage->dataSquareSize, 
                                      fontCache->fontScale, fontCache->fontScale, glyphIndex);
            }
            if(bakeGlyphOutlines)
            {
                const u8 on_edge_value     = 128;// This is rather arbitrary; it seems like we can just select any reasonable # here and it will be enough for most fonts/sizes.  This value seems rather finicky however...  So if we have to modify how any of this glyph outline stuff is drawn, this will likely have to change somehow.
                const int padding          = 1/*this is necessary to keep the SDF glyph outline from bleeding all the way to the bitmap edge*/ + KORL_C_CAST(int, korl_math_ceil(fontCache->pixelOutlineThickness));
                const f32 pixel_dist_scale = KORL_C_CAST(f32, on_edge_value)/KORL_C_CAST(f32, padding);
                int w, h, offsetX, offsetY;
                u8* bitmapSdf = stbtt_GetGlyphSDF(&(fontCache->fontInfo), 
                                                  fontCache->fontScale, 
                                                  glyphIndex, padding, on_edge_value, pixel_dist_scale, 
                                                  &w, &h, &offsetX, &offsetY);
                korl_assert(bitmapSdf);
                /* update the baked character outline meta data */
                _korl_gfx_glyphPage_insert(glyphPage, w, h, 
                                           &bakedChar->bboxOutline.x0, 
                                           &bakedChar->bboxOutline.y0, 
                                           &bakedChar->bboxOutline.x1, 
                                           &bakedChar->bboxOutline.y1);
                bakedChar->bboxOutline.offsetY =  KORL_C_CAST(f32, offsetX);
                bakedChar->bboxOutline.offsetY = -KORL_C_CAST(f32, offsetY);
                /* place the processed SDF glyph into the glyph page */
                for(int cy = 0; cy < h; cy++)
                    for(int cx = 0; cx < w; cx++)
                    {
                        u8*const dataPixel = glyphPage->data + ((bakedChar->bboxOutline.y0 + cy)*glyphPage->dataSquareSize + bakedChar->bboxOutline.x0 + cx);
                        // only use SDF pixels that are >= the outline thickness edge value
                        if(bitmapSdf[cy*w + cx] >= KORL_C_CAST(f32, on_edge_value) - (fontCache->pixelOutlineThickness*pixel_dist_scale))
                        {
                            /* In an effort to keep the outline "smooth" or "anti-aliased", 
                                we simply increase the magnitude of each SDF pixel by 
                                the maximum magnitude difference between the shape edge 
                                & the furthest outline edge value.  This will bring us 
                                closer to a normalized set of outline bitmap pixel values, 
                                such that we should approach KORL_U8_MAX the closer we 
                                get to the shape edge, while simultaneously preventing 
                                harsh edges around the outside of the outline bitmap. */
                            const u16 pixelValue = KORL_MATH_CLAMP(KORL_C_CAST(u8, KORL_C_CAST(f32, bitmapSdf[cy*w + cx]) + (fontCache->pixelOutlineThickness*pixel_dist_scale)), 0, KORL_U8_MAX);
                            *dataPixel = (u8/*safe, since we _just_ clamped the value to u8 range*/)pixelValue;
                        }
                        else
                            *dataPixel = 0;
                    }
                /**/
                stbtt_FreeSDF(bitmapSdf, NULL);
            }
        }// if(bakeGlyphs || bakeGlyphOutlines)
        /* debug print the glyph page data :) */
#if KORL_DEBUG && _KORL_GFX_DEBUG_LOG_GLYPH_PAGE_BITMAPS
        korl_shared_const wchar_t PIXEL_CHARS[] = L" ░▒▓█";
        wchar_t debugPixelCharBuffer[512 + 1];
        debugPixelCharBuffer[512] = L'\0';
        for(u16 y = 0; y < glyphPage->dataSquareSize; y++)
        {
            for(u16 x = 0; x < glyphPage->dataSquareSize; x++)
            {
                const u8 pixel = glyphPage->data[y*glyphPage->dataSquareSize + x];
#if 0
#if 1
                if(pixel)
                    debugPixelCharBuffer[x] = L'█';
                else
                    debugPixelCharBuffer[x] = L' ';
#else
                debugPixelCharBuffer[x] = PIXEL_CHARS[KORL_MATH_CLAMP(pixel/(KORL_U8_MAX/5), 0, korl_arraySize(PIXEL_CHARS) - 1)];
#endif
#else
                if(pixel == 0)
                    debugPixelCharBuffer[x] = PIXEL_CHARS[0];
                else if(pixel == KORL_U8_MAX)
                    debugPixelCharBuffer[x] = PIXEL_CHARS[4];
                else if(pixel < 85)
                    debugPixelCharBuffer[x] = PIXEL_CHARS[1];
                else if(pixel < 170)
                    debugPixelCharBuffer[x] = PIXEL_CHARS[2];
                else
                    debugPixelCharBuffer[x] = PIXEL_CHARS[3];
#endif
            }
            korl_log_noMeta(VERBOSE, "%ws", debugPixelCharBuffer);
        }
#endif// KORL_DEBUG && _KORL_GFX_DEBUG_LOG_GLYPH_PAGE_BITMAPS
        /* upload the glyph data to the GPU & obtain texture handle */
        /** we can do a couple of things here...
         * - "expand" the alpha glyph image buffer to occupy RGBA32 instead of 
         *   A8 format, allowing us to use most of the code that is currently 
         *   already known to be functioning correctly in the korl-vulkan module
         * - figure out how to just upload an A8 format image buffer to a vulkan 
         *   texture and use that in the current batch pipeline somehow... 
         *   - create a VkImage using 
         *     - the VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT flag 
         *     - the VK_FORMAT_R8_SRGB format
         *   - create a VkImageView using 
         *     - the VK_FORMAT_R8G8B8A8_SRGB format */
        //KORL-PERFORMANCE-000-000-001: gfx: inefficient texture upload process
        /* now that we have a complete baked glyph atlas for this font at the 
            required settings for the desired parameters of the provided batch, 
            we can upload this image buffer to the graphics device for rendering */
        //KORL-PERFORMANCE-000-000-001
        // allocate a temp R8G8B8A8-format image buffer //
        Korl_Vulkan_Color4u8*const tempImageBuffer = korl_allocate(context->allocatorHandle, 
                                                                   sizeof(Korl_Vulkan_Color4u8) * fontCache->glyphPage->dataSquareSize * fontCache->glyphPage->dataSquareSize);
        // "expand" the stbtt font bitmap into the image buffer //
        for(u$ y = 0; y < fontCache->glyphPage->dataSquareSize; y++)
            for(u$ x = 0; x < fontCache->glyphPage->dataSquareSize; x++)
                /* store a pure white pixel with the alpha component set to the stbtt font bitmap value */
                tempImageBuffer[y*fontCache->glyphPage->dataSquareSize + x] = (Korl_Vulkan_Color4u8){.r = 0xFF, .g = 0xFF, .b = 0xFF, .a = fontCache->glyphPage->data[y*fontCache->glyphPage->dataSquareSize + x]};
        // create a vulkan texture, upload the image buffer to it //
        if(fontCache->glyphPage->textureHandle)
            korl_vulkan_textureDestroy(fontCache->glyphPage->textureHandle);
        fontCache->glyphPage->textureHandle = korl_vulkan_textureCreate(fontCache->glyphPage->dataSquareSize, 
                                                                        fontCache->glyphPage->dataSquareSize, 
                                                                        tempImageBuffer);
        // free the temporary R8G8B8A8-format image buffer //
        korl_free(context->allocatorHandle, tempImageBuffer);
    }
    /* at this point, we should have an index to a valid font cache which 
        contains all the data necessary about the glyphs will want to render for 
        the batch's text string - all we have to do now is iterate over all the 
        pre-allocated vertex attributes (position & UVs) and update them to use 
        the correct  */
    Korl_Math_V2f32 textBaselineCursor = (Korl_Math_V2f32){0.f, 0.f};
    u32 currentGlyph = 0;
    batch->_textVisibleCharacterCount = 0;
    const bool outlinePass = (batch->_textPixelOutline > 0.f);
    for(const wchar_t* character = batch->_text; *character; character++)
    {
        korl_assert(*character >= fontCache->glyphDataCodepointStart && *character < fontCache->glyphDataCodepointStart + fontCache->glyphDataCodepointCount);//KORL-ISSUE-000-000-070: gfx/fontCache: Either cache glyphs dynamically instead of a fixed region all at once, or devise some way to configure the pre-cached range.
        const u$ characterOffset = *character - fontCache->glyphDataCodepointStart;
        _Korl_Gfx_FontGlyphBackedCharacterBoundingBox*const bbox = outlinePass 
            ? &fontCache->glyphData[characterOffset].bboxOutline
            : &fontCache->glyphData[characterOffset].bbox;
        const f32 x0 = textBaselineCursor.x + bbox->offsetX;
        const f32 y0 = textBaselineCursor.y + bbox->offsetY;
        const f32 x1 = x0 + (bbox->x1 - bbox->x0);
        const f32 y1 = y0 + (bbox->y1 - bbox->y0);
        textBaselineCursor.x += fontCache->glyphData[characterOffset].advanceX;
        if(*(character + 1))
        {
            korl_assert(*(character + 1) >= fontCache->glyphDataCodepointStart && *(character + 1) < fontCache->glyphDataCodepointStart + fontCache->glyphDataCodepointCount);//KORL-ISSUE-000-000-070: gfx/fontCache: Either cache glyphs dynamically instead of a fixed region all at once, or devise some way to configure the pre-cached range.
            const u$ characterOffsetNext = *(character + 1) - fontCache->glyphDataCodepointStart;
            const int kernAdvance = stbtt_GetGlyphKernAdvance(&fontCache->fontInfo, 
                                                              fontCache->glyphData[characterOffset    ].glyphIndex, 
                                                              fontCache->glyphData[characterOffsetNext].glyphIndex);
            textBaselineCursor.x += fontCache->fontScale*kernAdvance;
        }
        const f32 u0 = KORL_C_CAST(f32, bbox->x0) / KORL_C_CAST(f32, fontCache->glyphPage->dataSquareSize);
        const f32 u1 = KORL_C_CAST(f32, bbox->x1) / KORL_C_CAST(f32, fontCache->glyphPage->dataSquareSize);
        const f32 v0 = KORL_C_CAST(f32, bbox->y0) / KORL_C_CAST(f32, fontCache->glyphPage->dataSquareSize);
        const f32 v1 = KORL_C_CAST(f32, bbox->y1) / KORL_C_CAST(f32, fontCache->glyphPage->dataSquareSize);
        if(*character == L'\n')
        {
            textBaselineCursor.x = 0.f;
            textBaselineCursor.y -= (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
        }
        if(fontCache->glyphData[characterOffset].isEmpty)
            continue;
        /* using currentGlyph, we can now write the vertex data for 
            the current glyph quad we just obtained the metrics for */
        batch->_vertexIndices[6*currentGlyph + 0] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 0);
        batch->_vertexIndices[6*currentGlyph + 1] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 1);
        batch->_vertexIndices[6*currentGlyph + 2] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 3);
        batch->_vertexIndices[6*currentGlyph + 3] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 1);
        batch->_vertexIndices[6*currentGlyph + 4] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 2);
        batch->_vertexIndices[6*currentGlyph + 5] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 3);
        batch->_vertexPositions[4*currentGlyph + 0] = (Korl_Vulkan_Position){x0, y0, 0};
        batch->_vertexPositions[4*currentGlyph + 1] = (Korl_Vulkan_Position){x1, y0, 0};
        batch->_vertexPositions[4*currentGlyph + 2] = (Korl_Vulkan_Position){x1, y1, 0};
        batch->_vertexPositions[4*currentGlyph + 3] = (Korl_Vulkan_Position){x0, y1, 0};
        batch->_vertexUvs[4*currentGlyph + 0] = (Korl_Vulkan_Uv){u0, v1};
        batch->_vertexUvs[4*currentGlyph + 1] = (Korl_Vulkan_Uv){u1, v1};
        batch->_vertexUvs[4*currentGlyph + 2] = (Korl_Vulkan_Uv){u1, v0};
        batch->_vertexUvs[4*currentGlyph + 3] = (Korl_Vulkan_Uv){u0, v0};
        batch->_vertexColors[4*currentGlyph + 0] = outlinePass ? batch->_textColorOutline : batch->_textColor;
        batch->_vertexColors[4*currentGlyph + 1] = outlinePass ? batch->_textColorOutline : batch->_textColor;
        batch->_vertexColors[4*currentGlyph + 2] = outlinePass ? batch->_textColorOutline : batch->_textColor;
        batch->_vertexColors[4*currentGlyph + 3] = outlinePass ? batch->_textColorOutline : batch->_textColor;
        currentGlyph++;
        batch->_textVisibleCharacterCount++;
    }
    if(outlinePass)
    {
        textBaselineCursor = (Korl_Math_V2f32){0.f, 0.f};
        currentGlyph = 0;///TODO: no! don't reset currentGlyph!  This will eliminate the outline glyphs
        for(const wchar_t* character = batch->_text; *character; character++)
        {
            korl_assert(*character >= fontCache->glyphDataCodepointStart && *character < fontCache->glyphDataCodepointStart + fontCache->glyphDataCodepointCount);//KORL-ISSUE-000-000-070: gfx/fontCache: Either cache glyphs dynamically instead of a fixed region all at once, or devise some way to configure the pre-cached range.
            const u$ characterOffset = *character - fontCache->glyphDataCodepointStart;
        _Korl_Gfx_FontGlyphBackedCharacterBoundingBox*const bbox = &fontCache->glyphData[characterOffset].bbox;
        const f32 x0 = textBaselineCursor.x + bbox->offsetX;
        const f32 y0 = textBaselineCursor.y + bbox->offsetY;
        const f32 x1 = x0 + (bbox->x1 - bbox->x0);
        const f32 y1 = y0 + (bbox->y1 - bbox->y0);
        textBaselineCursor.x += fontCache->glyphData[characterOffset].advanceX;
        const f32 u0 = KORL_C_CAST(f32, bbox->x0) / KORL_C_CAST(f32, fontCache->glyphPage->dataSquareSize);
        const f32 u1 = KORL_C_CAST(f32, bbox->x1) / KORL_C_CAST(f32, fontCache->glyphPage->dataSquareSize);
        const f32 v0 = KORL_C_CAST(f32, bbox->y0) / KORL_C_CAST(f32, fontCache->glyphPage->dataSquareSize);
        const f32 v1 = KORL_C_CAST(f32, bbox->y1) / KORL_C_CAST(f32, fontCache->glyphPage->dataSquareSize);
            if(*character == L'\n')
            {
                textBaselineCursor.x = 0.f;
                textBaselineCursor.y -= (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
            }
            if(fontCache->glyphData[characterOffset].isEmpty)
                continue;
            /* using currentGlyph, we can now write the vertex data for 
                the current glyph quad we just obtained the metrics for */
            batch->_vertexIndices[6*currentGlyph + 0] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 0);
            batch->_vertexIndices[6*currentGlyph + 1] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 1);
            batch->_vertexIndices[6*currentGlyph + 2] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 3);
            batch->_vertexIndices[6*currentGlyph + 3] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 1);
            batch->_vertexIndices[6*currentGlyph + 4] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 2);
            batch->_vertexIndices[6*currentGlyph + 5] = korl_vulkan_safeCast_u$_to_vertexIndex(4*currentGlyph + 3);
            batch->_vertexPositions[4*currentGlyph + 0] = (Korl_Vulkan_Position){x0, y0, 0};
            batch->_vertexPositions[4*currentGlyph + 1] = (Korl_Vulkan_Position){x1, y0, 0};
            batch->_vertexPositions[4*currentGlyph + 2] = (Korl_Vulkan_Position){x1, y1, 0};
            batch->_vertexPositions[4*currentGlyph + 3] = (Korl_Vulkan_Position){x0, y1, 0};
            batch->_vertexUvs[4*currentGlyph + 0] = (Korl_Vulkan_Uv){u0, v1};
            batch->_vertexUvs[4*currentGlyph + 1] = (Korl_Vulkan_Uv){u1, v1};
            batch->_vertexUvs[4*currentGlyph + 2] = (Korl_Vulkan_Uv){u1, v0};
            batch->_vertexUvs[4*currentGlyph + 3] = (Korl_Vulkan_Uv){u0, v0};
            batch->_vertexColors[4*currentGlyph + 0] = batch->_textColor;
            batch->_vertexColors[4*currentGlyph + 1] = batch->_textColor;
            batch->_vertexColors[4*currentGlyph + 2] = batch->_textColor;
            batch->_vertexColors[4*currentGlyph + 3] = batch->_textColor;
            currentGlyph++;
        }
    }
    batch->_fontTextureHandle = fontCache->glyphPage->textureHandle;
}
korl_internal void korl_gfx_initialize(void)
{
    _Korl_Gfx_Context*const context = &_korl_gfx_context;
    korl_memory_zero(context, sizeof(*context));
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, 
                                                            korl_math_megabytes(4), L"korl-gfx", 
                                                            KORL_MEMORY_ALLOCATOR_FLAGS_NONE, 
                                                            NULL/*let platform choose address*/);
}
korl_internal void korl_gfx_clearFontCache(void)
{
    _Korl_Gfx_Context*const context = &_korl_gfx_context;
    for(Korl_MemoryPool_Size fc = 0; fc < KORL_MEMORY_POOL_SIZE(context->fontCaches); fc++)
    {
        ///@TODO: now that glyph atlas can potentially span multiple textures, this code must become slightly more complext
        korl_free(context->allocatorHandle, context->fontCaches[fc]);
    }
    KORL_MEMORY_POOL_EMPTY(context->fontCaches);
}
korl_internal KORL_PLATFORM_GFX_CREATE_CAMERA_FOV(korl_gfx_createCameraFov)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.position                                = position;
    result.target                                  = target;
    result._viewportScissorPosition                = (Korl_Math_V2f32){0, 0};
    result._viewportScissorSize                    = (Korl_Math_V2f32){1, 1};
    result.subCamera.perspective.clipNear          = clipNear;
    result.subCamera.perspective.clipFar           = clipFar;
    result.subCamera.perspective.fovHorizonDegrees = fovHorizonDegrees;
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO(korl_gfx_createCameraOrtho)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC;
    result.position                            = KORL_MATH_V3F32_ZERO;
    result.target                              = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result._viewportScissorPosition            = (Korl_Math_V2f32){0, 0};
    result._viewportScissorSize                = (Korl_Math_V2f32){1, 1};
    result.subCamera.orthographic.clipDepth    = clipDepth;
    result.subCamera.orthographic.originAnchor = (Korl_Math_V2f32){0.5f, 0.5f};
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO_FIXED_HEIGHT(korl_gfx_createCameraOrthoFixedHeight)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                           = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT;
    result.position                                       = KORL_MATH_V3F32_ZERO;
    result.target                                         = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result._viewportScissorPosition                       = (Korl_Math_V2f32){0, 0};
    result._viewportScissorSize                           = (Korl_Math_V2f32){1, 1};
    result.subCamera.orthographicFixedHeight.fixedHeight  = fixedHeight;
    result.subCamera.orthographicFixedHeight.clipDepth    = clipDepth;
    result.subCamera.orthographicFixedHeight.originAnchor = (Korl_Math_V2f32){0.5f, 0.5f};
    return result;
}
korl_internal KORL_PLATFORM_GFX_CAMERA_FOV_ROTATE_AROUND_TARGET(korl_gfx_cameraFov_rotateAroundTarget)
{
    korl_assert(context->type == KORL_GFX_CAMERA_TYPE_PERSPECTIVE);
    const Korl_Math_V3f32 newTargetOffset = 
        korl_math_quaternion_transformV3f32(
            korl_math_quaternion_fromAxisRadians(axisOfRotation, radians, false), 
            korl_math_v3f32_subtract(context->position, context->target), 
            true);
    context->position = korl_math_v3f32_add(context->target, newTargetOffset);
}
korl_internal KORL_PLATFORM_GFX_USE_CAMERA(korl_gfx_useCamera)
{
    const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize();
    switch(camera._scissorType)
    {
    case KORL_GFX_CAMERA_SCISSOR_TYPE_RATIO:{
        korl_assert(camera._viewportScissorPosition.x >= 0 && camera._viewportScissorPosition.x <= 1);
        korl_assert(camera._viewportScissorPosition.y >= 0 && camera._viewportScissorPosition.y <= 1);
        korl_assert(camera._viewportScissorSize.x >= 0 && camera._viewportScissorSize.x <= 1);
        korl_assert(camera._viewportScissorSize.y >= 0 && camera._viewportScissorSize.y <= 1);
        korl_vulkan_setScissor(korl_math_round_f32_to_u32(camera._viewportScissorPosition.x * swapchainSize.x), 
                               korl_math_round_f32_to_u32(camera._viewportScissorPosition.y * swapchainSize.y), 
                               korl_math_round_f32_to_u32(camera._viewportScissorSize.x * swapchainSize.x), 
                               korl_math_round_f32_to_u32(camera._viewportScissorSize.y * swapchainSize.y));
        break;}
    case KORL_GFX_CAMERA_SCISSOR_TYPE_ABSOLUTE:{
        korl_assert(camera._viewportScissorPosition.x >= 0);
        korl_assert(camera._viewportScissorPosition.y >= 0);
        korl_vulkan_setScissor(korl_math_round_f32_to_u32(camera._viewportScissorPosition.x), 
                               korl_math_round_f32_to_u32(camera._viewportScissorPosition.y), 
                               korl_math_round_f32_to_u32(camera._viewportScissorSize.x), 
                               korl_math_round_f32_to_u32(camera._viewportScissorSize.y));
        break;}
    }
    switch(camera.type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        korl_vulkan_setProjectionFov(camera.subCamera.perspective.fovHorizonDegrees, camera.subCamera.perspective.clipNear, camera.subCamera.perspective.clipFar);
        korl_vulkan_setView(camera.position, camera.target, KORL_MATH_V3F32_Z);
        }break;
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        korl_vulkan_setProjectionOrthographic(camera.subCamera.orthographic.clipDepth, camera.subCamera.orthographic.originAnchor.x, camera.subCamera.orthographic.originAnchor.y);
        korl_vulkan_setView(camera.position, camera.target, KORL_MATH_V3F32_Y);
        }break;
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        korl_vulkan_setProjectionOrthographicFixedHeight(camera.subCamera.orthographicFixedHeight.fixedHeight, camera.subCamera.orthographicFixedHeight.clipDepth, camera.subCamera.orthographicFixedHeight.originAnchor.x, camera.subCamera.orthographicFixedHeight.originAnchor.y);
        korl_vulkan_setView(camera.position, camera.target, KORL_MATH_V3F32_Y);
        }break;
    }
}
korl_internal KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR(korl_gfx_cameraSetScissor)
{
    f32 x2 = x + sizeX;
    f32 y2 = y + sizeY;
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x2 < 0) x2 = 0;
    if(y2 < 0) y2 = 0;
    context->_viewportScissorPosition.x = x;
    context->_viewportScissorPosition.y = y;
    context->_viewportScissorSize.x     = x2 - x;
    context->_viewportScissorSize.y     = y2 - y;
    context->_scissorType               = KORL_GFX_CAMERA_SCISSOR_TYPE_ABSOLUTE;
}
korl_internal KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR_PERCENT(korl_gfx_cameraSetScissorPercent)
{
    f32 x2 = viewportRatioX + viewportRatioWidth;
    f32 y2 = viewportRatioY + viewportRatioHeight;
    if(viewportRatioX < 0) viewportRatioX = 0;
    if(viewportRatioY < 0) viewportRatioY = 0;
    if(x2 < 0) x2 = 0;
    if(y2 < 0) y2 = 0;
    context->_viewportScissorPosition.x = viewportRatioX;
    context->_viewportScissorPosition.y = viewportRatioY;
    context->_viewportScissorSize.x     = x2 - viewportRatioX;
    context->_viewportScissorSize.y     = y2 - viewportRatioY;
    context->_scissorType               = KORL_GFX_CAMERA_SCISSOR_TYPE_RATIO;
}
korl_internal KORL_PLATFORM_GFX_CAMERA_ORTHO_SET_ORIGIN_ANCHOR(korl_gfx_cameraOrthoSetOriginAnchor)
{
    const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize();
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        korl_assert(!"origin anchor not supported for perspective camera");
        }break;
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        context->subCamera.orthographic.originAnchor.x = swapchainSizeRatioOriginX;
        context->subCamera.orthographic.originAnchor.y = swapchainSizeRatioOriginY;
        }break;
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        context->subCamera.orthographicFixedHeight.originAnchor.x = swapchainSizeRatioOriginX;
        context->subCamera.orthographicFixedHeight.originAnchor.y = swapchainSizeRatioOriginY;
        }break;
    }
}
korl_internal KORL_PLATFORM_GFX_BATCH(korl_gfx_batch)
{
    korl_time_probeStart(text_generate_mesh);
    _korl_gfx_textGenerateMesh(batch, KORL_ASSETCACHE_GET_FLAGS_LAZY);
    korl_time_probeStop(text_generate_mesh);
    if(batch->_vertexCount <= 0)
    {
        korl_log(WARNING, "attempted batch is empty");
        return;
    }
    if(batch->_assetNameFont && !batch->_fontTextureHandle)
    {
        ///@TODO: uncomment this// korl_log(WARNING, "text batch mesh not yet obtained from font asset");
        return;
    }
    u32 vertexIndexCount = batch->_vertexIndexCount;
    u32 vertexCount      = batch->_vertexCount;
    if(batch->_assetNameTexture)
        korl_vulkan_useImageAssetAsTexture(batch->_assetNameTexture);
    else if(batch->_assetNameFont)
    {
        korl_time_probeStart(vulkan_use_texture);
        korl_vulkan_useTexture(batch->_fontTextureHandle);
        korl_time_probeStop(vulkan_use_texture);
        vertexIndexCount = korl_checkCast_u$_to_u32(6*batch->_textVisibleCharacterCount * (batch->_textPixelOutline > 0.f ? 2 : 1));
        vertexCount      = korl_checkCast_u$_to_u32(4*batch->_textVisibleCharacterCount * (batch->_textPixelOutline > 0.f ? 2 : 1));
        korl_assert(vertexIndexCount <= batch->_vertexIndexCount);
        korl_assert(vertexCount      <= batch->_vertexCount);
    }
    korl_time_probeStart(vulkan_set_model);
    korl_vulkan_setModel(batch->_position, batch->_rotation, batch->_scale);
    korl_time_probeStop(vulkan_set_model);
    korl_time_probeStart(vulkan_set_depthTest);
    korl_vulkan_batchSetUseDepthTestAndWriteDepthBuffer(!(flags & KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST));
    korl_time_probeStop(vulkan_set_depthTest);
    korl_time_probeStart(vulkan_set_blend);
    korl_vulkan_batchBlend(!(flags & KORL_GFX_BATCH_FLAG_DISABLE_BLENDING), 
                           batch->opColor, batch->factorColorSource, batch->factorColorTarget, 
                           batch->opAlpha, batch->factorAlphaSource, batch->factorAlphaTarget);
    korl_time_probeStop(vulkan_set_blend);
    korl_time_probeStart(vulkan_batch);
    korl_vulkan_batch(batch->primitiveType, 
        vertexIndexCount, batch->_vertexIndices, 
        vertexCount, batch->_vertexPositions, batch->_vertexColors, batch->_vertexUvs);
    korl_time_probeStop(vulkan_batch);
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_TEXTURED(korl_gfx_createBatchRectangleTextured)
{
    /* calculate required amount of memory for the batch */
    const u$ assetNameTextureSize = korl_memory_stringSize(assetNameTexture) + 1;
    const u$ assetNameTextureBytes = assetNameTextureSize * sizeof(*assetNameTexture);
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + assetNameTextureBytes
        + 6 * sizeof(Korl_Vulkan_VertexIndex)
        + 4 * sizeof(Korl_Vulkan_Position)
        + 4 * sizeof(Korl_Vulkan_Uv);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle   = allocatorHandle;
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount = 6;
    result->_vertexCount      = 4;
    result->opColor           = KORL_BLEND_OP_ADD;
    result->factorColorSource = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha           = KORL_BLEND_OP_ADD;
    result->factorAlphaSource = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget = KORL_BLEND_FACTOR_ZERO;
    result->_assetNameTexture = KORL_C_CAST(wchar_t*, result + 1);
    result->_vertexIndices    = KORL_C_CAST(Korl_Vulkan_VertexIndex*, KORL_C_CAST(u8*, result->_assetNameTexture) + assetNameTextureBytes);
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*   , KORL_C_CAST(u8*, result->_vertexIndices   ) + 6*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexUvs        = KORL_C_CAST(Korl_Vulkan_Uv*         , KORL_C_CAST(u8*, result->_vertexPositions ) + 4*sizeof(Korl_Vulkan_Position));
    /* initialize the batch's dynamic data */
    if(korl_memory_stringCopy(assetNameTexture, result->_assetNameTexture, assetNameTextureSize) != korl_checkCast_u$_to_i$(assetNameTextureSize))
        korl_log(ERROR, "failed to copy asset name \"%ls\" to batch", assetNameTexture);
    korl_shared_const Korl_Vulkan_VertexIndex vertexIndices[] = 
        { 0, 1, 3
        , 1, 2, 3 };
    korl_memory_copy(result->_vertexIndices, vertexIndices, sizeof(vertexIndices));
    korl_shared_const Korl_Vulkan_Position vertexPositions[] = 
        { {-0.5f, -0.5f, 0.f}
        , { 0.5f, -0.5f, 0.f}
        , { 0.5f,  0.5f, 0.f}
        , {-0.5f,  0.5f, 0.f} };
    korl_memory_copy(result->_vertexPositions, vertexPositions, sizeof(vertexPositions));
    // scale the rectangle mesh vertices by the provided size //
    for(u$ i = 0; i < korl_arraySize(vertexPositions); ++i)
        korl_math_v2f32_assignMultiply(&result->_vertexPositions[i].xy, size);
    korl_shared_const Korl_Vulkan_Uv vertexTextureUvs[] = 
        { {0, 1}
        , {1, 1}
        , {1, 0}
        , {0, 0} };
    korl_memory_copy(result->_vertexUvs, vertexTextureUvs, sizeof(vertexTextureUvs));
    /* return the batch */
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_COLORED(korl_gfx_createBatchRectangleColored)
{
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 6 * sizeof(Korl_Vulkan_VertexIndex)
        + 4 * sizeof(Korl_Vulkan_Position)
        + 4 * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle   = allocatorHandle;
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount = 6;
    result->_vertexCount      = 4;
    result->opColor           = KORL_BLEND_OP_ADD;
    result->factorColorSource = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha           = KORL_BLEND_OP_ADD;
    result->factorAlphaSource = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget = KORL_BLEND_FACTOR_ZERO;
    result->_vertexIndices    = KORL_C_CAST(Korl_Vulkan_VertexIndex*, result + 1);
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*   , KORL_C_CAST(u8*, result->_vertexIndices   ) + 6*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexColors     = KORL_C_CAST(Korl_Vulkan_Color4u8*   , KORL_C_CAST(u8*, result->_vertexPositions ) + 4*sizeof(Korl_Vulkan_Position));
    /* initialize the batch's dynamic data */
    korl_shared_const Korl_Vulkan_VertexIndex vertexIndices[] = 
        { 0, 1, 3
        , 1, 2, 3 };
    korl_memory_copy(result->_vertexIndices, vertexIndices, sizeof(vertexIndices));
    korl_shared_const Korl_Vulkan_Position vertexPositions[] = 
        { {0.f, 0.f, 0.f}
        , {1.f, 0.f, 0.f}
        , {1.f, 1.f, 0.f}
        , {0.f, 1.f, 0.f} };
    korl_memory_copy(result->_vertexPositions, vertexPositions, sizeof(vertexPositions));
    // scale & offset the rectangle mesh vertices by the provided params //
    for(u$ i = 0; i < korl_arraySize(vertexPositions); ++i)
    {
        korl_math_v2f32_assignSubtract(&result->_vertexPositions[i].xy, localOriginNormal);
        korl_math_v2f32_assignMultiply(&result->_vertexPositions[i].xy, size);
    }
    for(u$ c = 0; c < result->_vertexCount; c++)
        result->_vertexColors[c] = color;
    /**/
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_CIRCLE(korl_gfx_createBatchCircle)
{
    /* we can't really make a circle shape with < 3 points around the circumference */
    korl_assert(pointCount >= 3);
    /* calculate required amount of memory for the batch */
    const u$ indices = 3*pointCount;//KORL-PERFORMANCE-000-000-018: GFX; (MINOR) use triangle fan primitive for less vertex indices
    const u$ vertices = 1/*for center vertex*/+pointCount;
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + indices * sizeof(Korl_Vulkan_VertexIndex)
        + vertices * sizeof(Korl_Vulkan_Position)
        + vertices * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle   = allocatorHandle;
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;//KORL-PERFORMANCE-000-000-018: GFX; (MINOR) use triangle fan primitive for less vertex indices
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount = korl_checkCast_u$_to_u32(indices);
    result->_vertexCount      = korl_checkCast_u$_to_u32(vertices);
    result->opColor           = KORL_BLEND_OP_ADD;
    result->factorColorSource = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha           = KORL_BLEND_OP_ADD;
    result->factorAlphaSource = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget = KORL_BLEND_FACTOR_ZERO;
    result->_vertexIndices    = KORL_C_CAST(Korl_Vulkan_VertexIndex*, result + 1);
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*   , KORL_C_CAST(u8*, result->_vertexIndices   ) + indices*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexColors     = KORL_C_CAST(Korl_Vulkan_Color4u8*   , KORL_C_CAST(u8*, result->_vertexPositions ) + vertices*sizeof(Korl_Vulkan_Position));
    /* initialize the batch's dynamic data */
    // vertex[0] is always the center point of the circle //
    result->_vertexPositions[0] = (Korl_Vulkan_Position){0,0,0};
    const f32 deltaRadians = 2*KORL_PI32 / pointCount;
    for(u32 p = 0; p < pointCount; p++)
    {
        const f32 spokeRadians = p*deltaRadians;
        const Korl_Math_V2f32 spoke = korl_math_v2f32_fromRotationZ(radius, spokeRadians);
        result->_vertexPositions[1 + p] = (Korl_Vulkan_Position){spoke.x, spoke.y, 0.f};
    }
    for(u$ c = 0; c < vertices; c++)
        result->_vertexColors[c] = color;
    for(u32 p = 0; p < pointCount; p++)
    {
        result->_vertexIndices[3*p + 0] = 0;
        result->_vertexIndices[3*p + 1] = korl_vulkan_safeCast_u$_to_vertexIndex(p + 1);
        result->_vertexIndices[3*p + 2] = korl_vulkan_safeCast_u$_to_vertexIndex(((p + 1) % pointCount) + 1);
    }
    /**/
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_TRIANGLES(korl_gfx_createBatchTriangles)
{
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 3*triangleCount * sizeof(Korl_Vulkan_Position)
        + 3*triangleCount * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle   = allocatorHandle;
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexCount      = 3*triangleCount;
    result->opColor           = KORL_BLEND_OP_ADD;
    result->factorColorSource = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha           = KORL_BLEND_OP_ADD;
    result->factorAlphaSource = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget = KORL_BLEND_FACTOR_ZERO;
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*, result + 1);
    result->_vertexColors     = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, result->_vertexPositions) + 3*triangleCount*sizeof(Korl_Vulkan_Position));
    /* return the batch */
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_LINES(korl_gfx_createBatchLines)
{
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 2*lineCount * sizeof(Korl_Vulkan_Position)
        + 2*lineCount * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle   = allocatorHandle;
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_LINES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexCount      = 2*lineCount;
    result->opColor           = KORL_BLEND_OP_ADD;
    result->factorColorSource = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha           = KORL_BLEND_OP_ADD;
    result->factorAlphaSource = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget = KORL_BLEND_FACTOR_ZERO;
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*, result + 1);
    result->_vertexColors     = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, result->_vertexPositions) + 2*lineCount*sizeof(Korl_Vulkan_Position));
    /* return the batch */
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_TEXT(korl_gfx_createBatchText)
{
    const Korl_Vulkan_Color4u8 color        = {255,255,255,255};///@TODO: make this a parameter
    const Korl_Vulkan_Color4u8 colorOutline = {  0,  0,  0,255};///@TODO: make this a parameter
    const f32 outlinePixelSize = 3.f;                           ///@TODO: make this a parameter
    korl_assert(text);
    korl_assert(textPixelHeight  >= 1.f);
    korl_assert(outlinePixelSize >= 0.f);
    if(!assetNameFont)
        assetNameFont = L"test-assets/source-sans/SourceSans3-Semibold.otf";
    /* calculate required amount of memory for the batch */
    const u$ assetNameFontBufferSize = korl_memory_stringSize(assetNameFont) + 1;
    const u$ assetNameFontBytes = assetNameFontBufferSize * sizeof(*assetNameFont);
    /** This value directly affects how much memory will be allocated for vertex 
     * index/attribute data.  Note that it is strictly an over-estimate, because 
     * we cannot possibly know ahead of time whether or not any particular glyph 
     * contains visible components until the font asset is loaded, and we are 
     * doing a sort of lazy-loading system here where the user is allowed to 
     * create & manipulate text batches even in the absence of a font asset. */
    const u$ textSize = korl_memory_stringSize(text);
    const u$ textBufferSize = textSize + 1;
    const u$ textBytes = textBufferSize * sizeof(*text);
    const u$ maxVisibleGlyphCount = textSize * (outlinePixelSize > 0.f ? 2 : 1);
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + assetNameFontBytes
        + textBytes
        + maxVisibleGlyphCount * 6*sizeof(Korl_Vulkan_VertexIndex)
        + maxVisibleGlyphCount * 4*sizeof(Korl_Vulkan_Position)
        + maxVisibleGlyphCount * 4*sizeof(Korl_Vulkan_Color4u8)
        + maxVisibleGlyphCount * 4*sizeof(Korl_Vulkan_Uv);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle   = allocatorHandle;
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount = korl_checkCast_u$_to_u32(maxVisibleGlyphCount*6);
    result->_vertexCount      = korl_checkCast_u$_to_u32(maxVisibleGlyphCount*4);
    result->_textPixelHeight  = textPixelHeight;
    result->_textPixelOutline = outlinePixelSize;
    result->_textColor        = color;
    result->_textColorOutline = colorOutline;
    result->_assetNameFont    = KORL_C_CAST(wchar_t*, result + 1);
    result->_text             = KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, result->_assetNameFont) + assetNameFontBytes);
    result->opColor           = KORL_BLEND_OP_ADD;
    result->factorColorSource = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha           = KORL_BLEND_OP_ADD;
    result->factorAlphaSource = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget = KORL_BLEND_FACTOR_ZERO;
    result->_vertexIndices    = KORL_C_CAST(Korl_Vulkan_VertexIndex*, KORL_C_CAST(u8*, result->_text) + textBytes);
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*   , KORL_C_CAST(u8*, result->_vertexIndices  ) + maxVisibleGlyphCount*6*sizeof(*result->_vertexIndices));
    result->_vertexColors     = KORL_C_CAST(Korl_Vulkan_Color4u8*   , KORL_C_CAST(u8*, result->_vertexPositions) + maxVisibleGlyphCount*4*sizeof(*result->_vertexPositions));
    result->_vertexUvs        = KORL_C_CAST(Korl_Vulkan_Uv*         , KORL_C_CAST(u8*, result->_vertexColors   ) + maxVisibleGlyphCount*4*sizeof(*result->_vertexColors));
    /* initialize the batch's dynamic data */
    if(    korl_memory_stringCopy(assetNameFont, result->_assetNameFont, assetNameFontBufferSize) 
        != korl_checkCast_u$_to_i$(assetNameFontBufferSize))
        korl_log(ERROR, "failed to copy asset name \"%ls\" to batch", assetNameFont);
    if(    korl_memory_stringCopy(text, result->_text, textBufferSize) 
        != korl_checkCast_u$_to_i$(textBufferSize))
        korl_log(ERROR, "failed to copy text \"%ls\" to batch", text);
    /* We can _not_ precompute vertex indices, see next comment for details... */
    /* We can _not_ precompute vertex colors, because we still don't know how 
        many visible characters in the text there are going to be, and the glyph 
        outlines _must_ be contiguous with the actual text glyphs, as we're 
        sending this to the renderer as a single draw call (one primitive set) */
    // make an initial attempt to generate the text mesh using the font asset, 
    //  and if the asset isn't available at the moment, there is nothing we can 
    //  do about it for now except defer until a later time //
    _korl_gfx_textGenerateMesh(result, KORL_ASSETCACHE_GET_FLAGS_LAZY);
    /* return the batch */
    return result;
}
korl_internal KORL_PLATFORM_GFX_BATCH_SET_BLEND_STATE(korl_gfx_batchSetBlendState)
{
    context->opColor = opColor;
    context->factorColorSource = factorColorSource;
    context->factorColorTarget = factorColorTarget;
    context->opAlpha = opAlpha;
    context->factorAlphaSource = factorAlphaSource;
    context->factorAlphaTarget = factorAlphaTarget;
}
korl_internal KORL_PLATFORM_GFX_BATCH_SET_POSITION(korl_gfx_batchSetPosition)
{
    context->_position = position;
}
korl_internal KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D(korl_gfx_batchSetPosition2d)
{
    context->_position.x = x;
    context->_position.y = y;
}
korl_internal KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D_V2F32(korl_gfx_batchSetPosition2dV2f32)
{
    context->_position.xy = position;
}
korl_internal KORL_PLATFORM_GFX_BATCH_SET_SCALE(korl_gfx_batchSetScale)
{
    context->_scale = scale;
}
korl_internal KORL_PLATFORM_GFX_BATCH_SET_QUATERNION(korl_gfx_batchSetQuaternion)
{
    context->_rotation = quaternion;
}
korl_internal KORL_PLATFORM_GFX_BATCH_SET_ROTATION(korl_gfx_batchSetRotation)
{
    context->_rotation = korl_math_quaternion_fromAxisRadians(axisOfRotation, radians, false);
}
korl_internal KORL_PLATFORM_GFX_BATCH_SET_VERTEX_COLOR(korl_gfx_batchSetVertexColor)
{
    korl_assert(context->_vertexCount > vertexIndex);
    korl_assert(context->_vertexColors);
    context->_vertexColors[vertexIndex] = color;
}
korl_internal KORL_PLATFORM_GFX_BATCH_ADD_LINE(korl_gfx_batchAddLine)
{
    korl_assert(pContext);
    korl_assert(*pContext);
    const u32 newVertexCount = (*pContext)->_vertexCount + 2;
    const u$ newTotalBytes = sizeof(Korl_Gfx_Batch)
        + newVertexCount * sizeof(Korl_Vulkan_Position)
        + newVertexCount * sizeof(Korl_Vulkan_Color4u8);
    (*pContext) = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_reallocate((*pContext)->allocatorHandle, *pContext, newTotalBytes));
    void*const previousVertexColors = (*pContext)->_vertexColors;
    korl_assert(*pContext);
    (*pContext)->_vertexPositions = KORL_C_CAST(Korl_Vulkan_Position*, (*pContext) + 1);
    (*pContext)->_vertexColors    = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, (*pContext)->_vertexPositions) + newVertexCount*sizeof(Korl_Vulkan_Position));
    korl_memory_move((*pContext)->_vertexColors, previousVertexColors, (*pContext)->_vertexCount*sizeof(Korl_Vulkan_Color4u8));
    (*pContext)->_vertexCount     = newVertexCount;
    /* set the properties of the new line */
    (*pContext)->_vertexPositions[newVertexCount - 2] = p0;
    (*pContext)->_vertexPositions[newVertexCount - 1] = p1;
    (*pContext)->_vertexColors[newVertexCount - 2] = color0;
    (*pContext)->_vertexColors[newVertexCount - 1] = color1;
}
korl_internal KORL_PLATFORM_GFX_BATCH_SET_LINE(korl_gfx_batchSetLine)
{
    korl_assert(context->_vertexCount > 2*lineIndex + 1);
    korl_assert(context->_vertexColors);
    context->_vertexPositions[2*lineIndex + 0] = p0;
    context->_vertexPositions[2*lineIndex + 1] = p1;
    context->_vertexColors[2*lineIndex + 0] = color;
    context->_vertexColors[2*lineIndex + 1] = color;
}
korl_internal KORL_PLATFORM_GFX_BATCH_TEXT_GET_AABB(korl_gfx_batchTextGetAabb)
{
    korl_assert(batchContext->_text && batchContext->_assetNameFont);
    _korl_gfx_textGenerateMesh(batchContext, KORL_ASSETCACHE_GET_FLAGS_LAZY);
    if(!batchContext->_fontTextureHandle)
        return (Korl_Math_Aabb2f32){{0, 0}, {0, 0}};
    ///@TODO: use font asset metrics instead of manually accumulating AABB from vertex data
    Korl_Math_Aabb2f32 result = {.min = {KORL_F32_MAX, KORL_F32_MAX}, .max = {KORL_F32_MIN, KORL_F32_MIN}};
    for(u$ c = 0; c < batchContext->_vertexCount; c++)
    {
        result.min = korl_math_v2f32_min(result.min, batchContext->_vertexPositions[c].xy);
        result.max = korl_math_v2f32_max(result.max, batchContext->_vertexPositions[c].xy);
    }
    return result;
}
korl_internal KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_SIZE(korl_gfx_batchRectangleSetSize)
{
    korl_assert(context->_vertexCount == 4 && context->_vertexIndexCount == 6);
    const Korl_Math_V2f32 originalSize = korl_math_v2f32_subtract(context->_vertexPositions[2].xy, context->_vertexPositions[0].xy);
    for(u$ c = 0; c < context->_vertexCount; c++)
        context->_vertexPositions[c].xy = korl_math_v2f32_multiply(size, korl_math_v2f32_divide(context->_vertexPositions[c].xy, originalSize));
}
korl_internal KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_COLOR(korl_gfx_batchRectangleSetColor)
{
    korl_assert(context->_vertexCount == 4 && context->_vertexIndexCount == 6);
    korl_assert(context->_vertexColors);
    for(u$ c = 0; c < context->_vertexCount; c++)
        context->_vertexColors[c] = color;
}
