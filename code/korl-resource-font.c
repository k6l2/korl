/** Some quick notes about korl-resource-font implementation:
 * - during design of this resource module, I had to decide between two approaches: 
 *   1) making a TEXT2D resource module, and consequently maintaining some kind of resource hierarchy
 *   2) not doing that, and instead just keep allowing the user to build & maintain their own RUNTIME gfx-buffer resources with baked glyph instance data
 * - I chose to go with (2), as it required strictly less implementation complexity
 * - as a result of the above choice, there are a few consequential limitations: 
 *   - during a font hot-reload, these scenarios will soft-fail (no crash, just incorrect rendering):
 *     - draw glyphs which changed "empty" state to "not-empty"
 *     - _not_ draw glyphs which changed from "not-empty" => "empty"
 *     - draw glyphs in the correct positions for BUFFERs generated pre-font-update
 * - as of right now, as far as I can consider, the above limitations are not a big deal, so I will probably not fix them any time soon */
#include "korl-resource-font.h"
#include "korl-interface-platform.h"
#include "korl-stb-truetype.h"
#include "utility/korl-utility-resource.h"
#include "utility/korl-utility-math.h"
#include "utility/korl-utility-stb-ds.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-checkCast.h"
#define _KORL_RESOURCE_FONT_SUPPORTED_PIXEL_HEIGHTS 11
korl_global_const f32 _KORL_RESOURCE_FONT_PIXEL_HEIGHTS[_KORL_RESOURCE_FONT_SUPPORTED_PIXEL_HEIGHTS] = {10, 14, 16, 18, 24, 30, 36, 48, 60, 72, 84};// just an arbitrary list of common text pixel heights
typedef struct _Korl_Resource_Font_BakedGlyph_BoundingBox
{
    u32             bakeOrder;// used to determine the "glyph instance index", which allows a vertex shader to choose this glyph's vertices from _Korl_Resource_Font::resourceHandleSsboGlyphMeshVertices
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
    u16                                        boxUsedFlags;// the presence of the `i`th bit means `boxes[i]` contains valid data
    int                                        stbtt_glyphIndex;// obtained form stbtt_FindGlyphIndex(fontInfo, codepoint)
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
typedef struct _Korl_Resource_Font_GlyphPage_PackRow
{
    u16 offsetY;
    u16 offsetX;
    u16 sizeY;// _including_ padding
} _Korl_Resource_Font_GlyphPage_PackRow;
typedef struct _Korl_Resource_Font_GlyphPage
{
    Korl_Resource_Handle                   resourceHandleTexture;
    _Korl_Resource_Font_GlyphPage_PackRow* stbDaPackRows;
} _Korl_Resource_Font_GlyphPage;
typedef struct _Korl_Resource_Font
{
    Korl_Memory_AllocatorHandle        allocator;// we need to remember the allocator so that user-facing APIs can create BakedGlyphs & GlyphPages
    stbtt_fontinfo                     stbtt_info;
    Korl_Resource_Handle               resourceHandleSsboGlyphMeshVertices;// array of _Korl_Resource_Font_GlyphVertex; stores vertex data for all baked glyphs across _all_ glyph pages
    u32                                ssboGlyphMeshVerticesSize;
    _Korl_Resource_Font_BakedGlyphMap* stbHmBakedGlyphs;
    _Korl_Resource_Font_GlyphPage*     stbDaGlyphPages;
} _Korl_Resource_Font;
korl_internal u8 _korl_resource_font_glyphPage_insert(_Korl_Resource_Font*const font, const int sizeX, const int sizeY, u16* out_x0, u16* out_y0, u16* out_x1, u16* out_y1)
{
    korl_shared_const u16 PACK_ROW_PADDING = 1;// amount of padding on any given edge of a packed glyph
    /* find a GlyphPage which can satisfy our size requirements */
    _Korl_Resource_Font_GlyphPage*         glyphPage = NULL;
    _Korl_Resource_Font_GlyphPage_PackRow* packRow   = NULL;
    {
        const _Korl_Resource_Font_GlyphPage*const glyphPageEnd = font->stbDaGlyphPages + arrlen(font->stbDaGlyphPages);
        for(glyphPage = font->stbDaGlyphPages; glyphPage < glyphPageEnd; glyphPage++)
        {
            /* check if the GlyphPage dimensions can even satisfy sizeX/Y */
            const Korl_Math_V2u32 textureSize = korl_resource_texture_getSize(glyphPage->resourceHandleTexture);
            if(   sizeX + 2 * PACK_ROW_PADDING > korl_checkCast_u$_to_i32(textureSize.x) 
               || sizeY + 2 * PACK_ROW_PADDING > korl_checkCast_u$_to_i32(textureSize.y))
                continue;
            /* iterate over all PackRows with valid sizeY & check their remaining sizeX; 
                we want to find the PackRow with the smallest sizeY to minimize wasted space */
            const _Korl_Resource_Font_GlyphPage_PackRow*const packRowsEnd = glyphPage->stbDaPackRows + arrlen(glyphPage->stbDaPackRows);
            u16 smallestPackRowSizeY = KORL_U16_MAX;
            for(_Korl_Resource_Font_GlyphPage_PackRow* packRow2 = glyphPage->stbDaPackRows; packRow2 < packRowsEnd; packRow2++)
            {
                if(   packRow2->sizeY >= sizeY + 2 * PACK_ROW_PADDING 
                   && (   !packRow 
                       || (   packRow2->sizeY < smallestPackRowSizeY
                           && textureSize.x - packRow2->offsetX >= korl_checkCast_i$_to_u32(sizeX + 2 * PACK_ROW_PADDING))))
                {
                    smallestPackRowSizeY = packRow2->sizeY;
                    packRow              = packRow2;
                }
            }
            if(packRow)
                break;// if we found a smallest PackRow, this GlyphPage is valid, as well as the PackRow we found, so we can exit the loop with both pointers
            packRow = NULL;
            /* check if there is enough room to make a new PackRow to satisfy sizeY */
            if(arrlenu(glyphPage->stbDaPackRows))
            {
                const u16 lastPackRowEndY = arrlast(glyphPage->stbDaPackRows).offsetY + arrlast(glyphPage->stbDaPackRows).sizeY;
                if(textureSize.y - lastPackRowEndY >= korl_checkCast_i$_to_u32(sizeY + 2 * PACK_ROW_PADDING))
                    break;
            }
        }
        /* if no GlyphPage can satisfy our requirements, we need to make a new one */
        if(glyphPage >= glyphPageEnd)
        {
            mcarrpush(KORL_STB_DS_MC_CAST(font->allocator), font->stbDaGlyphPages, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource_Font_GlyphPage));
            glyphPage = &arrlast(font->stbDaGlyphPages);
            KORL_ZERO_STACK(Korl_Resource_Texture_CreateInfo, createInfo);
            createInfo.formatImage = KORL_RESOURCE_TEXTURE_FORMAT_R8_UNORM;
            createInfo.size        = (Korl_Math_V2u32){512, 512};
            korl_assert(createInfo.size.x >= korl_checkCast_i$_to_u32(sizeX + 2 * PACK_ROW_PADDING));
            korl_assert(createInfo.size.y >= korl_checkCast_i$_to_u32(sizeY + 2 * PACK_ROW_PADDING));
            glyphPage->resourceHandleTexture = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE), &createInfo);
            mcarrsetcap(KORL_STB_DS_MC_CAST(font->allocator), glyphPage->stbDaPackRows, 16);
        }
    }
    /* at this point, we should have a GlyphPage which has enough room */
    if(!packRow)/* if the GlyphPage found in the above steps didn't also contain a valid PackRow, we need to make one now */
    {
        mcarrpush(KORL_STB_DS_MC_CAST(font->allocator), glyphPage->stbDaPackRows, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource_Font_GlyphPage_PackRow));
        packRow = &arrlast(glyphPage->stbDaPackRows);
        packRow->sizeY   = korl_checkCast_i$_to_u16(sizeY + 2 * PACK_ROW_PADDING);
        if(arrlen(glyphPage->stbDaPackRows) > 1)
            packRow->offsetY = (packRow - 1)->offsetY + (packRow - 1)->sizeY;
    }
    /* at this point, we should also have a valid PackRow; update the GlyphPage_PackRow's metrics */
    *out_x0 = packRow->offsetX + PACK_ROW_PADDING;
    *out_y0 = packRow->offsetY + PACK_ROW_PADDING;
    *out_x1 = korl_checkCast_i$_to_u16(*out_x0 + sizeX);
    *out_y1 = korl_checkCast_i$_to_u16(*out_y0 + sizeY);
    /* update the pack row metrics */
    packRow->offsetX += korl_checkCast_i$_to_u16(sizeX + 2 * PACK_ROW_PADDING);
    const u8 glyphPageIndex = korl_checkCast_i$_to_u8(glyphPage - font->stbDaGlyphPages);
    return glyphPageIndex;
}
korl_internal const _Korl_Resource_Font_BakedGlyph* _korl_resource_font_getGlyph(_Korl_Resource_Font*const font, const u32 codePoint, const u8 nearestSupportedPixelHeightIndex)
{
    _Korl_Resource_Font_BakedGlyph* glyph = NULL;
    const ptrdiff_t glyphMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(font->allocator), font->stbHmBakedGlyphs, codePoint);
    if(glyphMapIndex >= 0)
        glyph = &(font->stbHmBakedGlyphs[glyphMapIndex].value);
    /* if the glyph was not found, we need to cache it; note that adding the 
        glyph to our database does _not_ mean actually baking it, as we will be 
        baking glyphs at discretized sizes now */
    if(!glyph)
    {
        mchmput(KORL_STB_DS_MC_CAST(font->allocator), font->stbHmBakedGlyphs, codePoint, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource_Font_BakedGlyph));
        glyph = &((font->stbHmBakedGlyphs + hmlen(font->stbHmBakedGlyphs) - 1)->value);
        glyph->stbtt_glyphIndex = stbtt_FindGlyphIndex(&font->stbtt_info, codePoint);
        glyph->isEmpty          = stbtt_IsGlyphEmpty(&font->stbtt_info, glyph->stbtt_glyphIndex);
    }
    korl_assert(glyph);
    /* if the nearestSupportedPixelHeightIndex for this glyph has not yet been 
        baked into a glyph page, we must do so now */
    korl_assert(nearestSupportedPixelHeightIndex < 8 * sizeof(glyph->boxUsedFlags));// ensure that we can actually index a bit flag in boxUsedFlags
    korl_assert(nearestSupportedPixelHeightIndex < korl_arraySize(_KORL_RESOURCE_FONT_PIXEL_HEIGHTS));// sanity-check
    if(!glyph->isEmpty && !(glyph->boxUsedFlags & (1 << nearestSupportedPixelHeightIndex)))
    {
        const f32 nearestSupportedPixelHeight = _KORL_RESOURCE_FONT_PIXEL_HEIGHTS[nearestSupportedPixelHeightIndex];
        const f32 fontScale                   = stbtt_ScaleForPixelHeight(&font->stbtt_info, nearestSupportedPixelHeight);
        int ix0, iy0, ix1, iy1;
        stbtt_GetGlyphBitmapBox(&font->stbtt_info, glyph->stbtt_glyphIndex, fontScale, fontScale, &ix0, &iy0, &ix1, &iy1);
        const int w = ix1 - ix0;
        const int h = iy1 - iy0;
        int advanceX, bearingLeft;
        stbtt_GetGlyphHMetrics(&font->stbtt_info, glyph->stbtt_glyphIndex, &advanceX, &bearingLeft);
        _Korl_Resource_Font_BakedGlyph_BoundingBox*const bbox = glyph->boxes + nearestSupportedPixelHeightIndex;
        bbox->glyphPageIndex = _korl_resource_font_glyphPage_insert(font, w, h, &bbox->x0, &bbox->y0, &bbox->x1, &bbox->y1);
        bbox->offset.x =  KORL_C_CAST(f32, bearingLeft);
        bbox->offset.y = -KORL_C_CAST(f32, iy1);
        /* actually write the glyph bitmap in the glyph page data */
        _Korl_Resource_Font_GlyphPage*const glyphPage = font->stbDaGlyphPages + bbox->glyphPageIndex;
        u$ bytesRequested_bytesAvailable = KORL_U$_MAX;
        u8*const              glyphPageUpdateBuffer         = korl_resource_getUpdateBuffer(glyphPage->resourceHandleTexture, 0, &bytesRequested_bytesAvailable);
        const Korl_Math_V2u32 glyphPageTextureSize          = korl_resource_texture_getSize(glyphPage->resourceHandleTexture);
        const u32             glyphPageTextureRowByteStride = korl_resource_texture_getRowByteStride(glyphPage->resourceHandleTexture);
        stbtt_MakeGlyphBitmap(&font->stbtt_info
                             ,glyphPageUpdateBuffer + (bbox->y0 * glyphPageTextureRowByteStride + bbox->x0)
                             ,glyphPageTextureSize.x, glyphPageTextureSize.y, glyphPageTextureRowByteStride
                             ,fontScale, fontScale, glyph->stbtt_glyphIndex);
        /* determine the glyph's bakeOrder, & reallocate the font's GlyphVertex SSBO if necessary */
        bbox->bakeOrder = font->ssboGlyphMeshVerticesSize;
        u$ ssboGlyphMeshVerticesCapacity = korl_resource_getByteSize(font->resourceHandleSsboGlyphMeshVertices) / sizeof(_Korl_Resource_Font_GlyphVertex);
        if(font->ssboGlyphMeshVerticesSize + 4 > ssboGlyphMeshVerticesCapacity)
        {
            ssboGlyphMeshVerticesCapacity = KORL_MATH_MAX(font->ssboGlyphMeshVerticesSize + 4, 2 * ssboGlyphMeshVerticesCapacity);
            korl_resource_resize(font->resourceHandleSsboGlyphMeshVertices, ssboGlyphMeshVerticesCapacity * sizeof(_Korl_Resource_Font_GlyphVertex));
        }
        bytesRequested_bytesAvailable = 4 * sizeof(_Korl_Resource_Font_GlyphVertex);
        _Korl_Resource_Font_GlyphVertex*const glyphVertexUpdateBuffer = korl_resource_getUpdateBuffer(font->resourceHandleSsboGlyphMeshVertices, bbox->bakeOrder * sizeof(_Korl_Resource_Font_GlyphVertex), &bytesRequested_bytesAvailable);
        korl_assert(bytesRequested_bytesAvailable == 4 * sizeof(_Korl_Resource_Font_GlyphVertex));
        /* append the GlyphVertex data to the font's GlyphVertex SSBO */
        const f32 x0 = 0.f/*start glyph at baseline cursor origin*/ + bbox->offset.x;
        const f32 y0 = 0.f/*start glyph at baseline cursor origin*/ + bbox->offset.y;
        const f32 x1 = x0 + (bbox->x1 - bbox->x0);
        const f32 y1 = y0 + (bbox->y1 - bbox->y0);
        // @TODO: saving UV ratios will _not_ work if we ever resize the glyph page texture, _unless_ we use a fragment shader which performs this division for us!
        const f32 u0 = KORL_C_CAST(f32, bbox->x0) / KORL_C_CAST(f32, glyphPageTextureSize.x);
        const f32 u1 = KORL_C_CAST(f32, bbox->x1) / KORL_C_CAST(f32, glyphPageTextureSize.x);
        const f32 v0 = KORL_C_CAST(f32, bbox->y0) / KORL_C_CAST(f32, glyphPageTextureSize.y);
        const f32 v1 = KORL_C_CAST(f32, bbox->y1) / KORL_C_CAST(f32, glyphPageTextureSize.y);
        glyphVertexUpdateBuffer[0] = (_Korl_Resource_Font_GlyphVertex){.position2d_uv = (Korl_Math_V4f32){x0, y0, u0, v1}};
        glyphVertexUpdateBuffer[1] = (_Korl_Resource_Font_GlyphVertex){.position2d_uv = (Korl_Math_V4f32){x1, y0, u1, v1}};
        glyphVertexUpdateBuffer[2] = (_Korl_Resource_Font_GlyphVertex){.position2d_uv = (Korl_Math_V4f32){x1, y1, u1, v0}};
        glyphVertexUpdateBuffer[3] = (_Korl_Resource_Font_GlyphVertex){.position2d_uv = (Korl_Math_V4f32){x0, y1, u0, v0}};
        /* the glyph at the nearestSupportedPixelHeightIndex is now baked */
        glyph->boxUsedFlags |= 1 << nearestSupportedPixelHeightIndex;
    }
    // at this point, we should have a valid glyph //
    korl_assert(glyph);
    return glyph;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_font_descriptorStructCreate)
{
    _Korl_Resource_Font*const font = korl_allocate(allocator, sizeof(_Korl_Resource_Font));
    font->allocator = allocator;
    mchmdefault(KORL_STB_DS_MC_CAST(allocator), font->stbHmBakedGlyphs, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource_Font_BakedGlyph));
    mcarrsetcap(KORL_STB_DS_MC_CAST(allocator), font->stbDaGlyphPages, 8);
    return font;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_font_descriptorStructDestroy)
{
    _Korl_Resource_Font*const font = resourceDescriptorStruct;
    mchmfree(KORL_STB_DS_MC_CAST(allocator), font->stbHmBakedGlyphs);
    {
        const _Korl_Resource_Font_GlyphPage*const glyphPageEnd = font->stbDaGlyphPages + arrlen(font->stbDaGlyphPages);
        for(_Korl_Resource_Font_GlyphPage* glyphPage = font->stbDaGlyphPages; glyphPage < glyphPageEnd; glyphPage++)
            mcarrfree(KORL_STB_DS_MC_CAST(allocator), glyphPage->stbDaPackRows);
    }
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
    /* if this font resource has BakedGlyphs, we need to re-bake them all 
        - in order to keep all RUNTIME korl-resource-buffers that contain glyph instance data valid, 
          we need to re-bake each BakedGlyph_BoundingBox in the _same_ bakeOrder that currently exists
        - collect all BakedGlyph_BoundingBox (that raised a boxUsedFlags flag) from stbHmBakedGlyphs into a big temp array
        - sort the array by ascending bakeOrder
        - call `_korl_resource_font_getGlyph` on each element in order; remember, we don't actually care if they end up in the same GlyphPage location; the only thing that matters is the bakeOrder!
        - discard the temp array */
    ///@TODO: finish this to support korl-memoryState functionality
    #if 0//@TODO: figure this out
    const _Korl_Resource_Font_BakedGlyphMap*const bakedGlyphMapEnd = font->stbHmBakedGlyphs + hmlen(font->stbHmBakedGlyphs);
    for(_Korl_Resource_Font_BakedGlyphMap* bakedGlyphMap = font->stbHmBakedGlyphs; bakedGlyphMap < bakedGlyphMapEnd; bakedGlyphMap++)
    {
        for(u8 i = 0; i < korl_arraySize(_KORL_RESOURCE_FONT_PIXEL_HEIGHTS); i++)
        {
        }
    }
    #endif
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
    u8  result        = korl_arraySize(_KORL_RESOURCE_FONT_PIXEL_HEIGHTS);
    for(u8 i = 0; i < korl_arraySize(_KORL_RESOURCE_FONT_PIXEL_HEIGHTS); i++)
    {
        const f32 delta = korl_math_f32_positive(_KORL_RESOURCE_FONT_PIXEL_HEIGHTS[i] - pixelHeight);
        if(delta < smallestDelta)
        {
            smallestDelta = delta;
            result        = i;
        }
    }
    korl_assert(result < korl_arraySize(_KORL_RESOURCE_FONT_PIXEL_HEIGHTS));
    return result;
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
        o_glyphInstanceIndices  [textMetrics->visibleGlyphCount] = bakedGlyph->boxes[nearestSupportedPixelHeightIndex].bakeOrder;
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
