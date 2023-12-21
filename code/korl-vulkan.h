/** @TODO: update this API documentation
 * ----- API -----
 * korl_vulkan_safeCast_u$_to_vertexIndex
 * korl_vulkan_construct
 * korl_vulkan_destroy
 * korl_vulkan_createSurface
 *     Not to be confused with \c _korl_vulkan_createSurface (an internal API to 
 *     create an actual Vulkan Surface resource)!  "Surface" here refers to an 
 *     entire rendering region on the screen.  A common use case is to create a 
 *     desktop window, and then create a KORL Vulkan Surface to fill the entire 
 *     region of the window.
 * korl_vulkan_destroySurface
 * korl_vulkan_getSurfaceSize
 * korl_vulkan_clearAllDeviceAllocations
 *     Just to clarify here, calling this function doesn't actually destroy the 
 *     device allocations; it simply allows us to create new allocations which 
 *     can potentially occupy the same place in memory as well as possess the 
 *     same allocation handle.
 * korl_vulkan_setSurfaceClearColor
 * korl_vulkan_frameBegin
 * korl_vulkan_frameEnd
 * korl_vulkan_deferredResize
 *     Call this whenever the window is resized.  This will trigger a resize of the 
 *     swap chain right before the next draw operation in \c korl_vulkan_draw .
 * korl_vulkan_setDrawState
 * korl_vulkan_draw
 * korl_vulkan_allocateStaging
 *     When the user wants to generate their own vertex data each frame on the 
 *     CPU, there's no point in managing their own memory buffers to accomplish 
 *     this.  Instead, they can call this function to allocate CPU-visible 
 *     Vulkan buffer memory, write whatever vertex data they want directly to 
 *     the returned memory address, then draw the data however they want via a 
 *     call to \c korl_vulkan_drawStagingAllocation .
 * korl_vulkan_drawStagingAllocation
 * ----- Data Structures -----
 * Korl_Vulkan_DrawVertexData
 * Korl_Vulkan_DrawState
 */
#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
#include "utility/korl-utility-math.h"
#include "utility/korl-checkCast.h"
#include "korl-memory.h"
#include "korl-assetCache.h"
#include "korl-interface-platform-gfx.h"
#include "utility/korl-utility-resource.h"
#include "utility/korl-pool.h"
typedef u64              Korl_Vulkan_DeviceMemory_AllocationHandle;// 0 => NULL/invalid, as usual
typedef u64              Korl_Vulkan_ShaderHandle;                 // 0 => NULL/invalid, as usual
typedef Korl_Pool_Handle Korl_Vulkan_RenderPassHandle;             // 0 => NULL/invalid, as usual
typedef struct Korl_Vulkan_CreateInfoShader
{
    const void* data;
    u$          dataBytes;
} Korl_Vulkan_CreateInfoShader;
/** very similar to \c Korl_Gfx_StagingAllocation , but we have different 
 * alignment requirements */
typedef struct Korl_Vulkan_DescriptorStagingAllocation
{
    void*                  data;
    VkDescriptorBufferInfo descriptorBufferInfo;
} Korl_Vulkan_DescriptorStagingAllocation;
#if 0//@TODO: all of this kind of state should be tracked by individual RenderPasses
#define KORL_VULKAN_MAX_UBOS_VERTEX        2
#define KORL_VULKAN_MAX_UBOS_FRAGMENT      1
#define KORL_VULKAN_MAX_SSBOS_VERTEX       1
#define KORL_VULKAN_MAX_SSBOS_FRAGMENT     1
#define KORL_VULKAN_MAX_TEXTURE2D_FRAGMENT 3
typedef struct Korl_Vulkan_DrawState
{
    Korl_Vulkan_ShaderHandle                   shaderGeometry;
    Korl_Vulkan_ShaderHandle                   shaderVertex;
    Korl_Vulkan_ShaderHandle                   shaderFragment;
    const Korl_Gfx_Material_Modes*             materialModes;
    const Korl_Gfx_DrawState_Scissor*          scissor;
    const Korl_Gfx_DrawState_PushConstantData* pushConstantData;                                     // example: model/world transform
    Korl_Vulkan_DescriptorStagingAllocation    uboVertex[KORL_VULKAN_MAX_UBOS_VERTEX];               // example: SceneProperties (VP transform, seconds, etc.)
    Korl_Vulkan_DescriptorStagingAllocation    uboFragment[KORL_VULKAN_MAX_UBOS_FRAGMENT];           // example: material UBO
    Korl_Vulkan_DescriptorStagingAllocation    ssboVertex[KORL_VULKAN_MAX_SSBOS_VERTEX];             // example: glyph mesh vertex cache
    Korl_Vulkan_DescriptorStagingAllocation    ssboFragment[KORL_VULKAN_MAX_SSBOS_FRAGMENT];         // example: lighting
    Korl_Vulkan_DeviceMemory_AllocationHandle  texture2dFragment[KORL_VULKAN_MAX_TEXTURE2D_FRAGMENT];// example: material maps (base, specular, emissive)
} Korl_Vulkan_DrawState;
#endif
typedef struct Korl_Resource_Texture_CreateInfo Korl_Resource_Texture_CreateInfo;
korl_internal void                                      korl_vulkan_construct(void);
korl_internal void                                      korl_vulkan_destroy(void);
korl_internal void                                      korl_vulkan_createSurface(void* createSurfaceUserData, u32 sizeX, u32 sizeY);
korl_internal void                                      korl_vulkan_destroySurface(void);
korl_internal Korl_Math_V2u32                           korl_vulkan_getSurfaceSize(void);
korl_internal void                                      korl_vulkan_clearAllDeviceAllocations(void);
korl_internal void                                      korl_vulkan_setSurfaceClearColor(const f32 clearRgb[3]);
korl_internal void                                      korl_vulkan_frameEnd(void);
korl_internal void                                      korl_vulkan_deferredResize(u32 sizeX, u32 sizeY);
korl_internal Korl_Vulkan_DescriptorStagingAllocation   korl_vulkan_stagingAllocateDescriptorData(u$ bytes);// used for allocation of UBO & SSBO data
korl_internal Korl_Gfx_StagingAllocation                korl_vulkan_stagingAllocate(const Korl_Gfx_VertexStagingMeta* stagingMeta);
korl_internal Korl_Gfx_StagingAllocation                korl_vulkan_stagingReallocate(const Korl_Gfx_VertexStagingMeta* stagingMeta, const Korl_Gfx_StagingAllocation* stagingAllocation);
#if 0//@TODO: delete or update to support specific render passes
korl_internal void                                      korl_vulkan_setDrawState(const Korl_Vulkan_DrawState* state);
#endif
//@TODO: delete or update to support specific render passes
korl_internal void                                      korl_vulkan_drawStagingAllocation(const Korl_Gfx_StagingAllocation* stagingAllocation, const Korl_Gfx_VertexStagingMeta* stagingMeta, Korl_Gfx_Material_PrimitiveType primitiveType);
//@TODO: delete or update to support specific render passes
korl_internal void                                      korl_vulkan_drawVertexBuffer(Korl_Vulkan_DeviceMemory_AllocationHandle vertexBuffer, u$ vertexBufferByteOffset, const Korl_Gfx_VertexStagingMeta* stagingMeta, Korl_Gfx_Material_PrimitiveType primitiveType);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createTexture(const Korl_Resource_Texture_CreateInfo* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createBuffer(const Korl_Resource_GfxBuffer_CreateInfo* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle);
korl_internal void                                      korl_vulkan_deviceAsset_destroy(Korl_Vulkan_DeviceMemory_AllocationHandle deviceAssetHandle);
korl_internal void                                      korl_vulkan_texture_update(Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle, const void* pixelData);
korl_internal Korl_Math_V2u32                           korl_vulkan_texture_getSize(const Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle);
korl_internal void                                      korl_vulkan_buffer_resize(Korl_Vulkan_DeviceMemory_AllocationHandle* in_out_bufferHandle, u$ bytes);
korl_internal void                                      korl_vulkan_buffer_update(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle, const void* data, u$ dataBytes, u$ deviceLocalBufferOffset);
korl_internal void*                                     korl_vulkan_buffer_getStagingBuffer(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle, u$ bytes, u$ bufferByteOffset);
korl_internal VkDescriptorBufferInfo                    korl_vulkan_buffer_getDescriptorBufferInfo(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle);
korl_internal Korl_Vulkan_ShaderHandle                  korl_vulkan_shader_create(const Korl_Vulkan_CreateInfoShader* createInfo, Korl_Vulkan_ShaderHandle requiredHandle);
korl_internal void                                      korl_vulkan_shader_destroy(Korl_Vulkan_ShaderHandle shaderHandle);
//@TODO: move all these enums/structs into korl-platform-gfx
typedef enum Korl_Gfx_ImageFormat
{
    KORL_GFX_IMAGE_FORMAT_UNDEFINED,
    KORL_GFX_IMAGE_FORMAT_B8G8R8A8_SRGB,
} Korl_Gfx_ImageFormat;
//@TODO: do we need this?
// typedef enum Korl_Gfx_ImageLayout
// {
//     KORL_GFX_IMAGE_LAYOUT_UNDEFINED,
//     KORL_GFX_IMAGE_LAYOUT_PRESENT_SOURCE,
//     KORL_GFX_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT,
// } Korl_Gfx_ImageLayout;
typedef enum Korl_Gfx_RenderPass_Attachment_LoadOperation
{
    KORL_GFX_RENDER_PASS_ATTACHMENT_LOAD_OPERATION_LOAD,
    KORL_GFX_RENDER_PASS_ATTACHMENT_LOAD_OPERATION_DONT_CARE,
    KORL_GFX_RENDER_PASS_ATTACHMENT_LOAD_OPERATION_CLEAR,
} Korl_Gfx_RenderPass_Attachment_LoadOperation;
typedef enum Korl_Gfx_RenderPass_Attachment_StoreOperation
{
    KORL_GFX_RENDER_PASS_ATTACHMENT_STORE_OPERATION_STORE,
    KORL_GFX_RENDER_PASS_ATTACHMENT_STORE_OPERATION_DONT_CARE,
} Korl_Gfx_RenderPass_Attachment_StoreOperation;
typedef struct Korl_Gfx_RenderPass_Attachment
{
    Korl_Gfx_ImageFormat                          imageFormat;
    //@TODO: do we need this?
    // Korl_Gfx_ImageLayout                          imageLayout;// the "input" layout of the attached image; "output" layout can be determined by the RenderGraph based on where the attachment is hooked to as an input
    Korl_Gfx_RenderPass_Attachment_LoadOperation  loadOperation;
    Korl_Gfx_RenderPass_Attachment_StoreOperation storeOperation;
} Korl_Gfx_RenderPass_Attachment;
typedef struct Korl_Gfx_RenderPass
{
    Korl_Gfx_RenderPass_Attachment* attachments;
    u8                              attachmentsSize;
} Korl_Gfx_RenderPass;
korl_internal Korl_Vulkan_RenderPassHandle              korl_vulkan_renderGraph_addRenderPass(const Korl_Gfx_RenderPass* renderPass);
korl_internal void                                      korl_vulkan_renderGraph_build(void);// initializes all previously generated Korl_Vulkan_RenderPassHandles such that commands can be recorded for them
