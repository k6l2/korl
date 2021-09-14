/**
 * This header is used internally by vulkan code modules.
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"// for KORL vector structs, such as V3f32
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
typedef struct _Korl_Vulkan_Context
{
    /**
     * This member is a placeholder and should be replaced by an object instead 
     * of a pointer, and the code which uses this member should be refactored 
     * appropriately when I decide to use a custom host memory allocator.
     */
    VkAllocationCallbacks* allocator;
    VkInstance instance;
    /* instance extension function pointers */
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
#if KORL_DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
    VkDebugUtilsMessengerEXT debugMessenger;
#endif// KORL_DEBUG
    /**
     * @todo: move everything below this point into \c _Korl_Vulkan_SurfaceContext ???
     */
    /* for now we're just going to have a single window with Vulkan rendering, 
        so we only have to make one device, ergo we only need one global 
        instance of these variables */
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    /* we save this data member because once we query for this meta data in the 
        device creation routines, we basically never have to query for it again 
        (unless we need to create a device again for some reason), and this data 
        is needed for swap chain creation, which is very likely to happen 
        multiple times during program execution */
    _Korl_Vulkan_QueueFamilyMetaData queueFamilyMetaData;
    VkQueue queueGraphics;
    VkQueue queuePresent;
    VkCommandPool commandPoolGraphics;
    /** Used to issue commands to transfer memory to device-local storage */
    VkCommandPool commandPoolTransfer;
    /** 
     * @todo: rename everything w/ "immediate" to just "batch", differentiating 
     * between "batchDevice" & "batchHost" 
     */
    /**
     * @todo: store a collection of shader modules which can be referenced by 
     * external API, and store built-in shaders in known positions
     */
    VkShaderModule shaderImmediateColorVert;
    VkShaderModule shaderImmediateFrag;
    /**
     * @todo: store pipelines in a collection which can be referenced by 
     * external API, and store built-in pipelines in known positions 
     */
    /**
     * @todo: store pipeline meta data as part of the above collection so that 
     * we can rebuild all pipelines at any given time
     */
    VkPipeline pipelineImmediateColor;
    /* pipeline layouts (uniform data) are (potentially) shared between 
        pipelines */
    VkPipelineLayout pipelineLayout;
    /* render passes are (potentially) shared between pipelines */
    VkRenderPass renderPass;
} _Korl_Vulkan_Context;
#define _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE 8
#define _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES 2
#define _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_STAGING 1024
#define _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_DEVICE 10*1024
#define _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING 1024
#define _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE 10*1024
/**
 * It makes sense for this data structure to be separate from the 
 * \c Korl_Vulkan_Context , as this state needs to be created on a per-window 
 * basis, so there will potentially be a collection of these which all need to 
 * be created, destroyed, & managed separately.
 */
typedef struct _Korl_Vulkan_SurfaceContext
{
    VkSurfaceKHR surface;
    /** 
     * Used to ensure that the user calls \c korl_vulkan_frameBegin before, and 
     * the same number of times as, \c korl_vulkan_frameEnd .
     */
    u8 frameStackCounter;
    VkSurfaceFormatKHR swapChainSurfaceFormat;
    VkExtent2D swapChainImageExtent;
    VkSwapchainKHR  swapChain;
    u32             swapChainImagesSize;
    VkImage         swapChainImages        [_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    VkImageView     swapChainImageViews    [_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    VkFramebuffer   swapChainFrameBuffers  [_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    VkCommandBuffer swapChainCommandBuffers[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    VkFence         swapChainFences        [_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    unsigned    wipFrameCurrent;
    VkSemaphore wipFramesSemaphoreImageAvailable[_KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES];
    VkSemaphore wipFramesSemaphoreRenderDone    [_KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES];
    VkFence     wipFramesFence                  [_KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES];
    bool deferredResize;
    u32 deferredResizeX, deferredResizeY;
    /** @todo: all of this vertex batch stuff probably needs to be duplicated 
     *         over each "frame state" so that we can safely batch vertices 
     *         while other frames are still being processed! 
     * @korl-vulkan-parallel-vertex-batch */
    u32 batchVertexIndexCountStaging;
    u32 batchVertexIndexCountDevice;
    u32 batchVertexCountStaging;
    u32 batchVertexCountDevice;
    /**
     * Allocation layout:
     * ----- 0
     * - vertex batch indices
     * ----- _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_STAGING*Korl_Vulkan_VertexIndex
     * - vertex batch positions
     * ----- _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_STAGING*Korl_Vulkan_VertexIndex
     *     + _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*Korl_Vulkan_Position
     * - vertex batch colors
     * ----- _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_STAGING*Korl_Vulkan_VertexIndex
     *     + _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*Korl_Vulkan_Position
     *     + _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*Korl_Vulkan_Color
     */
    VkDeviceMemory deviceMemoryVertexBatchStaging;
    /**
     * \note: This layout looks SIMILAR to \c deviceMemoryVertexBatchStaging , 
     *        but it is NOT due to difference in size specification.
     * Allocation layout:
     * ----- 0
     * - vertex batch indices
     * ----- _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_DEVICE*Korl_Vulkan_VertexIndex
     * - vertex batch positions
     * ----- _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_DEVICE*Korl_Vulkan_VertexIndex
     *     + _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*Korl_Vulkan_Position
     * - vertex batch colors
     * ----- _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_DEVICE*Korl_Vulkan_VertexIndex
     *     + _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*Korl_Vulkan_Position
     *     + _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*Korl_Vulkan_Color
     */
    VkDeviceMemory deviceMemoryVertexBatchDevice;
    VkBuffer bufferVertexBatchStagingIndices;
    VkBuffer bufferVertexBatchStagingPositions;
    VkBuffer bufferVertexBatchStagingColors;
    VkBuffer bufferVertexBatchDeviceIndices;
    VkBuffer bufferVertexBatchDevicePositions;
    VkBuffer bufferVertexBatchDeviceColors;
} _Korl_Vulkan_SurfaceContext;
korl_global_variable _Korl_Vulkan_Context g_korl_vulkan_context;
/** 
 * for now we'll just have one global surface context, since the KORL 
 * application will only use one window 
 * */
korl_global_variable _Korl_Vulkan_SurfaceContext g_korl_vulkan_surfaceContext;
