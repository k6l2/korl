/**
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
#define KORL_VULKAN_MAX_LIGHTS 8
typedef u64 Korl_Vulkan_DeviceMemory_AllocationHandle;
typedef u64 Korl_Vulkan_ShaderHandle;
typedef struct Korl_Vulkan_CreateInfoShader
{
    const void* data;
    u$          dataBytes;
} Korl_Vulkan_CreateInfoShader;
korl_internal void                                      korl_vulkan_construct(void);
korl_internal void                                      korl_vulkan_destroy(void);
korl_internal void                                      korl_vulkan_createSurface(void* createSurfaceUserData, u32 sizeX, u32 sizeY);
korl_internal void                                      korl_vulkan_destroySurface(void);
korl_internal Korl_Math_V2u32                           korl_vulkan_getSurfaceSize(void);
korl_internal void                                      korl_vulkan_clearAllDeviceAllocations(void);
korl_internal void                                      korl_vulkan_setSurfaceClearColor(const f32 clearRgb[3]);
korl_internal void                                      korl_vulkan_frameEnd(void);
korl_internal void                                      korl_vulkan_deferredResize(u32 sizeX, u32 sizeY);
korl_internal void                                      korl_vulkan_setDrawState(const Korl_Gfx_DrawState* state);
korl_internal Korl_Gfx_StagingAllocation                korl_vulkan_stagingAllocate(const Korl_Gfx_VertexStagingMeta* stagingMeta);
korl_internal Korl_Gfx_StagingAllocation                korl_vulkan_stagingReallocate(const Korl_Gfx_VertexStagingMeta* stagingMeta, const Korl_Gfx_StagingAllocation* stagingAllocation);
korl_internal void                                      korl_vulkan_drawStagingAllocation(const Korl_Gfx_StagingAllocation* stagingAllocation, const Korl_Gfx_VertexStagingMeta* stagingMeta);
korl_internal void                                      korl_vulkan_drawVertexBuffer(Korl_Vulkan_DeviceMemory_AllocationHandle vertexBuffer, u$ vertexBufferByteOffset, const Korl_Gfx_VertexStagingMeta* stagingMeta);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createTexture(const Korl_Resource_Texture_CreateInfo* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createBuffer(const Korl_Resource_GfxBuffer_CreateInfo* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle);
korl_internal void                                      korl_vulkan_deviceAsset_destroy(Korl_Vulkan_DeviceMemory_AllocationHandle deviceAssetHandle);
korl_internal void                                      korl_vulkan_texture_update(Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle, const Korl_Gfx_Color4u8* pixelData);
korl_internal Korl_Math_V2u32                           korl_vulkan_texture_getSize(const Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle);
korl_internal void                                      korl_vulkan_buffer_resize(Korl_Vulkan_DeviceMemory_AllocationHandle* in_out_bufferHandle, u$ bytes);
korl_internal void                                      korl_vulkan_buffer_update(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle, const void* data, u$ dataBytes, u$ deviceLocalBufferOffset);
korl_internal void*                                     korl_vulkan_buffer_getStagingBuffer(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle, u$ bytes, u$ bufferByteOffset);
korl_internal Korl_Vulkan_ShaderHandle                  korl_vulkan_shader_create(const Korl_Vulkan_CreateInfoShader* createInfo, Korl_Vulkan_ShaderHandle requiredHandle);
korl_internal void                                      korl_vulkan_shader_destroy(Korl_Vulkan_ShaderHandle shaderHandle);
