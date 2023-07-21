#include "korl-gfx.h"
#include "korl-log.h"
#include "korl-assetCache.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-vulkan.h"
#include "korl-time.h"
#include "korl-stb-truetype.h"
#include "korl-stb-ds.h"
#include "korl-stb-image.h"
#include "utility/korl-stringPool.h"
#include "korl-resource.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-utility-gfx.h"
#if defined(_LOCAL_STRING_POOL_POINTER)
#   undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_gfx_context.stringPool))
#define _KORL_GFX_DEBUG_LOG_GLYPH_PAGE_BITMAPS 0
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
    Korl_Resource_Handle        blankTexture;// a 1x1 texture whose color channels are fully-saturated; can be used as a "default" material map texture
    Korl_Resource_Handle        resourceShaderKorlVertex2d;
    Korl_Resource_Handle        resourceShaderKorlVertex2dColor;
    Korl_Resource_Handle        resourceShaderKorlVertex2dUv;
    Korl_Resource_Handle        resourceShaderKorlVertex3d;
    Korl_Resource_Handle        resourceShaderKorlVertex3dColor;
    Korl_Resource_Handle        resourceShaderKorlVertex3dUv;
    Korl_Resource_Handle        resourceShaderKorlVertexLit;
    Korl_Resource_Handle        resourceShaderKorlVertexText;
    Korl_Resource_Handle        resourceShaderKorlFragmentColor;
    Korl_Resource_Handle        resourceShaderKorlFragmentColorTexture;
    Korl_Resource_Handle        resourceShaderKorlFragmentLit;
    KORL_MEMORY_POOL_DECLARE(Korl_Gfx_Light, pendingLights, KORL_VULKAN_MAX_LIGHTS);// after being added to this pool, lights are flushed to the renderer's draw state upon the next call to `korl_gfx_draw`
} _Korl_Gfx_Context;
typedef struct _Korl_Gfx_Text_Line
{
    u32                visibleCharacters;// used to determine how many glyph instances are in the draw call
    Korl_Math_Aabb2f32 modelAabb;
    Korl_Math_V4f32    color;
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
        if(   0 == korl_string_compareUtf16(utf16AssetNameFont.data, _korl_gfx_fontCache_getFontAssetName(context->stbDaFontCaches[existingFontCacheIndex]), KORL_DEFAULT_C_STRING_SIZE_LIMIT) 
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
        const u$ assetNameFontBufferSize = korl_string_sizeUtf16(utf16AssetNameFont.data, KORL_DEFAULT_C_STRING_SIZE_LIMIT) + 1/*null terminator*/;
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
        createInfoTexture.size = (Korl_Math_V2u32){glyphPage->dataSquareSize, glyphPage->dataSquareSize};
        glyphPage->resourceHandleTexture = korl_resource_createTexture(&createInfoTexture);
        KORL_ZERO_STACK(Korl_Vulkan_CreateInfoBuffer, createInfo);
        createInfo.usageFlags = KORL_VULKAN_BUFFER_USAGE_FLAG_STORAGE;
        createInfo.bytes      = 1/*placeholder non-zero size, since we don't know how many glyphs we are going to cache*/;
        glyphPage->resourceHandleSsboGlyphMeshVertices = korl_resource_createBuffer(&createInfo);
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
    KORL_MEMORY_POOL_EMPTY(_korl_gfx_context->pendingLights);
    if(!_korl_gfx_context->blankTexture)
    {
        KORL_ZERO_STACK(Korl_Vulkan_CreateInfoTexture, createInfoBlankTexture);
        createInfoBlankTexture.size = KORL_MATH_V2U32_ONE;
        _korl_gfx_context->blankTexture = korl_resource_createTexture(&createInfoBlankTexture);
        const Korl_Gfx_Color4u8 blankTextureColor = KORL_COLOR4U8_WHITE;
        korl_resource_update(_korl_gfx_context->blankTexture, &blankTextureColor, sizeof(blankTextureColor), 0);
        /* initiate loading of all built-in shaders */
        _korl_gfx_context->resourceShaderKorlVertex2d             = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-2d.vert.spv"           ), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlVertex2dColor        = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-2d-color.vert.spv"     ), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlVertex2dUv           = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-2d-uv.vert.spv"        ), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlVertex3d             = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-3d.vert.spv"           ), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlVertex3dColor        = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-3d-color.vert.spv"     ), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlVertex3dUv           = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-3d-uv.vert.spv"        ), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlVertexLit            = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-lit.vert.spv"          ), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlVertexText           = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-text.vert.spv"         ), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlFragmentColor        = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-color.frag.spv"        ), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlFragmentColorTexture = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-color-texture.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY);
        _korl_gfx_context->resourceShaderKorlFragmentLit          = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-lit.frag.spv"          ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    }
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
         *   texture and use that in the current pipeline somehow... 
         *   - create a VkImage using 
         *     - the VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT flag 
         *     - the VK_FORMAT_R8_SRGB format
         *   - create a VkImageView using 
         *     - the VK_FORMAT_R8G8B8A8_SRGB format */
        //KORL-PERFORMANCE-000-000-001: gfx: inefficient texture upload process
        /* now that we have a complete baked glyph atlas for this font at the 
            required settings for the desired parameters, 
            we can upload this image buffer to the graphics device for rendering */
        korl_time_probeStart(update_glyph_page_texture);
        // allocate a temp R8G8B8A8-format image buffer //
        const u$ tempImageBufferSize = sizeof(Korl_Gfx_Color4u8) * fontGlyphPage->dataSquareSize * fontGlyphPage->dataSquareSize;
        Korl_Gfx_Color4u8*const tempImageBuffer = korl_allocate(context->allocatorHandle, tempImageBufferSize);
        // "expand" the stbtt font bitmap into the image buffer //
        for(u$ y = 0; y < fontGlyphPage->dataSquareSize; y++)
            for(u$ x = 0; x < fontGlyphPage->dataSquareSize; x++)
                /* store a pure white pixel with the alpha component set to the stbtt font bitmap value */
                tempImageBuffer[y*fontGlyphPage->dataSquareSize + x] = (Korl_Gfx_Color4u8){.r = 0xFF, .g = 0xFF, .b = 0xFF, .a = _korl_gfx_fontGlyphPage_getData(fontGlyphPage)[y*fontGlyphPage->dataSquareSize + x]};
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
    KORL_ZERO_STACK(Korl_Vulkan_CreateInfoBuffer, createInfoBufferText);
    createInfoBufferText.bytes      = 1024;// some arbitrary non-zero value; likely not important to tune this, but we'll see
    createInfoBufferText.usageFlags =   KORL_VULKAN_BUFFER_USAGE_FLAG_VERTEX
                                      | KORL_VULKAN_BUFFER_USAGE_FLAG_INDEX;
    const u$ bytesRequired                  = sizeof(Korl_Gfx_Text) + (utf16AssetNameFont.size + 1/*for null-terminator*/)*sizeof(*utf16AssetNameFont.data);
    Korl_Gfx_Text*const result              = korl_allocate(allocator, bytesRequired);
    u16*const           resultAssetNameFont = KORL_C_CAST(u16*, result + 1);
    result->allocator                       = allocator;
    result->textPixelHeight                 = textPixelHeight;
    result->assetNameFontRawUtf16ByteOffset = sizeof(*result);
    result->assetNameFontRawUtf16Size       = korl_checkCast_u$_to_u32(utf16AssetNameFont.size);
    result->modelRotate                     = KORL_MATH_QUATERNION_IDENTITY;
    result->modelScale                      = KORL_MATH_V3F32_ONE;
    result->resourceHandleBufferText        = korl_resource_createBuffer(&createInfoBufferText);
    // defer adding the vertex indices until _just_ before we draw the text, as this will allow us to shift the entire buffer to effectively delete lines of text @korl-gfx-text-defer-index-buffer
    result->vertexStagingMeta.indexCount = korl_arraySize(KORL_GFX_TRI_QUAD_INDICES);
    result->vertexStagingMeta.indexType  = KORL_GFX_VERTEX_INDEX_TYPE_U16; korl_assert(sizeof(*KORL_GFX_TRI_QUAD_INDICES) == sizeof(u16));
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = offsetof(_Korl_Gfx_FontGlyphInstance, position);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(_Korl_Gfx_FontGlyphInstance);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = VK_VERTEX_INPUT_RATE_INSTANCE;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].byteOffsetBuffer = offsetof(_Korl_Gfx_FontGlyphInstance, meshIndex);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].byteStride       = sizeof(_Korl_Gfx_FontGlyphInstance);
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].inputRate        = VK_VERTEX_INPUT_RATE_INSTANCE;
    result->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].vectorSize       = 1;
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
    _Korl_Gfx_Text_Line* currentLine        = NULL;
    Korl_Math_V2f32      textBaselineCursor = (Korl_Math_V2f32){0.f, 0.f};
    int                  glyphIndexPrevious = -1;// used to calculate kerning advance between the previous glyph and the current glyph
    Korl_Math_V4f32      currentLineColor   = KORL_MATH_V4F32_ONE;// default all line colors to white
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
korl_internal void korl_gfx_text_draw(Korl_Gfx_Text* context, Korl_Math_Aabb2f32 visibleRegion)
{
    /* get the font asset matching the provided asset name */
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(korl_gfx_text_getUtf16AssetNameFont(context), context->textPixelHeight, 0.f/*textPixelOutline*/);
    if(!fontCache)
        return;
    const f32                     lineDeltaY    = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    _Korl_Gfx_FontGlyphPage*const fontGlyphPage = _korl_gfx_fontCache_getGlyphPage(fontCache);
    /* we can now send the vertex index data to the buffer, which will be used 
        for all subsequent text line draw calls @korl-gfx-text-defer-index-buffer */
    const u$ glyphInstanceBufferSize = context->totalVisibleGlyphs * sizeof(_Korl_Gfx_FontGlyphInstance);
    {
        const u32 indexBufferBytes   = sizeof(KORL_GFX_TRI_QUAD_INDICES);
        const u$  currentBufferBytes = korl_resource_getByteSize(context->resourceHandleBufferText);
        if(currentBufferBytes < glyphInstanceBufferSize + indexBufferBytes)
            korl_resource_resize(context->resourceHandleBufferText, glyphInstanceBufferSize + indexBufferBytes);
        korl_resource_update(context->resourceHandleBufferText, KORL_GFX_TRI_QUAD_INDICES, sizeof(KORL_GFX_TRI_QUAD_INDICES), glyphInstanceBufferSize);
        // context->vertexStagingMeta.indexByteOffsetBuffer = glyphInstanceBufferSize;//can't do this yet, since this is relative to the buffer offset byte we choose per line!
    }
    /* configure the renderer draw state */
    Korl_Gfx_Material material = korl_gfx_material_defaultUnlit(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES, KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_BLEND, korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    material.maps.resourceHandleTextureBase       = fontGlyphPage->resourceHandleTexture;
    material.shaders.resourceHandleShaderVertex   = _korl_gfx_context->resourceShaderKorlVertexText;
    material.shaders.resourceHandleShaderFragment = _korl_gfx_context->resourceShaderKorlFragmentColorTexture;
    KORL_ZERO_STACK(Korl_Gfx_DrawState_StorageBuffers, storageBuffers);
    storageBuffers.resourceHandleVertex = fontGlyphPage->resourceHandleSsboGlyphMeshVertices;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.storageBuffers = &storageBuffers;
    korl_vulkan_setDrawState(&drawState);
    /* now we can iterate over each text line and conditionally draw them using line-specific draw state */
    KORL_ZERO_STACK(Korl_Gfx_DrawState_PushConstantData, pushConstantData);
    Korl_Math_M4f32*const pushConstantModelM4f32 = KORL_C_CAST(Korl_Math_M4f32*, pushConstantData.vertex);
    Korl_Math_V3f32 modelTranslation = context->modelTranslate;
    modelTranslation.y -= fontCache->fontAscent;// start the text such that the translation XY position defines the location _directly_ above _all_ the text
    u$ currentVisibleGlyphOffset = 0;// used to determine the byte (transform required) offset into the Text object's text buffer resource
    for(const _Korl_Gfx_Text_Line* line = context->stbDaLines; line < context->stbDaLines + arrlen(context->stbDaLines); line++)
    {
        if(modelTranslation.y < visibleRegion.min.y - fontCache->fontAscent)
            break;
        if(modelTranslation.y <= visibleRegion.max.y + korl_math_f32_positive(fontCache->fontDescent))
        {
            material.fragmentShaderUniform.factorColorBase = line->color;
            *pushConstantModelM4f32                        = korl_math_makeM4f32_rotateScaleTranslate(context->modelRotate, context->modelScale, modelTranslation);
            KORL_ZERO_STACK(Korl_Gfx_DrawState, drawStateLine);
            drawStateLine.material         = &material;
            drawStateLine.pushConstantData = &pushConstantData;
            korl_vulkan_setDrawState(&drawStateLine);
            const u$ textLineByteOffset = currentVisibleGlyphOffset * sizeof(_Korl_Gfx_FontGlyphInstance);
            context->vertexStagingMeta.indexByteOffsetBuffer = korl_checkCast_u$_to_u32(glyphInstanceBufferSize - textLineByteOffset);
            context->vertexStagingMeta.instanceCount         = line->visibleCharacters;
            korl_vulkan_drawVertexBuffer(korl_resource_getVulkanDeviceMemoryAllocationHandle(context->resourceHandleBufferText), textLineByteOffset, &context->vertexStagingMeta);
        }
        modelTranslation.y        -= lineDeltaY;
        currentVisibleGlyphOffset += line->visibleCharacters;
    }
}
korl_internal Korl_Math_Aabb2f32 korl_gfx_text_getModelAabb(const Korl_Gfx_Text* context)
{
    return context->_modelAabb;
}
korl_internal void _korl_gfx_font_getUtfMetrics_common(_Korl_Gfx_FontCache*const fontCache, u32 codepoint, const f32 lineDeltaY, int* glyphIndexPrevious, Korl_Math_V2f32* textBaselineCursor, Korl_Math_Aabb2f32* aabb, Korl_Gfx_Font_TextMetrics* textMetrics)
{
    const _Korl_Gfx_FontBakedGlyph*const bakedGlyph = _korl_gfx_fontCache_getGlyph(fontCache, codepoint);
    if(!bakedGlyph->isEmpty)
    {
        /* instead of generating an AABB that is shrink-wrapped to the actual 
            rendered glyphs, let's generate the AABB with respect to the font's 
            height metrics; so we only really care about the glyph's graphical 
            AABB on the X-axis; why? because I am finding that in a lot of cases 
            the user code cares more about visual alignment of rendered text, 
            and this is a lot easier to achieve API-wise when we are calculating 
            text AABBs relatvie to font metrics */
        const f32 x0 = textBaselineCursor->x;
        const f32 y0 = textBaselineCursor->y + fontCache->fontAscent;
        const f32 x1 = x0 + bakedGlyph->bbox.offsetX + (bakedGlyph->bbox.x1 - bakedGlyph->bbox.x0);
        const f32 y1 = textBaselineCursor->y + fontCache->fontDescent;
        /* at this point, we know that this is a valid visible glyph */
        aabb->min = korl_math_v2f32_min(aabb->min, (Korl_Math_V2f32){x0, y1});
        aabb->max = korl_math_v2f32_max(aabb->max, (Korl_Math_V2f32){x1, y0});
        textMetrics->visibleGlyphCount++;
    }
    if(textBaselineCursor->x > 0.f)
    {
        const int kernAdvance = stbtt_GetGlyphKernAdvance(&fontCache->fontInfo
                                                         ,*glyphIndexPrevious
                                                         ,bakedGlyph->glyphIndex);
        *glyphIndexPrevious    = bakedGlyph->glyphIndex;
        textBaselineCursor->x += fontCache->fontScale*kernAdvance;
    }
    else
        *glyphIndexPrevious = -1;
    textBaselineCursor->x += bakedGlyph->advanceX;
    if(codepoint == L'\n')
    {
        textBaselineCursor->x  = 0.f;
        textBaselineCursor->y -= lineDeltaY;
    }
}
korl_internal Korl_Gfx_Font_TextMetrics _korl_gfx_font_getUtfMetrics(acu16 utf16AssetNameFont, f32 textPixelHeight, const void* utfTextData, u$ utfTextSize, u8 utfEncoding)
{
    Korl_Math_Aabb2f32 aabb = KORL_MATH_AABB2F32_EMPTY;
    KORL_ZERO_STACK(Korl_Gfx_Font_TextMetrics, textMetrics);
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(utf16AssetNameFont, textPixelHeight, 0.f/*textPixelOutline*/);
    if(!fontCache)
        return textMetrics;// silently return nothing if font is not loaded
    const f32       lineDeltaY         = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    Korl_Math_V2f32 textBaselineCursor = (Korl_Math_V2f32){0.f, -fontCache->fontAscent};// default the baseline cursor such that our local origin is placed at _exactly_ the upper-left corner of the text's local AABB based on font metrics, as we know a glyph will never rise _above_ the fontAscent
    int             glyphIndexPrevious = -1;
    switch(utfEncoding)
    {
    case 8:
        //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
        for(Korl_String_CodepointIteratorUtf8 codepointIt = korl_string_codepointIteratorUtf8_initialize(utfTextData, utfTextSize)
           ;!korl_string_codepointIteratorUtf8_done(&codepointIt)
           ; korl_string_codepointIteratorUtf8_next(&codepointIt))
            _korl_gfx_font_getUtfMetrics_common(fontCache, codepointIt._codepoint, lineDeltaY, &glyphIndexPrevious, &textBaselineCursor, &aabb, &textMetrics);
        break;
    case 16:
        //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
        for(Korl_String_CodepointIteratorUtf16 codepointIt = korl_string_codepointIteratorUtf16_initialize(utfTextData, utfTextSize)
           ;!korl_string_codepointIteratorUtf16_done(&codepointIt)
           ; korl_string_codepointIteratorUtf16_next(&codepointIt))
            _korl_gfx_font_getUtfMetrics_common(fontCache, codepointIt._codepoint, lineDeltaY, &glyphIndexPrevious, &textBaselineCursor, &aabb, &textMetrics);
        break;
    default:
        korl_log(ERROR, "unsupported UTF encoding: %hhu", utfEncoding);
        break;
    }
    textMetrics.aabbSize = korl_math_aabb2f32_size(aabb);
    return textMetrics;
}
korl_internal KORL_FUNCTION_korl_gfx_font_getUtf8Metrics(korl_gfx_font_getUtf8Metrics)
{
    return _korl_gfx_font_getUtfMetrics(utf16AssetNameFont, textPixelHeight, utf8Text.data, utf8Text.size, 8);
}
korl_internal KORL_FUNCTION_korl_gfx_font_getUtf16Metrics(korl_gfx_font_getUtf16Metrics)
{
    return _korl_gfx_font_getUtfMetrics(utf16AssetNameFont, textPixelHeight, utf16Text.data, utf16Text.size, 16);
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
korl_internal KORL_FUNCTION_korl_gfx_font_getResources(korl_gfx_font_getResources)
{
    KORL_ZERO_STACK(Korl_Gfx_Font_Resources, resources);
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(utf16AssetNameFont, textPixelHeight, 0.f/*textPixelOutline*/);
    if(!fontCache)
        return resources;
    _Korl_Gfx_FontGlyphPage*const fontGlyphPage = _korl_gfx_fontCache_getGlyphPage(fontCache);
    resources.resourceHandleSsboGlyphMeshVertices = fontGlyphPage->resourceHandleSsboGlyphMeshVertices;
    resources.resourceHandleTexture               = fontGlyphPage->resourceHandleTexture;
    return resources;
}
korl_internal void _korl_gfx_font_generateUtf_common(_Korl_Gfx_FontCache*const fontCache, u32 codepoint, const f32 lineDeltaY, int* glyphIndexPrevious, Korl_Math_V2f32* textBaselineCursor, Korl_Gfx_Font_TextMetrics* textMetrics, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u32* o_glyphInstanceIndices)
{
    const _Korl_Gfx_FontBakedGlyph*const bakedGlyph = _korl_gfx_fontCache_getGlyph(fontCache, codepoint);
    if(!bakedGlyph->isEmpty)
    {
        o_glyphInstancePositions[textMetrics->visibleGlyphCount] = korl_math_v2f32_add(*textBaselineCursor, instancePositionOffset);
        o_glyphInstanceIndices  [textMetrics->visibleGlyphCount] = bakedGlyph->bakeOrder;
        textMetrics->visibleGlyphCount++;
    }
    if(textBaselineCursor->x > 0.f)
    {
        const int kernAdvance = stbtt_GetGlyphKernAdvance(&fontCache->fontInfo
                                                         ,*glyphIndexPrevious
                                                         ,bakedGlyph->glyphIndex);
        *glyphIndexPrevious    = bakedGlyph->glyphIndex;
        textBaselineCursor->x += fontCache->fontScale*kernAdvance;
    }
    else
        *glyphIndexPrevious = -1;
    textBaselineCursor->x += bakedGlyph->advanceX;
    if(codepoint == L'\n')
    {
        textBaselineCursor->x  = 0.f;
        textBaselineCursor->y -= lineDeltaY;
    }
}
korl_internal void _korl_gfx_font_generateUtf(acu16 utf16AssetNameFont, f32 textPixelHeight, const void* utfText, u$ utfTextSize, u8 utfEncoding, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u32* o_glyphInstanceIndices)
{
    _Korl_Gfx_Context*const   context   = _korl_gfx_context;
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(utf16AssetNameFont, textPixelHeight, 0.f/*textPixelOutline*/);
    if(!fontCache)
        return;
    _Korl_Gfx_FontGlyphPage*const fontGlyphPage = _korl_gfx_fontCache_getGlyphPage(fontCache);
    const f32                     lineDeltaY    = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    Korl_Math_V2f32 textBaselineCursor  = (Korl_Math_V2f32){0.f, -fontCache->fontAscent};// default the baseline cursor such that our local origin is placed at _exactly_ the upper-left corner of the text's local AABB based on font metrics, as we know a glyph will never rise _above_ the fontAscent
    int             glyphIndexPrevious  = -1;
    KORL_ZERO_STACK(Korl_Gfx_Font_TextMetrics, textMetrics);
    switch(utfEncoding)
    {
    case 8:
        //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
        for(Korl_String_CodepointIteratorUtf8 codepointIt = korl_string_codepointIteratorUtf8_initialize(utfText, utfTextSize)
           ;!korl_string_codepointIteratorUtf8_done(&codepointIt)
           ; korl_string_codepointIteratorUtf8_next(&codepointIt))
            _korl_gfx_font_generateUtf_common(fontCache, codepointIt._codepoint, lineDeltaY, &glyphIndexPrevious, &textBaselineCursor, &textMetrics, instancePositionOffset, o_glyphInstancePositions, o_glyphInstanceIndices);
        break;
    case 16:
        //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
        for(Korl_String_CodepointIteratorUtf16 codepointIt = korl_string_codepointIteratorUtf16_initialize(utfText, utfTextSize)
           ;!korl_string_codepointIteratorUtf16_done(&codepointIt)
           ; korl_string_codepointIteratorUtf16_next(&codepointIt))
            _korl_gfx_font_generateUtf_common(fontCache, codepointIt._codepoint, lineDeltaY, &glyphIndexPrevious, &textBaselineCursor, &textMetrics, instancePositionOffset, o_glyphInstancePositions, o_glyphInstanceIndices);
        break;
    default:
        korl_log(ERROR, "unsupported UTF encoding: %hhu", utfEncoding);
        break;
    }
}
korl_internal KORL_FUNCTION_korl_gfx_font_generateUtf8(korl_gfx_font_generateUtf8)
{
    _korl_gfx_font_generateUtf(utf16AssetNameFont, textPixelHeight, utf8Text.data, utf8Text.size, 8, instancePositionOffset, o_glyphInstancePositions, o_glyphInstanceIndices);
}
korl_internal KORL_FUNCTION_korl_gfx_font_generateUtf16(korl_gfx_font_generateUtf16)
{
    _korl_gfx_font_generateUtf(utf16AssetNameFont, textPixelHeight, utf16Text.data, utf16Text.size, 16, instancePositionOffset, o_glyphInstancePositions, o_glyphInstanceIndices);
}
korl_internal Korl_Math_V2f32 korl_gfx_font_textGraphemePosition(acu16 utf16AssetNameFont, f32 textPixelHeight, acu8 utf8Text, u$ graphemeIndex)
{
    KORL_ZERO_STACK(Korl_Gfx_Font_Metrics, metrics);
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(utf16AssetNameFont, textPixelHeight, 0.f/*textPixelOutline*/);
    if(!fontCache)
        return KORL_MATH_V2F32_ZERO;// silently return 0 if font is not loaded
    const f32       lineDeltaY         = (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
    Korl_Math_V2f32 textBaselineCursor = KORL_MATH_V2F32_ZERO;
    int             glyphIndexPrevious = -1;
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
korl_internal KORL_FUNCTION_korl_gfx_useCamera(korl_gfx_useCamera)
{
    _Korl_Gfx_Context*const context = _korl_gfx_context;
    korl_time_probeStart(useCamera);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Scissor, scissor);
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
    KORL_ZERO_STACK(Korl_Gfx_DrawState_SceneProperties, sceneProperties);
    sceneProperties.view = korl_gfx_camera_view(&camera);
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
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
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
    Korl_Gfx_ResultRay3d result = {.position  = {korl_math_f32_nan(), korl_math_f32_nan(), korl_math_f32_nan()}
                                  ,.direction = {korl_math_f32_nan(), korl_math_f32_nan(), korl_math_f32_nan()}};
    //KORL-PERFORMANCE-000-000-041: gfx: I expect this to be SLOW; we should instead be caching the camera's VP matrices and only update them when they are "dirty"; I know for a fact that SFML does this in its sf::camera class
    const Korl_Math_M4f32 view                  = korl_gfx_camera_view(context);
    const Korl_Math_M4f32 projection            = korl_gfx_camera_projection(context);
    const Korl_Math_M4f32 viewProjection        = korl_math_m4f32_multiply(&projection, &view);
    const Korl_Math_M4f32 viewProjectionInverse = korl_math_m4f32_invert(&viewProjection);
    if(korl_math_f32_isNan(viewProjectionInverse.r0c0))
    {
        korl_log(WARNING, "failed to invert view-projection matrix");
        return result;
    }
    const Korl_Math_V2f32 v2f32WindowPos = {KORL_C_CAST(f32, windowPosition.x)
                                           ,KORL_C_CAST(f32, windowPosition.y)};
    /* viewport-space => normalized-device-space */
    //KORL-ISSUE-000-000-101: gfx: ASSUMPTION: viewport is the size of the entire window; if we ever want to handle separate viewport clip regions per-camera, we will have to modify this
    const Korl_Math_V2f32 eyeRayNds = {  2.f * v2f32WindowPos.x / KORL_C_CAST(f32, gfxContext->surfaceSize.x) - 1.f
                                      , -2.f * v2f32WindowPos.y / KORL_C_CAST(f32, gfxContext->surfaceSize.y) + 1.f };
    /* normalized-device-space => homogeneous-clip-space */
    /* here the eye ray becomes two coordinates, since the eye ray pierces the 
        clip-space box, which can be described as a line segment defined by the 
        eye ray's intersection w/ the near & far planes; the near & far clip 
        planes defined by korl-vulkan are 1 & 0 respectively, since Vulkan 
        (with no extensions, which I don't want to use) requires a normalized 
        depth buffer, and korl-vulkan uses a "reverse" depth buffer */
    //KORL-ISSUE-000-000-102: gfx: testing required; ASSUMPTION: right-handed HC-space coordinate system that spans [0,1]; need to actually test to see if this works
    const Korl_Math_V4f32 eyeRayHcsFar  = {eyeRayNds.x, eyeRayNds.y, 0.f, 1.f};
    const Korl_Math_V4f32 eyeRayHcsNear = {eyeRayNds.x, eyeRayNds.y, 1.f, 1.f};
    /* homogeneous-clip-space => UNSCALED world-space; unscaled because transforms through projection matrix likely modify the `w` component, so to get the "true" V3f32, we need to multiply all components by `1 / w` */
    const Korl_Math_V4f32 eyeRayFarUnscaled  = korl_math_m4f32_multiplyV4f32(&viewProjectionInverse, &eyeRayHcsFar);
    const Korl_Math_V4f32 eyeRayNearUnscaled = korl_math_m4f32_multiplyV4f32(&viewProjectionInverse, &eyeRayHcsNear);
    /* UNSCALED world-space => TRUE world-space */
    const Korl_Math_V3f32 eyeRayFar  = korl_math_v3f32_multiplyScalar(eyeRayFarUnscaled.xyz, 1.f / eyeRayFarUnscaled.w);
    const Korl_Math_V3f32 eyeRayNear = korl_math_v3f32_multiplyScalar(eyeRayNearUnscaled.xyz, 1.f / eyeRayNearUnscaled.w);
    /* compute the ray using the final transformed eye ray line segment */
    const Korl_Math_V3f32 eyeRayNearToFar         = korl_math_v3f32_subtract(eyeRayFar, eyeRayNear);
    const f32             eyeRayNearToFarDistance = korl_math_v3f32_magnitude(&eyeRayNearToFar);
    result.position        = eyeRayNear;
    result.direction       = korl_math_v3f32_normalKnownMagnitude(eyeRayNearToFar, eyeRayNearToFarDistance);
    result.segmentDistance = eyeRayNearToFarDistance;
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_camera_worldToWindow(korl_gfx_camera_worldToWindow)
{
    _Korl_Gfx_Context*const gfxContext = _korl_gfx_context;
    //KORL-PERFORMANCE-000-000-041: gfx: I expect this to be SLOW; we should instead be caching the camera's VP matrices and only update them when they are "dirty"; I know for a fact that SFML does this in its sf::camera class
    const Korl_Math_M4f32 view       = korl_gfx_camera_view(context);
    const Korl_Math_M4f32 projection = korl_gfx_camera_projection(context);
    //KORL-ISSUE-000-000-101: gfx: ASSUMPTION: viewport is the size of the entire window; if we ever want to handle separate viewport clip regions per-camera, we will have to modify this
    const Korl_Math_Aabb2f32 viewport     = {.min={0,0}
                                            ,.max={KORL_C_CAST(f32, gfxContext->surfaceSize.x)
                                                  ,KORL_C_CAST(f32, gfxContext->surfaceSize.y)}};
    const Korl_Math_V2f32    viewportSize = korl_math_aabb2f32_size(viewport);
    /* transform from world => camera => clip space */
    const Korl_Math_V4f32 worldPoint       = {worldPosition.x, worldPosition.y, worldPosition.z, 1};
    const Korl_Math_V4f32 cameraSpacePoint = korl_math_m4f32_multiplyV4f32(&view, &worldPoint);
    const Korl_Math_V4f32 clipSpacePoint   = korl_math_m4f32_multiplyV4f32(&projection, &cameraSpacePoint);
    if(korl_math_isNearlyZero(clipSpacePoint.w))
        return (Korl_Math_V2f32){korl_math_f32_nan(), korl_math_f32_nan()};
    /* calculate normalized-device-coordinate-space 
        y is inverted here because screen-space y axis is flipped! */
    const Korl_Math_V3f32 ndcSpacePoint = { clipSpacePoint.x /  clipSpacePoint.w
                                          , clipSpacePoint.y / -clipSpacePoint.w
                                          , clipSpacePoint.z /  clipSpacePoint.w };
    /* Calculate screen-space.  GLSL formula: ((ndcSpacePoint.xy + 1.0) / 2.0) * viewSize + viewOffset */
    const Korl_Math_V2f32 result = { ((ndcSpacePoint.x + 1.f) / 2.f) * viewportSize.x + viewport.min.x
                                   , ((ndcSpacePoint.y + 1.f) / 2.f) * viewportSize.y + viewport.min.y };
    return result;
}
korl_internal KORL_FUNCTION_korl_gfx_getSurfaceSize(korl_gfx_getSurfaceSize)
{
    return _korl_gfx_context->surfaceSize;
}
korl_internal KORL_FUNCTION_korl_gfx_setClearColor(korl_gfx_setClearColor)
{
    korl_vulkan_setSurfaceClearColor((f32[]){KORL_C_CAST(f32,   red) / KORL_C_CAST(f32, KORL_U8_MAX)
                                            ,KORL_C_CAST(f32, green) / KORL_C_CAST(f32, KORL_U8_MAX)
                                            ,KORL_C_CAST(f32,  blue) / KORL_C_CAST(f32, KORL_U8_MAX)});
}
korl_internal KORL_FUNCTION_korl_gfx_draw(korl_gfx_draw)
{
    if(materials)
        korl_assert(materialsSize);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_PushConstantData, pushConstantData);
    *KORL_C_CAST(Korl_Math_M4f32*, pushConstantData.vertex) = korl_math_transform3d_m4f32(&context->transform);
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.pushConstantData = &pushConstantData;
    korl_gfx_setDrawState(&drawState);
    switch(context->type)
    {
    case KORL_GFX_DRAWABLE_TYPE_IMMEDIATE:{
        /* configure the renderer draw state */
        Korl_Gfx_Material materialLocal;
        if(materials)
        {
            korl_assert(materialsSize == 1);
            materialLocal                      = materials[0];
            materialLocal.modes.primitiveType  = context->subType.immediate.primitiveType;
            materialLocal.modes.flags         |= context->subType.immediate.materialModeFlags;
        }
        else
            materialLocal = korl_gfx_material_defaultUnlit(context->subType.immediate.primitiveType, context->subType.immediate.materialModeFlags, korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
        if(context->subType.immediate.overrides.materialMapBase)
            materialLocal.maps.resourceHandleTextureBase = context->subType.immediate.overrides.materialMapBase;
        if(context->subType.immediate.overrides.shaderVertex)
            materialLocal.shaders.resourceHandleShaderVertex = context->subType.immediate.overrides.shaderVertex;
        else if(!materialLocal.shaders.resourceHandleShaderVertex)
            materialLocal.shaders.resourceHandleShaderVertex = korl_gfx_getBuiltInShaderVertex(&context->subType.immediate.vertexStagingMeta);
        if(context->subType.immediate.overrides.shaderFragment)
            materialLocal.shaders.resourceHandleShaderFragment = context->subType.immediate.overrides.shaderFragment;
        else if(!materialLocal.shaders.resourceHandleShaderFragment)
            materialLocal.shaders.resourceHandleShaderFragment = korl_gfx_getBuiltInShaderFragment(&materialLocal);
        KORL_ZERO_STACK(Korl_Gfx_DrawState_StorageBuffers, storageBuffers);
        storageBuffers.resourceHandleVertex = context->subType.immediate.overrides.storageBufferVertex;
        drawState = KORL_STRUCT_INITIALIZE_ZERO(Korl_Gfx_DrawState);
        if(context->subType.immediate.overrides.storageBufferVertex)
            drawState.storageBuffers = &storageBuffers;
        drawState.material = &materialLocal;
        korl_vulkan_setDrawState(&drawState);
        korl_gfx_drawStagingAllocation(&context->subType.immediate.stagingAllocation, &context->subType.immediate.vertexStagingMeta);
        break;}
    case KORL_GFX_DRAWABLE_TYPE_MESH:{
        const acu8 utf8MeshName = (acu8){.size = context->subType.mesh.rawUtf8Scene3dMeshNameSize
                                        ,.data = context->subType.mesh.rawUtf8Scene3dMeshName};
        u32                                       meshPrimitiveCount       = 0;
        Korl_Vulkan_DeviceMemory_AllocationHandle meshPrimitiveBuffer      = 0;
        const Korl_Gfx_VertexStagingMeta*         meshPrimitiveVertexMetas = NULL;
        const Korl_Gfx_Material*                  meshMaterials            = NULL;
        korl_resource_scene3d_getMeshDrawData(context->subType.mesh.resourceHandleScene3d, utf8MeshName
                                             ,&meshPrimitiveCount
                                             ,&meshPrimitiveBuffer
                                             ,&meshPrimitiveVertexMetas
                                             ,&meshMaterials);
        Korl_Gfx_Material materialLocal;
        for(u32 mp = 0; mp < meshPrimitiveCount; mp++)
        {
            drawState = KORL_STRUCT_INITIALIZE_ZERO(Korl_Gfx_DrawState);
            if(materials && mp < materialsSize)
            {
                materialLocal                      = materials[mp];
                materialLocal.modes.primitiveType  = meshMaterials[mp].modes.primitiveType;
                materialLocal.modes.flags         |= meshMaterials[mp].modes.flags;
                drawState.material = &materialLocal;
            }
            else
                drawState.material = meshMaterials + mp;
            korl_vulkan_setDrawState(&drawState);
            korl_vulkan_drawVertexBuffer(meshPrimitiveBuffer, 0, meshPrimitiveVertexMetas + mp);
        }
        break;}
    }
}
korl_internal KORL_FUNCTION_korl_gfx_setDrawState(korl_gfx_setDrawState)
{
    Korl_Gfx_DrawState drawState2 = *drawState;
    Korl_Gfx_DrawState_Lighting lighting;// leave uninitialized unless we need to flush light data
    if(!KORL_MEMORY_POOL_ISEMPTY(_korl_gfx_context->pendingLights))
    {
        korl_memory_zero(&lighting, sizeof(lighting));
        lighting.lightsCount = KORL_MEMORY_POOL_SIZE(_korl_gfx_context->pendingLights);
        lighting.lights      = _korl_gfx_context->pendingLights;
        drawState2.lighting = &lighting;
        _korl_gfx_context->pendingLights_korlMemoryPoolSize = 0;// does not destroy current lighting data, which is exactly what we need for the remainder of this stack!
    }
    korl_vulkan_setDrawState(&drawState2);
}
korl_internal KORL_FUNCTION_korl_gfx_stagingAllocate(korl_gfx_stagingAllocate)
{
    return korl_vulkan_stagingAllocate(stagingMeta);
}
korl_internal KORL_FUNCTION_korl_gfx_stagingReallocate(korl_gfx_stagingReallocate)
{
    return korl_vulkan_stagingReallocate(stagingMeta, stagingAllocation);
}
korl_internal KORL_FUNCTION_korl_gfx_drawStagingAllocation(korl_gfx_drawStagingAllocation)
{
    korl_vulkan_drawStagingAllocation(stagingAllocation, stagingMeta);
}
korl_internal KORL_FUNCTION_korl_gfx_getBuiltInShaderVertex(korl_gfx_getBuiltInShaderVertex)
{
    if(vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize == 2)
        if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
           && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex2dColor;
        else if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
                && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex2dUv;
        else
            return _korl_gfx_context->resourceShaderKorlVertex2d;
    else if(vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize == 3)
        if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
           && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex3dColor;
        else if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
                && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex3dUv;
        else
            return _korl_gfx_context->resourceShaderKorlVertex3d;
    korl_log(ERROR, "unsupported built-in vertex shader configuration");
    return 0;
}
korl_internal KORL_FUNCTION_korl_gfx_getBuiltInShaderFragment(korl_gfx_getBuiltInShaderFragment)
{
    if(material->maps.resourceHandleTextureBase)
        return _korl_gfx_context->resourceShaderKorlFragmentColorTexture;
    return _korl_gfx_context->resourceShaderKorlFragmentColor;
}
korl_internal KORL_FUNCTION_korl_gfx_light_use(korl_gfx_light_use)
{
    for(u$ i = 0; i < lightsSize; i++)
    {
        Korl_Gfx_Light*const newLight = KORL_MEMORY_POOL_ADD(_korl_gfx_context->pendingLights);
        *newLight = lights[i];
    }
}
korl_internal KORL_FUNCTION_korl_gfx_getBlankTexture(korl_gfx_getBlankTexture)
{
    return _korl_gfx_context->blankTexture;
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
