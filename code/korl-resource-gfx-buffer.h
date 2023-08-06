#pragma once
#include "korl-interface-platform-resource.h"
#include "korl-vulkan.h"
korl_internal void                                      korl_resource_gfxBuffer_register(void);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_resource_gfxBuffer_getVulkanDeviceMemoryAllocationHandle(Korl_Resource_Handle handleResourceGfxBuffer);
