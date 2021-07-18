/** This header is used internally by vulkan code modules. */
#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
typedef struct _Korl_Vulkan_Context
{
    /** This member is a placeholder and should be replaced by an object instead 
     * of a pointer, and the code which uses this member should be refactored 
     * appropriately when I decide to use a custom host memory allocator. */
    VkAllocationCallbacks* allocator;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    /* instance extension function pointers */
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
#if KORL_DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
    VkDebugUtilsMessengerEXT debugMessenger;
#endif// KORL_DEBUG
} _Korl_Vulkan_Context;
/** It makes sense for this data structure to be separate from the 
 * \c Korl_Vulkan_Context , as this state needs to be created on a per-window 
 * basis, so there will potentially be a collection of these which all need to 
 * be created, destroyed, & managed separately. */
typedef struct _Korl_Vulkan_SurfaceContext
{
    VkSurfaceKHR surface;
} _Korl_Vulkan_SurfaceContext;
korl_global_variable _Korl_Vulkan_Context g_korl_vulkan_context;
