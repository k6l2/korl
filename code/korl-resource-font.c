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
korl_global_const f32 _KORL_RESOURCE_FONT_PIXEL_HEIGHTS[] = {10, 14, 16, 18, 24, 30, 36, 48, 60, 72, 84};// just an arbitrary list of common text pixel heights
typedef struct _Korl_Resource_Font_BakedGlyph
{
    int  stbtt_glyphIndex;// obtained form stbtt_FindGlyphIndex(fontInfo, codepoint)
    bool isEmpty;// true if stbtt_IsGlyphEmpty for this glyph returned true
    // @TODO: baked glyph data for _all_ _KORL_RESOURCE_FONT_PIXEL_HEIGHTS?
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
