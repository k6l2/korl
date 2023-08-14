#include "korl-gfx.h"
#include "korl-log.h"
#include "korl-assetCache.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-vulkan.h"
#include "korl-time.h"
#include "korl-stb-ds.h"
#include "korl-stb-image.h"
#include "korl-resource.h"
#include "utility/korl-stringPool.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-utility-gfx.h"
#include "utility/korl-utility-resource.h"
#include "korl-resource-gfx-buffer.h"
#if defined(_LOCAL_STRING_POOL_POINTER)
#   undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_gfx_context.stringPool))
typedef struct _Korl_Gfx_Context
{
    /** used to store persistent data, such as Font asset glyph cache/database */
    Korl_Memory_AllocatorHandle allocatorHandle;
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
korl_global_variable _Korl_Gfx_Context* _korl_gfx_context;
korl_internal void korl_gfx_initialize(void)
{
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(8);
    const Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-gfx", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    _korl_gfx_context = korl_allocate(allocator, sizeof(*_korl_gfx_context));
    _korl_gfx_context->allocatorHandle = allocator;
    _korl_gfx_context->stringPool      = korl_allocate(allocator, sizeof(*_korl_gfx_context->stringPool));// allocate this ASAP to reduce fragmentation, since this struct _must_ remain UNMANAGED
    *_korl_gfx_context->stringPool = korl_stringPool_create(_korl_gfx_context->allocatorHandle);
}
korl_internal void korl_gfx_initializePostRendererLogicalDevice(void)
{
    /* why don't we do this in korl_gfx_initialize?  It's because when 
        korl_gfx_initialize happens, korl-vulkan is not yet fully initialized!  
        we can't actually configure graphics device assets when the graphics 
        device has not even been created */
    KORL_ZERO_STACK(Korl_Resource_Texture_CreateInfo, createInfoBlankTexture);
    createInfoBlankTexture.formatImage = KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8A8_UNORM;
    createInfoBlankTexture.size        = KORL_MATH_V2U32_ONE;
    _korl_gfx_context->blankTexture = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE), &createInfoBlankTexture);
    const Korl_Gfx_Color4u8 blankTextureColor = KORL_COLOR4U8_WHITE;
    korl_resource_update(_korl_gfx_context->blankTexture, &blankTextureColor, sizeof(blankTextureColor), 0);
    /* initiate loading of all built-in shaders */
    _korl_gfx_context->resourceShaderKorlVertex2d             = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-2d.vert.spv"           ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex2dColor        = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-2d-color.vert.spv"     ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex2dUv           = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-2d-uv.vert.spv"        ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex3d             = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-3d.vert.spv"           ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex3dColor        = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-3d-color.vert.spv"     ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertex3dUv           = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-3d-uv.vert.spv"        ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertexLit            = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.vert.spv"          ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlVertexText           = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-text.vert.spv"         ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlFragmentColor        = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-color.frag.spv"        ), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlFragmentColorTexture = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-color-texture.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY);
    _korl_gfx_context->resourceShaderKorlFragmentLit          = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER), KORL_RAW_CONST_UTF8("build/shaders/korl-lit.frag.spv"          ), KORL_ASSETCACHE_GET_FLAG_LAZY);
}
korl_internal void korl_gfx_update(Korl_Math_V2u32 surfaceSize, f32 deltaSeconds)
{
    _korl_gfx_context->surfaceSize = surfaceSize;
    _korl_gfx_context->seconds    += deltaSeconds;
    KORL_MEMORY_POOL_EMPTY(_korl_gfx_context->pendingLights);
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
    case KORL_GFX_DRAWABLE_TYPE_RUNTIME:{
        /* configure the renderer draw state */
        Korl_Gfx_Material materialLocal;
        if(materials)
        {
            korl_assert(materialsSize == 1);
            materialLocal                      = materials[0];
            materialLocal.modes.primitiveType  = context->subType.runtime.overrides.primitiveType;
            materialLocal.modes.flags         |= context->subType.runtime.overrides.materialModeFlags;
        }
        else
            materialLocal = korl_gfx_material_defaultUnlit(context->subType.runtime.overrides.primitiveType, context->subType.runtime.overrides.materialModeFlags, korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
        if(context->subType.runtime.overrides.materialMapBase)
            materialLocal.maps.resourceHandleTextureBase = context->subType.runtime.overrides.materialMapBase;
        if(context->subType.runtime.overrides.shaderVertex)
            materialLocal.shaders.resourceHandleShaderVertex = context->subType.runtime.overrides.shaderVertex;
        else if(!materialLocal.shaders.resourceHandleShaderVertex)
            materialLocal.shaders.resourceHandleShaderVertex = korl_gfx_getBuiltInShaderVertex(&context->subType.runtime.vertexStagingMeta);
        if(context->subType.runtime.overrides.shaderFragment)
            materialLocal.shaders.resourceHandleShaderFragment = context->subType.runtime.overrides.shaderFragment;
        else if(!materialLocal.shaders.resourceHandleShaderFragment)
            materialLocal.shaders.resourceHandleShaderFragment = korl_gfx_getBuiltInShaderFragment(&materialLocal);
        KORL_ZERO_STACK(Korl_Gfx_DrawState_StorageBuffers, storageBuffers);
        storageBuffers.resourceHandleVertex = context->subType.runtime.overrides.storageBufferVertex;
        drawState = KORL_STRUCT_INITIALIZE_ZERO(Korl_Gfx_DrawState);
        if(context->subType.runtime.overrides.storageBufferVertex)
            drawState.storageBuffers = &storageBuffers;
        drawState.material         = &materialLocal;
        drawState.pushConstantData = context->subType.runtime.overrides.pushConstantData;
        korl_vulkan_setDrawState(&drawState);
        switch(context->subType.runtime.type)
        {
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME:
            korl_gfx_drawStagingAllocation(&context->subType.runtime.subType.singleFrame.stagingAllocation, &context->subType.runtime.vertexStagingMeta);
            break;
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME:{
            korl_gfx_drawVertexBuffer(context->subType.runtime.subType.multiFrame.resourceHandleBuffer, 0, &context->subType.runtime.vertexStagingMeta);
            break;}
        }
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
korl_internal KORL_FUNCTION_korl_gfx_drawVertexBuffer(korl_gfx_drawVertexBuffer)
{
    const Korl_Vulkan_DeviceMemory_AllocationHandle bufferDeviceMemoryAllocationHandle = korl_resource_gfxBuffer_getVulkanDeviceMemoryAllocationHandle(resourceHandleBuffer);
    korl_vulkan_drawVertexBuffer(bufferDeviceMemoryAllocationHandle, bufferByteOffset, stagingMeta);
}
korl_internal KORL_FUNCTION_korl_gfx_getBuiltInShaderVertex(korl_gfx_getBuiltInShaderVertex)
{
    if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32
       && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].vectorSize  == 1)
        return _korl_gfx_context->resourceShaderKorlVertexText;
    if(vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize == 2)
        if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
           && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex2dColor;
        else if(   vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
                && vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            return _korl_gfx_context->resourceShaderKorlVertex2dUv;
        else
            return _korl_gfx_context->resourceShaderKorlVertex2d;
    if(vertexStagingMeta->vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize == 3)
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
}
#undef _LOCAL_STRING_POOL_POINTER
