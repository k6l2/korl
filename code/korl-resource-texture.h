#pragma once
#include "korl-interface-platform-resource.h"
#include "korl-vulkan.h"
korl_internal void                                      korl_resource_texture_register(void);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_resource_texture_getVulkanDeviceMemoryAllocationHandle(Korl_Resource_Handle handleResourceTexture);
