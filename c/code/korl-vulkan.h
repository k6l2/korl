#pragma once
#include "korl-globalDefines.h"
korl_internal void korl_vulkan_construct(void);
korl_internal void korl_vulkan_destroy(void);
/** This API is platform-specific, and thus must be defined within the code base 
 * of whatever the current target platform is. */
korl_internal void korl_vulkan_createSurface(void* userData);
korl_internal void korl_vulkan_destroySurface(void);
