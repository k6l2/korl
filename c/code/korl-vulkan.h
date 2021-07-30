#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
korl_internal void korl_vulkan_construct(void);
korl_internal void korl_vulkan_destroy(void);
korl_internal void korl_vulkan_createDevice(VkSurfaceKHR surface);
/** This API is platform-specific, and thus must be defined within the code base 
 * of whatever the current target platform is. */
korl_internal VkSurfaceKHR korl_vulkan_createSurface(void* userData);
korl_internal void korl_vulkan_destroySurface(void);
korl_internal void korl_vulkan_createSwapChain(u32 sizeX, u32 sizeY);
korl_internal void korl_vulkan_destroySwapChain(void);
/* @hack: shader loading should probably be more robust */
/** This must be called AFTER \c korl_vulkan_createDevice since shader modules 
 * require a device to be created! */
korl_internal void korl_vulkan_loadShaders(void);
/* @hack: pipeline creation should probably be a little more intelligent */
korl_internal void korl_vulkan_createPipeline(void);
/* @hack: record a simple set of commands to draw the triangle for each of 
    the command buffers.  In a more fleshed out application, a new set of 
    commands would likely get recorded each from for the next image in the 
    swap chain! */
korl_internal void korl_vulkan_recordAllCommandBuffers(void);
korl_internal void korl_vulkan_draw(void);
/** Call this whenever the window is resized.  This will trigger a resize of the 
 * swap chain right before the next draw operation in \c korl_vulkan_draw . */
korl_internal void korl_vulkan_deferredResize(u32 sizeX, u32 sizeY);
