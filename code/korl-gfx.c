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
#include "utility/korl-utility-algorithm.h"
#include "korl-resource-gfx-buffer.h"
#include "korl-resource-shader.h"
#include "korl-resource-texture.h"
#if defined(_LOCAL_STRING_POOL_POINTER)
#   undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_gfx_context.stringPool))
#define _KORL_GFX_MAX_LIGHTS 8// _technically_ we don't have to have a statically-defined max limit on lights, since we can now just choose to submit the lights as a UBO or a SSBO depending on how much data it is
typedef struct _Korl_Gfx_Context
{
    /** used to store persistent data, such as Font asset glyph cache/database */
    Korl_Memory_AllocatorHandle              allocatorHandle;
    u8                                       nextResourceSalt;
    Korl_StringPool*                         stringPool;// used for Resource database strings; Korl_StringPool structs _must_ be unmanaged allocations (allocations with an unchanging memory address), because we're likely going to have a shit-ton of Strings which point to the pool address for convenience
    Korl_Math_V2u32                          surfaceSize;// updated at the top of each frame, ideally before anything has a chance to use korl-gfx
    Korl_Gfx_Camera                          currentCameraState;
    f32                                      seconds;// passed to the renderer as UBO data to allow shader animations; passed when a Camera is used
    Korl_Resource_Handle                     blankTexture;// a 1x1 texture whose color channels are fully-saturated; can be used as a "default" material map texture
    Korl_Resource_Handle                     resourceShaderKorlVertex2d;
    Korl_Resource_Handle                     resourceShaderKorlVertex2dColor;
    Korl_Resource_Handle                     resourceShaderKorlVertex2dUv;
    Korl_Resource_Handle                     resourceShaderKorlVertex3d;
    Korl_Resource_Handle                     resourceShaderKorlVertex3dColor;
    Korl_Resource_Handle                     resourceShaderKorlVertex3dUv;
    Korl_Resource_Handle                     resourceShaderKorlVertexLit;
    Korl_Resource_Handle                     resourceShaderKorlVertexText;
    Korl_Resource_Handle                     resourceShaderKorlFragmentColor;
    Korl_Resource_Handle                     resourceShaderKorlFragmentColorTexture;
    Korl_Resource_Handle                     resourceShaderKorlFragmentLit;
    KORL_MEMORY_POOL_DECLARE(Korl_Gfx_Light, pendingLights, _KORL_GFX_MAX_LIGHTS);// after being added to this pool, lights are flushed to the renderer's draw state upon the next call to `korl_gfx_draw`
    i32                                      flushedLights;// initialized to -1 at to top of each frame; if < 0 when setDrawState gets called, we will update the proper descriptor so that vulkan doesn't hit a validation error
    Korl_Gfx_Material_FragmentShaderUniform  lastFragmentShaderUniform;
    bool                                     lastFragmentShaderUniformUsed;
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
    _korl_gfx_context->blankTexture = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE), &createInfoBlankTexture, false);
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
    _korl_gfx_context->flushedLights = -1;
    _korl_gfx_context->lastFragmentShaderUniformUsed = false;
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
    context->currentCameraState = camera;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.scissor         = &scissor;
    drawState.sceneProperties = &sceneProperties;
    korl_assert(korl_gfx_setDrawState(&drawState));
    korl_time_probeStop(useCamera);
}
korl_internal KORL_FUNCTION_korl_gfx_camera_getCurrent(korl_gfx_camera_getCurrent)
{
    return _korl_gfx_context->currentCameraState;
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
    korl_assert(korl_gfx_setDrawState(&drawState));
    switch(context->type)
    {
    case KORL_GFX_DRAWABLE_TYPE_INVALID:
        korl_log(ERROR, "invalid drawable");
        break;
    case KORL_GFX_DRAWABLE_TYPE_RUNTIME:{
        if(   context->subType.runtime.vertexStagingMeta.vertexCount == 0 
           && context->subType.runtime.vertexStagingMeta.instanceCount == 0)
        {
            korl_log(WARNING, "empty drawable");
            break;
        }
        /* configure the renderer draw state */
        Korl_Gfx_Material materialLocal;
        if(materials)
        {
            korl_assert(materialsSize == 1);
            materialLocal              = materials[0];
            materialLocal.modes.flags |= context->subType.runtime.overrides.materialModeFlags;
        }
        else
            materialLocal = korl_gfx_material_defaultUnlitFlags(context->subType.runtime.overrides.materialModeFlags);
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
        if(!korl_gfx_setDrawState(&drawState))
            break;
        switch(context->subType.runtime.type)
        {
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_SINGLE_FRAME:
            korl_gfx_drawStagingAllocation(&context->subType.runtime.subType.singleFrame.stagingAllocation, &context->subType.runtime.vertexStagingMeta, context->subType.runtime.primitiveType);
            break;
        case KORL_GFX_DRAWABLE_RUNTIME_TYPE_MULTI_FRAME:{
            korl_gfx_drawVertexBuffer(context->subType.runtime.subType.multiFrame.resourceHandleBuffer, 0, &context->subType.runtime.vertexStagingMeta, context->subType.runtime.primitiveType);
            break;}
        }
        break;}
    case KORL_GFX_DRAWABLE_TYPE_MESH:{
        if(!context->subType.mesh.meshPrimitives && korl_resource_isLoaded(context->subType.mesh.resourceHandleScene3d))
        {
            /* the user has the option of initializing a MESH Drawable with either a meshIndex or a meshName; 
                we can obtain all the rest of the mesh data with either of these */
            if(context->subType.mesh.rawUtf8Scene3dMeshNameSize)
                context->subType.mesh.meshIndex = korl_resource_scene3d_getMeshIndex(context->subType.mesh.resourceHandleScene3d
                                                                                    ,(acu8){.size = context->subType.mesh.rawUtf8Scene3dMeshNameSize
                                                                                           ,.data = context->subType.mesh.rawUtf8Scene3dMeshName});
            else
            {
                const acu8 meshName = korl_resource_scene3d_getMeshName(context->subType.mesh.resourceHandleScene3d, context->subType.mesh.meshIndex);
                korl_assert(meshName.size < korl_arraySize(context->subType.mesh.rawUtf8Scene3dMeshName) - 1/*leave room for null-terminator*/);
                context->subType.mesh.rawUtf8Scene3dMeshNameSize = korl_checkCast_u$_to_u8(meshName.size);
                korl_memory_copy(context->subType.mesh.rawUtf8Scene3dMeshName, meshName.data, meshName.size * sizeof(*meshName.data));
                context->subType.mesh.rawUtf8Scene3dMeshName[meshName.size] = '\0';
            }
            const u32 meshPrimitiveCount = korl_resource_scene3d_getMeshPrimitiveCount(context->subType.mesh.resourceHandleScene3d, context->subType.mesh.meshIndex);
            context->subType.mesh.meshPrimitives = korl_checkCast_u$_to_u8(meshPrimitiveCount);
        }
        if(!context->subType.mesh.meshPrimitives)
            break;// if the scene3d is _still_ not yet loaded, we can't draw anything
        if(context->subType.mesh.skin)
        {
            /* if we are skinning a mesh, we need to 
                - prepare a descriptor staging allocation for the bone transform matrix array
                - update the bone matrices with respect to their parents using topological order
                - multiply each bone matrix by their respective inverseBindMatrix
                - set the appropriate vulkan DrawState to use the bone descriptor staging allocation */
            /* allocate descriptor staging memory for bone matrices */
            const Korl_Vulkan_DescriptorStagingAllocation descriptorStagingAllocationBones = korl_vulkan_stagingAllocateDescriptorData(context->subType.mesh.skin->bonesSize * sizeof(Korl_Math_M4f32));
            Korl_Math_M4f32*const boneMatrices = descriptorStagingAllocationBones.data;
            /* compute bone matrices with respect to topological order */
            const i32*const                                        boneParentIndices    = korl_resource_scene3d_skin_getBoneParentIndices(context->subType.mesh.resourceHandleScene3d, context->subType.mesh.skin->skinIndex);
            const Korl_Algorithm_GraphDirected_SortedElement*const boneTopologicalOrder = korl_resource_scene3d_skin_getBoneTopologicalOrder(context->subType.mesh.resourceHandleScene3d, context->subType.mesh.skin->skinIndex);
            {/* we treat the (topologically) first bone's computation specially, since it _must_ be the root bone */
                korl_assert(context->subType.mesh.skin->bonesSize > 0);// ensure there is at least one bone in the skin
                const Korl_Algorithm_GraphDirected_SortedElement boneRoot = boneTopologicalOrder[0];
                korl_assert(boneParentIndices[boneRoot.index] < 0);// we expect the first bone in topological order to have _no_ parent, as it _must_ be the common root node
                korl_assert(context->transform._m4f32IsUpdated);// this should have been updated at the top of this function
                korl_math_transform3d_updateM4f32(&context->subType.mesh.skin->bones[boneRoot.index]);
                boneMatrices[boneRoot.index] = korl_math_m4f32_multiply(&context->transform._m4f32, &context->subType.mesh.skin->bones[boneRoot.index]._m4f32);// the root bone node can simply be the local transform with context's model xform applied, preventing the need to apply the model xform to all vertices in the shader
            }
            for(u32 bt = 1; bt < context->subType.mesh.skin->bonesSize; bt++)
            {
                const Korl_Algorithm_GraphDirected_SortedElement b  = boneTopologicalOrder[bt];// index of the current bone to process
                const u32                                        bp = boneParentIndices[b.index];    // index of the current bone's parent
                korl_assert(boneParentIndices[b.index] >= 0);// we expect _all_ bones after the root to have a parent
                korl_assert(context->subType.mesh.skin->bones[bp]._m4f32IsUpdated);// we're processing bones in topological order; our parent's transform matrix better be updated by now!
                korl_math_transform3d_updateM4f32(&context->subType.mesh.skin->bones[b.index]);
                boneMatrices[b.index] = korl_math_m4f32_multiply(boneMatrices + bp, &context->subType.mesh.skin->bones[b.index]._m4f32);
            }
            /* pre-multiply bone matrices with their respective inverseBindMatrix */
            const Korl_Math_M4f32*const boneInverseBindMatrices = korl_resource_scene3d_skin_getBoneInverseBindMatrices(context->subType.mesh.resourceHandleScene3d, context->subType.mesh.skin->skinIndex);
            for(u32 b = 0; b < context->subType.mesh.skin->bonesSize; b++)
                boneMatrices[b] = korl_math_m4f32_multiply(boneMatrices + b, boneInverseBindMatrices + b);
            /* apply the bone matrix descriptor to vulkan DrawState */
            KORL_ZERO_STACK(Korl_Vulkan_DrawState, vulkanDrawState);
            vulkanDrawState.uboVertex[1] = descriptorStagingAllocationBones;
            korl_vulkan_setDrawState(&vulkanDrawState);
        }
        for(u8 i = 0; i < context->subType.mesh.meshPrimitives; i++)
        {
            Korl_Resource_Scene3d_MeshPrimitive meshPrimitive = korl_resource_scene3d_getMeshPrimitive(context->subType.mesh.resourceHandleScene3d, context->subType.mesh.meshIndex, i);
            KORL_ZERO_STACK(Korl_Gfx_DrawState, drawStatePrimitive);
            drawStatePrimitive.material = (materialsSize && i < materialsSize) ? (materials + i) : (&meshPrimitive.material);
            if(!korl_gfx_setDrawState(&drawStatePrimitive))
                continue;
            korl_gfx_drawVertexBuffer(meshPrimitive.vertexBuffer, meshPrimitive.vertexBufferByteOffset, &meshPrimitive.vertexStagingMeta, meshPrimitive.primitiveType);
        }
        break;}
    }
}
korl_internal KORL_FUNCTION_korl_gfx_setDrawState(korl_gfx_setDrawState)
{
    //KORL-ISSUE-000-000-185: gfx: manage custom descriptor set layouts; here, we are currently manually selecting arbitrary descriptor data channels to propagate various data, which looks & feels very hacky; we should be able to modify korl-vulkan to allow us to create & manage our own custom descriptor set & pipeline layouts for specific rendering purposes; related to the todo item currently found in _korl_vulkan_flushDescriptors; related to KORL-ISSUE-000-000-184
    KORL_ZERO_STACK(Korl_Vulkan_DrawState, vulkanDrawState);
    if(drawState->material)
    {
        if(drawState->material->shaders.resourceHandleShaderGeometry)
            vulkanDrawState.shaderGeometry = korl_resource_shader_getHandle(drawState->material->shaders.resourceHandleShaderGeometry);
            // let's assume, for now, that geometry shaders are _optional_ for drawing
        if(drawState->material->shaders.resourceHandleShaderVertex)
        {
            vulkanDrawState.shaderVertex = korl_resource_shader_getHandle(drawState->material->shaders.resourceHandleShaderVertex);
            if(0 == vulkanDrawState.shaderVertex)
                return false;// signal to the caller that we cannot draw with the current state
        }
        if(drawState->material->shaders.resourceHandleShaderFragment)
        {
            vulkanDrawState.shaderFragment = korl_resource_shader_getHandle(drawState->material->shaders.resourceHandleShaderFragment);
            if(0 == vulkanDrawState.shaderFragment)
                return false;// signal to the caller that we cannot draw with the current state
        }
        vulkanDrawState.materialModes = &drawState->material->modes;
        if(   !_korl_gfx_context->lastFragmentShaderUniformUsed
           || 0 != korl_memory_compare(&_korl_gfx_context->lastFragmentShaderUniform, &drawState->material->fragmentShaderUniform, sizeof(drawState->material->fragmentShaderUniform)))
        {
            _korl_gfx_context->lastFragmentShaderUniform     = drawState->material->fragmentShaderUniform;
            _korl_gfx_context->lastFragmentShaderUniformUsed = true;
            vulkanDrawState.uboFragment[0] = korl_vulkan_stagingAllocateDescriptorData(sizeof(drawState->material->fragmentShaderUniform));
            *KORL_C_CAST(Korl_Gfx_Material_FragmentShaderUniform*, vulkanDrawState.uboFragment[0].data) = drawState->material->fragmentShaderUniform;
        }
        if(drawState->material->maps.resourceHandleTextureBase)
            vulkanDrawState.texture2dFragment[0] = korl_resource_texture_getVulkanDeviceMemoryAllocationHandle(drawState->material->maps.resourceHandleTextureBase);
        if(drawState->material->maps.resourceHandleTextureSpecular)
            vulkanDrawState.texture2dFragment[1] = korl_resource_texture_getVulkanDeviceMemoryAllocationHandle(drawState->material->maps.resourceHandleTextureSpecular);
        if(drawState->material->maps.resourceHandleTextureEmissive)
            vulkanDrawState.texture2dFragment[2] = korl_resource_texture_getVulkanDeviceMemoryAllocationHandle(drawState->material->maps.resourceHandleTextureEmissive);
    }
    vulkanDrawState.scissor          = drawState->scissor;
    vulkanDrawState.pushConstantData = drawState->pushConstantData;
    if(drawState->sceneProperties)
    {
        vulkanDrawState.uboVertex[0] = korl_vulkan_stagingAllocateDescriptorData(sizeof(*drawState->sceneProperties));
        *KORL_C_CAST(Korl_Gfx_DrawState_SceneProperties*, vulkanDrawState.uboVertex[0].data) = *drawState->sceneProperties;
    }
    if(drawState->storageBuffers)
    {
        if(drawState->storageBuffers->resourceHandleVertex)
        {
            const Korl_Vulkan_DeviceMemory_AllocationHandle deviceMemoryAllocation = korl_resource_gfxBuffer_getVulkanDeviceMemoryAllocationHandle(drawState->storageBuffers->resourceHandleVertex);
            vulkanDrawState.ssboVertex[0].descriptorBufferInfo = korl_vulkan_buffer_getDescriptorBufferInfo(deviceMemoryAllocation);
        }
    }
    if(!KORL_MEMORY_POOL_ISEMPTY(_korl_gfx_context->pendingLights) || _korl_gfx_context->flushedLights < 0)
    {
        typedef struct _Korl_Vulkan_ShaderBuffer_Lights
        {
            u32            lightsSize;
            u32            _padding_0[3];
            Korl_Gfx_Light lights[1];// array size == `lightsSize`
        } _Korl_Vulkan_ShaderBuffer_Lights;
        const VkDeviceSize bufferBytes = sizeof(_Korl_Vulkan_ShaderBuffer_Lights) - sizeof(Korl_Gfx_Light) + KORL_MEMORY_POOL_SIZE(_korl_gfx_context->pendingLights) * sizeof(Korl_Gfx_Light);
        vulkanDrawState.ssboFragment[0] = korl_vulkan_stagingAllocateDescriptorData(bufferBytes);
        _Korl_Vulkan_ShaderBuffer_Lights*const stagingMemoryBufferLights = vulkanDrawState.ssboFragment[0].data;
        stagingMemoryBufferLights->lightsSize = KORL_MEMORY_POOL_SIZE(_korl_gfx_context->pendingLights);
        _korl_gfx_context->flushedLights = stagingMemoryBufferLights->lightsSize;
        korl_memory_copy(stagingMemoryBufferLights->lights, _korl_gfx_context->pendingLights, KORL_MEMORY_POOL_SIZE(_korl_gfx_context->pendingLights) * sizeof(Korl_Gfx_Light));
        /* transform all the light positions & directions into view-space, as that is what our lighting shaders require */
        if(   korl_math_isNearlyEqual(korl_math_v3f32_magnitudeSquared(&_korl_gfx_context->currentCameraState.normalForward), 1)
           && korl_math_isNearlyEqual(korl_math_v3f32_magnitudeSquared(&_korl_gfx_context->currentCameraState.normalUp     ), 1))
        {
            const Korl_Math_M4f32 m4f32View = korl_gfx_camera_view(&_korl_gfx_context->currentCameraState);
            for(Korl_MemoryPool_Size l = 0; l < KORL_MEMORY_POOL_SIZE(_korl_gfx_context->pendingLights); l++)
            {
                Korl_Gfx_Light*const light = stagingMemoryBufferLights->lights + l;
                light->position  = korl_math_m4f32_multiplyV4f32Copy(&m4f32View, (Korl_Math_V4f32){.xyz = light->position, 1}).xyz;
                light->direction = korl_math_m4f32_multiplyV4f32Copy(&m4f32View, (Korl_Math_V4f32){.xyz = light->direction, 0}).xyz;
            }
            _korl_gfx_context->pendingLights_korlMemoryPoolSize = 0;// does not destroy current lighting data, which is exactly what we need for the remainder of this stack!; future Kyle here: why do we need to not destroy the data?...🤔
        }
    }
    korl_vulkan_setDrawState(&vulkanDrawState);
    return true;
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
    korl_vulkan_drawStagingAllocation(stagingAllocation, stagingMeta, primitiveType);
}
korl_internal KORL_FUNCTION_korl_gfx_drawVertexBuffer(korl_gfx_drawVertexBuffer)
{
    if(!resourceHandleBuffer)
        return;
    const Korl_Vulkan_DeviceMemory_AllocationHandle bufferDeviceMemoryAllocationHandle = korl_resource_gfxBuffer_getVulkanDeviceMemoryAllocationHandle(resourceHandleBuffer);
    if(!bufferDeviceMemoryAllocationHandle)
        return;
    korl_vulkan_drawVertexBuffer(bufferDeviceMemoryAllocationHandle, bufferByteOffset, stagingMeta, primitiveType);
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
