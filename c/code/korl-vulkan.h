#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
#include "korl-math.h"
korl_internal void korl_vulkan_construct(void);
korl_internal void korl_vulkan_destroy(void);
korl_internal void korl_vulkan_createDevice(
    void* createSurfaceUserData, u32 sizeX, u32 sizeY);
korl_internal void korl_vulkan_destroyDevice(void);
#if 0/* @todo: bring these API back later when we want the ability to create 
               pipelines using externally managed resources like shaders */
/* @hack: shader loading should probably be more robust */
/** 
 * This must be called AFTER \c korl_vulkan_createDevice since shader modules 
 * require a device to be created!
 */
korl_internal void korl_vulkan_loadShaders(void);
/* @hack: pipeline creation should probably be a little more intelligent */
korl_internal void korl_vulkan_createPipeline(void);
#endif//0
/**
 * \return the index of the next image in the swapchain
 */
korl_internal u32 korl_vulkan_frameBegin(const f32 clearRgb[3]);
/** submit all batched graphics command buffers to the Vulkan device */
korl_internal void korl_vulkan_frameEnd(u32 nextImageIndex);
/** 
 * Call this whenever the window is resized.  This will trigger a resize of the 
 * swap chain right before the next draw operation in \c korl_vulkan_draw .
 */
korl_internal void korl_vulkan_deferredResize(u32 sizeX, u32 sizeY);
korl_internal void korl_vulkan_batchTriangles_color(
    u32 vertexCount, const Korl_Math_V3f32* positions, 
    const Korl_Math_V3u8* colors);
