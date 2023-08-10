/** @TODO: delete; these are just transient notes on design considerations for FONT resources; except, maybe keep the notes about the final chosen implementation's limitations
 * To cater to the scenario where FONT resources retain all baked glyph data between memoryStates:
 * Even if korl-resource-font is backed by ASSET_CACHE, we _still_ need to 
 * manage _runtime_ memory because we need to remember the Korl_Resource_Handles 
 * of our glyph mesh vertices & glyph page textures.  
 * CONSEQUENCES: 
 *  - we store a ton of unnecessary data in memoryState
 *    - needlessly more work during _all_ frames of program execution :(
 *  - we wont be able to hot-reload font assets, since we would have no way of updating BUFFER assets which utilize the output of a FONT asset
 * 
 * To cater to the scenario where all baked FONT data is _transient_: 
 * We need to establish the idea of parent-backed resources.  In this scenario, 
 * we create a TEXT2D resource, and the FONT resource is completely wiped, since 
 * all glyph data can be re-baked using assetCache-backed data.  When our 
 * transient data is wiped, we need to ensure that all parent-backed resources 
 * (in this case, all our TEXT2D child resources) are unloaded during 
 * `korl_resource_memoryStateRead` & `korl_resource_onAssetHotReload`!  
 * CONSEQUENCES: 
 *  - the user who just wants to draw text is now burdoned with an API abstraction, so we need to ensure this is as flexible as possible
 *  - drawing transient (one-frame) text becomes a burdon, as we can't just call {`resource_create`; `draw`; `resource_destroy`} right now, 
 *    since the text resource would be destroyed before the renderer actually gets a chance to use it; 
 *    we would need to refactor korl-resource to defer total destruction of a resource until end-of-frame
 *    COMPLEXITY++ UUUUUUUUUGH
 * 
 * Here's a question: do we even actually _want_/_need_ to support FONT resource hot-reloading?!... HMMMMMMMmmmmm 🤔
 * We probably do want all these features.  I want to actually try first before giving up on this.
 * 
 * What if FONT resource data _was_ _all_ transient?  What would happen if it was 
 * actually just wiped when we load a memory state?  I dunno, this just feels bad; 
 * things just wouldn't load correctly between save states and, at best, look glitchy, while at worst, the program would just straight up crash...
 * 
 * It feels like, at the very least, we should adopt a strategy where _some_ of 
 * the font data is transient (GlyphMeshVertices, GlyphPages), and a _minimal_ amount of data 
 * necessary to perfectly re-create the FONT resource is runtime (BakedGlyphs).  
 * If we just had the assetCache data + BakedGlyph data (and _maybe_ GlyphPage packing data; it should be possible to re-create this as well by performing a sort on the BakedGlyph data), 
 * we could fully re-create _all_ multimedia assets (GlyphMeshVertices & GlyphPageTextures), 
 * which _should_ be the _vast_ majority of the data for a FONT asset in the first place!  
 * If we were able to achieve this, we should _not_ have to do silly nonsense like making 
 * special TEXT2D child resources or something like that, since all BUFFER data for drawing text
 * (glyph instance IDs & positions) _will_ remain valid!
 * 
 * After thinking about this for a bit, the above paragraph is actually _not_ true; 
 * imagine scenarios where the font asset is hot-reloaded, and the new font changes a glyph's geometry in some way, 
 * such as making a glyph larger, or making an "empty" glyph have non-zero geometry; 
 * in such cases, we can't possibly place the BakedGlyph in a GlyphPage in such a way that all GFX_BUFFER objects containing 
 * glyph instance data will remain valid anymore; this is because all glyph instance positions are based on the effects of 
 * all previous glyph positions in any given text "line" draw call; so, if any of the metrics of _any_ previous glyphs in a 
 * line has changed, that glyph instance position2d _must_ be adjusted to accommodate this change.
 * 
 * Is it _okay_ to accept the above paragraph as a limitation of korl-resource-font?  
 * All this would mean is that, although hot-reloading of FONT resources would work, 
 * and text buffers generated from said FONT resources would still draw the correct glyphs, 
 * we wouldn't be able to:
 * - draw glyphs which changed "empty" state to "not-empty"
 * - _not_ draw glyphs which changed from "not-empty" => "empty"
 * - draw glyphs in the correct positions for BUFFERs generated pre-font-update
 * These limitations are not terrible, and I don't see them as being a huge issue in _most_ scenarios.  
 * At the very least, we would be able to hot-reload FONT assets & see glyph changes immediately for testing purposes.
 */
#include "korl-resource-font.h"
#include "korl-interface-platform.h"
#include "korl-stb-truetype.h"
#include "utility/korl-utility-resource.h"
#include "utility/korl-utility-math.h"
#include "utility/korl-utility-stb-ds.h"
#define _KORL_RESOURCE_FONT_SUPPORTED_PIXEL_HEIGHTS 11
korl_global_const f32 _KORL_RESOURCE_FONT_PIXEL_HEIGHTS[_KORL_RESOURCE_FONT_SUPPORTED_PIXEL_HEIGHTS] = {10, 14, 16, 18, 24, 30, 36, 48, 60, 72, 84};// just an arbitrary list of common text pixel heights
typedef struct _Korl_Resource_Font_BakedGlyph_BoundingBox
{
    /** \c x : For non-outline glyphs, this value will always be == to \c -bearingLeft 
     * of the baked character
     *  \c y : How much we need to offset the glyph mesh to center it on the baseline 
     * cursor origin. */
    Korl_Math_V2f32 offset;
    u16             x0, y0, x1, y1;// coordinates of bbox in bitmap
    u8              glyphPageIndex;
} _Korl_Resource_Font_BakedGlyph_BoundingBox;
typedef struct _Korl_Resource_Font_BakedGlyph
{
    _Korl_Resource_Font_BakedGlyph_BoundingBox boxes[_KORL_RESOURCE_FONT_SUPPORTED_PIXEL_HEIGHTS];
    int                                        stbtt_glyphIndex;// obtained form stbtt_FindGlyphIndex(fontInfo, codepoint)
    u32                                        bakeOrder;
    bool                                       isEmpty;// true if stbtt_IsGlyphEmpty for this glyph returned true
} _Korl_Resource_Font_BakedGlyph;
typedef struct _Korl_Resource_Font_BakedGlyphMap
{
    u32                            key;// unicode codepoint
    _Korl_Resource_Font_BakedGlyph value;
} _Korl_Resource_Font_BakedGlyphMap;
typedef struct _Korl_Resource_Font_GlyphVertex
{
    Korl_Math_V4f32 position2d_uv;// xy => position2d; zw => uv
} _Korl_Resource_Font_GlyphVertex;
typedef struct _Korl_Resource_Font_GlyphPage
{
    Korl_Resource_Handle resourceHandleTexture;
    // @TODO: glyph packing data
} _Korl_Resource_Font_GlyphPage;
typedef struct _Korl_Resource_Font
{
    stbtt_fontinfo                     stbtt_info;
    Korl_Resource_Handle               resourceHandleSsboGlyphMeshVertices;// array of _Korl_Resource_Font_GlyphVertex; stores vertex data for all baked glyphs across _all_ glyph pages
    _Korl_Resource_Font_BakedGlyphMap* stbHmBakedGlyphs;
    _Korl_Resource_Font_GlyphPage*     stbDaGlyphPages;
} _Korl_Resource_Font;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_font_descriptorStructCreate)
{
    _Korl_Resource_Font*const font = korl_allocate(allocator, sizeof(_Korl_Resource_Font));
    mchmdefault(KORL_STB_DS_MC_CAST(allocator), font->stbHmBakedGlyphs, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource_Font_BakedGlyph));
    mcarrsetcap(KORL_STB_DS_MC_CAST(allocator), font->stbDaGlyphPages, 8);
    return font;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_font_descriptorStructDestroy)
{
    _Korl_Resource_Font*const font = resourceDescriptorStruct;
    mchmfree(KORL_STB_DS_MC_CAST(allocator), font->stbHmBakedGlyphs);
    mcarrfree(KORL_STB_DS_MC_CAST(allocator), font->stbDaGlyphPages);
    korl_free(allocator, font);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(_korl_resource_font_clearTransientData)
{
    _Korl_Resource_Font*const font = resourceDescriptorStruct;
    korl_resource_destroy(font->resourceHandleSsboGlyphMeshVertices);
    font->resourceHandleSsboGlyphMeshVertices = 0;
    {
        const _Korl_Resource_Font_GlyphPage*const glyphPageEnd = font->stbDaGlyphPages + arrlen(font->stbDaGlyphPages);
        for(_Korl_Resource_Font_GlyphPage* glyphPage = font->stbDaGlyphPages; glyphPage < glyphPageEnd; glyphPage++)
        {
            korl_resource_destroy(glyphPage->resourceHandleTexture);
            glyphPage->resourceHandleTexture = 0;
        }
    }
    korl_memory_zero(&font->stbtt_info, sizeof(font->stbtt_info));
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_font_unload)
{
    _korl_resource_font_clearTransientData(resourceDescriptorStruct);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_font_transcode)
{
    _Korl_Resource_Font*const font = resourceDescriptorStruct;
    /* sanity-check that the font is currently in an "unloaded" state (no transient data) */
    korl_assert(!font->stbtt_info.data);
    korl_assert(!font->resourceHandleSsboGlyphMeshVertices);
    /* load transient data */
    //KORL-ISSUE-000-000-130: gfx/font: (MAJOR) `font->stbtt_info` sets up pointers to data within `data`; if that data ever moves or gets wiped (defragment, memoryState-load, etc.), then `font->stbtt_info` will contain dangling pointers!
    korl_assert(stbtt_InitFont(&font->stbtt_info, data, 0/*font offset*/));
    KORL_ZERO_STACK(Korl_Resource_GfxBuffer_CreateInfo, createInfo);
    createInfo.usageFlags = KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_STORAGE;
    createInfo.bytes      = 1/*placeholder non-zero size, since we don't know how many glyphs we are going to cache*/;
    font->resourceHandleSsboGlyphMeshVertices = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_GFX_BUFFER), &createInfo);
    /* if this font resource has BakedGlyphs, we need to re-bake them all */
    ///@TODO
}
korl_internal void korl_resource_font_register(void)
{
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName                = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_FONT);
    descriptorManifest.callbacks.descriptorStructCreate  = korl_functionDynamo_register(_korl_resource_font_descriptorStructCreate);
    descriptorManifest.callbacks.descriptorStructDestroy = korl_functionDynamo_register(_korl_resource_font_descriptorStructDestroy);
    descriptorManifest.callbacks.unload                  = korl_functionDynamo_register(_korl_resource_font_unload);
    descriptorManifest.callbacks.transcode               = korl_functionDynamo_register(_korl_resource_font_transcode);
    korl_resource_descriptor_add(&descriptorManifest);
}
korl_internal Korl_Resource_Font_Metrics _korl_resource_font_getMetrics(_Korl_Resource_Font*const font, f32 textPixelHeight)
{
    KORL_ZERO_STACK(Korl_Resource_Font_Metrics, result);
    f32 fontScale = stbtt_ScaleForPixelHeight(&font->stbtt_info, textPixelHeight);
    int ascent;
    int descent;
    int lineGap;
    stbtt_GetFontVMetrics(&font->stbtt_info, &ascent, &descent, &lineGap);
    result.ascent  = fontScale * ascent;
    result.decent  = fontScale * descent;
    result.lineGap = fontScale * lineGap;
    return result;
}
korl_internal KORL_FUNCTION_korl_resource_font_getMetrics(korl_resource_font_getMetrics)
{
    KORL_ZERO_STACK(Korl_Resource_Font_Metrics, result);
    if(!handleResourceFont)
        return result;
    //@TODO: validate the underlying korl-resource-item descriptor type
    _Korl_Resource_Font*const font = korl_resource_getDescriptorStruct(handleResourceFont);
    if(!font->stbtt_info.data)
        return result;
    return _korl_resource_font_getMetrics(font, textPixelHeight);
}
korl_internal KORL_FUNCTION_korl_resource_font_getResources(korl_resource_font_getResources)
{
    KORL_ZERO_STACK(Korl_Resource_Font_Resources, result);
    if(!handleResourceFont)
        return result;
    //@TODO: validate the underlying korl-resource-item descriptor type
    _Korl_Resource_Font*const font = korl_resource_getDescriptorStruct(handleResourceFont);
    if(!font->stbtt_info.data)
        return result;
    result.resourceHandleSsboGlyphMeshVertices = font->resourceHandleSsboGlyphMeshVertices;
    if(arrlenu(font->stbDaGlyphPages))
        //@TODO: support multiple glyph pages; special text fragment shader with a texture array
        result.resourceHandleTexture = font->stbDaGlyphPages[0].resourceHandleTexture;
    return result;
}
korl_internal u8 _korl_resource_font_nearestSupportedPixelHeightIndex(f32 pixelHeight)
{
    f32 smallestDelta = KORL_F32_MAX;
    u8  result        = _KORL_RESOURCE_FONT_SUPPORTED_PIXEL_HEIGHTS;
    for(u8 i = 0; i < _KORL_RESOURCE_FONT_SUPPORTED_PIXEL_HEIGHTS; i++)
    {
        const f32 delta = korl_math_f32_positive(_KORL_RESOURCE_FONT_PIXEL_HEIGHTS[i] - pixelHeight);
        if(delta < smallestDelta)
        {
            smallestDelta = delta;
            result        = i;
        }
    }
    korl_assert(result < _KORL_RESOURCE_FONT_SUPPORTED_PIXEL_HEIGHTS);
    return result;
}
korl_internal const _Korl_Resource_Font_BakedGlyph* _korl_resource_font_getGlyph(_Korl_Resource_Font*const font, const u32 codePoint, const u8 nearestSupportedPixelHeightIndex)
{
    _Korl_Resource_Font_BakedGlyph* glyph = NULL;
#if 0//@TODO: recycle
    _Korl_Gfx_Context*const gfxContext      = _korl_gfx_context;
    _Korl_Gfx_FontGlyphPage*const glyphPage = _korl_gfx_fontCache_getGlyphPage(fontCache);
    /* iterate over the glyph page codepoints to see if any match codePoint */
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
#endif
    // at this point, we should have a valid glyph //
    korl_assert(glyph);
    return glyph;
}
korl_internal void _korl_resource_font_getUtfMetrics_common(_Korl_Resource_Font*const font, const f32 fontScale, const Korl_Resource_Font_Metrics fontMetrics, const u8 nearestSupportedPixelHeightIndex, const u32 codepoint, const f32 lineDeltaY, int* glyphIndexPrevious, Korl_Math_V2f32* textBaselineCursor, Korl_Math_Aabb2f32* aabb, Korl_Resource_Font_TextMetrics* textMetrics)
{
    const _Korl_Resource_Font_BakedGlyph*const bakedGlyph = _korl_resource_font_getGlyph(font, codepoint, nearestSupportedPixelHeightIndex);
    int advanceX;
    int bearingLeft;
    stbtt_GetGlyphHMetrics(&font->stbtt_info, bakedGlyph->stbtt_glyphIndex, &advanceX, &bearingLeft);
    if(!bakedGlyph->isEmpty)
    {
        /* instead of generating an AABB that is shrink-wrapped to the actual 
            rendered glyphs, let's generate the AABB with respect to the font's 
            height metrics; so we only really care about the glyph's graphical 
            AABB on the X-axis; why? because I am finding that in a lot of cases 
            the user code cares more about visual alignment of rendered text, 
            and this is a lot easier to achieve API-wise when we are calculating 
            text AABBs relatvie to font metrics */
        const _Korl_Resource_Font_BakedGlyph_BoundingBox*const bbox = bakedGlyph->boxes + nearestSupportedPixelHeightIndex;
        //@TODO: adjust this bbox by the `textPixelHeight / nearestSupportedPixelHeight` ratio?
        const f32 x0 = textBaselineCursor->x;
        const f32 y0 = textBaselineCursor->y + fontMetrics.ascent;
        const f32 x1 = x0 + bbox->offset.x + (bbox->x1 - bbox->x0);
        const f32 y1 = textBaselineCursor->y + fontMetrics.decent;
        /* at this point, we know that this is a valid visible glyph */
        aabb->min = korl_math_v2f32_min(aabb->min, (Korl_Math_V2f32){x0, y1});
        aabb->max = korl_math_v2f32_max(aabb->max, (Korl_Math_V2f32){x1, y0});
        textMetrics->visibleGlyphCount++;
    }
    if(textBaselineCursor->x > 0.f)
    {
        const int kernAdvance = stbtt_GetGlyphKernAdvance(&font->stbtt_info
                                                         ,*glyphIndexPrevious
                                                         ,bakedGlyph->stbtt_glyphIndex);
        *glyphIndexPrevious    = bakedGlyph->stbtt_glyphIndex;
        textBaselineCursor->x += fontScale * kernAdvance;
    }
    else
        *glyphIndexPrevious = -1;
    textBaselineCursor->x += fontScale * advanceX;
    if(codepoint == L'\n')
    {
        textBaselineCursor->x  = 0.f;
        textBaselineCursor->y -= lineDeltaY;
    }
}
korl_internal Korl_Resource_Font_TextMetrics _korl_resource_font_getUtfMetrics(_Korl_Resource_Font*const font, f32 textPixelHeight, const void* utfTextData, u$ utfTextSize, u8 utfEncoding)
{
    const f32                        fontScale                        = stbtt_ScaleForPixelHeight(&font->stbtt_info, textPixelHeight);
    const Korl_Resource_Font_Metrics fontMetrics                      = _korl_resource_font_getMetrics(font, textPixelHeight);
    const u8                         nearestSupportedPixelHeightIndex = _korl_resource_font_nearestSupportedPixelHeightIndex(textPixelHeight);
    const f32                        lineDeltaY                       = (fontMetrics.ascent - fontMetrics.decent) + fontMetrics.lineGap;
    Korl_Math_V2f32 textBaselineCursor = (Korl_Math_V2f32){0.f, -fontMetrics.ascent};// default the baseline cursor such that our local origin is placed at _exactly_ the upper-left corner of the text's local AABB based on font metrics, as we know a glyph will never rise _above_ the fontAscent
    int             glyphIndexPrevious = -1;
    Korl_Math_Aabb2f32 aabb = KORL_MATH_AABB2F32_EMPTY;
    KORL_ZERO_STACK(Korl_Resource_Font_TextMetrics, textMetrics);
    switch(utfEncoding)
    {
    case 8:
        //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
        for(Korl_String_CodepointIteratorUtf8 codepointIt = korl_string_codepointIteratorUtf8_initialize(utfTextData, utfTextSize)
           ;!korl_string_codepointIteratorUtf8_done(&codepointIt)
           ; korl_string_codepointIteratorUtf8_next(&codepointIt))
            _korl_resource_font_getUtfMetrics_common(font, fontScale, fontMetrics, nearestSupportedPixelHeightIndex, codepointIt._codepoint, lineDeltaY, &glyphIndexPrevious, &textBaselineCursor, &aabb, &textMetrics);
        break;
    case 16:
        //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
        for(Korl_String_CodepointIteratorUtf16 codepointIt = korl_string_codepointIteratorUtf16_initialize(utfTextData, utfTextSize)
           ;!korl_string_codepointIteratorUtf16_done(&codepointIt)
           ; korl_string_codepointIteratorUtf16_next(&codepointIt))
            _korl_resource_font_getUtfMetrics_common(font, fontScale, fontMetrics, nearestSupportedPixelHeightIndex, codepointIt._codepoint, lineDeltaY, &glyphIndexPrevious, &textBaselineCursor, &aabb, &textMetrics);
        break;
    default:
        korl_log(ERROR, "unsupported UTF encoding: %hhu", utfEncoding);
        break;
    }
    textMetrics.aabbSize = korl_math_aabb2f32_size(aabb);
    return textMetrics;
}
korl_internal void _korl_resource_font_generateUtf_common(_Korl_Resource_Font*const font, const f32 fontScale, const u8 nearestSupportedPixelHeightIndex, const u32 codepoint, const f32 lineDeltaY, int* glyphIndexPrevious, Korl_Math_V2f32* textBaselineCursor, Korl_Resource_Font_TextMetrics* textMetrics, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u32* o_glyphInstanceIndices)
{
    const _Korl_Resource_Font_BakedGlyph*const bakedGlyph = _korl_resource_font_getGlyph(font, codepoint, nearestSupportedPixelHeightIndex);
    int advanceX;
    int bearingLeft;
    stbtt_GetGlyphHMetrics(&font->stbtt_info, bakedGlyph->stbtt_glyphIndex, &advanceX, &bearingLeft);
    if(!bakedGlyph->isEmpty)
    {
        o_glyphInstancePositions[textMetrics->visibleGlyphCount] = korl_math_v2f32_add(*textBaselineCursor, instancePositionOffset);
        o_glyphInstanceIndices  [textMetrics->visibleGlyphCount] = bakedGlyph->bakeOrder;
        textMetrics->visibleGlyphCount++;
    }
    if(textBaselineCursor->x > 0.f)
    {
        const int kernAdvance = stbtt_GetGlyphKernAdvance(&font->stbtt_info
                                                         ,*glyphIndexPrevious
                                                         ,bakedGlyph->stbtt_glyphIndex);
        *glyphIndexPrevious    = bakedGlyph->stbtt_glyphIndex;
        textBaselineCursor->x += fontScale * kernAdvance;
    }
    else
        *glyphIndexPrevious = -1;
    textBaselineCursor->x += fontScale * advanceX;
    if(codepoint == L'\n')
    {
        textBaselineCursor->x  = 0.f;
        textBaselineCursor->y -= lineDeltaY;
    }
}
korl_internal void _korl_resource_font_generateUtf(_Korl_Resource_Font*const font, f32 textPixelHeight, const void* utfText, u$ utfTextSize, u8 utfEncoding, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u32* o_glyphInstanceIndices)
{
    const f32                        fontScale                        = stbtt_ScaleForPixelHeight(&font->stbtt_info, textPixelHeight);
    const Korl_Resource_Font_Metrics fontMetrics                      = _korl_resource_font_getMetrics(font, textPixelHeight);
    const u8                         nearestSupportedPixelHeightIndex = _korl_resource_font_nearestSupportedPixelHeightIndex(textPixelHeight);
    const f32                        lineDeltaY                       = (fontMetrics.ascent - fontMetrics.decent) + fontMetrics.lineGap;
    Korl_Math_V2f32 textBaselineCursor = (Korl_Math_V2f32){0.f, -fontMetrics.ascent};// default the baseline cursor such that our local origin is placed at _exactly_ the upper-left corner of the text's local AABB based on font metrics, as we know a glyph will never rise _above_ the fontAscent
    int             glyphIndexPrevious = -1;
    KORL_ZERO_STACK(Korl_Resource_Font_TextMetrics, textMetrics);
    switch(utfEncoding)
    {
    case 8:
        //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
        for(Korl_String_CodepointIteratorUtf8 codepointIt = korl_string_codepointIteratorUtf8_initialize(utfText, utfTextSize)
           ;!korl_string_codepointIteratorUtf8_done(&codepointIt)
           ; korl_string_codepointIteratorUtf8_next(&codepointIt))
            _korl_resource_font_generateUtf_common(font, fontScale, nearestSupportedPixelHeightIndex, codepointIt._codepoint, lineDeltaY, &glyphIndexPrevious, &textBaselineCursor, &textMetrics, instancePositionOffset, o_glyphInstancePositions, o_glyphInstanceIndices);
        break;
    case 16:
        //KORL-ISSUE-000-000-112: stringPool: incorrect grapheme detection; we are assuming 1 codepoint == 1 grapheme; in order to get true grapheme size, we have to detect unicode grapheme clusters; http://unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries; implementation example: https://stackoverflow.com/q/35962870/4526664; existing project which can achieve this (though, not sure about if it can be built in pure C, but the license seems permissive enough): https://github.com/unicode-org/icu, usage example: http://byronlai.com/jekyll/update/2014/03/20/unicode.html
        for(Korl_String_CodepointIteratorUtf16 codepointIt = korl_string_codepointIteratorUtf16_initialize(utfText, utfTextSize)
           ;!korl_string_codepointIteratorUtf16_done(&codepointIt)
           ; korl_string_codepointIteratorUtf16_next(&codepointIt))
            _korl_resource_font_generateUtf_common(font, fontScale, nearestSupportedPixelHeightIndex, codepointIt._codepoint, lineDeltaY, &glyphIndexPrevious, &textBaselineCursor, &textMetrics, instancePositionOffset, o_glyphInstancePositions, o_glyphInstanceIndices);
        break;
    default:
        korl_log(ERROR, "unsupported UTF encoding: %hhu", utfEncoding);
        break;
    }
}
korl_internal KORL_FUNCTION_korl_resource_font_getUtf8Metrics(korl_resource_font_getUtf8Metrics)
{
    KORL_ZERO_STACK(Korl_Resource_Font_TextMetrics, result);
    if(!handleResourceFont)
        return result;
    //@TODO: validate the underlying korl-resource-item descriptor type
    _Korl_Resource_Font*const font = korl_resource_getDescriptorStruct(handleResourceFont);
    if(!font->stbtt_info.data)
        return result;
    return _korl_resource_font_getUtfMetrics(font, textPixelHeight, utf8Text.data, utf8Text.size, 8);
}
korl_internal KORL_FUNCTION_korl_resource_font_generateUtf8(korl_resource_font_generateUtf8)
{
    if(!handleResourceFont)
        return;
    //@TODO: validate the underlying korl-resource-item descriptor type
    _Korl_Resource_Font*const font = korl_resource_getDescriptorStruct(handleResourceFont);
    if(!font->stbtt_info.data)
        return;
    _korl_resource_font_generateUtf(font, textPixelHeight, utf8Text.data, utf8Text.size, 8, instancePositionOffset, o_glyphInstancePositions, o_glyphInstanceIndices);
}
korl_internal KORL_FUNCTION_korl_resource_font_getUtf16Metrics(korl_resource_font_getUtf16Metrics)
{
    KORL_ZERO_STACK(Korl_Resource_Font_TextMetrics, result);
    if(!handleResourceFont)
        return result;
    //@TODO: validate the underlying korl-resource-item descriptor type
    _Korl_Resource_Font*const font = korl_resource_getDescriptorStruct(handleResourceFont);
    if(!font->stbtt_info.data)
        return result;
    return _korl_resource_font_getUtfMetrics(font, textPixelHeight, utf16Text.data, utf16Text.size, 16);
}
korl_internal KORL_FUNCTION_korl_resource_font_generateUtf16(korl_resource_font_generateUtf16)
{
    if(!handleResourceFont)
        return;
    //@TODO: validate the underlying korl-resource-item descriptor type
    _Korl_Resource_Font*const font = korl_resource_getDescriptorStruct(handleResourceFont);
    if(!font->stbtt_info.data)
        return;
    _korl_resource_font_generateUtf(font, textPixelHeight, utf16Text.data, utf16Text.size, 16, instancePositionOffset, o_glyphInstancePositions, o_glyphInstanceIndices);
}
