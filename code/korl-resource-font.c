#include "korl-resource-font.h"
#include "utility/korl-utility-resource.h"
#include "korl-stb-truetype.h"
#include "utility/korl-utility-math.h"
korl_global_const f32 _KORL_RESOURCE_FONT_PIXEL_HEIGHTS[] = {10, 14, 16, 18, 24, 30, 36, 48, 60, 72, 84};
typedef struct _Korl_Resource_Font_BakedGlyph
{
    int  stbtt_glyphIndex;// obtained form stbtt_FindGlyphIndex(fontInfo, codepoint)
    bool isEmpty;// true if stbtt_IsGlyphEmpty for this glyph returned true

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
} _Korl_Resource_Font_GlyphPage;
typedef struct _Korl_Resource_Font
{
    stbtt_fontinfo                     stbtt_info;
    Korl_Resource_Handle               resourceHandleSsboGlyphMeshVertices;// array of _Korl_Resource_Font_GlyphVertex; stores vertex data for all baked glyphs across _all_ glyph pages
    _Korl_Resource_Font_BakedGlyphMap* stbHmBakedGlyphs;
    _Korl_Resource_Font_GlyphPage*     stbDaGlyphPages;
} _Korl_Resource_Font;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_font_unload)
{
    _Korl_Resource_Font*const font = resourceDescriptorStruct;
    /* destroy child runtime resources */
    //@TODO
    /* free dynamic runtime resources */
    //@TODO
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_font_transcode)
{
    _Korl_Resource_Font*const font = resourceDescriptorStruct;
    korl_assert(!font->stbtt_info.data);// sanity-check that we aren't about to make any dangling references
    /* allocate dynamic resources */
    //@TODO
    /**/
    //KORL-ISSUE-000-000-130: gfx/font: (MAJOR) `font->stbtt_info` sets up pointers to data within `data`; if that data ever moves or gets wiped (defragment, memoryState-load, etc.), then `font->stbtt_info` will contain dangling pointers!
    korl_assert(stbtt_InitFont(&font->stbtt_info, data, 0/*font offset*/));
    //@TODO: finish setting up w/e else I need...
}
korl_internal void korl_resource_font_register(void)
{
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_FONT);
    descriptorManifest.resourceBytes      = sizeof(_Korl_Resource_Font);
    descriptorManifest.callbackUnload     = korl_functionDynamo_register(_korl_resource_font_unload);
    descriptorManifest.callbackTranscode  = korl_functionDynamo_register(_korl_resource_font_transcode);
    korl_resource_descriptor_add(&descriptorManifest);
}
