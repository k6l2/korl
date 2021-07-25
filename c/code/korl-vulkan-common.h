/** This header is used internally by vulkan code modules. */
#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
typedef struct _Korl_Vulkan_QueueFamilyMetaData
{
    /* unify the unique queue family index variables with an array so we can 
        easily iterate over them */
    union
    {
        struct
        {
            u32 graphics;
            u32 present;
        } indexQueues;
        u32 indices[2];
    } indexQueueUnion;
    bool hasIndexQueueGraphics;
    bool hasIndexQueuePresent;
} _Korl_Vulkan_QueueFamilyMetaData;
typedef struct _Korl_Vulkan_DeviceSurfaceMetaData
{
    VkSurfaceCapabilitiesKHR capabilities;
    u32 formatsSize;
    VkSurfaceFormatKHR formats[256];
    u32 presentModesSize;
    VkPresentModeKHR presentModes[256];
} _Korl_Vulkan_DeviceSurfaceMetaData;
typedef struct _Korl_Vulkan_Context
{
    /** This member is a placeholder and should be replaced by an object instead 
     * of a pointer, and the code which uses this member should be refactored 
     * appropriately when I decide to use a custom host memory allocator. */
    VkAllocationCallbacks* allocator;
    VkInstance instance;
    /* for now we're just going to have a single window with Vulkan rendering, 
        so we only have to make one device, ergo we only need one global 
        instance of these variables */
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    _Korl_Vulkan_QueueFamilyMetaData queueFamilyMetaData;
    VkQueue queueGraphics;
    VkQueue queuePresent;
    _Korl_Vulkan_DeviceSurfaceMetaData deviceSurfaceMetaData;
    /* @todo: move this data into a pipeline struct */
    VkShaderModule shaderTriangleVert;
    VkShaderModule shaderTriangleFrag;
    /* pipeline layouts (uniform data) are shared between layouts */
    VkPipelineLayout pipelineLayout;
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
    VkSurfaceFormatKHR swapChainSurfaceFormat;
    VkExtent2D swapChainImageExtent;
    VkSwapchainKHR swapChain;
    u32 swapChainImagesSize;
    VkImage swapChainImages[8];
    VkImageView swapChainImageViews[8];
} _Korl_Vulkan_SurfaceContext;
korl_global_variable _Korl_Vulkan_Context g_korl_vulkan_context;
/** for now we'll just have one global surface context, since the KORL 
 * application will only use one window */
korl_global_variable _Korl_Vulkan_SurfaceContext g_korl_windows_vulkan_surfaceContext;
