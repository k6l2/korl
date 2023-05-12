#include "korl-gfx.h"
#ifdef KORL_PLATFORM
#include "korl-log.h"
#include "korl-assetCache.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-vulkan.h"
#include "korl-time.h"
#include "korl-stb-truetype.h"
#include "korl-stb-ds.h"
#include "korl-stb-image.h"
#include "korl-stringPool.h"
#include "korl-resource.h"
#include "korl-string.h"
#if defined(_LOCAL_STRING_POOL_POINTER)
#   undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_gfx_context.stringPool))
#define _KORL_GFX_DEBUG_LOG_GLYPH_PAGE_BITMAPS 0
/* Since we expect that all KORL renderer code requires right-handed 
    triangle normals (counter-clockwise vertex winding), all tri quads have 
    the following formation:
    [3]-[2]
     | \ | 
    [0]-[1] */
korl_global_const Korl_Vulkan_VertexIndex _KORL_GFX_TRI_QUAD_INDICES[] = 
    { 0, 1, 3
    , 1, 2, 3 };
korl_global_const u8 _KORL_GFX_POSITION_DIMENSIONS = 3;// for now, we will always store 3D position data
typedef struct _Korl_Gfx_FontGlyphBakedCharacterBoundingBox
{
    u16 x0,y0,x1,y1; // coordinates of bbox in bitmap
    /** For non-outline glyphs, this value will always be == to \c -bearingLeft 
     * of the baked character */
    f32 offsetX;
    /** How much we need to offset the glyph mesh to center it on the baseline 
     * cursor origin. */
    f32 offsetY;
} _Korl_Gfx_FontGlyphBakedCharacterBoundingBox;
/** Basically just an extension of stbtt_bakedchar */
typedef struct _Korl_Gfx_FontBakedGlyph
{
    _Korl_Gfx_FontGlyphBakedCharacterBoundingBox bbox;
    _Korl_Gfx_FontGlyphBakedCharacterBoundingBox bboxOutline;
    f32 advanceX;   // pre-scaled by glyph scale at a particular pixelHeight value
    f32 bearingLeft;// pre-scaled by glyph scale at a particular pixelHeight value
    u32 codePoint;
    int glyphIndex;
    u32 bakeOrder;// the first glyph that gets baked will be 0, the next will be 1, etc...; can be useful for instanced glyph rendering
    bool isEmpty;// true if stbtt_IsGlyphEmpty for this glyph returned true
} _Korl_Gfx_FontBakedGlyph;
typedef struct _Korl_Gfx_FontBakedGlyphMap
{
    u32 key;// unicode codepoint
    _Korl_Gfx_FontBakedGlyph value;
} _Korl_Gfx_FontBakedGlyphMap;
typedef struct _Korl_Gfx_FontGlyphBitmapPackRow
{
    u16 offsetY;
    u16 offsetX;
    u16 sizeY;// _including_ padding
} _Korl_Gfx_FontGlyphBitmapPackRow;
typedef struct _Korl_Gfx_FontGlyphVertex
{
    Korl_Math_V4f32 position2d_uv;
} _Korl_Gfx_FontGlyphVertex;
typedef struct _Korl_Gfx_FontGlyphInstance
{
    u32             meshIndex;
    Korl_Math_V2f32 position;
} _Korl_Gfx_FontGlyphInstance;
typedef struct _Korl_Gfx_FontGlyphPage
{
    u16                        packRowsSize;
    u16                        packRowsCapacity;
    u32                        byteOffsetPackRows;// _Korl_Gfx_FontGlyphBitmapPackRow*; emplaced after `data` in memory
    Korl_Resource_Handle       resourceHandleTexture;
    Korl_Resource_Handle       resourceHandleSsboGlyphMeshVertices;
    u16                        dataSquareSize;
    u32                        byteOffsetData;//u8*; 1-channel, Alpha8 format, with an array size of dataSquareSize*dataSquareSize; currently being stored in contiguously memory immediately following this struct
    bool                       textureOutOfDate;// when this flag is set, it means that there are pending changes to `data` which have yet to be uploaded to the GPU
    _Korl_Gfx_FontGlyphVertex* stbDaGlyphMeshVertices;
} _Korl_Gfx_FontGlyphPage;
#define _korl_gfx_fontGlyphPage_getData(pFontGlyphPage) (KORL_C_CAST(u8*, (pFontGlyphPage)) + (pFontGlyphPage)->byteOffsetData)
#define _korl_gfx_fontGlyphPage_getPackRows(pFontGlyphPage) KORL_C_CAST(_Korl_Gfx_FontGlyphBitmapPackRow*, KORL_C_CAST(u8*, (pFontGlyphPage)) + (pFontGlyphPage)->byteOffsetPackRows)
// KORL-ISSUE-000-000-129: gfx/font: move all "FontCache" functionality into korl-resource, replace all "fontAssetName" strings in korl-gfx data structures with FONT Korl_Resource_Handles; this has gotten ridiculous: all the FontCache stuff should be managed by korl-resource because we need to be able to invalidate/reload this data properly when the underlying font asset is hot-reloaded, or when we load a memoryState or something; all the FontCache APIs should be `korl_resource_font_*` APIs, or something similar
typedef struct _Korl_Gfx_FontCache
{
    stbtt_fontinfo               fontInfo;
    bool                         fontInfoInitialized;
    f32                          fontScale;// scale calculated from fontInfo & pixelHeight
    /** From STB TrueType documentation: 
     * you should advance the vertical position by "*ascent - *descent + *lineGap" */
    f32                          fontAscent; // adjusted by pixelHeight already; relative to the text baseline; generally positive
    f32                          fontDescent;// adjusted by pixelHeight already; relative to the text baseline; generally negative
    f32                          fontLineGap;// adjusted by pixelHeight already
    f32                          pixelHeight;
    f32                          pixelOutlineThickness;// if this is 0.f, then an outline has not yet been cached in this fontCache glyphPageList
    u32                          byteOffsetFontAssetNameU16;// wchar_t*
    _Korl_Gfx_FontBakedGlyphMap* stbHmGlyphs;
    u32                          byteOffsetGlyphPage;// _Korl_Gfx_FontGlyphPage*
} _Korl_Gfx_FontCache;
#define _korl_gfx_fontCache_getFontAssetName(pFontCache) KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, (pFontCache)) + (pFontCache)->byteOffsetFontAssetNameU16)
#define _korl_gfx_fontCache_getGlyphPage(pFontCache) KORL_C_CAST(_Korl_Gfx_FontGlyphPage*, KORL_C_CAST(u8*, (pFontCache)) + (pFontCache)->byteOffsetGlyphPage)
typedef struct _Korl_Gfx_Context
{
    /** used to store persistent data, such as Font asset glyph cache/database */
    Korl_Memory_AllocatorHandle allocatorHandle;
    _Korl_Gfx_FontCache**       stbDaFontCaches;// KORL-ISSUE-000-000-129: gfx/font: move all "FontCache" functionality into korl-resource, replace all "fontAssetName" strings in korl-gfx data structures with FONT Korl_Resource_Handles
    u8                          nextResourceSalt;
    Korl_StringPool*            stringPool;// used for Resource database strings; Korl_StringPool structs _must_ be unmanaged allocations (allocations with an unchanging memory address), because we're likely going to have a shit-ton of Strings which point to the pool address for convenience
    Korl_Math_V2u32             surfaceSize;// updated at the top of each frame, ideally before anything has a chance to use korl-gfx
    Korl_Gfx_Camera             currentCameraState;
    f32                         seconds;// passed to the renderer as UBO data to allow shader animations; passed when a Camera is used
    KORL_MEMORY_POOL_DECLARE(Korl_Gfx_Light, pendingLights, KORL_VULKAN_MAX_LIGHTS);// after being added to this pool, lights are flushed to the renderer's draw state upon the next call to `korl_gfx_draw`
} _Korl_Gfx_Context;
typedef struct _Korl_Gfx_Text_Line
{
    u32 visibleCharacters;// used to determine how many glyph instances are in the draw call
    Korl_Math_Aabb2f32 modelAabb;
    Korl_Math_V4f32 color;
} _Korl_Gfx_Text_Line;
korl_global_variable _Korl_Gfx_Context* _korl_gfx_context;
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
        _Korl_Gfx_FontGlyphBitmapPackRow*const packRow = _korl_gfx_fontGlyphPage_getPackRows(glyphPage) + p;
        if(packRow->offsetX > glyphPage->dataSquareSize)
            continue;
        const u16 remainingSpaceX = glyphPage->dataSquareSize 
                                  - packRow->offsetX;
        if(    packRow->sizeY  < sizeY + 2*PACK_ROW_PADDING 
            || remainingSpaceX < sizeX + 2*PACK_ROW_PADDING)
            continue;
        if(packRowSizeY > packRow->sizeY)
        {
            packRowIndex = p;
            packRowSizeY = packRow->sizeY;
        }
    }
    /* if we couldn't find a pack row, we need to allocate a new one */
    if(packRowIndex == glyphPage->packRowsCapacity)
    {
        korl_assert(sizeX + 2*PACK_ROW_PADDING <= glyphPage->dataSquareSize);
        if(glyphPage->packRowsSize == 0)
            korl_assert(sizeY + 2*PACK_ROW_PADDING <= glyphPage->dataSquareSize);
        else
            korl_assert(sizeY + 2*PACK_ROW_PADDING + _korl_gfx_fontGlyphPage_getPackRows(glyphPage)[glyphPage->packRowsSize - 1].sizeY <= glyphPage->dataSquareSize);
        korl_assert(glyphPage->packRowsSize < glyphPage->packRowsCapacity);
        packRowIndex = glyphPage->packRowsSize++;
        _Korl_Gfx_FontGlyphBitmapPackRow*const packRow = _korl_gfx_fontGlyphPage_getPackRows(glyphPage) + packRowIndex;
        packRow->sizeY   = korl_checkCast_i$_to_u16(sizeY + 2*PACK_ROW_PADDING);
        packRow->offsetX = PACK_ROW_PADDING;
        if(packRowIndex > 0)
            packRow->offsetY =   (packRow - 1)->offsetY 
                               + (packRow - 1)->sizeY;
        else
            packRow->offsetY = PACK_ROW_PADDING;
    }
    korl_assert(packRowIndex < glyphPage->packRowsSize);
    /* At this point, we should be guaranteed to have a pack row which 
        can fit our SDF glyph!  This means we can generate all of the 
        metrics related to the baked form of this glyph: */
    *out_x0 = _korl_gfx_fontGlyphPage_getPackRows(glyphPage)[packRowIndex].offsetX;
    *out_y0 = _korl_gfx_fontGlyphPage_getPackRows(glyphPage)[packRowIndex].offsetY;
    *out_x1 = korl_checkCast_i$_to_u16(*out_x0 + sizeX);
    *out_y1 = korl_checkCast_i$_to_u16(*out_y0 + sizeY);
    /* update the pack row metrics */
    _korl_gfx_fontGlyphPage_getPackRows(glyphPage)[packRowIndex].offsetX += korl_checkCast_i$_to_u16(sizeX + PACK_ROW_PADDING);
}
korl_internal const _Korl_Gfx_FontBakedGlyph* _korl_gfx_fontCache_getGlyph(_Korl_Gfx_FontCache* fontCache, u32 codePoint)
{
    _Korl_Gfx_Context*const gfxContext      = _korl_gfx_context;
    _Korl_Gfx_FontGlyphPage*const glyphPage = _korl_gfx_fontCache_getGlyphPage(fontCache);
    /* iterate over the glyph page codepoints to see if any match codePoint */
    _Korl_Gfx_FontBakedGlyph* glyph = NULL;
    const ptrdiff_t glyphMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(gfxContext->allocatorHandle), fontCache->stbHmGlyphs, codePoint);
    if(glyphMapIndex >= 0)
        glyph = &(fontCache->stbHmGlyphs[glyphMapIndex].value);
    /* if the glyph was not found, we need to cache it */
    if(!glyph)
    {
        const int glyphIndex = stbtt_FindGlyphIndex(&(fontCache->fontInfo), codePoint);
        int advanceX;
        int bearingLeft;
        stbtt_GetGlyphHMetrics(&(fontCache->fontInfo), glyphIndex, &advanceX, &bearingLeft);
        const u$ firstVertexOffset = arrlenu(glyphPage->stbDaGlyphMeshVertices);
        mcarrsetlen(KORL_STB_DS_MC_CAST(gfxContext->allocatorHandle), glyphPage->stbDaGlyphMeshVertices, arrlenu(glyphPage->stbDaGlyphMeshVertices) + 4);
        mchmput(KORL_STB_DS_MC_CAST(gfxContext->allocatorHandle), fontCache->stbHmGlyphs, codePoint, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Gfx_FontBakedGlyph));
        glyph = &((fontCache->stbHmGlyphs + hmlen(fontCache->stbHmGlyphs) - 1)->value);
        glyph->codePoint   = codePoint;
        glyph->glyphIndex  = glyphIndex;
        glyph->bakeOrder   = korl_checkCast_u$_to_u32(hmlenu(fontCache->stbHmGlyphs) - 1);
        glyph->isEmpty     = stbtt_IsGlyphEmpty(&(fontCache->fontInfo), glyphIndex);
        glyph->advanceX    = fontCache->fontScale*KORL_C_CAST(f32, advanceX   );
        glyph->bearingLeft = fontCache->fontScale*KORL_C_CAST(f32, bearingLeft);
        if(!glyph->isEmpty)
        {
            /* bake the regular glyph */
            {
                int ix0, iy0, ix1, iy1;
                stbtt_GetGlyphBitmapBox(&(fontCache->fontInfo), glyphIndex, fontCache->fontScale, fontCache->fontScale, &ix0, &iy0, &ix1, &iy1);
                const int w = ix1 - ix0;
                const int h = iy1 - iy0;
                _korl_gfx_glyphPage_insert(glyphPage, w, h
                                          ,&glyph->bbox.x0
                                          ,&glyph->bbox.y0
                                          ,&glyph->bbox.x1
                                          ,&glyph->bbox.y1);
                glyph->bbox.offsetX = glyph->bearingLeft;
                glyph->bbox.offsetY = -KORL_C_CAST(f32, iy1);
                /* actually write the glyph bitmap in the glyph page data */
                stbtt_MakeGlyphBitmap(&(fontCache->fontInfo)
                                     ,_korl_gfx_fontGlyphPage_getData(glyphPage) + (glyph->bbox.y0*glyphPage->dataSquareSize + glyph->bbox.x0)
                                     ,w, h, glyphPage->dataSquareSize
                                     ,fontCache->fontScale, fontCache->fontScale, glyphIndex);
                /* create the glyph mesh vertices & indices, and append them to 
                    the appropriate stb_ds array to be uploaded to the GPU later */
                const f32 x0 = 0.f/*start glyph at baseline cursor origin*/ + glyph->bbox.offsetX;
                const f32 y0 = 0.f/*start glyph at baseline cursor origin*/ + glyph->bbox.offsetY;
                const f32 x1 = x0 + (glyph->bbox.x1 - glyph->bbox.x0);
                const f32 y1 = y0 + (glyph->bbox.y1 - glyph->bbox.y0);
                const f32 u0 = KORL_C_CAST(f32, glyph->bbox.x0) / KORL_C_CAST(f32, glyphPage->dataSquareSize);
                const f32 u1 = KORL_C_CAST(f32, glyph->bbox.x1) / KORL_C_CAST(f32, glyphPage->dataSquareSize);
                const f32 v0 = KORL_C_CAST(f32, glyph->bbox.y0) / KORL_C_CAST(f32, glyphPage->dataSquareSize);
                const f32 v1 = KORL_C_CAST(f32, glyph->bbox.y1) / KORL_C_CAST(f32, glyphPage->dataSquareSize);
                glyphPage->stbDaGlyphMeshVertices[firstVertexOffset + 0] = (_Korl_Gfx_FontGlyphVertex){.position2d_uv = (Korl_Math_V4f32){x0, y0, u0, v1}};
                glyphPage->stbDaGlyphMeshVertices[firstVertexOffset + 1] = (_Korl_Gfx_FontGlyphVertex){.position2d_uv = (Korl_Math_V4f32){x1, y0, u1, v1}};
                glyphPage->stbDaGlyphMeshVertices[firstVertexOffset + 2] = (_Korl_Gfx_FontGlyphVertex){.position2d_uv = (Korl_Math_V4f32){x1, y1, u1, v0}};
                glyphPage->stbDaGlyphMeshVertices[firstVertexOffset + 3] = (_Korl_Gfx_FontGlyphVertex){.position2d_uv = (Korl_Math_V4f32){x0, y1, u0, v0}};
            }
            /* bake the glyph outline if the font is configured for outlines */
            if(!korl_math_isNearlyZero(fontCache->pixelOutlineThickness))
            {
                //KORL-ISSUE-000-000-094: gfx: font outlines using new Vulkan rendering APIs are not complete
                korl_assert(!"not implemented: populate the stbDaGlyphMeshVertices with the outlined glyph");
#if 1
                /* Originally I was using stb_truetype's SDF API to generate 
                    glyph outlines, but as it turns out, that API just seems to 
                    not work very well.  So instead, I'm going to try doing my 
                    own janky shit and just use the original glyph bitmap in a 
                    buffer, and then "expand" the bitmap according to the 
                    desired outline thickness inside of a separate buffer.  */
                /* determine how big the outline glyph needs to be based on the 
                    original glyph size, since it _should_ be safe to assume 
                    that all original glyphs are baked at this point */
                const int w = glyph->bbox.x1 - glyph->bbox.x0;
                const int h = glyph->bbox.y1 - glyph->bbox.y0;
                const int outlineOffset = KORL_C_CAST(int, korl_math_f32_ceiling(fontCache->pixelOutlineThickness));
                const int outlineW = w + 2*outlineOffset;
                const int outlineH = h + 2*outlineOffset;
                /* Allocate a temp buffer to store the outline glyph.  
                    Technically, we don't _need_ to have a temp buffer as we 
                    could just simply use the glyph page data and operate on 
                    that memory, but we are going to be iterating over all 
                    pixels of the glyph multiple times, so that would likely be 
                    significantly more costly in memory operations (worse cache 
                    locality). */
                u8*const outlineBuffer = korl_allocate(gfxContext->allocatorHandle, (outlineW * outlineH) * sizeof(*outlineBuffer));
#if KORL_DEBUG
                korl_assert(korl_memory_isNull(outlineBuffer, (outlineW * outlineH) * sizeof(*outlineBuffer)));
#endif
                /* copy the baked glyph to the temp buffer, while making each 
                    non-zero pixel fully opaque */
                for(int y = 0; y < h; y++)
                {
                    const int pageY = glyph->bbox.y0 + y;
                    for(int x = 0; x < w; x++)
                    {
                        const int pageX = glyph->bbox.x0 + x;
                        if(_korl_gfx_fontGlyphPage_getData(_korl_gfx_fontCache_getGlyphPage(fontCache))[pageY*_korl_gfx_fontCache_getGlyphPage(fontCache)->dataSquareSize + pageX])
                            outlineBuffer[(outlineOffset+y)*outlineW + (outlineOffset+x)] = 1;
                    }
                }
                /* while we still haven't satisfied the font's pixel outline 
                    thickness, iterate over each pixel and set empty pixels that 
                    neighbor non-zero pixels to be the value of iteration 
                    (brushfire expansion of the glyph) */
                for(int t = 0; t < outlineOffset; t++)
                    for(int y = 0; y < outlineH; y++)
                        for(int x = 0; x < outlineW; x++)
                        {
                            if(outlineBuffer[y*outlineW + x])
                                // only expand to unmodified pixels
                                continue;
                            // if any neighbor pixels are from the previous brushfire step, 
                            //  we set this pixel to be in the current brushfire step:
                            if(   (x > 0            && outlineBuffer[y*outlineW + (x - 1)] == t+1)
                               || (x < outlineW - 1 && outlineBuffer[y*outlineW + (x + 1)] == t+1)
                               || (y > 0            && outlineBuffer[(y - 1)*outlineW + x] == t+1)
                               || (y < outlineH - 1 && outlineBuffer[(y + 1)*outlineW + x] == t+1))
                                outlineBuffer[y*outlineW + x] = korl_checkCast_i$_to_u8(t+2);
                        }
                /* iterate over each pixel one final time, assigning final 
                    values to the "brushfire" result pixels */
                //KORL-ISSUE-000-000-072: gfx/fontCache: glyph outlines don't utilize f32 accuracy of the outline thickness parameter
                for(int y = 0; y < outlineH; y++)
                    for(int x = 0; x < outlineW; x++)
                        if(outlineBuffer[y*outlineW + x])
                            outlineBuffer[y*outlineW + x] = KORL_U8_MAX;
                /* update our baked character outline meta data */
                _korl_gfx_glyphPage_insert(glyphPage, outlineW, outlineH, 
                                           &glyph->bboxOutline.x0, 
                                           &glyph->bboxOutline.y0, 
                                           &glyph->bboxOutline.x1, 
                                           &glyph->bboxOutline.y1);
                glyph->bboxOutline.offsetX = glyph->bbox.offsetX - outlineOffset;
                glyph->bboxOutline.offsetY = glyph->bbox.offsetY - outlineOffset;
                /* emplace the "outline" glyph into the glyph page */
                for(int y = 0; y < outlineH; y++)
                    for(int x = 0; x < outlineW; x++)
                    {
                        u8*const dataPixel = _korl_gfx_fontGlyphPage_getData(glyphPage) + ((glyph->bboxOutline.y0 + y)*glyphPage->dataSquareSize + glyph->bboxOutline.x0 + x);
                        *dataPixel = outlineBuffer[y*outlineW + x];
                    }
                /* free temporary resources */
                korl_free(gfxContext->allocatorHandle, outlineBuffer);
#else// This technique looks like poop, and I can't seem to change that, so maybe just delete this section?
                const u8 on_edge_value     = 180;// This is rather arbitrary; it seems like we can just select any reasonable # here and it will be enough for most fonts/sizes.  This value seems rather finicky however...  So if we have to modify how any of this glyph outline stuff is drawn, this will likely have to change somehow.
                const int padding          = 1/*this is necessary to keep the SDF glyph outline from bleeding all the way to the bitmap edge*/ + KORL_C_CAST(int, korl_math_f32_ceiling(fontCache->pixelOutlineThickness));
                const f32 pixel_dist_scale = KORL_C_CAST(f32, on_edge_value)/KORL_C_CAST(f32, padding);
                int w, h, offsetX, offsetY;
                u8* bitmapSdf = stbtt_GetGlyphSDF(&(fontCache->fontInfo), 
                                                  fontCache->fontScale, 
                                                  glyphIndex, padding, on_edge_value, pixel_dist_scale, 
                                                  &w, &h, &offsetX, &offsetY);
                korl_assert(bitmapSdf);
                /* update the baked character outline meta data */
                _korl_gfx_glyphPage_insert(glyphPage, w, h, 
                                           &glyph->bboxOutline.x0, 
                                           &glyph->bboxOutline.y0, 
                                           &glyph->bboxOutline.x1, 
                                           &glyph->bboxOutline.y1);
                glyph->bboxOutline.offsetX =  KORL_C_CAST(f32,     offsetX);
                glyph->bboxOutline.offsetY = -KORL_C_CAST(f32, h + offsetY);
                /* place the processed SDF glyph into the glyph page */
                for(int cy = 0; cy < h; cy++)
                    for(int cx = 0; cx < w; cx++)
                    {
                        u8*const dataPixel = glyphPage->data + ((glyph->bboxOutline.y0 + cy)*glyphPage->dataSquareSize + glyph->bboxOutline.x0 + cx);
                        // only use SDF pixels that are >= the outline thickness edge value
                        if(bitmapSdf[cy*w + cx] >= KORL_C_CAST(f32, on_edge_value) - (fontCache->pixelOutlineThickness*pixel_dist_scale))
                        {
#if 1
                            *dataPixel = 255;
#else// smoothing aside, these SDF bitmaps just don't look very good... after playing around with this stuff, it is looking like this technique is just unusable ☹
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
#endif
                        }
                        else
                            *dataPixel = 0;
                    }
                /**/
                stbtt_FreeSDF(bitmapSdf, NULL);
#endif
            }
        }//if(!glyph->isEmpty)
        _korl_gfx_fontCache_getGlyphPage(fontCache)->textureOutOfDate = true;
    }//if(!glyph)// create new glyph code
    // at this point, we should have a valid glyph //
    korl_assert(glyph);
    return glyph;
}
korl_internal _Korl_Gfx_FontCache* _korl_gfx_matchFontCache(acu16 utf16AssetNameFont, f32 textPixelHeight, f32 textPixelOutline)
{
    _Korl_Gfx_Context*const context = _korl_gfx_context;
    /* get the font asset */
    KORL_ZERO_STACK(Korl_AssetCache_AssetData, assetDataFont);
    const Korl_AssetCache_Get_Result resultAssetCacheGetFont = korl_assetCache_get(utf16AssetNameFont.data, KORL_ASSETCACHE_GET_FLAGS_NONE, &assetDataFont);
    if(resultAssetCacheGetFont != KORL_ASSETCACHE_GET_RESULT_LOADED)
        return NULL;
#if KORL_DEBUG && 0// testing stb_truetype bitmap rendering API
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
#endif// testing stb_truetype bitmap rendering API
#if KORL_DEBUG && 0// stb_truetype testing SDF API
    korl_shared_variable bool sdfTestDone = false;
    if(!sdfTestDone)
    {
        korl_shared_const int CODEPOINT = 'C';
        sdfTestDone = true;
        const f32 pixelOutlineThickness = 5.f;
        korl_log(INFO, "===== stb_truetype SDF test =====");
        stbtt_fontinfo fontInfo;
        korl_assert(stbtt_InitFont(&fontInfo, assetDataFont.data, 0/*font offset*/));
        int w, h, offsetX, offsetY;
        const u8 on_edge_value     = 100;// This is rather arbitrary; it seems like we can just select any reasonable # here and it will be enough for most fonts/sizes.  This value seems rather finicky however...  So if we have to modify how any of this glyph outline stuff is drawn, this will likely have to change somehow.
        const int padding          = 1/*this is necessary to keep the SDF glyph outline from bleeding all the way to the bitmap edge*/ + KORL_C_CAST(int, korl_math_f32_ceiling(pixelOutlineThickness));
        const f32 pixel_dist_scale = KORL_C_CAST(f32, on_edge_value)/KORL_C_CAST(f32, padding);
        u8* bitmapSdf = stbtt_GetCodepointSDF(&fontInfo, 
                                              stbtt_ScaleForPixelHeight(&fontInfo, batch->_textPixelHeight), 
                                              CODEPOINT, padding, on_edge_value, pixel_dist_scale, 
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
#endif// stb_truetype testing SDF API
    /* check and see if a matching font cache already exists */
    u$ existingFontCacheIndex = 0;
    korl_time_probeStart(match_font_cache);
    for(; existingFontCacheIndex < arrlenu(context->stbDaFontCaches); existingFontCacheIndex++)
        // find the font cache with a matching font asset name AND render 
        //  parameters such as font pixel height, etc... //
        if(   0 == korl_string_compareUtf16(utf16AssetNameFont.data, _korl_gfx_fontCache_getFontAssetName(context->stbDaFontCaches[existingFontCacheIndex])) 
           && context->stbDaFontCaches[existingFontCacheIndex]->pixelHeight == textPixelHeight 
           && (   context->stbDaFontCaches[existingFontCacheIndex]->pixelOutlineThickness == 0.f // the font cache has not yet been cached with outline glyphs
               || context->stbDaFontCaches[existingFontCacheIndex]->pixelOutlineThickness == textPixelOutline))
            break;
    korl_time_probeStop(match_font_cache);
    _Korl_Gfx_FontCache* fontCache = NULL;
    if(existingFontCacheIndex < arrlenu(context->stbDaFontCaches))
        fontCache = context->stbDaFontCaches[existingFontCacheIndex];
    else// if we could not find a matching fontCache, we need to create one //
    {
        korl_time_probeStart(create_font_cache);
        /* add a new font cache address to the context's font cache address pool */
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaFontCaches, NULL);
        _Korl_Gfx_FontCache**const newFontCache = &arrlast(context->stbDaFontCaches);
        /* calculate how much memory we need */
        korl_shared_const u16 GLYPH_PAGE_SQUARE_SIZE = 512;
        korl_shared_const u16 PACK_ROWS_CAPACITY     = 64;
        const u$ assetNameFontBufferSize = korl_string_sizeUtf16(utf16AssetNameFont.data) + 1/*null terminator*/;
        const u$ assetNameFontBufferBytes = assetNameFontBufferSize*sizeof(*utf16AssetNameFont.data);
        const u$ fontCacheRequiredBytes = sizeof(_Korl_Gfx_FontCache)
                                        + sizeof(_Korl_Gfx_FontGlyphPage)
                                        + PACK_ROWS_CAPACITY*sizeof(_Korl_Gfx_FontGlyphBitmapPackRow)
                                        + GLYPH_PAGE_SQUARE_SIZE*GLYPH_PAGE_SQUARE_SIZE// glyph page bitmap (data)
                                        + assetNameFontBufferBytes;
        /* allocate the required memory from the korl-gfx context allocator */
        *newFontCache = korl_allocate(context->allocatorHandle, fontCacheRequiredBytes);
        korl_assert(*newFontCache);
        fontCache = *newFontCache;
        /* initialize the memory */
        fontCache->pixelHeight                = textPixelHeight;
        fontCache->pixelOutlineThickness      = textPixelOutline;
        fontCache->byteOffsetFontAssetNameU16 = sizeof(*fontCache);
        fontCache->byteOffsetGlyphPage        = sizeof(*fontCache) + korl_checkCast_u$_to_u32(assetNameFontBufferBytes);
        mchmdefault(KORL_STB_DS_MC_CAST(context->allocatorHandle), fontCache->stbHmGlyphs, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Gfx_FontBakedGlyph));
        _Korl_Gfx_FontGlyphPage*const glyphPage = _korl_gfx_fontCache_getGlyphPage(fontCache);
        glyphPage->packRowsCapacity   = PACK_ROWS_CAPACITY;
        glyphPage->dataSquareSize     = GLYPH_PAGE_SQUARE_SIZE;
        glyphPage->byteOffsetData     = sizeof(*glyphPage);
        glyphPage->byteOffsetPackRows = sizeof(*glyphPage) + (GLYPH_PAGE_SQUARE_SIZE * GLYPH_PAGE_SQUARE_SIZE);
        mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), glyphPage->stbDaGlyphMeshVertices, 512);
        korl_assert(korl_checkCast_u$_to_i$(assetNameFontBufferSize) == korl_string_copyUtf16(utf16AssetNameFont.data, (au16){assetNameFontBufferSize, _korl_gfx_fontCache_getFontAssetName(fontCache)}));
        /* initialize render device memory allocations */
        KORL_ZERO_STACK(Korl_Vulkan_CreateInfoTexture, createInfoTexture);
        createInfoTexture.sizeX = glyphPage->dataSquareSize;
        createInfoTexture.sizeY = glyphPage->dataSquareSize;
        glyphPage->resourceHandleTexture = korl_resource_createTexture(&createInfoTexture);
        KORL_ZERO_STACK(Korl_Vulkan_CreateInfoVertexBuffer, createInfo);
        createInfo.bytes              = 1/*placeholder non-zero size, since we don't know how many glyphs we are going to cache*/;
        createInfo.useAsStorageBuffer = true;
        glyphPage->resourceHandleSsboGlyphMeshVertices = korl_resource_createVertexBuffer(&createInfo);
        /**/
        korl_time_probeStop(create_font_cache);
    }
    if(!fontCache->fontInfoInitialized)
    {
        /* initialize the font info using the raw font asset data */
        //KORL-ISSUE-000-000-130: gfx/font: (MAJOR) `fontCache->fontInfo` sets up pointers to data within `assetDataFont.data`; if that data ever moves or gets wiped (defragment, memoryState-load, etc.), then `fontCache->fontInfo` will contain dangling pointers!
        korl_assert(stbtt_InitFont(&(fontCache->fontInfo), assetDataFont.data, 0/*font offset*/));
        fontCache->fontInfoInitialized = true;
        fontCache->fontScale = stbtt_ScaleForPixelHeight(&(fontCache->fontInfo), fontCache->pixelHeight);
        int ascent;
        int descent;
        int lineGap;
        stbtt_GetFontVMetrics(&(fontCache->fontInfo), &ascent, &descent, &lineGap);
        fontCache->fontAscent  = fontCache->fontScale*KORL_C_CAST(f32, ascent );
        fontCache->fontDescent = fontCache->fontScale*KORL_C_CAST(f32, descent);
        fontCache->fontLineGap = fontCache->fontScale*KORL_C_CAST(f32, lineGap);
    }
    return fontCache;
}
/** Vertex order for each glyph quad:
 * [ bottom-left
 * , bottom-right
 * , top-right
 * , top-left ] */
korl_internal void _korl_gfx_textGenerateMesh(Korl_Gfx_Batch*const batch, Korl_AssetCache_Get_Flags assetCacheGetFlags)
{
    _Korl_Gfx_Context*const context = _korl_gfx_context;
    if(!batch->_assetNameFont || batch->_fontTextureHandle)
        return;
    _Korl_Gfx_FontCache* fontCache = _korl_gfx_matchFontCache((acu16){.data = batch->_assetNameFont
                                                                     ,.size = korl_string_sizeUtf16(batch->_assetNameFont)}
                                                             ,batch->_textPixelHeight
                                                             ,batch->_textPixelOutline);
    if(!fontCache)
        return;
    _Korl_Gfx_FontGlyphPage*const fontGlyphPage = _korl_gfx_fontCache_getGlyphPage(fontCache);
    if(batch->_textVisibleCharacterCount)
        goto done_setFontTextureHandle;
    /* iterate over each character in the batch text, and update the 
        pre-allocated vertex attributes to use the data for the corresponding 
        codepoint in the glyph cache */
    Korl_Math_V2f32 textBaselineCursor = (Korl_Math_V2f32){0.f, -fontCache->fontAscent};// default the baseline cursor such that our local origin is placed at _exactly_ the upper-left corner of the text's local AABB based on font metrics, as we know a glyph will never rise _above_ the fontAscent
    u32 currentGlyph = 0;
    const bool outlinePass = (batch->_textPixelOutline > 0.f);
    /* While we're at it, we can also generate the text mesh AABB...
        We need to accumulate the AABB taking into account the following properties:
        - outline thickness
        - font vertical metrics
        - glyph horizontal metrics
        - glyph X bearings */
    batch->_textAabb = KORL_MATH_AABB2F32_EMPTY;
    int glyphIndexPrevious = -1;// used to calculate kerning advance between the previous glyph and the current glyph
    korl_time_probeStart(process_characters);
    for(const wchar_t* character = batch->_text; *character; character++)
    {
        const _Korl_Gfx_FontBakedGlyph*const bakedGlyph = _korl_gfx_fontCache_getGlyph(fontCache, *character);
        // KORL-PERFORMANCE-000-000-039: gfx: apparently calculating kerning advance is _extremely_ expensive when the batch->_text is large; should this be an opt-in option to trade CPU time for font rendering accuracy?  In practice though, we really should _not_ be making huge text batches; we should always be using Gfx_Text objects for large amounts of text!  
        if(textBaselineCursor.x > 0.f)
        {
            const int kernAdvance = stbtt_GetGlyphKernAdvance(&fontCache->fontInfo
                                                             ,glyphIndexPrevious
                                                             ,bakedGlyph->glyphIndex);
            textBaselineCursor.x += fontCache->fontScale*kernAdvance;
        }
        /* set text instanced vertex data */
        KORL_C_CAST(Korl_Math_V2f32*, batch->_instancePositions)[currentGlyph] = textBaselineCursor;
        batch->_instanceU32s[currentGlyph]                                     = bakedGlyph->bakeOrder;
        /* using glyph metrics, we can calculate text AABB & advance the baseline cursor */
        const f32 x0 = textBaselineCursor.x + bakedGlyph->bbox.offsetX;
        const f32 y0 = textBaselineCursor.y + bakedGlyph->bbox.offsetY;
        const f32 x1 = x0 + (bakedGlyph->bbox.x1 - bakedGlyph->bbox.x0);
        const f32 y1 = y0 + (bakedGlyph->bbox.y1 - bakedGlyph->bbox.y0);
        textBaselineCursor.x += bakedGlyph->advanceX;
        if(*character == L'\n')
        {
            textBaselineCursor.x  = 0.f;
            textBaselineCursor.y -= (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
            continue;
        }
        glyphIndexPrevious = bakedGlyph->glyphIndex;
        if(bakedGlyph->isEmpty)
            continue;
        /* at this point, we know this glyph contains graphical components; 
            let's expand the text's local AABB such that the y-axis matches the 
            font's metrics precisely */
        KORL_MATH_ASSIGN_CLAMP_MAX(batch->_textAabb.min.x, x0);
        KORL_MATH_ASSIGN_CLAMP_MIN(batch->_textAabb.max.x, x1);
        KORL_MATH_ASSIGN_CLAMP_MAX(batch->_textAabb.min.y, textBaselineCursor.y + fontCache->fontDescent);
        KORL_MATH_ASSIGN_CLAMP_MIN(batch->_textAabb.max.y, textBaselineCursor.y + fontCache->fontAscent);
        /**/
        currentGlyph++;
        batch->_textVisibleCharacterCount++;
    }
    korl_time_probeStop(process_characters);
    if(outlinePass)
    {
        korl_assert(!"not implemented; this implementation sucked anyway");
    }
    batch->_instanceCount = currentGlyph;
    /* setting the font texture handle to a valid value signifies to other gfx 
        module code that the text mesh has been generated, and thus dependent 
        assets (font, glyph pages) are all loaded & ready to go */
    done_setFontTextureHandle:
    batch->_fontTextureHandle       = fontGlyphPage->resourceHandleTexture;
    batch->_glyphMeshBufferVertices = fontGlyphPage->resourceHandleSsboGlyphMeshVertices;
}
korl_internal Korl_Math_M4f32 _korl_gfx_camera_projection(const Korl_Gfx_Camera*const context)
{
    _Korl_Gfx_Context*const gfxContext = _korl_gfx_context;
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        const f32 viewportWidthOverHeight = gfxContext->surfaceSize.y == 0 
            ? 1.f 
            :  KORL_C_CAST(f32, gfxContext->surfaceSize.x)
             / KORL_C_CAST(f32, gfxContext->surfaceSize.y);
        return korl_math_m4f32_projectionFov(context->subCamera.perspective.fovVerticalDegrees
                                            ,viewportWidthOverHeight
                                            ,context->subCamera.perspective.clipNear
                                            ,context->subCamera.perspective.clipFar);}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        const f32 left   = 0.f - context->subCamera.orthographic.originAnchor.x*KORL_C_CAST(f32, gfxContext->surfaceSize.x);
        const f32 bottom = 0.f - context->subCamera.orthographic.originAnchor.y*KORL_C_CAST(f32, gfxContext->surfaceSize.y);
        const f32 right  = KORL_C_CAST(f32, gfxContext->surfaceSize.x) - context->subCamera.orthographic.originAnchor.x*KORL_C_CAST(f32, gfxContext->surfaceSize.x);
        const f32 top    = KORL_C_CAST(f32, gfxContext->surfaceSize.y) - context->subCamera.orthographic.originAnchor.y*KORL_C_CAST(f32, gfxContext->surfaceSize.y);
        const f32 far    = -context->subCamera.orthographic.clipDepth;
        const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        return korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = gfxContext->surfaceSize.y == 0 
            ? 1.f 
            :  KORL_C_CAST(f32, gfxContext->surfaceSize.x) 
             / KORL_C_CAST(f32, gfxContext->surfaceSize.y);
        /* w / fixedHeight == windowAspectRatio */
        const f32 width  = context->subCamera.orthographic.fixedHeight * viewportWidthOverHeight;
        const f32 left   = 0.f - context->subCamera.orthographic.originAnchor.x*width;
        const f32 bottom = 0.f - context->subCamera.orthographic.originAnchor.y*context->subCamera.orthographic.fixedHeight;
        const f32 right  = width       - context->subCamera.orthographic.originAnchor.x*width;
        const f32 top    = context->subCamera.orthographic.fixedHeight - context->subCamera.orthographic.originAnchor.y*context->subCamera.orthographic.fixedHeight;
        const f32 far    = -context->subCamera.orthographic.clipDepth;
        const f32 near   = 1e-7f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        return korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);}
    default:{
        korl_log(ERROR, "invalid camera type: %i", context->type);
        return KORL_STRUCT_INITIALIZE_ZERO(Korl_Math_M4f32);}
    }
}
korl_internal Korl_Math_M4f32 _korl_gfx_camera_view(const Korl_Gfx_Camera*const context)
{
    return korl_math_m4f32_lookAt(&context->position, &context->target, &context->worldUpNormal);
}
korl_internal void korl_gfx_initialize(void)
{
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(8);
    const Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-gfx", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    _korl_gfx_context = korl_allocate(allocator, sizeof(*_korl_gfx_context));
    _korl_gfx_context->allocatorHandle = allocator;
    _korl_gfx_context->stringPool      = korl_allocate(allocator, sizeof(*_korl_gfx_context->stringPool));// allocate this ASAP to reduce fragmentation, since this struct _must_ remain UNMANAGED
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_gfx_context->allocatorHandle), _korl_gfx_context->stbDaFontCaches, 16);
    *_korl_gfx_context->stringPool = korl_stringPool_create(_korl_gfx_context->allocatorHandle);
}
korl_internal void korl_gfx_update(Korl_Math_V2u32 surfaceSize, f32 deltaSeconds)
{
    _korl_gfx_context->surfaceSize = surfaceSize;
    _korl_gfx_context->seconds    += deltaSeconds;
}
korl_internal void korl_gfx_flushGlyphPages(void)
{
    _Korl_Gfx_Context*const context = _korl_gfx_context;
    for(Korl_MemoryPool_Size fc = 0; fc < arrlenu(context->stbDaFontCaches); fc++)
    {
        _Korl_Gfx_FontCache*const     fontCache     = context->stbDaFontCaches[fc];
        _Korl_Gfx_FontGlyphPage*const fontGlyphPage = _korl_gfx_fontCache_getGlyphPage(fontCache);
        if(!fontGlyphPage->textureOutOfDate)
            continue;
        /* upload the glyph data to the GPU & obtain texture handle */
#if KORL_DEBUG && _KORL_GFX_DEBUG_LOG_GLYPH_PAGE_BITMAPS
        /* debug print the glyph page data :) */
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
        korl_time_probeStart(update_glyph_page_texture);
        // allocate a temp R8G8B8A8-format image buffer //
        const u$ tempImageBufferSize = sizeof(Korl_Vulkan_Color4u8) * fontGlyphPage->dataSquareSize * fontGlyphPage->dataSquareSize;
        Korl_Vulkan_Color4u8*const tempImageBuffer = korl_allocate(context->allocatorHandle, tempImageBufferSize);
        // "expand" the stbtt font bitmap into the image buffer //
        for(u$ y = 0; y < fontGlyphPage->dataSquareSize; y++)
            for(u$ x = 0; x < fontGlyphPage->dataSquareSize; x++)
                /* store a pure white pixel with the alpha component set to the stbtt font bitmap value */
                tempImageBuffer[y*fontGlyphPage->dataSquareSize + x] = (Korl_Vulkan_Color4u8){.r = 0xFF, .g = 0xFF, .b = 0xFF, .a = _korl_gfx_fontGlyphPage_getData(fontGlyphPage)[y*fontGlyphPage->dataSquareSize + x]};
        // upload the image buffer to graphics device texture //
        korl_assert(fontGlyphPage->resourceHandleTexture);
        korl_resource_update(fontGlyphPage->resourceHandleTexture, tempImageBuffer, tempImageBufferSize, 0/*destinationByteOffset*/);
        // free the temporary R8G8B8A8-format image buffer //
        korl_free(context->allocatorHandle, tempImageBuffer);
        fontGlyphPage->textureOutOfDate = false;
        korl_time_probeStop(update_glyph_page_texture);
        /* resize & update the glyph mesh vertex buffer SSBO */
        korl_time_probeStart(update_glyph_mesh_ssbo);
        const u$ newGlyphMeshVertexBufferBytes = sizeof(*fontGlyphPage->stbDaGlyphMeshVertices)*arrlenu(fontGlyphPage->stbDaGlyphMeshVertices);
        korl_assert(fontGlyphPage->resourceHandleSsboGlyphMeshVertices);
        korl_resource_resize(fontGlyphPage->resourceHandleSsboGlyphMeshVertices, newGlyphMeshVertexBufferBytes);
        korl_resource_update(fontGlyphPage->resourceHandleSsboGlyphMeshVertices, fontGlyphPage->stbDaGlyphMeshVertices, newGlyphMeshVertexBufferBytes, 0/*destinationByteOffset*/);
        /**/
        korl_time_probeStop(update_glyph_mesh_ssbo);
    }
}
korl_internal Korl_Gfx_Text* korl_gfx_text_create(Korl_Memory_AllocatorHandle allocator, acu16 utf16AssetNameFont, f32 textPixelHeight)
{
    KORL_ZERO_STACK_ARRAY(Korl_Vulkan_VertexAttributeDescriptor, vertexAttributeDescriptors, 2);
    vertexAttributeDescriptors[0].offset          = offsetof(_Korl_Gfx_FontGlyphInstance, position);
    vertexAttributeDescriptors[0].stride          = sizeof(_Korl_Gfx_FontGlyphInstance);
    vertexAttributeDescriptors[0].vertexAttribute = KORL_VULKAN_VERTEX_ATTRIBUTE_INSTANCE_POSITION_2D;
    vertexAttributeDescriptors[1].offset          = offsetof(_Korl_Gfx_FontGlyphInstance, meshIndex);
    vertexAttributeDescriptors[1].stride          = sizeof(_Korl_Gfx_FontGlyphInstance);
    vertexAttributeDescriptors[1].vertexAttribute = KORL_VULKAN_VERTEX_ATTRIBUTE_INSTANCE_UINT;
    KORL_ZERO_STACK(Korl_Vulkan_CreateInfoVertexBuffer, createInfoBufferText);
    createInfoBufferText.vertexAttributeDescriptorCount = korl_arraySize(vertexAttributeDescriptors);
    createInfoBufferText.vertexAttributeDescriptors     = vertexAttributeDescriptors;
    createInfoBufferText.bytes                          = 1024;// some arbitrary non-zero value; likely not important to tune this, but we'll see
    const u$ bytesRequired = sizeof(Korl_Gfx_Text) + (utf16AssetNameFont.size + 1/*for null-terminator*/)*sizeof(*utf16AssetNameFont.data);
    Korl_Gfx_Text*const result    = korl_allocate(allocator, bytesRequired);
    u16*const resultAssetNameFont = KORL_C_CAST(u16*, result + 1);
    result->allocator                       = allocator;
    result->textPixelHeight                 = textPixelHeight;
    result->assetNameFontRawUtf16ByteOffset = sizeof(*result);
    result->assetNameFontRawUtf16Size       = korl_checkCast_u$_to_u32(utf16AssetNameFont.size);
    result->modelRotate                     = KORL_MATH_QUATERNION_IDENTITY;
    result->modelScale                      = KORL_MATH_V3F32_ONE;
    result->resourceHandleBufferText        = korl_resource_createVertexBuffer(&createInfoBufferText);
    mcarrsetcap(KORL_STB_DS_MC_CAST(result->allocator), result->stbDaLines, 64);
    korl_memory_copy(resultAssetNameFont, utf16AssetNameFont.data, utf16AssetNameFont.size*sizeof(*utf16AssetNameFont.data));
    return result;
}
korl_internal void korl_gfx_text_destroy(Korl_Gfx_Text* context)
{
    korl_resource_destroy(context->resourceHandleBufferText);
    mcarrfree(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaLines);
    korl_free(context->allocator, context);
}
korl_internal void korl_gfx_text_collectDefragmentPointers(Korl_Gfx_Text* context, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent)
{
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stbDaMemoryContext, *pStbDaDefragmentPointers, context->stbDaLines, parent);
}
korl_internal void korl_gfx_text_fifoAdd(Korl_Gfx_Text* context, acu16 utf16Text, Korl_Memory_AllocatorHandle stackAllocator, fnSig_korl_gfx_text_codepointTest* codepointTest, void* codepointTestUserData)
{
    /* get the font asset matching the provided asset name */
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(korl_gfx_text_getUtf16AssetNameFont(context), context->textPixelHeight, 0.f/*textPixelOutline*/);
    korl_assert(fontCache);
    /* initialize scratch space for storing glyph instance data of the current 
        working text line */
    const u$ currentLineBufferBytes = utf16Text.size*(sizeof(u32/*glyph mesh bake order*/) + sizeof(Korl_Math_V2f32/*glyph position*/));
    _Korl_Gfx_FontGlyphInstance*const currentLineBuffer = korl_allocate(stackAllocator, currentLineBufferBytes);
    /* iterate over each character of utf16Text & build all the text lines */
    _Korl_Gfx_Text_Line* currentLine = NULL;
    Korl_Math_V2f32 textBaselineCursor = (Korl_Math_V2f32){0.f, 0.f};
    int glyphIndexPrevious = -1;// used to calculate kerning advance between the previous glyph and the current glyph
    Korl_Math_V4f32 currentLineColor = KORL_MATH_V4F32_ONE;// default all line colors to white
    for(Korl_String_CodepointIteratorUtf16 utf16It = korl_string_codepointIteratorUtf16_initialize(utf16Text.data, utf16Text.size)
       ;!korl_string_codepointIteratorUtf16_done(&utf16It)
       ; korl_string_codepointIteratorUtf16_next(&utf16It))
    {
        if(    codepointTest 
           && !codepointTest(codepointTestUserData
                            ,utf16It._codepoint, utf16It._codepointSize
                            ,KORL_C_CAST(const u8*, utf16It._currentRawUtf16)
                            ,sizeof(*utf16It._currentRawUtf16)
                            ,&currentLineColor))
            continue;
        u32 glyphCodepoint     = utf16It._codepoint;
        f32 advanceXMultiplier = 1;
        if(utf16It._codepoint == '\t')
        {
            glyphCodepoint     = ' ';
            advanceXMultiplier = 4;
        }
        const _Korl_Gfx_FontBakedGlyph*const bakedGlyph = _korl_gfx_fontCache_getGlyph(fontCache, glyphCodepoint);
        if(textBaselineCursor.x > 0.f)
        {
            const int kernAdvance = stbtt_GetGlyphKernAdvance(&fontCache->fontInfo
                                                             ,glyphIndexPrevious
                                                             ,bakedGlyph->glyphIndex);
            glyphIndexPrevious = bakedGlyph->glyphIndex;
            textBaselineCursor.x += fontCache->fontScale*kernAdvance;
        }
        const f32 x0 = textBaselineCursor.x + bakedGlyph->bbox.offsetX;
        const f32 y0 = textBaselineCursor.y + bakedGlyph->bbox.offsetY;
        const f32 x1 = x0 + (bakedGlyph->bbox.x1 - bakedGlyph->bbox.x0);
        const f32 y1 = y0 + (bakedGlyph->bbox.y1 - bakedGlyph->bbox.y0);
        const Korl_Math_V2f32 glyphPosition = textBaselineCursor;
        textBaselineCursor.x += advanceXMultiplier*bakedGlyph->advanceX;
        if(utf16It._codepoint == '\n')
        {
            textBaselineCursor.x  = 0.f;
            // textBaselineCursor.y -= lineDeltaY;// no need to do this; each line has an implicit Y size, and when we draw we will move the line's model position appropriately
            glyphIndexPrevious = -1;
            if(currentLine)
            {
                /* if we had a current working text line, we need to flush the text 
                    instance data we've accumulated so far into a vertex buffer 
                    device asset; instead of creating a separate buffer resource 
                    for each line, we are just going to accumulate a giant 
                    buffer used by the entire Gfx_Text object */
                const u$ bufferBytesRequiredMin = (context->totalVisibleGlyphs + currentLine->visibleCharacters) * sizeof(*currentLineBuffer);
                const u$ currentBufferBytes     = korl_resource_getByteSize(context->resourceHandleBufferText);
                if(currentBufferBytes < bufferBytesRequiredMin)
                {
                    const u$ bufferBytesNew = KORL_MATH_MAX(bufferBytesRequiredMin, 2*currentBufferBytes);
                    korl_resource_resize(context->resourceHandleBufferText, bufferBytesNew);
                }
                korl_resource_update(context->resourceHandleBufferText, currentLineBuffer, currentLine->visibleCharacters * sizeof(*currentLineBuffer), context->totalVisibleGlyphs * sizeof(*currentLineBuffer));
                /* update the Text object's model AABB with the new line graphics we just added */
                KORL_MATH_ASSIGN_CLAMP_MIN(context->_modelAabb.max.x, currentLine->modelAabb.max.x);
                /* update other Text object metrics */
                context->totalVisibleGlyphs += currentLine->visibleCharacters;
            }
            currentLine      = NULL;
            currentLineColor = KORL_MATH_V4F32_ONE;// default next line color to white
            continue;
        }
        if(bakedGlyph->isEmpty)
            continue;
        /* at this point, we know that this is a valid visible character, which 
            must be accumulated into a text line; if we don't have a current 
            working text line at this point, we need to make one */
        if(!currentLine)
        {
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocator), context->stbDaLines, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Gfx_Text_Line));
            currentLine = &arrlast(context->stbDaLines);
            currentLine->color = currentLineColor;
        }
        currentLineBuffer[currentLine->visibleCharacters].position  = glyphPosition;
        currentLineBuffer[currentLine->visibleCharacters].meshIndex = bakedGlyph->bakeOrder;
        currentLine->modelAabb.min = korl_math_v2f32_min(currentLine->modelAabb.min, (Korl_Math_V2f32){x0, y0});
        currentLine->modelAabb.max = korl_math_v2f32_max(currentLine->modelAabb.max, (Korl_Math_V2f32){x1, y1});
        currentLine->visibleCharacters++;
    }
    /* we can easily calculate the height of the Text object's current AABB 
        using the font's cached metrics */
    const f32 lineDeltaY = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    context->_modelAabb.min.y = arrlenu(context->stbDaLines) * -lineDeltaY;
    /* clean up */
    korl_free(stackAllocator, currentLineBuffer);
}
korl_internal void korl_gfx_text_fifoRemove(Korl_Gfx_Text* context, u$ lineCount)
{
    /* get the font asset matching the provided asset name */
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(korl_gfx_text_getUtf16AssetNameFont(context), context->textPixelHeight, 0.f/*textPixelOutline*/);
    korl_assert(fontCache);
    /**/
    const u$ linesToRemove = KORL_MATH_MIN(lineCount, arrlenu(context->stbDaLines));
    context->_modelAabb.max.x = 0;
    u$ glyphsToRemove = 0;
    for(u$ l = 0; l < arrlenu(context->stbDaLines); l++)
    {
        if(l < linesToRemove)
        {
            glyphsToRemove += context->stbDaLines[l].visibleCharacters;
            continue;
        }
        const _Korl_Gfx_Text_Line*const line = &(context->stbDaLines[l]);
        KORL_MATH_ASSIGN_CLAMP_MIN(context->_modelAabb.max.x, line->modelAabb.max.x);
    }
    arrdeln(context->stbDaLines, 0, linesToRemove);
    context->totalVisibleGlyphs -= korl_checkCast_u$_to_u32(glyphsToRemove);
    korl_resource_shift(context->resourceHandleBufferText, -1/*shift to the left; remove the glyphs instances at the beginning of the buffer*/ * glyphsToRemove*sizeof(_Korl_Gfx_FontGlyphInstance));
    const f32 lineDeltaY = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    context->_modelAabb.min.y = arrlenu(context->stbDaLines) * -lineDeltaY;
}
korl_internal void korl_gfx_text_draw(const Korl_Gfx_Text* context, Korl_Math_Aabb2f32 visibleRegion)
{
    /* get the font asset matching the provided asset name */
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(korl_gfx_text_getUtf16AssetNameFont(context), context->textPixelHeight, 0.f/*textPixelOutline*/);
    korl_assert(fontCache);
    const f32 lineDeltaY = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    /**/
    korl_shared_const Korl_Vulkan_VertexIndex triQuadIndices[] = 
        { 0, 1, 3
        , 1, 2, 3 };
    KORL_ZERO_STACK(Korl_Vulkan_DrawVertexData, vertexData);
    vertexData.primitiveType           = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    vertexData.indexCount              = korl_arraySize(triQuadIndices);
    vertexData.indices                 = triQuadIndices;
    vertexData.instancePositionsStride = 2*sizeof(f32);
    vertexData.instanceUintStride      = sizeof(u32);
    Korl_Vulkan_DrawState_Material material = KORL_GFX_MATERIAL_DEFAULT;
    material.maps.resourceHandleTextureBase = _korl_gfx_fontCache_getGlyphPage(fontCache)->resourceHandleTexture;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_StorageBuffers, storageBuffers);
    storageBuffers.resourceHandleVertex = _korl_gfx_fontCache_getGlyphPage(fontCache)->resourceHandleSsboGlyphMeshVertices;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Features, features);
    features.enableBlend = true;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Blend, blend);
    blend.opColor           = KORL_GFX_BLEND_ALPHA.color.operation;
    blend.factorColorSource = KORL_GFX_BLEND_ALPHA.color.factorSource;
    blend.factorColorTarget = KORL_GFX_BLEND_ALPHA.color.factorTarget;
    blend.opAlpha           = KORL_GFX_BLEND_ALPHA.alpha.operation;
    blend.factorAlphaSource = KORL_GFX_BLEND_ALPHA.alpha.factorSource;
    blend.factorAlphaTarget = KORL_GFX_BLEND_ALPHA.alpha.factorTarget;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawState);
    drawState.features       = &features;
    drawState.blend          = &blend;
    drawState.material       = &material;
    drawState.storageBuffers = &storageBuffers;
    korl_vulkan_setDrawState(&drawState);
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Model, model);
    Korl_Math_V3f32 modelTranslation = context->modelTranslate;
    modelTranslation.y -= fontCache->fontAscent;// start the text such that the translation XY position defines the location _directly_ above _all_ the text
    u$ currentVisibleGlyphOffset = 0;// used to determine the byte (transform required) offset into the Text object's text buffer resource
    for(const _Korl_Gfx_Text_Line* line = context->stbDaLines; line < context->stbDaLines + arrlen(context->stbDaLines); line++)
    {
        if(modelTranslation.y < visibleRegion.min.y - fontCache->fontAscent)
            break;
        if(modelTranslation.y <= visibleRegion.max.y + korl_math_f32_positive(fontCache->fontDescent))
        {
            material.properties.factorColorBase = line->color;
            model.transform = korl_math_makeM4f32_rotateScaleTranslate(context->modelRotate, context->modelScale, modelTranslation);
            KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawStateLine);
            drawStateLine.material = &material;
            drawStateLine.model    = &model;
            korl_vulkan_setDrawState(&drawStateLine);
            vertexData.instanceCount                        = line->visibleCharacters;
            vertexData.vertexBuffer.type                   = KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_RESOURCE;
            vertexData.vertexBuffer.subType.handleResource = context->resourceHandleBufferText;
            vertexData.vertexBuffer.byteOffset             = currentVisibleGlyphOffset*sizeof(_Korl_Gfx_FontGlyphInstance);
            korl_vulkan_draw(&vertexData);
        }
        modelTranslation.y        -= lineDeltaY;
        currentVisibleGlyphOffset += line->visibleCharacters;
    }
}
korl_internal Korl_Math_Aabb2f32 korl_gfx_text_getModelAabb(const Korl_Gfx_Text* context)
{
    return context->_modelAabb;
}
korl_internal KORL_FUNCTION_korl_gfx_font_getMetrics(korl_gfx_font_getMetrics)
{
    KORL_ZERO_STACK(Korl_Gfx_Font_Metrics, metrics);
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(utf16AssetNameFont, textPixelHeight, 0.f/*textPixelOutline*/);
    if(!fontCache)
        return metrics;// silently return empty data if the font is not yet loaded
    metrics.ascent  = fontCache->fontAscent;
    metrics.decent  = fontCache->fontDescent;
    metrics.lineGap = fontCache->fontLineGap;
    korl_assert(metrics.ascent >= metrics.decent);// no idea if this is a hard requirement, but based on how these #s are used, it seems _very_ unlikely that this will ever be false
    return metrics;
}
korl_internal Korl_Math_Aabb2f32 korl_gfx_font_getTextAabb(acu16 utf16AssetNameFont, f32 textPixelHeight, acu8 utf8Text)
{
    Korl_Math_Aabb2f32 result = KORL_MATH_AABB2F32_EMPTY;
    KORL_ZERO_STACK(Korl_Gfx_Font_Metrics, metrics);
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(utf16AssetNameFont, textPixelHeight, 0.f/*textPixelOutline*/);
    if(!fontCache)
        return KORL_MATH_AABB2F32_EMPTY;// silently return empty AABB if font is not loaded
    const f32 lineDeltaY = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    Korl_Math_V2f32 textBaselineCursor = KORL_MATH_V2F32_ZERO;
    int glyphIndexPrevious = -1;
    //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
    for(Korl_String_CodepointIteratorUtf8 codepointIt = korl_string_codepointIteratorUtf8_initialize(utf8Text.data, utf8Text.size)
       ;!korl_string_codepointIteratorUtf8_done(&codepointIt)
       ; korl_string_codepointIteratorUtf8_next(&codepointIt))
    {
        const _Korl_Gfx_FontBakedGlyph*const bakedGlyph = _korl_gfx_fontCache_getGlyph(fontCache, codepointIt._codepoint);
        if(textBaselineCursor.x > 0.f)
        {
            const int kernAdvance = stbtt_GetGlyphKernAdvance(&fontCache->fontInfo
                                                             ,glyphIndexPrevious
                                                             ,bakedGlyph->glyphIndex);
            glyphIndexPrevious = bakedGlyph->glyphIndex;
            textBaselineCursor.x += fontCache->fontScale*kernAdvance;
        }
        /* instead of generating an AABB that is shrink-wrapped to the actual 
            rendered glyphs, let's generate the AABB with respect to the font's 
            height metrics; so we only really care about the glyph's graphical 
            AABB on the X-axis; why? because I am finding that in a lot of cases 
            the user code cares more about visual alignment of rendered text, 
            and this is a lot easier to achieve API-wise when we are calculating 
            text AABBs relatvie to font metrics */
        const f32 x0 = textBaselineCursor.x + bakedGlyph->bbox.offsetX;
        const f32 y0 = textBaselineCursor.y + fontCache->fontAscent;
        const f32 x1 = x0 + (bakedGlyph->bbox.x1 - bakedGlyph->bbox.x0);
        const f32 y1 = textBaselineCursor.y - fontCache->fontDescent;
        textBaselineCursor.x += bakedGlyph->advanceX;
        if(codepointIt._codepoint == L'\n')
        {
            textBaselineCursor.x  = 0.f;
            textBaselineCursor.y -= lineDeltaY;
            glyphIndexPrevious = -1;
            continue;
        }
        if(bakedGlyph->isEmpty)
            continue;
        /* at this point, we know that this is a valid visible character, which 
            must be accumulated into a text line; if we don't have a current 
            working text line at this point, we need to make one */
        result.min = korl_math_v2f32_min(result.min, (Korl_Math_V2f32){x0, y0});
        result.max = korl_math_v2f32_max(result.max, (Korl_Math_V2f32){x1, y1});
    }
    return result;
}
korl_internal Korl_Math_V2f32 korl_gfx_font_textGraphemePosition(acu16 utf16AssetNameFont, f32 textPixelHeight, acu8 utf8Text, u$ graphemeIndex)
{
    KORL_ZERO_STACK(Korl_Gfx_Font_Metrics, metrics);
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(utf16AssetNameFont, textPixelHeight, 0.f/*textPixelOutline*/);
    if(!fontCache)
        return KORL_MATH_V2F32_ZERO;// silently return 0 if font is not loaded
    const f32 lineDeltaY = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    Korl_Math_V2f32 textBaselineCursor = KORL_MATH_V2F32_ZERO;
    int glyphIndexPrevious = -1;
    //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
    for(Korl_String_CodepointIteratorUtf8 codepointIt = korl_string_codepointIteratorUtf8_initialize(utf8Text.data, utf8Text.size)
       ;!korl_string_codepointIteratorUtf8_done(&codepointIt)
       ; korl_string_codepointIteratorUtf8_next(&codepointIt))
    {
        if(codepointIt.codepointIndex >= graphemeIndex)
            break;
        const _Korl_Gfx_FontBakedGlyph*const bakedGlyph = _korl_gfx_fontCache_getGlyph(fontCache, codepointIt._codepoint);
        if(textBaselineCursor.x > 0.f)
        {
            const int kernAdvance = stbtt_GetGlyphKernAdvance(&fontCache->fontInfo
                                                             ,glyphIndexPrevious
                                                             ,bakedGlyph->glyphIndex);
            glyphIndexPrevious = bakedGlyph->glyphIndex;
            textBaselineCursor.x += fontCache->fontScale*kernAdvance;
        }
        textBaselineCursor.x += bakedGlyph->advanceX;
        if(codepointIt._codepoint == L'\n')
        {
            textBaselineCursor.x  = 0.f;
            textBaselineCursor.y -= lineDeltaY;
            glyphIndexPrevious = -1;
            continue;
        }
        if(bakedGlyph->isEmpty)
            continue;
    }
    return textBaselineCursor;
}
korl_internal KORL_FUNCTION_korl_gfx_createCameraFov(korl_gfx_createCameraFov)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.position                                 = position;
    result.target                                   = target;
    result.worldUpNormal                            = up;
    result._viewportScissorPosition                 = (Korl_Math_V2f32){0, 0};
    result._viewportScissorSize                     = (Korl_Math_V2f32){1, 1};
    result.subCamera.perspective.clipNear           = clipNear;
    result.subCamera.perspective.clipFar            = clipFar;
    result.subCamera.perspective.fovVerticalDegrees = fovVerticalDegrees;
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createCameraOrtho(korl_gfx_createCameraOrtho)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC;
    result.position                            = KORL_MATH_V3F32_ZERO;
    result.worldUpNormal                       = KORL_MATH_V3F32_Y;
    result.target                              = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result._viewportScissorPosition            = (Korl_Math_V2f32){0, 0};
    result._viewportScissorSize                = (Korl_Math_V2f32){1, 1};
    result.subCamera.orthographic.clipDepth    = clipDepth;
    result.subCamera.orthographic.originAnchor = (Korl_Math_V2f32){0.5f, 0.5f};
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createCameraOrthoFixedHeight(korl_gfx_createCameraOrthoFixedHeight)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT;
    result.position                            = KORL_MATH_V3F32_ZERO;
    result.worldUpNormal                       = KORL_MATH_V3F32_Y;
    result.target                              = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result._viewportScissorPosition            = (Korl_Math_V2f32){0, 0};
    result._viewportScissorSize                = (Korl_Math_V2f32){1, 1};
    result.subCamera.orthographic.fixedHeight  = fixedHeight;
    result.subCamera.orthographic.clipDepth    = clipDepth;
    result.subCamera.orthographic.originAnchor = (Korl_Math_V2f32){0.5f, 0.5f};
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_cameraFov_rotateAroundTarget(korl_gfx_cameraFov_rotateAroundTarget)
{
    korl_assert(context->type == KORL_GFX_CAMERA_TYPE_PERSPECTIVE);
    const Korl_Math_V3f32 newTargetOffset = 
        korl_math_quaternion_transformV3f32(
            korl_math_quaternion_fromAxisRadians(axisOfRotation, radians, false), 
            korl_math_v3f32_subtract(context->position, context->target), 
            true);
    context->position = korl_math_v3f32_add(context->target, newTargetOffset);
}
korl_internal KORL_FUNCTION_korl_gfx_useCamera(korl_gfx_useCamera)
{
    _Korl_Gfx_Context*const context = _korl_gfx_context;
    korl_time_probeStart(useCamera);
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Scissor, scissor);
    switch(camera._scissorType)
    {
    case KORL_GFX_CAMERA_SCISSOR_TYPE_RATIO:{
        korl_assert(camera._viewportScissorPosition.x >= 0 && camera._viewportScissorPosition.x <= 1);
        korl_assert(camera._viewportScissorPosition.y >= 0 && camera._viewportScissorPosition.y <= 1);
        korl_assert(camera._viewportScissorSize.x >= 0 && camera._viewportScissorSize.x <= 1);
        korl_assert(camera._viewportScissorSize.y >= 0 && camera._viewportScissorSize.y <= 1);
        scissor.x      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.x * context->surfaceSize.x);
        scissor.y      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.y * context->surfaceSize.y);
        scissor.width  = korl_math_round_f32_to_u32(camera._viewportScissorSize.x * context->surfaceSize.x);
        scissor.height = korl_math_round_f32_to_u32(camera._viewportScissorSize.y * context->surfaceSize.y);
        break;}
    case KORL_GFX_CAMERA_SCISSOR_TYPE_ABSOLUTE:{
        korl_assert(camera._viewportScissorPosition.x >= 0);
        korl_assert(camera._viewportScissorPosition.y >= 0);
        scissor.x      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.x);
        scissor.y      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.y);
        scissor.width  = korl_math_round_f32_to_u32(camera._viewportScissorSize.x);
        scissor.height = korl_math_round_f32_to_u32(camera._viewportScissorSize.y);
        break;}
    }
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_SceneProperties, sceneProperties);
    sceneProperties.view = korl_math_m4f32_lookAt(&camera.position, &camera.target, &camera.worldUpNormal);
    switch(camera.type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        const f32 viewportWidthOverHeight = context->surfaceSize.y == 0 
                                            ? 1.f 
                                            : KORL_C_CAST(f32, context->surfaceSize.x) / KORL_C_CAST(f32, context->surfaceSize.y);
        sceneProperties.projection = korl_math_m4f32_projectionFov(camera.subCamera.perspective.fovVerticalDegrees
                                                                  ,viewportWidthOverHeight
                                                                  ,camera.subCamera.perspective.clipNear
                                                                  ,camera.subCamera.perspective.clipFar);
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        const f32 left   = 0.f - camera.subCamera.orthographic.originAnchor.x * KORL_C_CAST(f32, context->surfaceSize.x);
        const f32 bottom = 0.f - camera.subCamera.orthographic.originAnchor.y * KORL_C_CAST(f32, context->surfaceSize.y);
        const f32 right  = KORL_C_CAST(f32, context->surfaceSize.x) - camera.subCamera.orthographic.originAnchor.x * KORL_C_CAST(f32, context->surfaceSize.x);
        const f32 top    = KORL_C_CAST(f32, context->surfaceSize.y) - camera.subCamera.orthographic.originAnchor.y * KORL_C_CAST(f32, context->surfaceSize.y);
        const f32 far    = -camera.subCamera.orthographic.clipDepth;
        const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        sceneProperties.projection = korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = context->surfaceSize.y == 0 
                                            ? 1.f 
                                            : KORL_C_CAST(f32, context->surfaceSize.x) / KORL_C_CAST(f32, context->surfaceSize.y);
        /* w / fixedHeight == windowAspectRatio */
        const f32 width  = camera.subCamera.orthographic.fixedHeight * viewportWidthOverHeight;
        const f32 left   = 0.f - camera.subCamera.orthographic.originAnchor.x * width;
        const f32 bottom = 0.f - camera.subCamera.orthographic.originAnchor.y * camera.subCamera.orthographic.fixedHeight;
        const f32 right  = width       - camera.subCamera.orthographic.originAnchor.x * width;
        const f32 top    = camera.subCamera.orthographic.fixedHeight - camera.subCamera.orthographic.originAnchor.y * camera.subCamera.orthographic.fixedHeight;
        const f32 far    = -camera.subCamera.orthographic.clipDepth;
        const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        sceneProperties.projection = korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);
        break;}
    }
    sceneProperties.seconds = context->seconds;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawState);
    drawState.scissor         = &scissor;
    drawState.sceneProperties = &sceneProperties;
    korl_vulkan_setDrawState(&drawState);
    context->currentCameraState = camera;
    korl_time_probeStop(useCamera);
}
korl_internal KORL_FUNCTION_korl_gfx_camera_getCurrent(korl_gfx_camera_getCurrent)
{
    return _korl_gfx_context->currentCameraState;
}
korl_internal KORL_FUNCTION_korl_gfx_cameraSetScissor(korl_gfx_cameraSetScissor)
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
korl_internal KORL_FUNCTION_korl_gfx_cameraSetScissorPercent(korl_gfx_cameraSetScissorPercent)
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
korl_internal KORL_FUNCTION_korl_gfx_cameraOrthoSetOriginAnchor(korl_gfx_cameraOrthoSetOriginAnchor)
{
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        korl_assert(!"origin anchor not supported for perspective camera");
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        context->subCamera.orthographic.originAnchor.x = swapchainSizeRatioOriginX;
        context->subCamera.orthographic.originAnchor.y = swapchainSizeRatioOriginY;
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        context->subCamera.orthographic.originAnchor.x = swapchainSizeRatioOriginX;
        context->subCamera.orthographic.originAnchor.y = swapchainSizeRatioOriginY;
        break;}
    }
}
korl_internal KORL_FUNCTION_korl_gfx_cameraOrthoGetSize(korl_gfx_cameraOrthoGetSize)
{
    _Korl_Gfx_Context*const gfxContext = _korl_gfx_context;
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        return (Korl_Math_V2f32){KORL_C_CAST(f32, gfxContext->surfaceSize.x)
                                ,KORL_C_CAST(f32, gfxContext->surfaceSize.y)};}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = gfxContext->surfaceSize.y == 0 
            ? 1.f 
            :   KORL_C_CAST(f32, gfxContext->surfaceSize.x) 
              / KORL_C_CAST(f32, gfxContext->surfaceSize.y);
        return (Korl_Math_V2f32){context->subCamera.orthographic.fixedHeight*viewportWidthOverHeight// w / fixedHeight == windowAspectRatio
                                ,context->subCamera.orthographic.fixedHeight};}
    default:{
        korl_log(ERROR, "invalid camera type: %i", context->type);
        return (Korl_Math_V2f32){korl_math_f32_nan(), korl_math_f32_nan()};}
    }
}
korl_internal KORL_FUNCTION_korl_gfx_camera_windowToWorld(korl_gfx_camera_windowToWorld)
{
    _Korl_Gfx_Context*const gfxContext = _korl_gfx_context;
    Korl_Gfx_ResultRay3d result = {.position ={korl_math_f32_nan(),korl_math_f32_nan(),korl_math_f32_nan()}
                                  ,.direction={korl_math_f32_nan(),korl_math_f32_nan(),korl_math_f32_nan()}};
    //KORL-PERFORMANCE-000-000-041: gfx: I expect this to be SLOW; we should instead be caching the camera's VP matrices and only update them when they are "dirty"; I know for a fact that SFML does this in its sf::camera class
    const Korl_Math_M4f32 view       = _korl_gfx_camera_view(context);
    const Korl_Math_M4f32 projection = _korl_gfx_camera_projection(context);
    const Korl_Math_M4f32 viewInverse       = korl_math_m4f32_invert(&view);
    const Korl_Math_M4f32 projectionInverse = korl_math_m4f32_invert(&projection);
    if(korl_math_f32_isNan(viewInverse.r0c0))
    {
        korl_log(WARNING, "failed to invert view matrix");
        return result;
    }
    if(korl_math_f32_isNan(projectionInverse.r0c0))
    {
        korl_log(WARNING, "failed to invert projection matrix");
        return result;
    }
    const Korl_Math_V2f32 v2f32WindowPos = {KORL_C_CAST(f32, windowPosition.x)
                                           ,KORL_C_CAST(f32, windowPosition.y)};
    /* We can determine if a projection matrix is orthographic or frustum based 
        on the last element of the W row.  See: 
        http://www.songho.ca/opengl/gl_projectionmatrix.html */
    korl_assert(   projection.r3c3 == 1 
                || projection.r3c3 == 0);
    const bool isOrthographic = projection.r3c3 == 1;
    /* viewport-space => normalized-device-space */
    //KORL-ISSUE-000-000-101: gfx: ASSUMPTION: viewport is the size of the entire window; if we ever want to handle separate viewport clip regions per-camera, we will have to modify this
    const Korl_Math_V2f32 eyeRayNds = 
        {  2*v2f32WindowPos.x / gfxContext->surfaceSize.x - 1
        , -2*v2f32WindowPos.y / gfxContext->surfaceSize.y + 1 };
    /* normalized-device-space => homogeneous-clip-space */
    /* arbitrarily set the eye ray direction vector as far to the "back" of the 
        homogeneous clip space box; we set the Z coordinate to 0, since Vulkan 
        (with no extensions, which I don't want to use) requires a normalized 
        depth buffer */
    //KORL-ISSUE-000-000-102: gfx: testing required; ASSUMPTION: right-handed HC-space coordinate system that spans [0,1]; need to actually test to see if this works
    const Korl_Math_V4f32 eyeRayDirectionHcs = {eyeRayNds.x, eyeRayNds.y, 0, 1};
    /* homogeneous-clip-space => eye-space */
    Korl_Math_V4f32 eyeRayDirectionEs = korl_math_m4f32_multiplyV4f32(&projectionInverse, &eyeRayDirectionHcs);
    if(!isOrthographic)
        eyeRayDirectionEs.w = 0;
    /* eye-space => world-space */
    const Korl_Math_V4f32 eyeRayDirectionWs = korl_math_m4f32_multiplyV4f32(&viewInverse, &eyeRayDirectionEs);
    if(isOrthographic)
    {
        /* the orthographic eye position should be as far to the "front" of the 
            homogenous clip space box, which means setting the Z coordinate to a 
            value of 1 */
        //KORL-ISSUE-000-000-102: gfx: testing required; ASSUMPTION: right-handed HC-space coordinate system that spans [0,1]; need to actually test to see if this works
        const Korl_Math_V4f32 eyeRayPositionHcs = {eyeRayNds.x, eyeRayNds.y, 1, 1};
        const Korl_Math_V4f32 eyeRayPositionEs  = korl_math_m4f32_multiplyV4f32(&projectionInverse, &eyeRayPositionHcs);
        const Korl_Math_V4f32 eyeRayPositionWs  = korl_math_m4f32_multiplyV4f32(&viewInverse      , &eyeRayPositionEs);
        result.position  = eyeRayPositionWs.xyz;
        result.direction = korl_math_v3f32_normal(korl_math_v3f32_subtract(eyeRayDirectionWs.xyz, eyeRayPositionWs.xyz));
    }
    else
    {
        /* camera's eye position can be easily derived from the inverse view 
            matrix: https://stackoverflow.com/a/39281556/4526664 */
        result.position  = KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){viewInverse.r0c3, viewInverse.r1c3, viewInverse.r2c3};
        result.direction = korl_math_v3f32_normal(eyeRayDirectionWs.xyz);
    }
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_camera_worldToWindow(korl_gfx_camera_worldToWindow)
{
    _Korl_Gfx_Context*const gfxContext = _korl_gfx_context;
    //KORL-PERFORMANCE-000-000-041: gfx: I expect this to be SLOW; we should instead be caching the camera's VP matrices and only update them when they are "dirty"; I know for a fact that SFML does this in its sf::camera class
    const Korl_Math_M4f32 view       = _korl_gfx_camera_view(context);
    const Korl_Math_M4f32 projection = _korl_gfx_camera_projection(context);
    //KORL-ISSUE-000-000-101: gfx: ASSUMPTION: viewport is the size of the entire window; if we ever want to handle separate viewport clip regions per-camera, we will have to modify this
    const Korl_Math_Aabb2f32 viewport     = {.min={0,0}
                                            ,.max={KORL_C_CAST(f32, gfxContext->surfaceSize.x)
                                                  ,KORL_C_CAST(f32, gfxContext->surfaceSize.y)}};
    const Korl_Math_V2f32    viewportSize = korl_math_aabb2f32_size(viewport);
    /* transform from world=>camera=>clip space */
    const Korl_Math_V4f32 worldPoint       = {worldPosition.x, worldPosition.y, worldPosition.z, 1};
    const Korl_Math_V4f32 cameraSpacePoint = korl_math_m4f32_multiplyV4f32(&view, &worldPoint);
    const Korl_Math_V4f32 clipSpacePoint   = korl_math_m4f32_multiplyV4f32(&projection, &cameraSpacePoint);
    if(korl_math_isNearlyZero(clipSpacePoint.w))
        return (Korl_Math_V2f32){korl_math_f32_nan(), korl_math_f32_nan()};
    /* calculate normalized-device-coordinate-space 
        y is inverted here because screen-space y axis is flipped! */
    const Korl_Math_V3f32 ndcSpacePoint = 
        { clipSpacePoint.x /  clipSpacePoint.w
        , clipSpacePoint.y / -clipSpacePoint.w
        , clipSpacePoint.z /  clipSpacePoint.w };
    /* Calculate screen-space.  GLSL formula: 
        ((ndcSpacePoint.xy + 1.0) / 2.0) * viewSize + viewOffset */
    const Korl_Math_V2f32 result = 
        { ((ndcSpacePoint.x + 1.f) / 2.f) * viewportSize.x + viewport.min.x
        , ((ndcSpacePoint.y + 1.f) / 2.f) * viewportSize.y + viewport.min.y };
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_batch(korl_gfx_batch)
{
    korl_time_probeStart(gfx_batch);
    korl_time_probeStart(text_generate_mesh);
    if(batch->_assetNameFont)
        _korl_gfx_textGenerateMesh(batch, KORL_ASSETCACHE_GET_FLAG_LAZY);
    korl_time_probeStop(text_generate_mesh);
    if(batch->_vertexCount <= 0 && batch->_instanceCount <= 0)
    {
        korl_log(WARNING, "attempted batch is empty");
        goto done;
    }
    if(batch->_assetNameFont && !batch->_fontTextureHandle)
    {
        korl_log(WARNING, "text batch mesh not yet obtained from font asset");
        goto done;
    }
    KORL_ZERO_STACK(Korl_Vulkan_DrawVertexData, vertexData);
    vertexData.primitiveType              = batch->primitiveType;
    vertexData.indexCount                 = korl_vulkan_safeCast_u$_to_vertexIndex(batch->_vertexIndexCount);
    vertexData.indices                    = batch->_vertexIndices;
    vertexData.vertexCount                = batch->_vertexCount;
    vertexData.positionDimensions         = batch->_vertexPositionDimensions;
    vertexData.positions                  = batch->_vertexPositions;
    vertexData.colors                     = batch->_vertexColors;
    vertexData.uvs                        = batch->_vertexUvs;
    vertexData.instanceCount              = batch->_instanceCount;
    vertexData.instancePositions          = batch->_instancePositions;
    vertexData.instancePositionDimensions = batch->_instancePositionDimensions;
    vertexData.instanceUint               = batch->_instanceU32s;
    if(batch->_vertexPositions)   vertexData.positionsStride         = batch->_vertexPositionDimensions*sizeof(*batch->_vertexPositions);
    if(batch->_vertexColors)      vertexData.colorsStride            = sizeof(*batch->_vertexColors);
    if(batch->_vertexUvs)         vertexData.uvsStride               = sizeof(*batch->_vertexUvs);
    if(batch->_instancePositions) vertexData.instancePositionsStride = batch->_instancePositionDimensions*sizeof(*batch->_instancePositions);
    if(batch->_instanceU32s)      vertexData.instanceUintStride      = sizeof(*batch->_instanceU32s);
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Model, model);
    model.transform = korl_math_makeM4f32_rotateScaleTranslate(batch->_rotation, batch->_scale, batch->_position);
    if(batch->_vertexUvs)
    {
        model.uvAabb = KORL_MATH_AABB2F32_EMPTY;
        for(u$ i = 0; i < batch->_vertexCount; i++)
            korl_math_aabb2f32_addPointV2(&model.uvAabb, batch->_vertexUvs[i]);
        korl_math_v2f32_assignAddScalar     (&model.uvAabb.min, batch->uvAabbOffset);
        korl_math_v2f32_assignSubtractScalar(&model.uvAabb.max, batch->uvAabbOffset);
    }
    Korl_Vulkan_DrawState_Material material = KORL_GFX_MATERIAL_DEFAULT;
    material.properties.factorColorBase = (Korl_Math_V4f32){batch->modelColor.r / KORL_C_CAST(f32, KORL_U8_MAX)
                                                           ,batch->modelColor.g / KORL_C_CAST(f32, KORL_U8_MAX)
                                                           ,batch->modelColor.b / KORL_C_CAST(f32, KORL_U8_MAX)
                                                           ,batch->modelColor.a / KORL_C_CAST(f32, KORL_U8_MAX)};
    material.maps.resourceHandleTextureBase = batch->_texture;
    material.shaders.resourceHandleShaderFragment = batch->resourceHandleShaderFragment;
    material.shaders.resourceHandleShaderVertex   = batch->resourceHandleShaderVertex;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_StorageBuffers, storageBuffers);
    if(batch->_assetNameFont)
    {
        /* each glyph is going to be drawn with the same set of vertex indices, 
            so we can just use a common data structure to draw all the glyph 
            mesh instances; since we are currently limited to triangles, we can 
            use a universal triangle quad index set here */
        vertexData.indices    = _KORL_GFX_TRI_QUAD_INDICES;
        vertexData.indexCount = korl_arraySize(_KORL_GFX_TRI_QUAD_INDICES);
        /* use glyph texture */
        material.maps.resourceHandleTextureBase = batch->_fontTextureHandle;
        /* use the glyph mesh vertices buffer as the shader storage buffer 
            object binding */
        storageBuffers.resourceHandleVertex = batch->_glyphMeshBufferVertices;
        /* we need to somehow position the text mesh in a way that satisfies the 
            text position anchor */
        Korl_Math_V3f32 textPosition = batch->_position;
        // align position with the bottom-left corner of the batch AABB
        korl_math_v2f32_assignSubtract(&textPosition.xy, batch->_textAabb.min);
        // offset position by the position anchor ratio, using the AABB size
        korl_math_v2f32_assignSubtract(&textPosition.xy, korl_math_v2f32_multiply(korl_math_aabb2f32_size(batch->_textAabb)
                                                                                 ,batch->_textPositionAnchor));
        //  //
        model.transform = korl_math_makeM4f32_rotateScaleTranslate(batch->_rotation, batch->_scale, batch->_position);
        /* disable vertex colors for now, since this is super expensive for text anyway! */
        vertexData.colors       = NULL;
        vertexData.colorsStride = 0;
    }
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Features, features);
    features.enableBlend     = !(flags & KORL_GFX_BATCH_FLAG_DISABLE_BLENDING);
    features.enableDepthTest = !(flags & KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Blend, blend);
    blend.opColor           = batch->blend.color.operation;
    blend.factorColorSource = batch->blend.color.factorSource;
    blend.factorColorTarget = batch->blend.color.factorTarget;
    blend.opAlpha           = batch->blend.alpha.operation;
    blend.factorAlphaSource = batch->blend.alpha.factorSource;
    blend.factorAlphaTarget = batch->blend.alpha.factorTarget;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawState);
    drawState.features       = &features;
    drawState.blend          = &blend;
    drawState.model          = &model;
    drawState.material       = &material;
    drawState.storageBuffers = &storageBuffers;
    korl_vulkan_setDrawState(&drawState);
    korl_vulkan_draw(&vertexData);
    done:
    korl_time_probeStop(gfx_batch);
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchRectangleTextured(korl_gfx_createBatchRectangleTextured)
{
    korl_time_probeStart(create_batch_rect_tex);
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
                        + 6 * sizeof(Korl_Vulkan_VertexIndex)
                        + 4 * _KORL_GFX_POSITION_DIMENSIONS*sizeof(f32)
                        + 4 * sizeof(Korl_Math_V2f32)/*UVs*/;
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->modelColor                = KORL_COLOR4U8_WHITE;
    result->_vertexIndexCount         = 6;
    result->_vertexCount              = 4;
    result->blend                     = KORL_GFX_BLEND_ALPHA;
    result->_texture                  = resourceHandleTexture;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexIndices            = KORL_C_CAST(wchar_t*        , result + 1);
    result->_vertexPositions          = KORL_C_CAST(f32*            , KORL_C_CAST(u8*, result->_vertexIndices  ) + 6*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexUvs                = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, result->_vertexPositions) + 4*result->_vertexPositionDimensions*sizeof(f32));
    /* initialize the batch's dynamic data */
    korl_memory_copy(result->_vertexIndices, _KORL_GFX_TRI_QUAD_INDICES, sizeof(_KORL_GFX_TRI_QUAD_INDICES));
    korl_shared_const Korl_Math_V3f32 vertexPositions[] = 
        { {-0.5f, -0.5f, 0.f}
        , { 0.5f, -0.5f, 0.f}
        , { 0.5f,  0.5f, 0.f}
        , {-0.5f,  0.5f, 0.f} };
    korl_memory_copy(result->_vertexPositions, vertexPositions, sizeof(vertexPositions));
    // scale the rectangle mesh vertices by the provided size //
    for(u$ i = 0; i < korl_arraySize(vertexPositions); ++i)
        korl_math_v2f32_assignMultiply(&KORL_C_CAST(Korl_Math_V3f32*, result->_vertexPositions)[i].xy, size);
    korl_shared_const Korl_Math_V2f32 vertexTextureUvs[] = 
        { {0, 1}
        , {1, 1}
        , {1, 0}
        , {0, 0} };
    korl_memory_copy(result->_vertexUvs, vertexTextureUvs, sizeof(vertexTextureUvs));
    /* return the batch */
    korl_time_probeStop(create_batch_rect_tex);
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchRectangleColored(korl_gfx_createBatchRectangleColored)
{
    korl_time_probeStart(create_batch_rect_color);
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
                        + 6 * sizeof(Korl_Vulkan_VertexIndex)
                        + 4 * _KORL_GFX_POSITION_DIMENSIONS*sizeof(f32)
                        + 4 * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*
                                             ,korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->modelColor                = KORL_COLOR4U8_WHITE;
    result->_vertexIndexCount         = 6;
    result->_vertexCount              = 4;
    result->blend                     = KORL_GFX_BLEND_ALPHA;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexIndices            = KORL_C_CAST(Korl_Vulkan_VertexIndex*, result + 1);
    result->_vertexPositions          = KORL_C_CAST(f32*                    , KORL_C_CAST(u8*, result->_vertexIndices   ) + 6*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexColors             = KORL_C_CAST(Korl_Vulkan_Color4u8*   , KORL_C_CAST(u8*, result->_vertexPositions ) + 4*result->_vertexPositionDimensions*sizeof(f32));
    /* initialize the batch's dynamic data */
    korl_shared_const Korl_Vulkan_VertexIndex vertexIndices[] = 
        { 0, 1, 3
        , 1, 2, 3 };
    korl_memory_copy(result->_vertexIndices, vertexIndices, sizeof(vertexIndices));
    korl_shared_const Korl_Math_V3f32 vertexPositions[] = 
        { {0.f, 0.f, 0.f}
        , {1.f, 0.f, 0.f}
        , {1.f, 1.f, 0.f}
        , {0.f, 1.f, 0.f} };
    korl_memory_copy(result->_vertexPositions, vertexPositions, sizeof(vertexPositions));
    // scale & offset the rectangle mesh vertices by the provided params //
    for(u$ i = 0; i < korl_arraySize(vertexPositions); ++i)
    {
        korl_math_v2f32_assignSubtract(&KORL_C_CAST(Korl_Math_V3f32*, result->_vertexPositions)[i].xy, localOriginNormal);
        korl_math_v2f32_assignMultiply(&KORL_C_CAST(Korl_Math_V3f32*, result->_vertexPositions)[i].xy, size);
    }
    for(u$ c = 0; c < result->_vertexCount; c++)
        result->_vertexColors[c] = color;
    /**/
    korl_time_probeStop(create_batch_rect_color);
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchCircle(korl_gfx_createBatchCircle)
{
    return korl_gfx_createBatchCircleSector(allocatorHandle, radius, pointCount, color, KORL_TAU32);
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchCircleSector(korl_gfx_createBatchCircleSector)
{
    korl_time_probeStart(create_batch_circle);
    /* we can't really make a circle shape with < 3 points around the circumference */
    korl_assert(pointCount >= 3);
    /* calculate required amount of memory for the batch */
    const u$ indices = 3*pointCount;//KORL-PERFORMANCE-000-000-018: GFX; (MINOR) use triangle fan primitive for less vertex indices
    const u$ vertices = 1/*for center vertex*/+pointCount;
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + indices * sizeof(Korl_Vulkan_VertexIndex)
        + vertices * _KORL_GFX_POSITION_DIMENSIONS*sizeof(f32)
        + vertices * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;//KORL-PERFORMANCE-000-000-018: GFX; (MINOR) use triangle fan primitive for less vertex indices
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->modelColor                = KORL_COLOR4U8_WHITE;
    result->_vertexIndexCount         = korl_checkCast_u$_to_u32(indices);
    result->_vertexCount              = korl_checkCast_u$_to_u32(vertices);
    result->blend                     = KORL_GFX_BLEND_ALPHA;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexIndices            = KORL_C_CAST(Korl_Vulkan_VertexIndex*, result + 1);
    result->_vertexPositions          = KORL_C_CAST(f32*                    , KORL_C_CAST(u8*, result->_vertexIndices   ) + indices*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexColors             = KORL_C_CAST(Korl_Vulkan_Color4u8*   , KORL_C_CAST(u8*, result->_vertexPositions ) + vertices*result->_vertexPositionDimensions*sizeof(f32));
    /* initialize the batch's dynamic data */
    // vertex[0] is always the center point of the circle //
    KORL_C_CAST(Korl_Math_V3f32*, result->_vertexPositions)[0] = KORL_MATH_V3F32_ZERO;
    const f32 deltaRadians = KORL_TAU32 / pointCount;
    for(u32 p = 0; p < pointCount; p++)
    {
        const f32 spokeRadians = KORL_MATH_MIN(p*deltaRadians, sectorRadians);
        const Korl_Math_V2f32 spoke = korl_math_v2f32_fromRotationZ(radius, spokeRadians);
        KORL_C_CAST(Korl_Math_V3f32*, result->_vertexPositions)[1 + p] = (Korl_Math_V3f32){spoke.x, spoke.y, 0.f};
    }
    for(u$ c = 0; c < vertices; c++)
        result->_vertexColors[c] = color;
    for(u32 p = 0; p < pointCount; p++)
    {
        result->_vertexIndices[3*p + 0] = 0;
        result->_vertexIndices[3*p + 1] = korl_vulkan_safeCast_u$_to_vertexIndex(p + 1);
        result->_vertexIndices[3*p + 2] = korl_vulkan_safeCast_u$_to_vertexIndex(((p + 1) % pointCount) + 1);
        if(p == pointCount - 1 && sectorRadians < KORL_TAU32)
            /* if the circle sector radians does not fill the whole circle, 
                don't connect the last triangle with the first vertex */
            result->_vertexIndices[3*p + 2] = korl_vulkan_safeCast_u$_to_vertexIndex(p + 1);
    }
    /**/
    korl_time_probeStop(create_batch_circle);
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchTriangles(korl_gfx_createBatchTriangles)
{
    korl_time_probeStart(create_batch_tris);
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 3*triangleCount * _KORL_GFX_POSITION_DIMENSIONS*sizeof(f32)
        + 3*triangleCount * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = korl_allocate(allocatorHandle, totalBytes);
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->modelColor                = KORL_COLOR4U8_WHITE;
    result->_vertexCount              = 3*triangleCount;
    result->blend                     = KORL_GFX_BLEND_ALPHA;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexPositions          = KORL_C_CAST(f32*                 , result + 1);
    result->_vertexColors             = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, result->_vertexPositions) + 3*triangleCount*result->_vertexPositionDimensions*sizeof(f32));
    /* return the batch */
    korl_time_probeStop(create_batch_tris);
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchQuadsTextured(korl_gfx_createBatchQuadsTextured)
{
    korl_assert(quadCount > 0);
    korl_assert(quadCount <= KORL_U16_MAX / 6);// ensure that we can store the required vertex index count
    korl_time_probeStart(create_batch_quads_textured);
    /* calculate required amount of memory for the batch */
    const u$ totalBytes =  sizeof(Korl_Gfx_Batch)
                         + 6 * quadCount * sizeof(Korl_Vulkan_VertexIndex)
                         + 4 * quadCount * _KORL_GFX_POSITION_DIMENSIONS * sizeof(f32)
                         + 4 * quadCount * sizeof(Korl_Math_V2f32)/*UVs*/;
    /* allocate the memory */
    Korl_Gfx_Batch*const result = korl_allocate(allocatorHandle, totalBytes);
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->modelColor                = KORL_COLOR4U8_WHITE;
    result->_vertexIndexCount         = 6 * quadCount;
    result->_vertexCount              = 4 * quadCount;
    result->blend                     = KORL_GFX_BLEND_ALPHA;
    result->_texture                  = resourceHandleTexture;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexIndices            = KORL_C_CAST(Korl_Vulkan_VertexIndex*, result + 1);
    result->_vertexPositions          = KORL_C_CAST(f32*            , KORL_C_CAST(u8*, result->_vertexIndices)   + 6 * quadCount * sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexUvs                = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, result->_vertexPositions) + 4 * quadCount * _KORL_GFX_POSITION_DIMENSIONS * sizeof(f32));
    /* we can only initialize vertex indices here, as the user must specify 
        vertex data manually via `korl_gfx_batch_quadsTextured_setQuad` for example */
    for(u32 i = 0; i < quadCount; i++)
        for(u32 j = 0; j < korl_arraySize(_KORL_GFX_TRI_QUAD_INDICES); j++)
            result->_vertexIndices[i * korl_arraySize(_KORL_GFX_TRI_QUAD_INDICES) + j] = korl_vulkan_safeCast_u$_to_vertexIndex(_KORL_GFX_TRI_QUAD_INDICES[j] + (4 * i));// if this cast fails, we have too many quads; u16 index => maximum of 65'535 / 6 == 10922 quads
    /* return the batch */
    korl_time_probeStop(create_batch_quads_textured);
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchQuadsTexturedColored(korl_gfx_createBatchQuadsTexturedColored)
{
    korl_assert(quadCount > 0);
    korl_assert(quadCount <= KORL_U16_MAX / 6);// ensure that we can store the required vertex index count
    korl_time_probeStart(create_batch_quads_textured);
    /* calculate required amount of memory for the batch */
    const u$ totalBytes =  sizeof(Korl_Gfx_Batch)
                         + 6 * quadCount * sizeof(Korl_Vulkan_VertexIndex)
                         + 4 * quadCount * _KORL_GFX_POSITION_DIMENSIONS * sizeof(f32)
                         + 4 * quadCount * sizeof(Korl_Vulkan_Color4u8)
                         + 4 * quadCount * sizeof(Korl_Math_V2f32)/*UVs*/;
    /* allocate the memory */
    Korl_Gfx_Batch*const result = korl_allocate(allocatorHandle, totalBytes);
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->modelColor                = KORL_COLOR4U8_WHITE;
    result->_vertexIndexCount         = 6 * quadCount;
    result->_vertexCount              = 4 * quadCount;
    result->blend                     = KORL_GFX_BLEND_ALPHA;
    result->_texture                  = resourceHandleTexture;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexIndices            = KORL_C_CAST(Korl_Vulkan_VertexIndex*, result + 1);
    result->_vertexPositions          = KORL_C_CAST(f32*                 , KORL_C_CAST(u8*, result->_vertexIndices)   + 6 * quadCount * sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexColors             = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, result->_vertexPositions) + 4 * quadCount * _KORL_GFX_POSITION_DIMENSIONS * sizeof(f32));
    result->_vertexUvs                = KORL_C_CAST(Korl_Math_V2f32*     , KORL_C_CAST(u8*, result->_vertexColors)    + 4 * quadCount * sizeof(Korl_Vulkan_Color4u8));
    /* we can only initialize vertex indices here, as the user must specify 
        vertex data manually via `korl_gfx_batch_quadsTextured_setQuad` for example */
    for(u32 i = 0; i < quadCount; i++)
        for(u32 j = 0; j < korl_arraySize(_KORL_GFX_TRI_QUAD_INDICES); j++)
            result->_vertexIndices[i * korl_arraySize(_KORL_GFX_TRI_QUAD_INDICES) + j] = korl_vulkan_safeCast_u$_to_vertexIndex(_KORL_GFX_TRI_QUAD_INDICES[j] + (4 * i));// if this cast fails, we have too many quads; u16 index => maximum of 65'535 / 6 == 10922 quads
    /* return the batch */
    korl_time_probeStop(create_batch_quads_textured);
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchLines(korl_gfx_createBatchLines)
{
    korl_time_probeStart(create_batch_lines);
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 2*lineCount * _KORL_GFX_POSITION_DIMENSIONS*sizeof(f32)
        + 2*lineCount * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = korl_allocate(allocatorHandle, totalBytes);
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_LINES;
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->modelColor                = KORL_COLOR4U8_WHITE;
    result->_vertexCount              = 2*lineCount;
    result->blend                     = KORL_GFX_BLEND_ALPHA;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexPositions          = KORL_C_CAST(f32*                 , result + 1);
    result->_vertexColors             = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, result->_vertexPositions) + 2*lineCount*result->_vertexPositionDimensions*sizeof(f32));
    /* return the batch */
    korl_time_probeStop(create_batch_lines);
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchAxisLines(korl_gfx_createBatchAxisLines)
{
    Korl_Gfx_Batch*const result = korl_gfx_createBatchLines(allocatorHandle, 3);
    korl_gfx_batchSetLine(result, 0, KORL_MATH_V3F32_ZERO.elements, (Korl_Math_V3f32){1,0,0}.elements, 3, KORL_COLOR4U8_RED);
    korl_gfx_batchSetLine(result, 1, KORL_MATH_V3F32_ZERO.elements, (Korl_Math_V3f32){0,1,0}.elements, 3, KORL_COLOR4U8_GREEN);
    korl_gfx_batchSetLine(result, 2, KORL_MATH_V3F32_ZERO.elements, (Korl_Math_V3f32){0,0,1}.elements, 3, KORL_COLOR4U8_BLUE);
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_createBatchText(korl_gfx_createBatchText)
{
    korl_time_probeStart(create_batch_text);
    korl_assert(text);
    korl_assert(textPixelHeight  >= 1.f);
    korl_assert(outlinePixelSize >= 0.f);
    if(!assetNameFont)
        assetNameFont = L"test-assets/source-sans/SourceSans3-Semibold.otf";//KORL-ISSUE-000-000-086: gfx: default font path doesn't work, since this subdirectly is unlikely in the game project
    /* calculate required amount of memory for the batch */
    korl_time_probeStart(measure_font);
    const u$ assetNameFontBufferSize = korl_string_sizeUtf16(assetNameFont) + 1;
    korl_time_probeStop(measure_font);
    const u$ assetNameFontBytes = assetNameFontBufferSize * sizeof(*assetNameFont);
    /** This value directly affects how much memory will be allocated for vertex 
     * index/attribute data.  Note that it is strictly an over-estimate, because 
     * we cannot possibly know ahead of time whether or not any particular glyph 
     * contains visible components until the font asset is loaded, and we are 
     * doing a sort of lazy-loading system here where the user is allowed to 
     * create & manipulate text batches even in the absence of a font asset. */
    korl_time_probeStart(measure_text);
    const u$ textSize = korl_string_sizeUtf16(text);
    korl_time_probeStop(measure_text);
    const u$ textBufferSize = textSize + 1;
    const u$ textBytes = textBufferSize * sizeof(*text);
    const u$ maxVisibleGlyphCount = textSize * (outlinePixelSize > 0.f ? 2 : 1);
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
                        + assetNameFontBytes
                        + textBytes
                        + maxVisibleGlyphCount * sizeof(Korl_Math_V2f32)// glyph instance positions
                        + maxVisibleGlyphCount * sizeof(u32);           // instance glyph mesh index
    /* allocate the memory */
    korl_time_probeStart(allocate_batch);
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, korl_allocate(allocatorHandle, totalBytes));
    korl_time_probeStop(allocate_batch);
    /* initialize the batch struct */
    result->allocatorHandle             = allocatorHandle;
    result->primitiveType               = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale                      = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                   = KORL_MATH_QUATERNION_IDENTITY;
    result->modelColor                  = KORL_COLOR4U8_WHITE;
    result->_textPixelHeight            = textPixelHeight;
    result->_textPixelOutline           = outlinePixelSize;
    result->_textColor                  = color;
    result->_textColorOutline           = colorOutline;
    result->_textPositionAnchor         = (Korl_Math_V2f32){0,1};// by default, we align the batch position to be the top-left corner of the text's local AABB
    result->_assetNameFont              = KORL_C_CAST(wchar_t*, result + 1);
    result->_text                       = KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, result->_assetNameFont) + assetNameFontBytes);
    result->blend                       = KORL_GFX_BLEND_ALPHA;
    result->_instancePositionDimensions = 2;
    result->_instancePositions          = KORL_C_CAST(f32*, KORL_C_CAST(u8*, result->_text             ) + textBytes);
    result->_instanceU32s               = KORL_C_CAST(u32*, KORL_C_CAST(u8*, result->_instancePositions) + maxVisibleGlyphCount*result->_instancePositionDimensions*sizeof(*result->_instancePositions));
    /* initialize the batch's dynamic data */
    korl_time_probeStart(copy_font);
    if(    korl_string_copyUtf16(assetNameFont, (au16){assetNameFontBufferSize, result->_assetNameFont}) 
        != korl_checkCast_u$_to_i$(assetNameFontBufferSize))
        korl_log(ERROR, "failed to copy asset name \"%ls\" to batch", assetNameFont);
    korl_time_probeStop(copy_font);
    korl_time_probeStart(copy_text);
    if(    korl_string_copyUtf16(text, (au16){textBufferSize, result->_text}) 
        != korl_checkCast_u$_to_i$(textBufferSize))
        korl_log(ERROR, "failed to copy text \"%ls\" to batch", text);
    korl_time_probeStop(copy_text);
    /* We can _not_ precompute vertex indices, see next comment for details... */
    /* We can _not_ precompute vertex colors, because we still don't know how 
        many visible characters in the text there are going to be, and the glyph 
        outlines _must_ be contiguous with the actual text glyphs, as we're 
        sending this to the renderer as a single draw call (one primitive set) */
    // make an initial attempt to generate the text mesh using the font asset, 
    //  and if the asset isn't available at the moment, there is nothing we can 
    //  do about it for now except defer until a later time //
    korl_time_probeStart(gen_mesh);
    _korl_gfx_textGenerateMesh(result, KORL_ASSETCACHE_GET_FLAG_LAZY);
    korl_time_probeStop(gen_mesh);
    /* return the batch */
    korl_time_probeStop(create_batch_text);
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_batchSetBlendState(korl_gfx_batchSetBlendState)
{
    context->blend = blend;
}
korl_internal KORL_FUNCTION_korl_gfx_batchSetPosition(korl_gfx_batchSetPosition)
{
    korl_assert(positionDimensions == 2 || positionDimensions == 3);
    for(u8 d = 0; d < positionDimensions; d++)
        context->_position.elements[d] = position[d];
}
korl_internal KORL_FUNCTION_korl_gfx_batchSetPosition2d(korl_gfx_batchSetPosition2d)
{
    context->_position.x = x;
    context->_position.y = y;
}
korl_internal KORL_FUNCTION_korl_gfx_batchSetPosition2dV2f32(korl_gfx_batchSetPosition2dV2f32)
{
    context->_position.xy = position;
}
korl_internal KORL_FUNCTION_korl_gfx_batchSetScale(korl_gfx_batchSetScale)
{
    context->_scale = scale;
}
korl_internal KORL_FUNCTION_korl_gfx_batchSetQuaternion(korl_gfx_batchSetQuaternion)
{
    context->_rotation = quaternion;
}
korl_internal KORL_FUNCTION_korl_gfx_batchSetRotation(korl_gfx_batchSetRotation)
{
    context->_rotation = korl_math_quaternion_fromAxisRadians(axisOfRotation, radians, false);
}
korl_internal KORL_FUNCTION_korl_gfx_batchSetVertexColor(korl_gfx_batchSetVertexColor)
{
    korl_assert(context->_vertexCount > vertexIndex);
    korl_assert(context->_vertexColors);
    context->_vertexColors[vertexIndex] = color;
}
korl_internal KORL_FUNCTION_korl_gfx_batchAddLine(korl_gfx_batchAddLine)
{
    korl_assert(pContext);
    korl_assert(*pContext);
    const u32 newVertexCount = (*pContext)->_vertexCount + 2;
    const u$ newTotalBytes = sizeof(Korl_Gfx_Batch)
                           + newVertexCount * _KORL_GFX_POSITION_DIMENSIONS*sizeof(f32)
                           + newVertexCount * sizeof(Korl_Vulkan_Color4u8);
    (*pContext) = KORL_C_CAST(Korl_Gfx_Batch*, korl_reallocate((*pContext)->allocatorHandle, *pContext, newTotalBytes));
    void*const previousVertexColors = (*pContext)->_vertexColors;
    korl_assert(*pContext);
    (*pContext)->_vertexPositions = KORL_C_CAST(f32*                 , (*pContext) + 1);
    (*pContext)->_vertexColors    = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, (*pContext)->_vertexPositions) + newVertexCount*_KORL_GFX_POSITION_DIMENSIONS*sizeof(f32));
    korl_memory_move((*pContext)->_vertexColors, previousVertexColors, (*pContext)->_vertexCount*sizeof(Korl_Vulkan_Color4u8));
    (*pContext)->_vertexCount     = newVertexCount;
    /* set the properties of the new line */
    korl_assert(positionDimensions == 2 || positionDimensions == 3);
    for(u8 d = 0; d < positionDimensions; d++)
    {
        KORL_C_CAST(Korl_Math_V3f32*, (*pContext)->_vertexPositions)[newVertexCount - 2].elements[d] = p0[d];
        KORL_C_CAST(Korl_Math_V3f32*, (*pContext)->_vertexPositions)[newVertexCount - 1].elements[d] = p1[d];
    }
    (*pContext)->_vertexColors[newVertexCount - 2] = color0;
    (*pContext)->_vertexColors[newVertexCount - 1] = color1;
}
korl_internal KORL_FUNCTION_korl_gfx_batchSetLine(korl_gfx_batchSetLine)
{
    korl_assert(context->_vertexCount > 2*lineIndex + 1);
    korl_assert(context->_vertexColors);
    korl_assert(positionDimensions == 2 || positionDimensions == 3);
    for(u8 d = 0; d < positionDimensions; d++)
    {
        KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[2*lineIndex + 0].elements[d] = p0[d];
        KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[2*lineIndex + 1].elements[d] = p1[d];
    }
    context->_vertexColors[2*lineIndex + 0] = color;
    context->_vertexColors[2*lineIndex + 1] = color;
}
korl_internal KORL_FUNCTION_korl_gfx_batchTextGetAabb(korl_gfx_batchTextGetAabb)
{
    korl_assert(batchContext->_text && batchContext->_assetNameFont);
    _korl_gfx_textGenerateMesh(batchContext, KORL_ASSETCACHE_GET_FLAG_LAZY);
    if(!batchContext->_textVisibleCharacterCount)
        return (Korl_Math_Aabb2f32){{0, 0}, {0, 0}};
    return batchContext->_textAabb;
}
korl_internal KORL_FUNCTION_korl_gfx_batchTextSetPositionAnchor(korl_gfx_batchTextSetPositionAnchor)
{
    korl_assert(batchContext->_text && batchContext->_assetNameFont);
    batchContext->_textPositionAnchor = textPositionAnchor;
}
korl_internal KORL_FUNCTION_korl_gfx_batchRectangleSetSize(korl_gfx_batchRectangleSetSize)
{
    korl_assert(context->_vertexCount == 4 && context->_vertexIndexCount == 6);
    const Korl_Math_V2f32 originalSize = korl_math_v2f32_subtract(KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[2].xy, KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[0].xy);
    for(u$ c = 0; c < context->_vertexCount; c++)
        KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[c].xy = korl_math_v2f32_multiply(size, korl_math_v2f32_divide(KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[c].xy, originalSize));
}
korl_internal KORL_FUNCTION_korl_gfx_batchRectangleSetColor(korl_gfx_batchRectangleSetColor)
{
    korl_assert(context->_vertexCount == 4 && context->_vertexIndexCount == 6);
    context->modelColor = color;
}
korl_internal KORL_FUNCTION_korl_gfx_batch_rectangle_setUv_raw(korl_gfx_batch_rectangle_setUv_raw)
{
    korl_assert(context->_vertexCount == 4 && context->_vertexIndexCount == 6);
    korl_assert(context->_vertexUvs);
    /* NOTE: the renderer interprets UV {0,0} as the upper-left corner of the image */
    context->_vertexUvs[0] = (Korl_Math_V2f32){aabbRawUvs.min.x, aabbRawUvs.max.y};
    context->_vertexUvs[1] = aabbRawUvs.max;
    context->_vertexUvs[2] = (Korl_Math_V2f32){aabbRawUvs.max.x, aabbRawUvs.min.y};
    context->_vertexUvs[3] = aabbRawUvs.min;
}
korl_internal KORL_FUNCTION_korl_gfx_batch_rectangle_setUv_pixel_to_normal(korl_gfx_batch_rectangle_setUv_pixel_to_normal)
{
    korl_assert(context->_vertexCount == 4 && context->_vertexIndexCount == 6);
    korl_assert(context->_vertexUvs);
    Korl_Math_V2f32 textureSize = korl_math_v2f32_fromV2u32(korl_resource_texture_getSize(context->_texture));
    if(!textureSize.x || !textureSize.y)
        return;// we can't do anything if the texture isn't loaded yet
    /* NOTE: the renderer interprets UV {0,0} as the upper-left corner of the image */
    context->_vertexUvs[0] = korl_math_v2f32_divide((Korl_Math_V2f32){pixelSpaceAabb.min.x, pixelSpaceAabb.max.y}, textureSize);
    context->_vertexUvs[1] = korl_math_v2f32_divide(pixelSpaceAabb.max                                           , textureSize);
    context->_vertexUvs[2] = korl_math_v2f32_divide((Korl_Math_V2f32){pixelSpaceAabb.max.x, pixelSpaceAabb.min.y}, textureSize);
    context->_vertexUvs[3] = korl_math_v2f32_divide(pixelSpaceAabb.min                                           , textureSize);
}
korl_internal KORL_FUNCTION_korl_gfx_batchCircleSetColor(korl_gfx_batchCircleSetColor)
{
    /// for refactoring this code module in the future; we should probably assert this is a circle batch?
    korl_assert(context->_vertexColors);
    for(u$ c = 0; c < context->_vertexCount; c++)
        context->_vertexColors[c] = color;
}
korl_internal KORL_FUNCTION_korl_gfx_batch_quadsTexturedColored_set(korl_gfx_batch_quadsTexturedColored_set)
{
    korl_assert(context->_vertexIndexCount % 6 == 0);
    korl_assert(context->_vertexCount      % 4 == 0);
    korl_assert(context->_vertexPositionDimensions == 3);
    korl_assert(context->_vertexUvs);
    Korl_Math_V3f32*const vertexPositions = KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions);
    for(u8 i = 0; i < 4; i++)
        vertexPositions[(quadIndex * 4) + i] = (Korl_Math_V3f32){.xy = positions[i]};
    /* NOTE: the renderer interprets UV {0,0} as the upper-left corner of the image */
    context->_vertexUvs[(quadIndex * 4) + 0] = (Korl_Math_V2f32){aabbRawUvs.min.x, aabbRawUvs.max.y};
    context->_vertexUvs[(quadIndex * 4) + 1] = aabbRawUvs.max;
    context->_vertexUvs[(quadIndex * 4) + 2] = (Korl_Math_V2f32){aabbRawUvs.max.x, aabbRawUvs.min.y};
    context->_vertexUvs[(quadIndex * 4) + 3] = aabbRawUvs.min;
    if(colors || colorsSize)
    {
        korl_assert(colors);
        korl_assert(colorsSize);
        korl_assert(context->_vertexColors);
        for(u8 i = 0; i < 4; i++)
            context->_vertexColors[(quadIndex * 4) + i] = colors[i % colorsSize];
    }
}
korl_internal KORL_FUNCTION_korl_gfx_batch_quadsTextured_raw(korl_gfx_batch_quadsTextured_raw)
{
    korl_assert(context->_vertexIndexCount % 6 == 0);
    korl_assert(context->_vertexCount      % 4 == 0);
    korl_assert(context->_vertexPositionDimensions == 3);
    korl_assert(context->_vertexUvs);
    const Korl_Math_Aabb2f32 aabb      = korl_math_aabb2f32_fromPoints(positionMinimum.x, positionMinimum.y, positionMinimum.x + size.x, positionMinimum.y + size.y);
    Korl_Math_V3f32*const    positions = KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions);
    positions[(quadIndex * 4) + 0] = (Korl_Math_V3f32){.x = aabb.min.x, .y = aabb.min.y};
    positions[(quadIndex * 4) + 1] = (Korl_Math_V3f32){.x = aabb.max.x, .y = aabb.min.y};
    positions[(quadIndex * 4) + 2] = (Korl_Math_V3f32){.x = aabb.max.x, .y = aabb.max.y};
    positions[(quadIndex * 4) + 3] = (Korl_Math_V3f32){.x = aabb.min.x, .y = aabb.max.y};
    /* NOTE: the renderer interprets UV {0,0} as the upper-left corner of the image */
    context->_vertexUvs[(quadIndex * 4) + 0] = (Korl_Math_V2f32){aabbRawUvs.min.x, aabbRawUvs.max.y};
    context->_vertexUvs[(quadIndex * 4) + 1] = aabbRawUvs.max;
    context->_vertexUvs[(quadIndex * 4) + 2] = (Korl_Math_V2f32){aabbRawUvs.max.x, aabbRawUvs.min.y};
    context->_vertexUvs[(quadIndex * 4) + 3] = aabbRawUvs.min;
    if(colors || colorsSize)
    {
        korl_assert(colors);
        korl_assert(colorsSize);
        korl_assert(context->_vertexColors);
        for(u8 i = 0; i < 4; i++)
            context->_vertexColors[(quadIndex * 4) + i] = colors[i % colorsSize];
    }
}
korl_internal KORL_FUNCTION_korl_gfx_batch_quadsTextured_pixel_to_normal(korl_gfx_batch_quadsTextured_pixel_to_normal)
{
    korl_assert(context->_vertexIndexCount % 6 == 0);
    korl_assert(context->_vertexCount      % 4 == 0);
    korl_assert(context->_vertexPositionDimensions == 3);
    korl_assert(context->_vertexUvs);
    const Korl_Math_Aabb2f32 aabb      = korl_math_aabb2f32_fromPoints(positionMinimum.x, positionMinimum.y, positionMinimum.x + size.x, positionMinimum.y + size.y);
    Korl_Math_V3f32*const    positions = KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions);
    positions[(quadIndex * 4) + 0] = (Korl_Math_V3f32){.x = aabb.min.x, .y = aabb.min.y};
    positions[(quadIndex * 4) + 1] = (Korl_Math_V3f32){.x = aabb.max.x, .y = aabb.min.y};
    positions[(quadIndex * 4) + 2] = (Korl_Math_V3f32){.x = aabb.max.x, .y = aabb.max.y};
    positions[(quadIndex * 4) + 3] = (Korl_Math_V3f32){.x = aabb.min.x, .y = aabb.max.y};
    Korl_Math_V2f32 textureSize = korl_math_v2f32_fromV2u32(korl_resource_texture_getSize(context->_texture));
    if(textureSize.x && textureSize.y)
    {
        /* NOTE: the renderer interprets UV {0,0} as the upper-left corner of the image */
        context->_vertexUvs[(quadIndex * 4) + 0] = korl_math_v2f32_divide((Korl_Math_V2f32){pixelSpaceAabb.min.x, pixelSpaceAabb.max.y}, textureSize);
        context->_vertexUvs[(quadIndex * 4) + 1] = korl_math_v2f32_divide(pixelSpaceAabb.max                                           , textureSize);
        context->_vertexUvs[(quadIndex * 4) + 2] = korl_math_v2f32_divide((Korl_Math_V2f32){pixelSpaceAabb.max.x, pixelSpaceAabb.min.y}, textureSize);
        context->_vertexUvs[(quadIndex * 4) + 3] = korl_math_v2f32_divide(pixelSpaceAabb.min                                           , textureSize);
    }
    //KORL-ISSUE-000-000-146: gfx: korl_gfx_batch_quadsTextured_setQuad: pixel-space => normalized UV coordinate transform requires the underlying texture to be loaded, but the implementation currently _assumes_ that it is already loaded; this leads to bugs where tile maps have corrupted UV data when the OS file cache is cold for the texture file asset (https://learn.microsoft.com/en-us/windows/win32/fileio/file-caching); easily reproduced by manually clearing the file cache using RAMMap: https://superuser.com/a/417112 https://learn.microsoft.com/en-us/sysinternals/downloads/rammap; solutions: (1) store whether or not _any_ quad is "dirty" (add a new flag to Korl_Gfx_Batch?), automatically attempt to re-obtain the correct texture coords each time the Batch is batched using the pixel-space coordinates provided by the user earlier, (2) return whether or not a quad was set with dirty UVs to the caller, then let them decide when to re-set the quads of this batch
}
korl_internal KORL_HEAP_ON_ALLOCATION_MOVED_CALLBACK(_korl_gfx_batch_collectDefragmentPointers_onAllocationMoved)
{
    Korl_Gfx_Batch*const batch = KORL_C_CAST(Korl_Gfx_Batch*, allocationAddress);
    if(batch->_text)
        batch->_text = KORL_C_CAST(wchar_t*, KORL_C_CAST(i$, batch->_text) + byteOffsetFromOldAddress);
    if(batch->_assetNameFont)
        batch->_assetNameFont = KORL_C_CAST(wchar_t*, KORL_C_CAST(i$, batch->_assetNameFont) + byteOffsetFromOldAddress);
    if(batch->_vertexIndices)
        batch->_vertexIndices = KORL_C_CAST(Korl_Vulkan_VertexIndex*, KORL_C_CAST(i$, batch->_vertexIndices) + byteOffsetFromOldAddress);
    if(batch->_vertexPositions)
        batch->_vertexPositions = KORL_C_CAST(f32*, KORL_C_CAST(i$, batch->_vertexPositions) + byteOffsetFromOldAddress);
    if(batch->_vertexColors)
        batch->_vertexColors = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(i$, batch->_vertexColors) + byteOffsetFromOldAddress);
    if(batch->_vertexUvs)
        batch->_vertexUvs = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(i$, batch->_vertexUvs) + byteOffsetFromOldAddress);
    if(batch->_instancePositions)
        batch->_instancePositions = KORL_C_CAST(f32*, KORL_C_CAST(i$, batch->_instancePositions) + byteOffsetFromOldAddress);
    if(batch->_instanceU32s)
        batch->_instanceU32s = KORL_C_CAST(u32*, KORL_C_CAST(i$, batch->_instanceU32s) + byteOffsetFromOldAddress);
}
korl_internal KORL_FUNCTION_korl_gfx_setClearColor(korl_gfx_setClearColor)
{
    korl_vulkan_setSurfaceClearColor((f32[]){KORL_C_CAST(f32,   red) / KORL_C_CAST(f32, KORL_U8_MAX)
                                            ,KORL_C_CAST(f32, green) / KORL_C_CAST(f32, KORL_U8_MAX)
                                            ,KORL_C_CAST(f32,  blue) / KORL_C_CAST(f32, KORL_U8_MAX)});
}
korl_internal KORL_FUNCTION_korl_gfx_batch_collectDefragmentPointers(korl_gfx_batch_collectDefragmentPointers)
{
    // KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD(stbDaMemoryContext, *pStbDaDefragmentPointers, context, parent);
    if(!(*pContext))
        return;
    mcarrpush(KORL_STB_DS_MC_CAST(stbDaMemoryContext), (*pStbDaDefragmentPointers)
             ,(KORL_STRUCT_INITIALIZE(Korl_Heap_DefragmentPointer){pContext, 0, parent, _korl_gfx_batch_collectDefragmentPointers_onAllocationMoved, NULL}));
}
korl_internal KORL_FUNCTION_korl_gfx_drawable_scene3d_initialize(korl_gfx_drawable_scene3d_initialize)
{
    korl_memory_zero(context, sizeof(*context));
    context->type            = KORL_GFX_DRAWABLE_TYPE_SCENE3D;
    context->_model.rotation = KORL_MATH_QUATERNION_IDENTITY;
    context->_model.scale    = KORL_MATH_V3F32_ONE;
    context->subType.scene3d.resourceHandle = resourceHandleScene3d;
    #if 0
    context->subType.scene3d.resourceHandleShaderVertex   = ;
    context->subType.scene3d.resourceHandleShaderFragment = ;
    #endif
}
korl_internal KORL_FUNCTION_korl_gfx_draw(korl_gfx_draw)
{
    Korl_Vulkan_DrawState_Lighting lighting;// leave uninitialized unless we need to flush light data
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Model, model);
    model.transform = korl_math_makeM4f32_rotateScaleTranslate(context->_model.rotation, context->_model.scale, context->_model.position);
    Korl_Vulkan_DrawState_Material material;
    /* if the user provided a material to use with this VertexData, then we just 
        override whatever Material was provided by the SCENE3D Resource */
    if(context->subType.scene3d.materialSlots->used)
        material = context->subType.scene3d.materialSlots[0].material;
    else
        material = korl_resource_scene3d_getMaterial(context->subType.scene3d.resourceHandle);
    // KORL-ISSUE-000-000-156: gfx: if a texture is not present, default to a 1x1 "default" texture (base & specular => white, emissive => black); this would allow the user to choose which textures to provide to a lit material without having to use a different shader/pipeline
    KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawState);
    drawState.model    = &model;
    drawState.material = &material;
    if(!KORL_MEMORY_POOL_ISEMPTY(_korl_gfx_context->pendingLights))
    {
        korl_memory_zero(&lighting, sizeof(lighting));
        lighting.lightsCount = KORL_MEMORY_POOL_SIZE(_korl_gfx_context->pendingLights);
        lighting.lights      = _korl_gfx_context->pendingLights;
        drawState.lighting = &lighting;
        _korl_gfx_context->pendingLights_korlMemoryPoolSize = 0;// does not destroy current lighting data, which is exactly what we need for the remainder of this stack!
    }
    korl_vulkan_setDrawState(&drawState);
    Korl_Vulkan_DrawVertexData drawVertexData = korl_resource_scene3d_getDrawVertexData(context->subType.scene3d.resourceHandle);
    if(drawVertexData.vertexBuffer.type != KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_UNUSED)
        /* we can't draw the scene3d if it has not yet loaded*/
        korl_vulkan_draw(&drawVertexData);
}
korl_internal KORL_FUNCTION_korl_gfx_light_use(korl_gfx_light_use)
{
    for(u$ i = 0; i < lightsSize; i++)
    {
        Korl_Gfx_Light*const newLight = KORL_MEMORY_POOL_ADD(_korl_gfx_context->pendingLights);
        *newLight = lights[i];
    }
}
korl_internal void korl_gfx_defragment(Korl_Memory_AllocatorHandle stackAllocator)
{
    if(!korl_memory_allocator_isFragmented(_korl_gfx_context->allocatorHandle))
        return;
    Korl_Heap_DefragmentPointer* stbDaDefragmentPointers = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(stackAllocator), stbDaDefragmentPointers, 16);
    KORL_MEMORY_STB_DA_DEFRAGMENT                (stackAllocator, stbDaDefragmentPointers, _korl_gfx_context);
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocator, stbDaDefragmentPointers, _korl_gfx_context->stbDaFontCaches, _korl_gfx_context);
    const _Korl_Gfx_FontCache*const*const fontCachesEnd = _korl_gfx_context->stbDaFontCaches + arrlen(_korl_gfx_context->stbDaFontCaches);
    for(_Korl_Gfx_FontCache*const* fontCache = _korl_gfx_context->stbDaFontCaches; fontCache < fontCachesEnd; fontCache++)
    {
        _Korl_Gfx_FontGlyphPage*const fontGlyphPage = _korl_gfx_fontCache_getGlyphPage(*fontCache);
        KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD            (stackAllocator,  stbDaDefragmentPointers, *fontCache                           , _korl_gfx_context->stbDaFontCaches);
        KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD  (stackAllocator,  stbDaDefragmentPointers, fontGlyphPage->stbDaGlyphMeshVertices, *fontCache);
        KORL_MEMORY_STB_DA_DEFRAGMENT_STB_HASHMAP_CHILD(stackAllocator, &stbDaDefragmentPointers, (*fontCache)->stbHmGlyphs            , *fontCache);
    }
    korl_stringPool_collectDefragmentPointers(_korl_gfx_context->stringPool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_gfx_context);
    korl_memory_allocator_defragment(_korl_gfx_context->allocatorHandle, stbDaDefragmentPointers, arrlenu(stbDaDefragmentPointers), stackAllocator);
}
korl_internal u32 korl_gfx_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer)
{
    const u32 byteOffset = korl_checkCast_u$_to_u32((*pByteBuffer)->size);
    korl_memory_byteBuffer_append(pByteBuffer, (acu8){.data = KORL_C_CAST(u8*, &_korl_gfx_context), .size = sizeof(_korl_gfx_context)});
    return byteOffset;
}
korl_internal void korl_gfx_memoryStateRead(const u8* memoryState)
{
    _korl_gfx_context = *KORL_C_CAST(_Korl_Gfx_Context**, memoryState);
    _Korl_Gfx_Context*const context = _korl_gfx_context;
    /* when a memory state is loaded, the raw font assets will need to be 
        re-loaded, and each FontCache contains pointers to the raw font asset's 
        data; ergo, we must tell each FontCache to re-establish these addresses */
    for(Korl_MemoryPool_Size fc = 0; fc < arrlenu(context->stbDaFontCaches); fc++)
    {
        _Korl_Gfx_FontCache*const fontCache = context->stbDaFontCaches[fc];
        fontCache->fontInfoInitialized = false;
    }
}
#undef _LOCAL_STRING_POOL_POINTER
#endif
