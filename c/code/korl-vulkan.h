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
korl_internal void korl_vulkan_createPipeline(void);
