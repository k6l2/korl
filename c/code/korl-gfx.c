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
#include "korl-stringPool.h"
#include "korl-resource.h"
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
_STATIC_ASSERT(sizeof(Korl_Vulkan_DeviceMemory_AllocationHandle) == sizeof(Korl_Gfx_DeviceMemoryAllocationHandle));
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
    u16                               packRowsSize;
    u16                               packRowsCapacity;
    _Korl_Gfx_FontGlyphBitmapPackRow* packRows;// emplaced after `data` in memory
    Korl_Resource_Handle resourceHandleTexture;
    Korl_Resource_Handle resourceHandleSsboGlyphMeshVertices;
    u16 dataSquareSize;
    u8* data;//1-channel, Alpha8 format, with an array size of dataSquareSize*dataSquareSize; currently being stored in contiguously memory immediately following this struct
    bool textureOutOfDate;// when this flag is set, it means that there are pending changes to `data` which have yet to be uploaded to the GPU
    _Korl_Gfx_FontGlyphVertex* stbDaGlyphMeshVertices;
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
    f32 pixelHeight;
    f32 pixelOutlineThickness;// if this is 0.f, then an outline has not yet been cached in this fontCache glyphPageList
    //KORL-PERFORMANCE-000-000-000: (minor) convert pointers to contiguous memory into offsets to save some space
    wchar_t* fontAssetName;
    _Korl_Gfx_FontBakedGlyphMap* stbHmGlyphs;
    _Korl_Gfx_FontGlyphPage* glyphPage;
} _Korl_Gfx_FontCache;
typedef struct _Korl_Gfx_Context
{
    /** used to store persistent data, such as Font asset glyph cache/database */
    Korl_Memory_AllocatorHandle allocatorHandle;
    _Korl_Gfx_FontCache** stbDaFontCaches;
    u8 nextResourceSalt;
    Korl_StringPool stringPool;// used for Resource database strings
    Korl_Math_V2u32 surfaceSize;// updated at the top of each frame, ideally before anything has a chance to use korl-gfx
    Korl_Gfx_Camera currentCameraState;
} _Korl_Gfx_Context;
typedef struct _Korl_Gfx_Text_Line
{
    u32 visibleCharacters;// used to determine how many glyph instances are in the draw call
    Korl_Math_Aabb2f32 modelAabb;
    Korl_Math_V4f32 color;
} _Korl_Gfx_Text_Line;
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
korl_internal const _Korl_Gfx_FontBakedGlyph* _korl_gfx_fontCache_getGlyph(_Korl_Gfx_FontCache* fontCache, u32 codePoint)
{
    _Korl_Gfx_Context*const gfxContext      = &_korl_gfx_context;
    _Korl_Gfx_FontGlyphPage*const glyphPage = fontCache->glyphPage;
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
                _korl_gfx_glyphPage_insert(glyphPage, w, h, 
                                           &glyph->bbox.x0, 
                                           &glyph->bbox.y0, 
                                           &glyph->bbox.x1, 
                                           &glyph->bbox.y1);
                glyph->bbox.offsetX = glyph->bearingLeft;
                glyph->bbox.offsetY = -KORL_C_CAST(f32, iy1);
                /* actually write the glyph bitmap in the glyph page data */
                stbtt_MakeGlyphBitmap(&(fontCache->fontInfo), 
                                      glyphPage->data + (glyph->bbox.y0*glyphPage->dataSquareSize + glyph->bbox.x0), 
                                      w, h, glyphPage->dataSquareSize, 
                                      fontCache->fontScale, fontCache->fontScale, glyphIndex);
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
                const int outlineOffset = KORL_C_CAST(int, korl_math_ceil(fontCache->pixelOutlineThickness));
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
                        if(fontCache->glyphPage->data[pageY*fontCache->glyphPage->dataSquareSize + pageX])
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
                        u8*const dataPixel = glyphPage->data + ((glyph->bboxOutline.y0 + y)*glyphPage->dataSquareSize + glyph->bboxOutline.x0 + x);
                        *dataPixel = outlineBuffer[y*outlineW + x];
                    }
                /* free temporary resources */
                korl_free(gfxContext->allocatorHandle, outlineBuffer);
#else// This technique looks like poop, and I can't seem to change that, so maybe just delete this section?
                const u8 on_edge_value     = 180;// This is rather arbitrary; it seems like we can just select any reasonable # here and it will be enough for most fonts/sizes.  This value seems rather finicky however...  So if we have to modify how any of this glyph outline stuff is drawn, this will likely have to change somehow.
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
        fontCache->glyphPage->textureOutOfDate = true;
    }//if(!glyph)// create new glyph code
    // at this point, we should have a valid glyph //
    korl_assert(glyph);
    return glyph;
}
korl_internal _Korl_Gfx_FontCache* _korl_gfx_matchFontCache(acu16 utf16AssetNameFont, f32 textPixelHeight, f32 textPixelOutline)
{
    _Korl_Gfx_Context*const context = &_korl_gfx_context;
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
        const int padding          = 1/*this is necessary to keep the SDF glyph outline from bleeding all the way to the bitmap edge*/ + KORL_C_CAST(int, korl_math_ceil(pixelOutlineThickness));
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
        if(   0 == korl_memory_stringCompare(utf16AssetNameFont.data, context->stbDaFontCaches[existingFontCacheIndex]->fontAssetName) 
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
        const u$ assetNameFontBufferSize = korl_memory_stringSize(utf16AssetNameFont.data) + 1/*null terminator*/;
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
#if KORL_DEBUG
        korl_assert(korl_memory_isNull(*newFontCache, fontCacheRequiredBytes));
#endif//KORL_DEBUG
        fontCache->pixelHeight             = textPixelHeight;
        fontCache->pixelOutlineThickness   = textPixelOutline;
        fontCache->fontAssetName           = KORL_C_CAST(wchar_t*, fontCache + 1);
        fontCache->glyphPage               = KORL_C_CAST(_Korl_Gfx_FontGlyphPage*, KORL_C_CAST(u8*, fontCache->fontAssetName) + assetNameFontBufferBytes);
        mchmdefault(KORL_STB_DS_MC_CAST(context->allocatorHandle), fontCache->stbHmGlyphs, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Gfx_FontBakedGlyph));
        _Korl_Gfx_FontGlyphPage*const glyphPage = fontCache->glyphPage;
        glyphPage->packRowsCapacity = PACK_ROWS_CAPACITY;
        glyphPage->dataSquareSize   = GLYPH_PAGE_SQUARE_SIZE;
        glyphPage->data             = KORL_C_CAST(u8*, glyphPage + 1/*pointer arithmetic trick to skip to the address following the glyphPage*/);
        glyphPage->packRows         = KORL_C_CAST(_Korl_Gfx_FontGlyphBitmapPackRow*, KORL_C_CAST(u8*, glyphPage->data + GLYPH_PAGE_SQUARE_SIZE*GLYPH_PAGE_SQUARE_SIZE));
        mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), glyphPage->stbDaGlyphMeshVertices, 512);
        korl_assert(korl_checkCast_u$_to_i$(assetNameFontBufferSize) == korl_memory_stringCopy(utf16AssetNameFont.data, fontCache->fontAssetName, assetNameFontBufferSize));
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
    return fontCache;
}
/** Vertex order for each glyph quad:
 * [ bottom-left
 * , bottom-right
 * , top-right
 * , top-left ] */
korl_internal void _korl_gfx_textGenerateMesh(Korl_Gfx_Batch*const batch, Korl_AssetCache_Get_Flags assetCacheGetFlags)
{
    _Korl_Gfx_Context*const context = &_korl_gfx_context;
    if(!batch->_assetNameFont || batch->_fontTextureHandle)
        return;
    _Korl_Gfx_FontCache* fontCache = _korl_gfx_matchFontCache((acu16){.data = batch->_assetNameFont
                                                                     ,.size = korl_memory_stringSize(batch->_assetNameFont)}
                                                             ,batch->_textPixelHeight
                                                             ,batch->_textPixelOutline);
    if(!fontCache)
        return;
    _Korl_Gfx_FontGlyphPage*const fontGlyphPage = fontCache->glyphPage;
    if(batch->_textVisibleCharacterCount)
        goto done_setFontTextureHandle;
    /* iterate over each character in the batch text, and update the 
        pre-allocated vertex attributes to use the data for the corresponding 
        codepoint in the glyph cache */
    Korl_Math_V2f32 textBaselineCursor = (Korl_Math_V2f32){0.f, 0.f};
    u32 currentGlyph = 0;
    const bool outlinePass = (batch->_textPixelOutline > 0.f);
    /* While we're at it, we can also generate the text mesh AABB...
        We need to accumulate the AABB taking into account the following properties:
        - outline thickness
        - font vertical metrics
        - glyph horizontal metrics
        - glyph X bearings */
    batch->_textAabb = (Korl_Math_Aabb2f32 ){.min = {0.f, 0.f}, .max = {0.f, 0.f}};
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
            textBaselineCursor.x = 0.f;
            textBaselineCursor.y -= (fontCache->fontAscent - fontCache->fontDescent) + fontCache->fontLineGap;
            continue;
        }
        glyphIndexPrevious = bakedGlyph->glyphIndex;
        if(bakedGlyph->isEmpty)
            continue;
        batch->_textAabb.min = korl_math_v2f32_min(batch->_textAabb.min, (Korl_Math_V2f32){x0, y0});
        batch->_textAabb.max = korl_math_v2f32_max(batch->_textAabb.max, (Korl_Math_V2f32){x1, y1});
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
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        const f32 viewportWidthOverHeight = _korl_gfx_context.surfaceSize.y == 0 
            ? 1.f 
            :  KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.x)
             / KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.y);
        return korl_math_m4f32_projectionFov(context->subCamera.perspective.fovHorizonDegrees
                                            ,viewportWidthOverHeight
                                            ,context->subCamera.perspective.clipNear
                                            ,context->subCamera.perspective.clipFar);}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        const f32 left   = 0.f - context->subCamera.orthographic.originAnchor.x*KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.x);
        const f32 bottom = 0.f - context->subCamera.orthographic.originAnchor.y*KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.y);
        const f32 right  = KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.x) - context->subCamera.orthographic.originAnchor.x*KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.x);
        const f32 top    = KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.y) - context->subCamera.orthographic.originAnchor.y*KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.y);
        const f32 far    = -context->subCamera.orthographic.clipDepth;
        const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        return korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = _korl_gfx_context.surfaceSize.y == 0 
            ? 1.f 
            :  KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.x) 
             / KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.y);
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
    _Korl_Gfx_Context*const context = &_korl_gfx_context;
    korl_memory_zero(context, sizeof(*context));
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL
                                                           ,korl_math_megabytes(8), L"korl-gfx"
                                                           ,KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE
                                                           ,NULL/*let platform choose address*/);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaFontCaches, 16);
    context->stringPool = korl_stringPool_create(context->allocatorHandle);
}
korl_internal void korl_gfx_updateSurfaceSize(Korl_Math_V2u32 size)
{
    _korl_gfx_context.surfaceSize = size;
}
korl_internal void korl_gfx_flushGlyphPages(void)
{
    _Korl_Gfx_Context*const context = &_korl_gfx_context;
    for(Korl_MemoryPool_Size fc = 0; fc < arrlenu(context->stbDaFontCaches); fc++)
    {
        _Korl_Gfx_FontCache*const     fontCache     = context->stbDaFontCaches[fc];
        _Korl_Gfx_FontGlyphPage*const fontGlyphPage = fontCache->glyphPage;
        if(!fontCache->glyphPage->textureOutOfDate)
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
                tempImageBuffer[y*fontGlyphPage->dataSquareSize + x] = (Korl_Vulkan_Color4u8){.r = 0xFF, .g = 0xFF, .b = 0xFF, .a = fontGlyphPage->data[y*fontGlyphPage->dataSquareSize + x]};
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
    const u$ bytesRequired = sizeof(Korl_Gfx_Text) + utf16AssetNameFont.size*sizeof(*utf16AssetNameFont.data);
    Korl_Gfx_Text*const result    = korl_allocate(allocator, bytesRequired);
    u16*const resultAssetNameFont = KORL_C_CAST(u16*, result + 1);
    result->allocator                = allocator;
    result->textPixelHeight          = textPixelHeight;
    result->utf16AssetNameFont       = (acu16){.data = resultAssetNameFont, .size = utf16AssetNameFont.size};
    result->modelRotate              = KORL_MATH_QUATERNION_IDENTITY;
    result->modelScale               = KORL_MATH_V3F32_ONE;
    result->resourceHandleBufferText = korl_resource_createVertexBuffer(&createInfoBufferText);
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
korl_internal void korl_gfx_text_fifoAdd(Korl_Gfx_Text* context, acu16 utf16Text, Korl_Memory_AllocatorHandle stackAllocator, fnSig_korl_gfx_text_codepointTest* codepointTest, void* codepointTestUserData)
{
    /* get the font asset matching the provided asset name */
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(context->utf16AssetNameFont, context->textPixelHeight, 0.f/*textPixelOutline*/);
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
    for(u$ c = 0; c < utf16Text.size; c++)
    {
        if(codepointTest && !codepointTest(codepointTestUserData, utf16Text.data + c, &currentLineColor))
            continue;
        const _Korl_Gfx_FontBakedGlyph*const bakedGlyph = _korl_gfx_fontCache_getGlyph(fontCache, utf16Text.data[c]);
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
        textBaselineCursor.x += bakedGlyph->advanceX;
        if(utf16Text.data[c] == L'\n')
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
                KORL_MATH_ASSIGN_CLAMP_MAX(context->_modelAabb.max.x, currentLine->modelAabb.max.x);
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
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(context->utf16AssetNameFont, context->textPixelHeight, 0.f/*textPixelOutline*/);
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
        KORL_MATH_ASSIGN_CLAMP_MAX(context->_modelAabb.max.x, line->modelAabb.max.x);
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
    _Korl_Gfx_FontCache*const fontCache = _korl_gfx_matchFontCache(context->utf16AssetNameFont, context->textPixelHeight, 0.f/*textPixelOutline*/);
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
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Samplers, samplers);
    samplers.resourceHandleTexture = fontCache->glyphPage->resourceHandleTexture;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_StorageBuffers, storageBuffers);
    storageBuffers.resourceHandleVertex = fontCache->glyphPage->resourceHandleSsboGlyphMeshVertices;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Features, features);
    features.enableBlend = true;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Blend, blend);
    blend.opColor           = KORL_BLEND_OP_ADD;
    blend.factorColorSource = KORL_BLEND_FACTOR_SRC_ALPHA;
    blend.factorColorTarget = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.opAlpha           = KORL_BLEND_OP_ADD;
    blend.factorAlphaSource = KORL_BLEND_FACTOR_ONE;
    blend.factorAlphaTarget = KORL_BLEND_FACTOR_ZERO;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawState);
    drawState.features       = &features;
    drawState.blend          = &blend;
    drawState.samplers       = &samplers;
    drawState.storageBuffers = &storageBuffers;
    korl_vulkan_setDrawState(&drawState);
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Model, model);
    model.scale       = context->modelScale;
    model.rotation    = context->modelRotate;
    model.translation = context->modelTranslate;
    model.color       = KORL_MATH_V4F32_ONE;
    model.translation.y -= fontCache->fontAscent;// start the text such that the translation XY position defines the location _directly_ above _all_ the text
    u$ currentVisibleGlyphOffset = 0;// used to determine the byte (transform required) offset into the Text object's text buffer resource
    for(const _Korl_Gfx_Text_Line* line = context->stbDaLines; line < context->stbDaLines + arrlen(context->stbDaLines); line++)
    {
        if(model.translation.y < visibleRegion.min.y - fontCache->fontAscent)
            break;
        if(model.translation.y <= visibleRegion.max.y + korl_math_abs(fontCache->fontDescent))
        {
            model.color = line->color;
            KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawStateLine);
            drawStateLine.model = &model;
            korl_vulkan_setDrawState(&drawStateLine);
            vertexData.instanceCount                        = line->visibleCharacters;
            vertexData.resourceHandleVertexBuffer           = context->resourceHandleBufferText;
            vertexData.resourceHandleVertexBufferByteOffset = currentVisibleGlyphOffset*sizeof(_Korl_Gfx_FontGlyphInstance);
            korl_vulkan_draw(&vertexData);
        }
        model.translation.y       -= lineDeltaY;
        currentVisibleGlyphOffset += line->visibleCharacters;
    }
}
korl_internal KORL_PLATFORM_GFX_CREATE_CAMERA_FOV(korl_gfx_createCameraFov)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.position                                = position;
    result.target                                  = target;
    result.worldUpNormal                           = KORL_MATH_V3F32_Z;
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
    result.worldUpNormal                       = KORL_MATH_V3F32_Y;
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
    korl_time_probeStart(useCamera);
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Scissor, scissor);
    switch(camera._scissorType)
    {
    case KORL_GFX_CAMERA_SCISSOR_TYPE_RATIO:{
        korl_assert(camera._viewportScissorPosition.x >= 0 && camera._viewportScissorPosition.x <= 1);
        korl_assert(camera._viewportScissorPosition.y >= 0 && camera._viewportScissorPosition.y <= 1);
        korl_assert(camera._viewportScissorSize.x >= 0 && camera._viewportScissorSize.x <= 1);
        korl_assert(camera._viewportScissorSize.y >= 0 && camera._viewportScissorSize.y <= 1);
        scissor.x      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.x * _korl_gfx_context.surfaceSize.x);
        scissor.y      = korl_math_round_f32_to_u32(camera._viewportScissorPosition.y * _korl_gfx_context.surfaceSize.y);
        scissor.width  = korl_math_round_f32_to_u32(camera._viewportScissorSize.x * _korl_gfx_context.surfaceSize.x);
        scissor.height = korl_math_round_f32_to_u32(camera._viewportScissorSize.y * _korl_gfx_context.surfaceSize.y);
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
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_View, view);
    view.positionEye    = camera.position;
    view.positionTarget = camera.target;
    view.worldUpNormal  = camera.worldUpNormal;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Projection, projection);
    switch(camera.type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        projection.type                             = KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_FOV;
        projection.subType.fov.horizontalFovDegrees = camera.subCamera.perspective.fovHorizonDegrees;
        projection.subType.fov.clipNear             = camera.subCamera.perspective.clipNear;
        projection.subType.fov.clipFar              = camera.subCamera.perspective.clipFar;
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        projection.type                              = KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_ORTHOGRAPHIC;
        projection.subType.orthographic.depth        = camera.subCamera.orthographic.clipDepth;
        projection.subType.orthographic.originRatioX = camera.subCamera.orthographic.originAnchor.x;
        projection.subType.orthographic.originRatioY = camera.subCamera.orthographic.originAnchor.y;
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        projection.type                              = KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT;
        projection.subType.orthographic.depth        = camera.subCamera.orthographic.clipDepth;
        projection.subType.orthographic.originRatioX = camera.subCamera.orthographic.originAnchor.x;
        projection.subType.orthographic.originRatioY = camera.subCamera.orthographic.originAnchor.y;
        projection.subType.orthographic.fixedHeight  = camera.subCamera.orthographic.fixedHeight;
        break;}
    }
    KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawState);
    drawState.scissor    = &scissor;
    drawState.view       = &view;
    drawState.projection = &projection;
    korl_vulkan_setDrawState(&drawState);
    _korl_gfx_context.currentCameraState = camera;
    korl_time_probeStop(useCamera);
}
korl_internal KORL_PLATFORM_GFX_CAMERA_GET_CURRENT(korl_gfx_camera_getCurrent)
{
    return _korl_gfx_context.currentCameraState;
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
korl_internal KORL_PLATFORM_GFX_CAMERA_ORTHO_GET_SIZE(korl_gfx_cameraOrthoGetSize)
{
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        return (Korl_Math_V2f32){KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.x)
                                ,KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.y)};}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = _korl_gfx_context.surfaceSize.y == 0 
            ? 1.f 
            :   KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.x) 
              / KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.y);
        return (Korl_Math_V2f32){context->subCamera.orthographic.fixedHeight*viewportWidthOverHeight// w / fixedHeight == windowAspectRatio
                                ,context->subCamera.orthographic.fixedHeight};}
    default:{
        korl_log(ERROR, "invalid camera type: %i", context->type);
        return (Korl_Math_V2f32){korl_math_nanf32(), korl_math_nanf32()};}
    }
}
korl_internal KORL_PLATFORM_GFX_CAMERA_WINDOW_TO_WORLD(korl_gfx_camera_windowToWorld)
{
    Korl_Gfx_ResultRay3d result = {.position ={korl_math_nanf32(),korl_math_nanf32(),korl_math_nanf32()}
                                  ,.direction={korl_math_nanf32(),korl_math_nanf32(),korl_math_nanf32()}};
    //@TODO: I expect this to be SLOW; we should instead be caching the camera's VP matrices and only update them when they are "dirty"; I know for a fact that SFML does this in its sf::camera class
    const Korl_Math_M4f32 view       = _korl_gfx_camera_view(context);
    const Korl_Math_M4f32 projection = _korl_gfx_camera_projection(context);
    const Korl_Math_M4f32 viewInverse       = korl_math_m4f32_invert(&view);
    const Korl_Math_M4f32 projectionInverse = korl_math_m4f32_invert(&projection);
    if(korl_math_isNanf32(viewInverse.r0c0))
    {
        korl_log(WARNING, "failed to invert view matrix");
        return result;
    }
    if(korl_math_isNanf32(projectionInverse.r0c0))
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
    /* @TODO; ASSUMPTION: viewport is the size of the entire window; if we ever want to 
                   handle separate viewport clip regions per-camera, we will 
                   have to modify this */
    const Korl_Math_V2f32 eyeRayNds = 
        {  2*v2f32WindowPos.x / _korl_gfx_context.surfaceSize.x - 1
        , -2*v2f32WindowPos.y / _korl_gfx_context.surfaceSize.y + 1 };
    /* normalized-device-space => homogeneous-clip-space */
    /* arbitrarily set the eye ray direction vector as far to the "back" of the 
        homogeneous clip space box; we set the Z coordinate to 0, since Vulkan 
        (with no extensions, which I don't want to use) requires a normalized 
        depth buffer */
    /// @TODO: ASSUMPTION: right-handed HC-space coordinate system that spans [0,1]; need to actually test to see if this works
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
        /// @TODO: ASSUMPTION: right-handed HC-space coordinate system that spans [0,1]; need to actually test to see if this works
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
korl_internal KORL_PLATFORM_GFX_CAMERA_WORLD_TO_WINDOW(korl_gfx_camera_worldToWindow)
{
    //@TODO: I expect this to be SLOW; we should instead be caching the camera's VP matrices and only update them when they are "dirty"; I know for a fact that SFML does this in its sf::camera class
    const Korl_Math_M4f32 view       = _korl_gfx_camera_view(context);
    const Korl_Math_M4f32 projection = _korl_gfx_camera_projection(context);
    /* @TODO; ASSUMPTION: viewport is the size of the entire window; if we ever want to 
                   handle separate viewport clip regions per-camera, we will 
                   have to modify this */
    const Korl_Math_Aabb2f32 viewport     = {.min={0,0}
                                            ,.max={KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.x)
                                                  ,KORL_C_CAST(f32, _korl_gfx_context.surfaceSize.y)}};
    const Korl_Math_V2f32    viewportSize = korl_math_aabb2f32_size(viewport);
    /* transform from world=>camera=>clip space */
    const Korl_Math_V4f32 worldPoint       = {worldPosition.x, worldPosition.y, worldPosition.z, 1};
    const Korl_Math_V4f32 cameraSpacePoint = korl_math_m4f32_multiplyV4f32(&view, &worldPoint);
    const Korl_Math_V4f32 clipSpacePoint   = korl_math_m4f32_multiplyV4f32(&projection, &cameraSpacePoint);
    if(korl_math_isNearlyZero(clipSpacePoint.w))
        return (Korl_Math_V2f32){korl_math_nanf32(), korl_math_nanf32()};
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
korl_internal KORL_PLATFORM_GFX_BATCH(korl_gfx_batch)
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
    model.scale       = batch->_scale;
    model.rotation    = batch->_rotation;
    model.translation = batch->_position;
    model.color       = KORL_MATH_V4F32_ONE;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Samplers, samplers);
    samplers.resourceHandleTexture = batch->_texture;
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
        samplers.resourceHandleTexture = batch->_fontTextureHandle;
        /* use the glyph mesh vertices buffer as the shader storage buffer 
            object binding */
        storageBuffers.resourceHandleVertex = batch->_glyphMeshBufferVertices;
        /* we need to somehow position the text mesh in a way that satisfies the 
            text position anchor */
        // align position with the bottom-left corner of the batch AABB
        korl_math_v2f32_assignSubtract(&model.translation.xy, batch->_textAabb.min);
        // offset position by the position anchor ratio, using the AABB size
        korl_math_v2f32_assignSubtract(&model.translation.xy, korl_math_v2f32_multiply(korl_math_aabb2f32_size(batch->_textAabb)
                                                                                      ,batch->_textPositionAnchor));
        /* disable vertex colors for now, since this is super expensive for text anyway! */
        vertexData.colors       = NULL;
        vertexData.colorsStride = 0;
    }
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Features, features);
    features.enableBlend     = !(flags & KORL_GFX_BATCH_FLAG_DISABLE_BLENDING);
    features.enableDepthTest = !(flags & KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    KORL_ZERO_STACK(Korl_Vulkan_DrawState_Blend, blend);
    blend.opColor           = batch->opColor;
    blend.factorColorSource = batch->factorColorSource;
    blend.factorColorTarget = batch->factorColorTarget;
    blend.opAlpha           = batch->opAlpha;
    blend.factorAlphaSource = batch->factorAlphaSource;
    blend.factorAlphaTarget = batch->factorAlphaTarget;
    KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawState);
    drawState.features       = &features;
    drawState.blend          = &blend;
    drawState.model          = &model;
    drawState.samplers       = &samplers;
    drawState.storageBuffers = &storageBuffers;
    korl_vulkan_setDrawState(&drawState);
    korl_vulkan_draw(&vertexData);
    done:
    korl_time_probeStop(gfx_batch);
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_TEXTURED(korl_gfx_createBatchRectangleTextured)
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
    result->_vertexIndexCount         = 6;
    result->_vertexCount              = 4;
    result->opColor                   = KORL_BLEND_OP_ADD;
    result->factorColorSource         = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget         = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha                   = KORL_BLEND_OP_ADD;
    result->factorAlphaSource         = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget         = KORL_BLEND_FACTOR_ZERO;
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
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_COLORED(korl_gfx_createBatchRectangleColored)
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
    result->_vertexIndexCount         = 6;
    result->_vertexCount              = 4;
    result->opColor                   = KORL_BLEND_OP_ADD;
    result->factorColorSource         = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget         = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha                   = KORL_BLEND_OP_ADD;
    result->factorAlphaSource         = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget         = KORL_BLEND_FACTOR_ZERO;
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
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_CIRCLE(korl_gfx_createBatchCircle)
{
    return korl_gfx_createBatchCircleSector(allocatorHandle, radius, pointCount, color, 2*KORL_PI32);
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_CIRCLE_SECTOR(korl_gfx_createBatchCircleSector)
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
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;//KORL-PERFORMANCE-000-000-018: GFX; (MINOR) use triangle fan primitive for less vertex indices
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount         = korl_checkCast_u$_to_u32(indices);
    result->_vertexCount              = korl_checkCast_u$_to_u32(vertices);
    result->opColor                   = KORL_BLEND_OP_ADD;
    result->factorColorSource         = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget         = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha                   = KORL_BLEND_OP_ADD;
    result->factorAlphaSource         = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget         = KORL_BLEND_FACTOR_ZERO;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexIndices            = KORL_C_CAST(Korl_Vulkan_VertexIndex*, result + 1);
    result->_vertexPositions          = KORL_C_CAST(f32*                    , KORL_C_CAST(u8*, result->_vertexIndices   ) + indices*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexColors             = KORL_C_CAST(Korl_Vulkan_Color4u8*   , KORL_C_CAST(u8*, result->_vertexPositions ) + vertices*result->_vertexPositionDimensions*sizeof(f32));
    /* initialize the batch's dynamic data */
    // vertex[0] is always the center point of the circle //
    KORL_C_CAST(Korl_Math_V3f32*, result->_vertexPositions)[0] = KORL_MATH_V3F32_ZERO;
    const f32 deltaRadians = 2*KORL_PI32 / pointCount;
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
        if(p == pointCount - 1 && sectorRadians < 2*KORL_PI32)
            /* if the circle sector radians does not fill the whole circle, 
                don't connect the last triangle with the first vertex */
            result->_vertexIndices[3*p + 2] = korl_vulkan_safeCast_u$_to_vertexIndex(p + 1);
    }
    /**/
    korl_time_probeStop(create_batch_circle);
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_TRIANGLES(korl_gfx_createBatchTriangles)
{
    korl_time_probeStart(create_batch_tris);
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 3*triangleCount * _KORL_GFX_POSITION_DIMENSIONS*sizeof(f32)
        + 3*triangleCount * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexCount              = 3*triangleCount;
    result->opColor                   = KORL_BLEND_OP_ADD;
    result->factorColorSource         = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget         = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha                   = KORL_BLEND_OP_ADD;
    result->factorAlphaSource         = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget         = KORL_BLEND_FACTOR_ZERO;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexPositions          = KORL_C_CAST(f32*                 , result + 1);
    result->_vertexColors             = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, result->_vertexPositions) + 3*triangleCount*result->_vertexPositionDimensions*sizeof(f32));
    /* return the batch */
    korl_time_probeStop(create_batch_tris);
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_LINES(korl_gfx_createBatchLines)
{
    korl_time_probeStart(create_batch_lines);
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 2*lineCount * _KORL_GFX_POSITION_DIMENSIONS*sizeof(f32)
        + 2*lineCount * sizeof(Korl_Vulkan_Color4u8);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_allocate(allocatorHandle, totalBytes));
    /* initialize the batch struct */
    result->allocatorHandle           = allocatorHandle;
    result->primitiveType             = KORL_VULKAN_PRIMITIVETYPE_LINES;
    result->_scale                    = (Korl_Math_V3f32){1.f, 1.f, 1.f};
    result->_rotation                 = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexCount              = 2*lineCount;
    result->opColor                   = KORL_BLEND_OP_ADD;
    result->factorColorSource         = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget         = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha                   = KORL_BLEND_OP_ADD;
    result->factorAlphaSource         = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget         = KORL_BLEND_FACTOR_ZERO;
    result->_vertexPositionDimensions = _KORL_GFX_POSITION_DIMENSIONS;
    result->_vertexPositions          = KORL_C_CAST(f32*                 , result + 1);
    result->_vertexColors             = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, result->_vertexPositions) + 2*lineCount*result->_vertexPositionDimensions*sizeof(f32));
    /* return the batch */
    korl_time_probeStop(create_batch_lines);
    return result;
}
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_TEXT(korl_gfx_createBatchText)
{
    korl_time_probeStart(create_batch_text);
    korl_assert(text);
    korl_assert(textPixelHeight  >= 1.f);
    korl_assert(outlinePixelSize >= 0.f);
    if(!assetNameFont)
        assetNameFont = L"test-assets/source-sans/SourceSans3-Semibold.otf";//KORL-ISSUE-000-000-086: gfx: default font path doesn't work, since this subdirectly is unlikely in the game project
    /* calculate required amount of memory for the batch */
    korl_time_probeStart(measure_font);
    const u$ assetNameFontBufferSize = korl_memory_stringSize(assetNameFont) + 1;
    korl_time_probeStop(measure_font);
    const u$ assetNameFontBytes = assetNameFontBufferSize * sizeof(*assetNameFont);
    /** This value directly affects how much memory will be allocated for vertex 
     * index/attribute data.  Note that it is strictly an over-estimate, because 
     * we cannot possibly know ahead of time whether or not any particular glyph 
     * contains visible components until the font asset is loaded, and we are 
     * doing a sort of lazy-loading system here where the user is allowed to 
     * create & manipulate text batches even in the absence of a font asset. */
    korl_time_probeStart(measure_text);
    const u$ textSize = korl_memory_stringSize(text);
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
    result->_textPixelHeight            = textPixelHeight;
    result->_textPixelOutline           = outlinePixelSize;
    result->_textColor                  = color;
    result->_textColorOutline           = colorOutline;
    result->_textPositionAnchor         = (Korl_Math_V2f32){0,1};
    result->_assetNameFont              = KORL_C_CAST(wchar_t*, result + 1);
    result->_text                       = KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, result->_assetNameFont) + assetNameFontBytes);
    result->opColor                     = KORL_BLEND_OP_ADD;
    result->factorColorSource           = KORL_BLEND_FACTOR_SRC_ALPHA;
    result->factorColorTarget           = KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    result->opAlpha                     = KORL_BLEND_OP_ADD;
    result->factorAlphaSource           = KORL_BLEND_FACTOR_ONE;
    result->factorAlphaTarget           = KORL_BLEND_FACTOR_ZERO;
    result->_instancePositionDimensions = 2;
    result->_instancePositions          = KORL_C_CAST(f32*, KORL_C_CAST(u8*, result->_text             ) + textBytes);
    result->_instanceU32s               = KORL_C_CAST(u32*, KORL_C_CAST(u8*, result->_instancePositions) + maxVisibleGlyphCount*result->_instancePositionDimensions*sizeof(*result->_instancePositions));
    /* initialize the batch's dynamic data */
    korl_time_probeStart(copy_font);
    if(    korl_memory_stringCopy(assetNameFont, result->_assetNameFont, assetNameFontBufferSize) 
        != korl_checkCast_u$_to_i$(assetNameFontBufferSize))
        korl_log(ERROR, "failed to copy asset name \"%ls\" to batch", assetNameFont);
    korl_time_probeStop(copy_font);
    korl_time_probeStart(copy_text);
    if(    korl_memory_stringCopy(text, result->_text, textBufferSize) 
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
    korl_assert(positionDimensions == 2 || positionDimensions == 3);
    for(u8 d = 0; d < positionDimensions; d++)
        context->_position.elements[d] = position[d];
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
                           + newVertexCount * _KORL_GFX_POSITION_DIMENSIONS*sizeof(f32)
                           + newVertexCount * sizeof(Korl_Vulkan_Color4u8);
    (*pContext) = KORL_C_CAST(Korl_Gfx_Batch*, 
        korl_reallocate((*pContext)->allocatorHandle, *pContext, newTotalBytes));
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
korl_internal KORL_PLATFORM_GFX_BATCH_SET_LINE(korl_gfx_batchSetLine)
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
korl_internal KORL_PLATFORM_GFX_BATCH_TEXT_GET_AABB(korl_gfx_batchTextGetAabb)
{
    korl_assert(batchContext->_text && batchContext->_assetNameFont);
    _korl_gfx_textGenerateMesh(batchContext, KORL_ASSETCACHE_GET_FLAG_LAZY);
    if(!batchContext->_textVisibleCharacterCount)
        return (Korl_Math_Aabb2f32){{0, 0}, {0, 0}};
    return batchContext->_textAabb;
}
korl_internal KORL_PLATFORM_GFX_BATCH_TEXT_SET_POSITION_ANCHOR(korl_gfx_batchTextSetPositionAnchor)
{
    korl_assert(batchContext->_text && batchContext->_assetNameFont);
    batchContext->_textPositionAnchor = textPositionAnchor;
}
korl_internal KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_SIZE(korl_gfx_batchRectangleSetSize)
{
    korl_assert(context->_vertexCount == 4 && context->_vertexIndexCount == 6);
    const Korl_Math_V2f32 originalSize = korl_math_v2f32_subtract(KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[2].xy, KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[0].xy);
    for(u$ c = 0; c < context->_vertexCount; c++)
        KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[c].xy = korl_math_v2f32_multiply(size, korl_math_v2f32_divide(KORL_C_CAST(Korl_Math_V3f32*, context->_vertexPositions)[c].xy, originalSize));
}
korl_internal KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_COLOR(korl_gfx_batchRectangleSetColor)
{
    korl_assert(context->_vertexCount == 4 && context->_vertexIndexCount == 6);
    korl_assert(context->_vertexColors);
    for(u$ c = 0; c < context->_vertexCount; c++)
        context->_vertexColors[c] = color;
}
korl_internal KORL_PLATFORM_GFX_BATCH_CIRCLE_SET_COLOR(korl_gfx_batchCircleSetColor)
{
    /// for refactoring this code module in the future; we should probably assert this is a circle batch?
    korl_assert(context->_vertexColors);
    for(u$ c = 0; c < context->_vertexCount; c++)
        context->_vertexColors[c] = color;
}
korl_internal void korl_gfx_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer)
{
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_gfx_context, sizeof(_korl_gfx_context));
}
korl_internal bool korl_gfx_saveStateRead(HANDLE hFile)
{
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    if(!ReadFile(hFile, &_korl_gfx_context, sizeof(_korl_gfx_context), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    return true;
}
#undef _LOCAL_STRING_POOL_POINTER
