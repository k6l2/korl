#include "korl-gfx.h"
#include "korl-io.h"
#include "korl-assetCache.h"
#include "korl-memory.h"
#include "korl-vulkan.h"
#include "stb/stb_truetype.h"
typedef struct _Korl_Gfx_FontCache
{
    u16 alphaGlyphImageBufferSizeX;
    u16 alphaGlyphImageBufferSizeY;
    int glyphDataFirstCharacterCode;
    int glyphDataSize;
    f32 pixelHeight;
    Korl_Vulkan_TextureHandle textureHandleGlyphAtlas;
    /** @space: MINOR - these pointers all reference contiguous memory regions 
     * which immediately follow this struct itself, ergo can be converted to 
     * offsets, saving us space but making the code less readable */
    wchar_t* fontAssetName;
    u8* alphaGlyphImageBuffer;//1-channel, Alpha8 format
    stbtt_bakedchar* glyphData;
} _Korl_Gfx_FontCache;
typedef struct _Korl_Gfx_Context
{
    /** used to store persistent data, such as Font asset glyph cache/database */
    Korl_Memory_Allocator allocator;
    KORL_MEMORY_POOL_DECLARE(_Korl_Gfx_FontCache*, fontCaches, 16);
} _Korl_Gfx_Context;
korl_global_variable _Korl_Gfx_Context _korl_gfx_context;
korl_internal bool _korl_gfx_isVisibleCharacter(wchar_t c)
{
    /** @robustness: do I even need to say anything about this?  lol */
    return c >= ' ';
}
/** Vertex order for each glyph quad:
 * [ bottom-left
 * , bottom-right
 * , top-right
 * , top-left ] */
korl_internal void _korl_gfx_textLazyGenerateMesh(Korl_Gfx_Batch*const batch)
{
    _Korl_Gfx_Context*const context = &_korl_gfx_context;
    if(!batch->_assetNameFont || batch->_fontTextureHandle)
        return;
    Korl_AssetCache_AssetData assetDataFont = korl_assetCache_get(batch->_assetNameFont, KORL_ASSETCACHE_GET_FLAGS_LAZY);
    if(!assetDataFont.data)
        return;
#if 0 /* some thoughts on ways to store/manage glyph cache data */
    I guess it's okay to store this data in the korl-stb-truetype allocator?
    How do we tie the lifetime of this data to the font asset?...
    [x] create an allocator in korl-gfx for persistent data instead
        - have a global korl-gfx context
        - create a database of font atlas data, with a unique entry per font 
          asset string
    [-] just don't; leak memory forever since it likely wont be much ;D
    [-] pass callbacks to korl_assetCache_get?...
        - this requires us to expose the allocator in the korl-stb-truetype 
          module, but that's okay since this API most likely wont be exposed to 
          the code which consumes KORL anyways
        - this could set limits on which code module is allowed to call this API 
          due to function pointer validity!!!
    [-] store this data inside Korl_AssetCache_AssetData itself?...
        - specialize the AssetData struct (PTU) like I used to do in C++ KORL
        - automatically detect what kind of asset it is when it is loaded 
          (based on file extension)
        - this might require the assetCache module to have a dependency on the 
          backend renderer, for example if we need to query Vulkan for max 
          image2D dimensions, which might get a little... hairy...
#endif
    /* create a temporary 1-channel,1-byte image buffer to store the glyph atlas 
        if it doesn't already exist, as well as a glyph atlas database */
    // find the font cache with a matching font asset name AND render parameters 
    //  such as font pixel height, etc... //
    u$ existingFontCacheIndex = 0;
    for(; existingFontCacheIndex < KORL_MEMORY_POOL_SIZE(context->fontCaches); existingFontCacheIndex++)
        if(0 == korl_memory_stringCompare(batch->_assetNameFont, context->fontCaches[existingFontCacheIndex]->fontAssetName)
            && context->fontCaches[existingFontCacheIndex]->pixelHeight == batch->_textPixelHeight)
            break;
    // if it doesn't exist, allocate/create one //
    if(existingFontCacheIndex >= KORL_MEMORY_POOL_SIZE(context->fontCaches))
    {
        /* add a new font cache address to the context's font cache address pool */
        korl_assert(!KORL_MEMORY_POOL_ISFULL(context->fontCaches));
        _Korl_Gfx_FontCache**const newFontCache = KORL_MEMORY_POOL_ADD(context->fontCaches);
        /* calculate how much memory we need */
        korl_shared_const int GLYPH_FIRST_CODE = 32;
        korl_shared_const int GLYPH_DATA_SIZE = 96;// ASCII [32, 126]|[' ', '~'] is 96 glyphs
        korl_shared_const u16 GLYPH_ALPHA_BUFFER_SIZE_XY = 512;
        const u$ assetNameFontBufferSize = korl_memory_stringSize(batch->_assetNameFont) + 1;
        const u$ assetNameFontBufferBytes = assetNameFontBufferSize*sizeof(*batch->_assetNameFont);
        const u$ fontCacheRequiredBytes = sizeof(_Korl_Gfx_FontCache)
            + assetNameFontBufferBytes
            + GLYPH_ALPHA_BUFFER_SIZE_XY*GLYPH_ALPHA_BUFFER_SIZE_XY
            + GLYPH_DATA_SIZE*sizeof(stbtt_bakedchar);
        /* allocate the required memory from the korl-gfx context allocator */
        *newFontCache = context->allocator.callbackAllocate(context->allocator.userData, 
                                                            fontCacheRequiredBytes, __FILEW__, __LINE__);
        /* initialize the memory 
            - note that it should already all be nullified by the allocator
            - the data structures referenced by the _Korl_Gfx_FontCache struct 
              all point to contiguous regions of memory immediately following 
              the struct itself */
        (*newFontCache)->alphaGlyphImageBufferSizeX  = GLYPH_ALPHA_BUFFER_SIZE_XY;
        (*newFontCache)->alphaGlyphImageBufferSizeY  = GLYPH_ALPHA_BUFFER_SIZE_XY;
        (*newFontCache)->glyphDataFirstCharacterCode = GLYPH_FIRST_CODE;
        (*newFontCache)->glyphDataSize               = GLYPH_DATA_SIZE;
        (*newFontCache)->pixelHeight                 = batch->_textPixelHeight;
        (*newFontCache)->fontAssetName               = KORL_C_CAST(wchar_t*, (*newFontCache) + 1);
        (*newFontCache)->alphaGlyphImageBuffer       = KORL_C_CAST(u8*, (*newFontCache)->fontAssetName) + assetNameFontBufferBytes;
        (*newFontCache)->glyphData                   = KORL_C_CAST(stbtt_bakedchar*, KORL_C_CAST(u8*, (*newFontCache)->alphaGlyphImageBuffer) + GLYPH_ALPHA_BUFFER_SIZE_XY*GLYPH_ALPHA_BUFFER_SIZE_XY);
        korl_assert(korl_checkCast_u$_to_i$(assetNameFontBufferSize) == korl_memory_stringCopy(batch->_assetNameFont, (*newFontCache)->fontAssetName, assetNameFontBufferSize));
        /* at this point, we can just pre-bake the entire range of glyphs we 
            need in order to use this font */
        /** @robustness: this is the "quick & dirty" stbtt API, ergo it's 
         * probably a good idea to switch to the lower-level API to be able to 
         * do things like lazy glyph caching, better packing, outlines, etc... */
        stbtt_BakeFontBitmap(assetDataFont.data, /*offset*/0, 
                             (*newFontCache)->pixelHeight, 
                             (*newFontCache)->alphaGlyphImageBuffer, 
                             (*newFontCache)->alphaGlyphImageBufferSizeX, 
                             (*newFontCache)->alphaGlyphImageBufferSizeY, 
                             (*newFontCache)->glyphDataFirstCharacterCode, 
                             (*newFontCache)->glyphDataSize, 
                             (*newFontCache)->glyphData);
        /* now that we have a complete baked glyph atlas for this font at the 
            required settings for the desired parameters of the provided batch, 
            we can upload this image buffer to the graphics device for rendering */
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
        /** @speed: @memory: okay, I'm making a decision to just do the stupid 
         * method of "expanding" the 1-channel A8-format bitmap generated by 
         * stbtt into a R8G8B8A8-format image buffer, sending it off to vulkan, 
         * then destroying the temp buffer.  I'm just going to do this because 
         * for now I know it will work, and we can just come back to this if we 
         * ever encounter any performance issues! */
        // allocate a temp R8G8B8A8-format image buffer //
        Korl_Vulkan_Color4u8*const tempImageBuffer = context->allocator.callbackAllocate(context->allocator.userData, 
                                                                                         sizeof(Korl_Vulkan_Color4u8) * (*newFontCache)->alphaGlyphImageBufferSizeX * (*newFontCache)->alphaGlyphImageBufferSizeY, 
                                                                                         __FILEW__, __LINE__);
        // "expand" the stbtt font bitmap into the image buffer //
        for(u$ y = 0; y < (*newFontCache)->alphaGlyphImageBufferSizeY; y++)
            for(u$ x = 0; x < (*newFontCache)->alphaGlyphImageBufferSizeX; x++)
                /* store a pure white pixel with the alpha component set to the stbtt font bitmap value */
                tempImageBuffer[y*(*newFontCache)->alphaGlyphImageBufferSizeX + x] = (Korl_Vulkan_Color4u8){.rgba.r = 0xFF, .rgba.g = 0xFF, .rgba.b = 0xFF, .rgba.a = (*newFontCache)->alphaGlyphImageBuffer[y*(*newFontCache)->alphaGlyphImageBufferSizeX + x]};
        // create a vulkan texture, upload the image buffer to it //
        (*newFontCache)->textureHandleGlyphAtlas = korl_vulkan_createTexture((*newFontCache)->alphaGlyphImageBufferSizeX, 
                                                                             (*newFontCache)->alphaGlyphImageBufferSizeY, 
                                                                             tempImageBuffer);
        // free the temporary R8G8B8A8-format image buffer //
        context->allocator.callbackFree(context->allocator.userData, tempImageBuffer, __FILEW__, __LINE__);
    }
    /* at this point, we should have an index to a valid font cache which 
        contains all the data necessary about the glyphs will want to render for 
        the batch's text string - all we have to do now is iterate over all the 
        pre-allocated vertex attributes (position & UVs) and update them to use 
        the correct  */
    const _Korl_Gfx_FontCache*const fontCache = context->fontCaches[existingFontCacheIndex];
    float textBaselinePositionX = 0, textBaselinePositionY = 0;
    u32 currentGlyph = 0;
    for(const wchar_t* character = batch->_text; *character; character++)
    {
        if(!_korl_gfx_isVisibleCharacter(*character))
            continue;
        korl_assert(currentGlyph < batch->_vertexCount);
        KORL_ZERO_STACK(stbtt_aligned_quad, stbAlignedQuad);
        stbtt_GetBakedQuad(fontCache->glyphData, 
                           fontCache->alphaGlyphImageBufferSizeX, 
                           fontCache->alphaGlyphImageBufferSizeY, 
                           *character - fontCache->glyphDataFirstCharacterCode, 
                           &textBaselinePositionX, 
                           &textBaselinePositionY,
                           &stbAlignedQuad,
                           1);//1=opengl & d3d10+,0=d3d9
        /* using currentGlyph, we can now write the vertex positions & UVs for 
            the current glyph quad we just obtained the metrics for 
            - note that we invert the vertex position Y coordinates, because 
              stbtt works in a coordinate space where +Y is down, and I strongly 
              dislike working with inverted Y coordinate spaces, ergo KORL is 
              always going to expect the standard +Y => UP */
        batch->_vertexPositions[4*currentGlyph + 0] = (Korl_Vulkan_Position){.xyz.x = stbAlignedQuad.x0, .xyz.y = -stbAlignedQuad.y1, .xyz.z = 0};
        batch->_vertexPositions[4*currentGlyph + 1] = (Korl_Vulkan_Position){.xyz.x = stbAlignedQuad.x1, .xyz.y = -stbAlignedQuad.y1, .xyz.z = 0};
        batch->_vertexPositions[4*currentGlyph + 2] = (Korl_Vulkan_Position){.xyz.x = stbAlignedQuad.x1, .xyz.y = -stbAlignedQuad.y0, .xyz.z = 0};
        batch->_vertexPositions[4*currentGlyph + 3] = (Korl_Vulkan_Position){.xyz.x = stbAlignedQuad.x0, .xyz.y = -stbAlignedQuad.y0, .xyz.z = 0};
        batch->_vertexUvs[4*currentGlyph + 0] = (Korl_Vulkan_Uv){.xy.x = stbAlignedQuad.s0, .xy.y = stbAlignedQuad.t1};
        batch->_vertexUvs[4*currentGlyph + 1] = (Korl_Vulkan_Uv){.xy.x = stbAlignedQuad.s1, .xy.y = stbAlignedQuad.t1};
        batch->_vertexUvs[4*currentGlyph + 2] = (Korl_Vulkan_Uv){.xy.x = stbAlignedQuad.s1, .xy.y = stbAlignedQuad.t0};
        batch->_vertexUvs[4*currentGlyph + 3] = (Korl_Vulkan_Uv){.xy.x = stbAlignedQuad.s0, .xy.y = stbAlignedQuad.t0};
        currentGlyph++;
    }
    batch->_fontTextureHandle = fontCache->textureHandleGlyphAtlas;
}
korl_internal void korl_gfx_initialize(void)
{
    _Korl_Gfx_Context*const context = &_korl_gfx_context;
    korl_memory_nullify(context, sizeof(*context));
    context->allocator = korl_memory_createAllocatorLinear(korl_math_megabytes(2));
}
korl_internal Korl_Gfx_Camera korl_gfx_createCameraFov(f32 fovHorizonDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 target)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                    = KORL_GFX_CAMERA_TYPE_PERSPECTIVE;
    result.position                                = position;
    result.target                                  = target;
    result.viewportScissorRatioPosition            = (Korl_Math_V2f32){.xy = {0, 0}};
    result.viewportScissorRatioSize                = (Korl_Math_V2f32){.xy = {1, 1}};
    result.subCamera.perspective.clipNear          = clipNear;
    result.subCamera.perspective.clipFar           = clipFar;
    result.subCamera.perspective.fovHorizonDegrees = fovHorizonDegrees;
    return result;
}
korl_internal Korl_Gfx_Camera korl_gfx_createCameraOrtho(f32 clipDepth)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                        = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC;
    result.position                                    = KORL_MATH_V3F32_ZERO;
    result.target                                      = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result.viewportScissorRatioPosition                = (Korl_Math_V2f32){.xy = {0, 0}};
    result.viewportScissorRatioSize                    = (Korl_Math_V2f32){.xy = {1, 1}};
    result.subCamera.orthographicFixedHeight.clipDepth = clipDepth;
    return result;
}
korl_internal Korl_Gfx_Camera korl_gfx_createCameraOrthoFixedHeight(f32 fixedHeight, f32 clipDepth)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                          = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT;
    result.position                                      = KORL_MATH_V3F32_ZERO;
    result.target                                        = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result.viewportScissorRatioPosition                  = (Korl_Math_V2f32){.xy = {0, 0}};
    result.viewportScissorRatioSize                      = (Korl_Math_V2f32){.xy = {1, 1}};
    result.subCamera.orthographicFixedHeight.fixedHeight = fixedHeight;
    result.subCamera.orthographicFixedHeight.clipDepth   = clipDepth;
    return result;
}
korl_internal void korl_gfx_cameraFov_rotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians)
{
    korl_assert(context->type == KORL_GFX_CAMERA_TYPE_PERSPECTIVE);
    const Korl_Math_V3f32 newTargetOffset = 
        korl_math_quaternion_transformV3f32(
            korl_math_quaternion_fromAxisRadians(axisOfRotation, radians, false), 
            korl_math_v3f32_subtract(context->position, &context->target), 
            true);
    context->position = korl_math_v3f32_add(context->target, newTargetOffset);
}
korl_internal void korl_gfx_useCamera(Korl_Gfx_Camera camera)
{
    const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize();
    korl_assert(camera.viewportScissorRatioPosition.xy.x >= 0 && camera.viewportScissorRatioPosition.xy.x <= 1);
    korl_assert(camera.viewportScissorRatioPosition.xy.y >= 0 && camera.viewportScissorRatioPosition.xy.y <= 1);
    korl_assert(camera.viewportScissorRatioSize.xy.x >= 0 && camera.viewportScissorRatioSize.xy.x <= 1);
    korl_assert(camera.viewportScissorRatioSize.xy.y >= 0 && camera.viewportScissorRatioSize.xy.y <= 1);
    korl_vulkan_setScissor(korl_math_round_f32_to_u32(camera.viewportScissorRatioPosition.xy.x * swapchainSize.xy.x),
                           korl_math_round_f32_to_u32(camera.viewportScissorRatioPosition.xy.y * swapchainSize.xy.y),
                           korl_math_round_f32_to_u32(camera.viewportScissorRatioSize.xy.x * swapchainSize.xy.x), 
                           korl_math_round_f32_to_u32(camera.viewportScissorRatioSize.xy.y * swapchainSize.xy.y));
    switch(camera.type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        korl_vulkan_setProjectionFov(camera.subCamera.perspective.fovHorizonDegrees, camera.subCamera.perspective.clipNear, camera.subCamera.perspective.clipFar);
        korl_vulkan_setView(camera.position, camera.target, KORL_MATH_V3F32_Z);
        }break;
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        korl_vulkan_setProjectionOrthographic(camera.subCamera.orthographicFixedHeight.clipDepth);
        korl_vulkan_setView(camera.position, camera.target, KORL_MATH_V3F32_Y);
        }break;
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        korl_vulkan_setProjectionOrthographicFixedHeight(camera.subCamera.orthographicFixedHeight.fixedHeight, camera.subCamera.orthographicFixedHeight.clipDepth);
        korl_vulkan_setView(camera.position, camera.target, KORL_MATH_V3F32_Y);
        }break;
    }
}
korl_internal void korl_gfx_cameraSetScissorPercent(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight)
{
    context->viewportScissorRatioPosition.xy.x = viewportRatioX;
    context->viewportScissorRatioPosition.xy.y = viewportRatioY;
    context->viewportScissorRatioSize.xy.x     = viewportRatioWidth;
    context->viewportScissorRatioSize.xy.y     = viewportRatioHeight;
}
korl_internal void korl_gfx_batch(Korl_Gfx_Batch*const batch, Korl_Gfx_Batch_Flags flags)
{
    _korl_gfx_textLazyGenerateMesh(batch);
    if(batch->_vertexCount <= 0)
    {
        korl_log(WARNING, "attempted batch is empty");
        return;
    }
    if(batch->_assetNameFont && !batch->_fontTextureHandle)
    {
        korl_log(WARNING, "text batch mesh not yet obtained from font asset");
        return;
    }
    if(batch->_assetNameTexture)
        korl_vulkan_useImageAssetAsTexture(batch->_assetNameTexture);
    else if(batch->_assetNameFont)
        korl_vulkan_useTexture(batch->_fontTextureHandle);
    korl_vulkan_setModel(batch->_position, batch->_rotation, batch->_scale);
    korl_vulkan_batchSetUseDepthTestAndWriteDepthBuffer(!(flags & KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST));
    korl_vulkan_batch(batch->primitiveType, 
        batch->_vertexIndexCount, batch->_vertexIndices, 
        batch->_vertexCount, batch->_vertexPositions, batch->_vertexColors, batch->_vertexUvs);
}
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchRectangleTextured(Korl_Memory_Allocator allocator, Korl_Math_V2f32 size, const wchar_t* assetNameTexture)
{
    /* calculate required amount of memory for the batch */
    const u$ assetNameTextureSize = korl_memory_stringSize(assetNameTexture) + 1;
    const u$ assetNameTextureBytes = assetNameTextureSize * sizeof(wchar_t);
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + assetNameTextureBytes
        + 6 * sizeof(Korl_Vulkan_VertexIndex)
        + 4 * sizeof(Korl_Vulkan_Position)
        + 4 * sizeof(Korl_Vulkan_Uv);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        allocator.callbackAllocate(allocator.userData, totalBytes, __FILEW__, __LINE__));
    /* initialize the batch struct */
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount = 6;
    result->_vertexCount      = 4;
    result->_assetNameTexture = KORL_C_CAST(wchar_t*, result + 1);
    result->_vertexIndices    = KORL_C_CAST(Korl_Vulkan_VertexIndex*, KORL_C_CAST(u8*, result->_assetNameTexture) + assetNameTextureBytes);
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*   , KORL_C_CAST(u8*, result->_vertexIndices   ) + 6*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexUvs        = KORL_C_CAST(Korl_Vulkan_Uv*         , KORL_C_CAST(u8*, result->_vertexPositions ) + 4*sizeof(Korl_Vulkan_Position));
    /* initialize the batch's dynamic data */
    if(korl_memory_stringCopy(assetNameTexture, result->_assetNameTexture, assetNameTextureSize) != korl_checkCast_u$_to_i$(assetNameTextureSize))
        korl_log(ERROR, "failed to copy asset name \"%s\" to batch", assetNameTexture);
    korl_shared_const Korl_Vulkan_VertexIndex vertexIndices[] = 
        { 0, 1, 3
        , 1, 2, 3 };
    memcpy(result->_vertexIndices, vertexIndices, sizeof(vertexIndices));
    korl_shared_const Korl_Vulkan_Position vertexPositions[] = 
        { {-0.5f, -0.5f, 0.f}
        , { 0.5f, -0.5f, 0.f}
        , { 0.5f,  0.5f, 0.f}
        , {-0.5f,  0.5f, 0.f} };
    memcpy(result->_vertexPositions, vertexPositions, sizeof(vertexPositions));
    // scale the rectangle mesh vertices by the provided size //
    for(u$ i = 0; i < korl_arraySize(vertexPositions); ++i)
    {
        result->_vertexPositions[i].xyz.x *= size.xy.x;
        result->_vertexPositions[i].xyz.y *= size.xy.y;
    }
    korl_shared_const Korl_Vulkan_Uv vertexTextureUvs[] = 
        { {0, 1}
        , {1, 1}
        , {1, 0}
        , {0, 0} };
    memcpy(result->_vertexUvs, vertexTextureUvs, sizeof(vertexTextureUvs));
    /* return the batch */
    return result;
}
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchRectangleColored(Korl_Memory_Allocator allocator, Korl_Math_V2f32 size, Korl_Vulkan_Color color)
{
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 6 * sizeof(Korl_Vulkan_VertexIndex)
        + 4 * sizeof(Korl_Vulkan_Position)
        + 4 * sizeof(Korl_Vulkan_Color);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        allocator.callbackAllocate(allocator.userData, totalBytes, __FILEW__, __LINE__));
    /* initialize the batch struct */
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount = 6;
    result->_vertexCount      = 4;
    result->_vertexIndices    = KORL_C_CAST(Korl_Vulkan_VertexIndex*, result + 1);
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*   , KORL_C_CAST(u8*, result->_vertexIndices   ) + 6*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexColors     = KORL_C_CAST(Korl_Vulkan_Color*      , KORL_C_CAST(u8*, result->_vertexPositions ) + 4*sizeof(Korl_Vulkan_Position));
    /* initialize the batch's dynamic data */
    korl_shared_const Korl_Vulkan_VertexIndex vertexIndices[] = 
        { 0, 1, 3
        , 1, 2, 3 };
    memcpy(result->_vertexIndices, vertexIndices, sizeof(vertexIndices));
    korl_shared_const Korl_Vulkan_Position vertexPositions[] = 
        { {-0.5f, -0.5f, 0.f}
        , { 0.5f, -0.5f, 0.f}
        , { 0.5f,  0.5f, 0.f}
        , {-0.5f,  0.5f, 0.f} };
    memcpy(result->_vertexPositions, vertexPositions, sizeof(vertexPositions));
    // scale the rectangle mesh vertices by the provided size //
    for(u$ i = 0; i < korl_arraySize(vertexPositions); ++i)
    {
        result->_vertexPositions[i].xyz.x *= size.xy.x;
        result->_vertexPositions[i].xyz.y *= size.xy.y;
    }
    for(u$ c = 0; c < result->_vertexCount; c++)
        result->_vertexColors[c] = color;
    /* return the batch */
    return result;
}
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchLines(Korl_Memory_Allocator allocator, u32 lineCount)
{
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 2 * lineCount * sizeof(Korl_Vulkan_Position)
        + 2 * lineCount * sizeof(Korl_Vulkan_Color);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        allocator.callbackAllocate(allocator.userData, totalBytes, __FILEW__, __LINE__));
    /* initialize the batch struct */
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_LINES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexCount      = 2 * lineCount;
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*, result + 1);
    result->_vertexColors     = KORL_C_CAST(Korl_Vulkan_Color*   , KORL_C_CAST(u8*, result->_vertexPositions ) + 2*lineCount*sizeof(Korl_Vulkan_Position));
    /* return the batch */
    return result;
}
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchText(Korl_Memory_Allocator allocator, const wchar_t* assetNameFont, const wchar_t* text, f32 textPixelHeight)
{
    /* calculate required amount of memory for the batch */
    const u$ assetNameFontBufferSize = korl_memory_stringSize(assetNameFont) + 1;
    const u$ assetNameFontBytes = assetNameFontBufferSize * sizeof(*assetNameFont);
    u$ textVisibleCharacterCount = 0;
    // calculate the # of visible characters in the text //
    {
        const wchar_t* textIterator = text;
        for(; *textIterator; textIterator++)
            if(_korl_gfx_isVisibleCharacter(*textIterator))
                textVisibleCharacterCount++;
    }
    const u$ textBufferSize = korl_memory_stringSize(text) + 1;
    const u$ textBytes = textBufferSize * sizeof(*text);
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + assetNameFontBytes
        + textBytes
        + textVisibleCharacterCount * 6*sizeof(Korl_Vulkan_VertexIndex)
        + textVisibleCharacterCount * 4*sizeof(Korl_Vulkan_Position)
        + textVisibleCharacterCount * 4*sizeof(Korl_Vulkan_Uv);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        allocator.callbackAllocate(allocator.userData, totalBytes, __FILEW__, __LINE__));
    /* initialize the batch struct */
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount = korl_checkCast_u$_to_u32(textVisibleCharacterCount*6);
    result->_vertexCount      = korl_checkCast_u$_to_u32(textVisibleCharacterCount*4);
    result->_textPixelHeight  = textPixelHeight;
    result->_assetNameFont    = KORL_C_CAST(wchar_t*, result + 1);
    result->_text             = KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, result->_assetNameFont) + assetNameFontBytes);
    result->_vertexIndices    = KORL_C_CAST(Korl_Vulkan_VertexIndex*, KORL_C_CAST(u8*, result->_text) + textBytes);
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*   , KORL_C_CAST(u8*, result->_vertexIndices   ) + textVisibleCharacterCount*6*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexUvs        = KORL_C_CAST(Korl_Vulkan_Uv*         , KORL_C_CAST(u8*, result->_vertexPositions ) + textVisibleCharacterCount*4*sizeof(Korl_Vulkan_Position));
    /* initialize the batch's dynamic data */
    if(    korl_memory_stringCopy(assetNameFont, result->_assetNameFont, assetNameFontBufferSize) 
        != korl_checkCast_u$_to_i$(assetNameFontBufferSize))
        korl_log(ERROR, "failed to copy asset name \"%s\" to batch", assetNameFont);
    if(    korl_memory_stringCopy(text, result->_text, textBufferSize) 
        != korl_checkCast_u$_to_i$(textBufferSize))
        korl_log(ERROR, "failed to copy text \"%s\" to batch", text);
    // the one thing we can store right now without the need for any other data 
    //  is the vertex indices of each glyph quad //
    for(u$ c = 0; c < textVisibleCharacterCount; c++)
    {
        result->_vertexIndices[6*c + 0] = korl_vulkan_safeCast_u$_to_vertexIndex(4*c + 0);
        result->_vertexIndices[6*c + 1] = korl_vulkan_safeCast_u$_to_vertexIndex(4*c + 1);
        result->_vertexIndices[6*c + 2] = korl_vulkan_safeCast_u$_to_vertexIndex(4*c + 3);
        result->_vertexIndices[6*c + 3] = korl_vulkan_safeCast_u$_to_vertexIndex(4*c + 1);
        result->_vertexIndices[6*c + 4] = korl_vulkan_safeCast_u$_to_vertexIndex(4*c + 2);
        result->_vertexIndices[6*c + 5] = korl_vulkan_safeCast_u$_to_vertexIndex(4*c + 3);
    }
    // make an initial attempt to generate the text mesh using the font asset, 
    //  and if the asset isn't available at the moment, there is nothing we can 
    //  do about it for now except defer until a later time //
    _korl_gfx_textLazyGenerateMesh(result);
    /* return the batch */
    return result;
}
korl_internal void korl_gfx_batchSetPosition(Korl_Gfx_Batch*const context, Korl_Vulkan_Position position)
{
    context->_position = position;
}
korl_internal void korl_gfx_batchSetScale(Korl_Gfx_Batch*const context, Korl_Vulkan_Position scale)
{
    context->_scale = scale;
}
korl_internal void korl_gfx_batchSetVertexColor(Korl_Gfx_Batch*const context, u32 vertexIndex, Korl_Vulkan_Color color)
{
    korl_assert(context->_vertexCount > vertexIndex);
    korl_assert(context->_vertexColors);
    context->_vertexColors[vertexIndex] = color;
}
korl_internal void korl_gfx_batchSetLine(Korl_Gfx_Batch*const context, u32 lineIndex, Korl_Vulkan_Position p0, Korl_Vulkan_Position p1, Korl_Vulkan_Color color)
{
    korl_assert(context->_vertexCount > 2*lineIndex + 1);
    korl_assert(context->_vertexColors);
    context->_vertexPositions[2*lineIndex + 0] = p0;
    context->_vertexPositions[2*lineIndex + 1] = p1;
    context->_vertexColors[2*lineIndex + 0] = color;
    context->_vertexColors[2*lineIndex + 1] = color;
}
