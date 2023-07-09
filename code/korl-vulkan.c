/**
 * Good Vulkan guides I've been using to figure out how to do all this nonsense:  
 * - https://vulkan-tutorial.com/
 * - https://vkguide.dev/
 */
#include "korl-vulkan.h"
#include "korl-log.h"
#include "korl-assetCache.h"
#include "korl-vulkan-common.h"
#include "korl-stb-image.h"
#include "korl-stb-ds.h"
#include "korl-time.h"
#include "korl-resource.h"
#include "utility/korl-utility-gfx.h"
#if defined(KORL_PLATFORM_WINDOWS)
    #include <vulkan/vulkan_win32.h>
#endif// defined(KORL_PLATFORM_WINDOWS)
//#define _KORL_VULKAN_LOG_REPORTS
/** I'm going to be using a so-called "reversed" depth buffer, where the 
 * far-plane is to be mapped to 0, and the near-plane is to be mapped to 1.  
 * We do this for better precision distribution to minimize error, and more 
 * intuitive debugging (in my opinion).  It's nice to keep everything right-handed as well!  
 * For details, see: https://developer.nvidia.com/content/depth-precision-visualized */
korl_global_const VkCompareOp _KORL_VULKAN_DEPTH_COMPARE_OP  = VK_COMPARE_OP_GREATER;// a depth of 0 => back of clip-space; we only want to accept fragments that have GREATER depth values, as those should be displayed in front; see above comment
korl_global_const f32         _KORL_VULKAN_DEPTH_CLEAR_VALUE = 0.f;                  // 0 => back of the clip-space, 1 => the front; see above
korl_global_const char* G_KORL_VULKAN_ENABLED_LAYERS[] = 
    { "VK_LAYER_KHRONOS_validation"
    , "VK_LAYER_KHRONOS_synchronization2" };
korl_global_const char* G_KORL_VULKAN_DEVICE_EXTENSIONS[] = 
    { VK_KHR_SWAPCHAIN_EXTENSION_NAME
    , VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME };
#define _KORL_VULKAN_GET_INSTANCE_PROC_ADDR(context, proc)                     \
    {                                                                          \
        context->vk##proc =                                                    \
            (PFN_vk##proc)vkGetInstanceProcAddr(context->instance, "vk"#proc); \
        korl_assert(context->vk##proc);                                        \
    }
#define _KORL_VULKAN_CHECK(operation) \
    do\
    {\
        const VkResult _korl_vulkan_checkedResult = (operation);\
        korl_assert(_korl_vulkan_checkedResult == VK_SUCCESS);\
    } while(0)
#if KORL_DEBUG
korl_internal VkBool32 VKAPI_CALL _korl_vulkan_debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData)
{
    // pUserData is currently unused, and we don't pass one so it should be NULL
    korl_assert(!pUserData);
    char* messageTypeString = "GVP";
    if(!(messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT))
        messageTypeString[0] = ' ';
    if(!(messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT))
        messageTypeString[1] = ' ';
    if(!(messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT))
        messageTypeString[2] = ' ';
    if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        korl_log(ERROR  , "{%hs} %hs", messageTypeString, pCallbackData->pMessage);
    else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        korl_log(WARNING, "{%hs} %hs", messageTypeString, pCallbackData->pMessage);
    else
        korl_log(INFO   , "{%hs} %hs", messageTypeString, pCallbackData->pMessage);
    /* according to documentation, we MUST always return VK_FALSE! */
    return VK_FALSE;
}
#endif// KORL_DEBUG
typedef struct _Korl_Vulkan_DeviceSurfaceMetaData
{
    VkSurfaceCapabilitiesKHR capabilities;
    u32 formatsSize;
    VkSurfaceFormatKHR formats[256];
    u32 presentModesSize;
    VkPresentModeKHR presentModes[256];
} _Korl_Vulkan_DeviceSurfaceMetaData;
/** 
 * \return true if \c deviceNew is a "better" device than \c deviceOld
 */
korl_internal bool _korl_vulkan_isBetterDevice(const VkPhysicalDevice deviceOld
                                              ,const VkPhysicalDevice deviceNew
                                              ,const _Korl_Vulkan_QueueFamilyMetaData*const queueFamilyMetaData
                                              ,const _Korl_Vulkan_DeviceSurfaceMetaData*const deviceSurfaceMetaData
                                              ,const VkPhysicalDeviceProperties*const propertiesOld
                                              ,const VkPhysicalDeviceProperties*const propertiesNew
                                              ,const VkPhysicalDeviceFeatures*const featuresOld
                                              ,const VkPhysicalDeviceFeatures*const featuresNew)
{
    korl_assert(deviceNew != VK_NULL_HANDLE);
    /* don't even consider devices that don't support all the extensions we need */
    VkExtensionProperties extensionProperties[256];
    KORL_ZERO_STACK(u32, extensionPropertiesSize);
    _KORL_VULKAN_CHECK(
        vkEnumerateDeviceExtensionProperties(deviceNew, NULL/*pLayerName*/, &extensionPropertiesSize, NULL/*pProperties*/));
    korl_assert(extensionPropertiesSize <= korl_arraySize(extensionProperties));
    extensionPropertiesSize = korl_arraySize(extensionProperties);
    _KORL_VULKAN_CHECK(
        vkEnumerateDeviceExtensionProperties(deviceNew, NULL/*pLayerName*/, &extensionPropertiesSize, extensionProperties));
    bool deviceNewSupportsExtensions = true;
    for(size_t e = 0; e < korl_arraySize(G_KORL_VULKAN_DEVICE_EXTENSIONS); e++)
    {
        bool extensionFound = false;
        for(u32 de = 0; de < extensionPropertiesSize; de++)
            if(strcmp(extensionProperties[de].extensionName, 
                      G_KORL_VULKAN_DEVICE_EXTENSIONS[e]) == 0)
            {
                extensionFound = true;
                break;
            }
        if(!extensionFound)
        {
            deviceNewSupportsExtensions = false;
            break;
        }
    }
    if(!deviceNewSupportsExtensions)
        return false;
    /* don't consider devices that don't have the ability to create a swap chain 
        for the surface we provided earlier */
    if(    deviceSurfaceMetaData->formatsSize      == 0 
        || deviceSurfaceMetaData->presentModesSize == 0)
        return false;
    /* don't even consider using devices that can't render graphics */
    if(!queueFamilyMetaData->hasIndexQueueGraphics)
        return false;
    /* don't consider using devices that don't support anisotropic filtering */
    if(!featuresNew->samplerAnisotropy)
        return false;
    /* always attempt to use a discrete GPU first */
    if(    propertiesNew->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && propertiesOld->deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return true;
    /* if the old device hasn't been selected, just use the new device */
    if(deviceOld == VK_NULL_HANDLE)
        return true;
    return false;
}
korl_internal _Korl_Vulkan_DeviceSurfaceMetaData _korl_vulkan_deviceSurfaceMetaData(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceSurfaceMetaData, result);
    /* query for device-surface capabilities */
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &result.capabilities));
    /* query for device-surface formats */
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &result.formatsSize, 
                                             NULL/*pSurfaceFormats*/));
    korl_assert(result.formatsSize <= korl_arraySize(result.formats));
    result.formatsSize = korl_arraySize(result.formats);
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &result.formatsSize, result.formats));
    /* query for device-surface present modes */
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &result.presentModesSize, 
                                                  NULL/*pPresentModes*/));
    korl_assert(result.presentModesSize <= korl_arraySize(result.presentModes));
    result.presentModesSize = korl_arraySize(result.presentModes);
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &result.presentModesSize, 
                                                  result.presentModes));
    return result;
}
korl_internal _Korl_Vulkan_QueueFamilyMetaData _korl_vulkan_queueFamilyMetaData(
    VkPhysicalDevice physicalDevice, 
    const u32 queueFamilyCount, 
    const VkQueueFamilyProperties*const queueFamilies, 
    VkSurfaceKHR surface)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    KORL_ZERO_STACK(_Korl_Vulkan_QueueFamilyMetaData, result);
    for(u32 q = 0; q < queueFamilyCount; q++)
    {
        if(!result.hasIndexQueueGraphics 
            && (queueFamilies[q].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            result.hasIndexQueueGraphics = true;
            result.indexQueueUnion.indexQueues.graphics = q;
        }
        if(!result.hasIndexQueuePresent)
        {
            KORL_ZERO_STACK(VkBool32, surfaceSupport);
            _KORL_VULKAN_CHECK(
                context->vkGetPhysicalDeviceSurfaceSupportKHR(
                    physicalDevice, q, surface, &surfaceSupport));
            if(surfaceSupport)
            {
                result.hasIndexQueuePresent = true;
                result.indexQueueUnion.indexQueues.present = q;
            }
        }
    }
    return result;
}
/** This API will create the parts of the swap chain which can be destroyed & 
 * re-created in the event that the swap chain needs to be resized. */
korl_internal void _korl_vulkan_createSwapChain(u32 sizeX, u32 sizeY, 
                                                const _Korl_Vulkan_DeviceSurfaceMetaData*const deviceSurfaceMetaData)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* given what we know about the device & the surface, select the best 
        settings for the swap chain */
    korl_assert(deviceSurfaceMetaData->formatsSize > 0);
    surfaceContext->swapChainSurfaceFormat = deviceSurfaceMetaData->formats[0];
    for(u32 f = 0; f < deviceSurfaceMetaData->formatsSize; f++)
        if(    deviceSurfaceMetaData->formats[f].format     == VK_FORMAT_B8G8R8A8_SRGB
            && deviceSurfaceMetaData->formats[f].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surfaceContext->swapChainSurfaceFormat = deviceSurfaceMetaData->formats[f];
            break;
        }
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
    for(u32 pm = 0; pm < deviceSurfaceMetaData->presentModesSize; pm++)
        if(deviceSurfaceMetaData->presentModes[pm] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        else if(presentMode == VK_PRESENT_MODE_MAX_ENUM_KHR 
                && deviceSurfaceMetaData->presentModes[pm] == VK_PRESENT_MODE_FIFO_KHR)
            /* According to Vulkan spec, this is the only present mode that is 
                required to be supported, which should guarantee that 
                presentMode will be set to a valid mode. */
            presentMode = VK_PRESENT_MODE_FIFO_KHR;
    korl_assert(presentMode != VK_PRESENT_MODE_MAX_ENUM_KHR);
    korl_assert(presentMode == VK_PRESENT_MODE_MAILBOX_KHR);/* For now, we only support mailbox mode.  This is because in order to support something like FIFO, 
                                                               which will cause the render thread to block on certain Vulkan API calls, we can only do the 
                                                               following (as far as I know):
                                                               - add a frame rate regulation logic in the render thread, which controls how many logical loops 
                                                                 occur for every render loop
                                                                 ISSUE: this will cause logic loops to be skipped occasionally when the logic loop consumption 
                                                                        is de-synced from the render loop production, because we cannot know ahead of time how 
                                                                        many logic loops need to occur before the next swap chain vsync
                                                               - base the # of logic loops off the refresh rate of the window
                                                                 ISSUE: potentially doable, but different implementation is required for "true" full-screen:
                                                                        https://stackoverflow.com/a/18902024
                                                               - put the logic loops on a thread separate from the render loops
                                                                 ISSUE: making making logic run on a separate thread will open a bunch of different cans of 
                                                                        worms all at once that I don't really want to deal with at the moment */
    const wchar_t* presentModeString = NULL;
    switch(presentMode)
    {
    case VK_PRESENT_MODE_MAILBOX_KHR: {presentModeString = L"VK_PRESENT_MODE_MAILBOX_KHR"; break;}
    case VK_PRESENT_MODE_FIFO_KHR:    {presentModeString = L"VK_PRESENT_MODE_FIFO_KHR";    break;}
    default:                          {presentModeString = L"INVALID";                     break;}
    }
    korl_log(INFO, "presentMode: %ws", presentModeString);
    surfaceContext->swapChainImageExtent = deviceSurfaceMetaData->capabilities.currentExtent;
    if(    surfaceContext->swapChainImageExtent.width  == 0xFFFFFFFF 
        && surfaceContext->swapChainImageExtent.height == 0xFFFFFFFF)
    {
        surfaceContext->swapChainImageExtent.width = KORL_MATH_CLAMP(sizeX, 
            deviceSurfaceMetaData->capabilities.minImageExtent.width, 
            deviceSurfaceMetaData->capabilities.maxImageExtent.width);
        surfaceContext->swapChainImageExtent.height = KORL_MATH_CLAMP(sizeY, 
            deviceSurfaceMetaData->capabilities.minImageExtent.height, 
            deviceSurfaceMetaData->capabilities.maxImageExtent.height);
    }
    u32 minImageCount = deviceSurfaceMetaData->capabilities.minImageCount + 1;
    if(    deviceSurfaceMetaData->capabilities.maxImageCount > 0 
        && deviceSurfaceMetaData->capabilities.maxImageCount < minImageCount)
        minImageCount = deviceSurfaceMetaData->capabilities.maxImageCount;
    /* ------ create the swap chain ------ */
    KORL_ZERO_STACK(VkSwapchainCreateInfoKHR, createInfoSwapChain);
    createInfoSwapChain.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfoSwapChain.surface          = surfaceContext->surface;
    createInfoSwapChain.minImageCount    = minImageCount;
    createInfoSwapChain.imageFormat      = surfaceContext->swapChainSurfaceFormat.format;
    createInfoSwapChain.imageColorSpace  = surfaceContext->swapChainSurfaceFormat.colorSpace;
    createInfoSwapChain.imageExtent      = surfaceContext->swapChainImageExtent;
    createInfoSwapChain.imageArrayLayers = 1;//non-stereoscopic
    createInfoSwapChain.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfoSwapChain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfoSwapChain.preTransform     = deviceSurfaceMetaData->capabilities.currentTransform;
    createInfoSwapChain.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfoSwapChain.presentMode      = presentMode;
    createInfoSwapChain.clipped          = VK_TRUE;
    createInfoSwapChain.oldSwapchain     = VK_NULL_HANDLE;
    if(   context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics 
       != context->queueFamilyMetaData.indexQueueUnion.indexQueues.present)
    {
        createInfoSwapChain.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfoSwapChain.queueFamilyIndexCount = 2;
        createInfoSwapChain.pQueueFamilyIndices   = context->queueFamilyMetaData.indexQueueUnion.indices;
    }
    _KORL_VULKAN_CHECK(
        vkCreateSwapchainKHR(context->device, &createInfoSwapChain, 
                             context->allocator, &surfaceContext->swapChain));
    /* ------ create a depth buffer ------ */
    // first, attempt to find the "most optimal" format for the depth buffer //
    const VkFormat depthBufferFormatCandidates[] = 
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    const VkImageTiling depthBufferTiling          = VK_IMAGE_TILING_OPTIMAL;
    const VkFormatFeatureFlags depthBufferFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    u$ depthBufferFormatSelection = 0; 
    for(; depthBufferFormatSelection < korl_arraySize(depthBufferFormatCandidates); depthBufferFormatSelection++)
    {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(context->physicalDevice, depthBufferFormatCandidates[depthBufferFormatSelection], 
                                            &formatProperties);
        if(   depthBufferTiling == VK_IMAGE_TILING_LINEAR 
           && ((formatProperties.linearTilingFeatures & depthBufferFeatures) == depthBufferFeatures))
            break;
        if(   depthBufferTiling == VK_IMAGE_TILING_OPTIMAL 
           && ((formatProperties.optimalTilingFeatures & depthBufferFeatures) == depthBufferFeatures))
            break;
    }
    korl_assert(depthBufferFormatSelection < korl_arraySize(depthBufferFormatCandidates));
    const VkFormat formatDepthBuffer = depthBufferFormatCandidates[depthBufferFormatSelection];
    // at this point, we could cache whether or not we have a stencil component 
    //  on the surface like this (but I don't need this information right now): 
    surfaceContext->hasStencilComponent = formatDepthBuffer == VK_FORMAT_D32_SFLOAT_S8_UINT
                                       || formatDepthBuffer == VK_FORMAT_D24_UNORM_S8_UINT;
    // now that we have selected a format, we can create the depth buffer image //
    _Korl_Vulkan_DeviceMemory_Alloctation* allocationDepthStencilImageBuffer = NULL;
    surfaceContext->depthStencilImageBuffer = _korl_vulkan_deviceMemory_allocateImageBuffer(&surfaceContext->deviceMemoryRenderResources
                                                                                           ,surfaceContext->swapChainImageExtent.width
                                                                                           ,surfaceContext->swapChainImageExtent.height
                                                                                           ,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                                                           ,VK_IMAGE_ASPECT_DEPTH_BIT, formatDepthBuffer, depthBufferTiling
                                                                                           ,0// 0 => generate new handle automatically
                                                                                           ,&allocationDepthStencilImageBuffer);
    /* ----- create render pass ----- */
    KORL_ZERO_STACK_ARRAY(VkAttachmentDescription, attachments, 2);
    attachments[0].format         = surfaceContext->swapChainSurfaceFormat.format;
    attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;// clears the attachment to the clear values passed to vkCmdBeginRenderPass
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[1].format         = formatDepthBuffer;
    attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;// clears the attachment to the clear values passed to vkCmdBeginRenderPass 
    attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    KORL_ZERO_STACK(VkAttachmentReference, attachmentReferenceColor);
    attachmentReferenceColor.attachment = 0;
    attachmentReferenceColor.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    KORL_ZERO_STACK(VkAttachmentReference, attachmentReferenceDepth);
    attachmentReferenceDepth.attachment = 1;
    attachmentReferenceDepth.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    KORL_ZERO_STACK(VkSubpassDescription, subPass);
    subPass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPass.colorAttachmentCount    = 1;
    subPass.pColorAttachments       = &attachmentReferenceColor;
    subPass.pDepthStencilAttachment = &attachmentReferenceDepth;
    /* setup subpass dependencies for synchronization between frames; derived from:
        https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#swapchain-image-acquire-and-present */
    KORL_ZERO_STACK_ARRAY(VkSubpassDependency, subpassDependency, 2);
    subpassDependency[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    subpassDependency[0].dstSubpass    = 0;
    subpassDependency[0].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency[0].srcAccessMask = VK_ACCESS_NONE_KHR;
    subpassDependency[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    subpassDependency[1].srcSubpass    = 0;
    subpassDependency[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    subpassDependency[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependency[1].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency[1].dstAccessMask = VK_ACCESS_NONE_KHR;
    KORL_ZERO_STACK(VkRenderPassCreateInfo, createInfoRenderPass);
    createInfoRenderPass.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfoRenderPass.attachmentCount = korl_arraySize(attachments);
    createInfoRenderPass.pAttachments    = attachments;
    createInfoRenderPass.subpassCount    = 1;
    createInfoRenderPass.pSubpasses      = &subPass;
    createInfoRenderPass.dependencyCount = korl_arraySize(subpassDependency);
    createInfoRenderPass.pDependencies   = subpassDependency;
    _KORL_VULKAN_CHECK(
        vkCreateRenderPass(context->device, &createInfoRenderPass, context->allocator, 
                           &context->renderPass));
    /* get swap chain images */
    _KORL_VULKAN_CHECK(
        vkGetSwapchainImagesKHR(context->device, surfaceContext->swapChain, 
                                &surfaceContext->swapChainImagesSize, NULL/*pSwapchainImages*/));
    surfaceContext->swapChainImagesSize = KORL_MATH_MIN(surfaceContext->swapChainImagesSize, 
                                                        korl_arraySize(surfaceContext->swapChainImages));
    korl_log(INFO, "swapChainImagesSize=%i", surfaceContext->swapChainImagesSize);
    _KORL_VULKAN_CHECK(
        vkGetSwapchainImagesKHR(context->device, surfaceContext->swapChain, 
                                &surfaceContext->swapChainImagesSize, 
                                surfaceContext->swapChainImages));
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        /* create image views for all the swap chain images */
        KORL_ZERO_STACK(VkImageViewCreateInfo, createInfoImageView);
        createInfoImageView.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfoImageView.image                           = surfaceContext->swapChainImages[i];
        createInfoImageView.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        createInfoImageView.format                          = surfaceContext->swapChainSurfaceFormat.format;
        createInfoImageView.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfoImageView.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfoImageView.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfoImageView.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfoImageView.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfoImageView.subresourceRange.layerCount     = 1;
        createInfoImageView.subresourceRange.baseArrayLayer = 0;
        createInfoImageView.subresourceRange.levelCount     = 1;
        createInfoImageView.subresourceRange.baseMipLevel   = 0;
        _KORL_VULKAN_CHECK(
            vkCreateImageView(context->device, &createInfoImageView, context->allocator, 
                              &surfaceContext->swapChainImageContexts[i].imageView));
        /* create a frame buffer for all the swap chain image views */
        VkImageView frameBufferAttachments[] = 
            { surfaceContext->swapChainImageContexts[i].imageView
            , allocationDepthStencilImageBuffer->subType.imageBuffer.imageView };
        KORL_ZERO_STACK(VkFramebufferCreateInfo, createInfoFrameBuffer);
        createInfoFrameBuffer.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfoFrameBuffer.renderPass      = context->renderPass;
        createInfoFrameBuffer.attachmentCount = korl_arraySize(frameBufferAttachments);
        createInfoFrameBuffer.pAttachments    = frameBufferAttachments;
        createInfoFrameBuffer.width           = surfaceContext->swapChainImageExtent.width;
        createInfoFrameBuffer.height          = surfaceContext->swapChainImageExtent.height;
        createInfoFrameBuffer.layers          = 1;
        _KORL_VULKAN_CHECK(
            vkCreateFramebuffer(context->device, &createInfoFrameBuffer, context->allocator, 
                                &surfaceContext->swapChainImageContexts[i].frameBuffer));
        /* initialize pool of descriptor pools */
        mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), surfaceContext->swapChainImageContexts[i].stbDaDescriptorPools, 8);
#if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
        /* initialize a debug pool of device asset indices */
        mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), surfaceContext->swapChainImageContexts[i].stbDaInUseDeviceAssetIndices, 2048);
#endif
    }
}
korl_internal void _korl_vulkan_destroySwapChain(void)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    _KORL_VULKAN_CHECK(vkDeviceWaitIdle(context->device));
    _korl_vulkan_deviceMemory_allocator_free(&surfaceContext->deviceMemoryRenderResources, surfaceContext->depthStencilImageBuffer);
    surfaceContext->depthStencilImageBuffer = 0;
    vkDestroyRenderPass(context->device, context->renderPass, context->allocator);
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &(surfaceContext->swapChainImageContexts[i]);
        vkDestroyFramebuffer(context->device,swapChainImageContext->frameBuffer, context->allocator);
        vkDestroyImageView  (context->device,swapChainImageContext->imageView  , context->allocator);
        for(u$ d = 0; d < arrlenu(swapChainImageContext->stbDaDescriptorPools); d++)
        {
            vkResetDescriptorPool(context->device, swapChainImageContext->stbDaDescriptorPools[d].vkDescriptorPool, /*flags; reserved*/0);
            vkDestroyDescriptorPool(context->device, swapChainImageContext->stbDaDescriptorPools[d].vkDescriptorPool, context->allocator);
        }
        mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaDescriptorPools);
#if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
        mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaInUseDeviceAssetIndices);
#endif
    }
    korl_memory_zero(surfaceContext->swapChainImageContexts, sizeof(surfaceContext->swapChainImageContexts));
    vkDestroySwapchainKHR(context->device, surfaceContext->swapChain, context->allocator);
}
/** 
 * \param memoryTypeBits Flags representing the incides in a list of memory 
 * types for a physical device are supported for a given resource - the caller 
 * is responsible for querying for these flags using vulkan API, such as 
 * \c vkGetBufferMemoryRequirements .  From the spec: Bit \c i is set if and 
 * only if the memory type \c i in the \c VkPhysicalDeviceMemoryProperties 
 * structure for the physical device is supported for the resource.
 * \param memoryPropertyFlagBits Flags representing the caller's required flags 
 * which must be present in the memory type index returned by this function.
 * \return An index identifying a memory type from the \c memoryTypes array of a 
 * \c VkPhysicalDeviceMemoryProperties structure.  If no memory type on the 
 * physical device can satisfy the requirements, the \c memoryTypeCount is 
 * returned. 
 */
korl_internal uint32_t _korl_vulkan_findMemoryType(
    uint32_t memoryTypeBits, VkMemoryPropertyFlags memoryPropertyFlagBits)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    KORL_ZERO_STACK(VkPhysicalDeviceMemoryProperties, physicalDeviceMemoryProperties);
    vkGetPhysicalDeviceMemoryProperties(
        context->physicalDevice, &physicalDeviceMemoryProperties);
    for(uint32_t m = 0; m < physicalDeviceMemoryProperties.memoryTypeCount; m++)
    {
        /* if this index isn't supported by the user-provided index list, 
            continue searching elsewhere */
        if(!(memoryTypeBits & (1 << m)))
            continue;
        /* if this memory type doesn't support the user-requested property 
            flags, continue searching elsewhere */
        if((physicalDeviceMemoryProperties.memoryTypes[m].propertyFlags & memoryPropertyFlagBits) 
                != memoryPropertyFlagBits)
            continue;
        /* otherwise, we can just return the first index we find */
        return m;
    }
    return physicalDeviceMemoryProperties.memoryTypeCount;
}
korl_internal bool _korl_vulkan_pipeline_isMetaDataSame(_Korl_Vulkan_Pipeline p0, _Korl_Vulkan_Pipeline p1)
{
    /* we only want to compare the meta data - we don't care about the actual 
        VkPipeline handle */
    p0.pipeline = VK_NULL_HANDLE;
    p1.pipeline = VK_NULL_HANDLE;
    /* these are POD structs, so we can just use memcmp */
    return korl_memory_compare(&p0, &p1, sizeof(p0)) == 0;
}
korl_internal _Korl_Vulkan_Pipeline _korl_vulkan_pipeline_default(void)
{
    KORL_ZERO_STACK(_Korl_Vulkan_Pipeline, pipeline);
    pipeline.modes.primitiveType   = KORL_GFX_PRIMITIVE_TYPE_INVALID;// we expect the user to set the topology for every draw call, so we might as well invalidate this
    pipeline.modes.enableDepthTest = true;
    return pipeline;
}
korl_internal void _korl_vulkan_createPipeline(u$ pipelineIndex)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    korl_assert(pipelineIndex < arrlenu(context->stbDaPipelines));
    korl_assert(context->stbDaPipelines[pipelineIndex].pipeline == VK_NULL_HANDLE);
    _Korl_Vulkan_Pipeline*const pipeline = &context->stbDaPipelines[pipelineIndex];
    /* set fixed functions & other pipeline parameters */
    KORL_ZERO_STACK_ARRAY(VkVertexInputBindingDescription  , vertexInputBindings, KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT);
    KORL_ZERO_STACK_ARRAY(VkVertexInputAttributeDescription, vertexAttributes   , KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT);
    uint32_t vertexAttributeCount = 0;
    for(u32 i = 0; i < KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT; i++)
    {
        if(pipeline->vertexAttributes[i].format == VK_FORMAT_UNDEFINED)
            continue;
        vertexInputBindings[vertexAttributeCount].binding   = i;
        vertexInputBindings[vertexAttributeCount].stride    = pipeline->vertexAttributes[i].byteStride;
        vertexInputBindings[vertexAttributeCount].inputRate = pipeline->vertexAttributes[i].inputRate;
        vertexAttributes[vertexAttributeCount].binding  = i;
        vertexAttributes[vertexAttributeCount].location = i;
        vertexAttributes[vertexAttributeCount].format   = pipeline->vertexAttributes[i].format;
        vertexAttributes[vertexAttributeCount].offset   = pipeline->vertexAttributes[i].byteOffset;
        vertexAttributeCount++;
    }
    KORL_ZERO_STACK(VkPipelineVertexInputStateCreateInfo, createInfoVertexInput);
    createInfoVertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfoVertexInput.vertexBindingDescriptionCount   = vertexAttributeCount;
    createInfoVertexInput.pVertexBindingDescriptions      = vertexInputBindings;
    createInfoVertexInput.vertexAttributeDescriptionCount = vertexAttributeCount;
    createInfoVertexInput.pVertexAttributeDescriptions    = vertexAttributes;
    KORL_ZERO_STACK(VkPipelineInputAssemblyStateCreateInfo, createInfoInputAssembly);
    createInfoInputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    switch(pipeline->modes.primitiveType)
    {
    case KORL_GFX_PRIMITIVE_TYPE_TRIANGLES     : createInfoInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  break;
    case KORL_GFX_PRIMITIVE_TYPE_TRIANGLE_STRIP: createInfoInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
    case KORL_GFX_PRIMITIVE_TYPE_LINES         : createInfoInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;      break;
    case KORL_GFX_PRIMITIVE_TYPE_LINE_STRIP    : createInfoInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;     break;
    case KORL_GFX_PRIMITIVE_TYPE_INVALID       : createInfoInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;       break;
    }
    VkViewport viewPort;
    viewPort.x        = 0.f;
    viewPort.y        = 0.f;
    viewPort.width    = KORL_C_CAST(float, surfaceContext->swapChainImageExtent.width);
    viewPort.height   = KORL_C_CAST(float, surfaceContext->swapChainImageExtent.height);
    viewPort.minDepth = 0.f;
    viewPort.maxDepth = 1.f;
    KORL_ZERO_STACK(VkPipelineViewportStateCreateInfo, createInfoViewport);
    createInfoViewport.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    createInfoViewport.viewportCount = 1;
    createInfoViewport.pViewports    = &viewPort;
    createInfoViewport.scissorCount  = 1;
    /* no longer used, since we are using dynamic scissor commands instead */
    //createInfoViewport.pScissors     = &scissor;
    KORL_ZERO_STACK(VkPipelineRasterizationStateCreateInfo, createInfoRasterizer);
    createInfoRasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    createInfoRasterizer.lineWidth   = 1.f;
    switch(pipeline->modes.polygonMode)
    {
    case KORL_GFX_POLYGON_MODE_FILL: createInfoRasterizer.polygonMode = VK_POLYGON_MODE_FILL; break;
    case KORL_GFX_POLYGON_MODE_LINE: createInfoRasterizer.polygonMode = VK_POLYGON_MODE_LINE; break;
    }
    switch(pipeline->modes.cullMode)
    {
    case KORL_GFX_CULL_MODE_NONE: createInfoRasterizer.cullMode = VK_CULL_MODE_NONE;     break;
    case KORL_GFX_CULL_MODE_BACK: createInfoRasterizer.cullMode = VK_CULL_MODE_BACK_BIT; break;
    }
    //createInfoRasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;//right-handed triangles! (default)
    KORL_ZERO_STACK(VkPipelineMultisampleStateCreateInfo, createInfoMultisample);
    createInfoMultisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    createInfoMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    createInfoMultisample.minSampleShading     = 1.f;
    // we have only one framebuffer, so we only need one attachment:
    // enable alpha blending
    KORL_ZERO_STACK(VkPipelineColorBlendAttachmentState, colorBlendAttachment);
    colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = 0 != pipeline->modes.enableBlend;
    colorBlendAttachment.colorBlendOp        = pipeline->blend.color.operation;
    colorBlendAttachment.srcColorBlendFactor = pipeline->blend.color.factorSource;
    colorBlendAttachment.dstColorBlendFactor = pipeline->blend.color.factorTarget;
    colorBlendAttachment.alphaBlendOp        = pipeline->blend.alpha.operation;
    colorBlendAttachment.srcAlphaBlendFactor = pipeline->blend.alpha.factorSource;
    colorBlendAttachment.dstAlphaBlendFactor = pipeline->blend.alpha.factorTarget;
    KORL_ZERO_STACK(VkPipelineColorBlendStateCreateInfo, createInfoColorBlend);
    createInfoColorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    createInfoColorBlend.attachmentCount = 1;
    createInfoColorBlend.pAttachments    = &colorBlendAttachment;
    VkDynamicState dynamicStates[] = 
        { VK_DYNAMIC_STATE_SCISSOR };
    KORL_ZERO_STACK(VkPipelineDynamicStateCreateInfo, createInfoDynamicState);
    createInfoDynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    createInfoDynamicState.dynamicStateCount = korl_arraySize(dynamicStates);
    createInfoDynamicState.pDynamicStates    = dynamicStates;
    /* create pipeline */
    KORL_ZERO_STACK_ARRAY(VkPipelineShaderStageCreateInfo, createInfoShaderStages, 2);
    createInfoShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfoShaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    createInfoShaderStages[0].module = pipeline->shaderVertex;
    createInfoShaderStages[0].pName  = "main";
    createInfoShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfoShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    createInfoShaderStages[1].module = pipeline->shaderFragment;
    createInfoShaderStages[1].pName  = "main";
    KORL_ZERO_STACK(VkPipelineDepthStencilStateCreateInfo, createInfoDepthStencil);
    createInfoDepthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    createInfoDepthStencil.depthTestEnable  = 0 != pipeline->modes.enableDepthTest ? VK_TRUE : VK_FALSE;
    createInfoDepthStencil.depthWriteEnable = 0 != pipeline->modes.enableDepthTest ? VK_TRUE : VK_FALSE;
    createInfoDepthStencil.depthCompareOp   = _KORL_VULKAN_DEPTH_COMPARE_OP;
    KORL_ZERO_STACK(VkGraphicsPipelineCreateInfo, createInfoPipeline);
    createInfoPipeline.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfoPipeline.stageCount          = korl_arraySize(createInfoShaderStages);
    createInfoPipeline.pStages             = createInfoShaderStages;
    createInfoPipeline.pVertexInputState   = &createInfoVertexInput;
    createInfoPipeline.pInputAssemblyState = &createInfoInputAssembly;
    createInfoPipeline.pViewportState      = &createInfoViewport;
    createInfoPipeline.pRasterizationState = &createInfoRasterizer;
    createInfoPipeline.pMultisampleState   = &createInfoMultisample;
    createInfoPipeline.pColorBlendState    = &createInfoColorBlend;
    createInfoPipeline.pDepthStencilState  = &createInfoDepthStencil;
    createInfoPipeline.pDynamicState       = &createInfoDynamicState;
    createInfoPipeline.layout              = context->pipelineLayout;
    createInfoPipeline.renderPass          = context->renderPass;
    createInfoPipeline.subpass             = 0;
    _KORL_VULKAN_CHECK(
        vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE/*pipeline cache*/
                                 ,1, &createInfoPipeline, context->allocator, &pipeline->pipeline));
}
/**
 * \return The index of the pipeline in \c context->pipelines , or 
 *     \c context->pipelineCount if we failed to create a pipeline.
 */
korl_internal u$ _korl_vulkan_addPipeline(_Korl_Vulkan_Pipeline pipeline)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* iterate over all pipelines and ensure that there is, in fact, no other 
        pipeline with the same meta data */
    u$ pipelineIndex = arrlenu(context->stbDaPipelines);
    for(u$ p = 0; p < arrlenu(context->stbDaPipelines); p++)
        if(_korl_vulkan_pipeline_isMetaDataSame(pipeline, context->stbDaPipelines[p]))
        {
            pipelineIndex = p;
            break;
        }
    /* if no pipeline found, add a new pipeline to the list */
    if(pipelineIndex >= arrlenu(context->stbDaPipelines))
    {
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaPipelines, pipeline);
        arrlast(context->stbDaPipelines).pipeline = VK_NULL_HANDLE;
        _korl_vulkan_createPipeline(arrlenu(context->stbDaPipelines) - 1);
    }
    return pipelineIndex;
}
korl_internal VkIndexType _korl_vulkan_vertexIndexType(void)//@TODO: delete/deprecate
{
    switch(sizeof(Korl_Vulkan_VertexIndex))
    {
    case 2: return VK_INDEX_TYPE_UINT16;
    case 4: return VK_INDEX_TYPE_UINT32;
    }
    return VK_INDEX_TYPE_MAX_ENUM;
}
/**
 * Set the current pipeline render state to match the provided \c pipeline 
 * parameter.  If a pipeline which matches the meta data of \c pipeline doesn't 
 * yet exist, a new pipeline will be created.  If a pipeline which satisfies the 
 * \c pipeline parameter exists and is selected, no work is done.  If a pipeline 
 * is selected which doesn't satisfy the \c pipeline parameter and the new 
 * selected pipeline meta data will differ from the previously selected 
 * pipeline, the current pipeline batch will be flushed to the current frame's 
 * draw command buffer.
 */
korl_internal void _korl_vulkan_setPipelineMetaData(_Korl_Vulkan_Pipeline pipeline)
{
    _Korl_Vulkan_Context*const               context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const        surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    u$ newPipelineIndex = arrlenu(context->stbDaPipelines);
    if(   surfaceContext->drawState.currentPipeline < arrlenu(context->stbDaPipelines)
       && _korl_vulkan_pipeline_isMetaDataSame(pipeline, context->stbDaPipelines[surfaceContext->drawState.currentPipeline]))
        // do nothing; just leave the current pipeline selected
        newPipelineIndex = surfaceContext->drawState.currentPipeline;
    else
    {
        /* search for a pipeline that can handle the data we're trying to batch */
        u$ pipelineIndex = _korl_vulkan_addPipeline(pipeline);
        korl_assert(pipelineIndex < arrlenu(context->stbDaPipelines));
        newPipelineIndex      = pipelineIndex;
    }
    /* then, actually change to a new pipeline for the next batch */
    korl_assert(newPipelineIndex < arrlenu(context->stbDaPipelines));//sanity check!
    surfaceContext->drawState.currentPipeline = newPipelineIndex;
}
korl_internal VkBlendOp _korl_vulkan_blendOperation_to_vulkan(Korl_Gfx_BlendOperation blendOp)
{
    switch(blendOp)
    {
    case(KORL_GFX_BLEND_OPERATION_ADD):              return VK_BLEND_OP_ADD;
    case(KORL_GFX_BLEND_OPERATION_SUBTRACT):         return VK_BLEND_OP_SUBTRACT;
    case(KORL_GFX_BLEND_OPERATION_REVERSE_SUBTRACT): return VK_BLEND_OP_REVERSE_SUBTRACT;
    case(KORL_GFX_BLEND_OPERATION_MIN):              return VK_BLEND_OP_MIN;
    case(KORL_GFX_BLEND_OPERATION_MAX):              return VK_BLEND_OP_MAX;
    }
    korl_log(ERROR, "Unsupported blend operation: %d", blendOp);
    return 0;
}
korl_internal VkBlendFactor _korl_vulkan_blendFactor_to_vulkan(Korl_Gfx_BlendFactor blendFactor)
{
    switch(blendFactor)
    {
    case(KORL_GFX_BLEND_FACTOR_ZERO):                     return VK_BLEND_FACTOR_ZERO;
    case(KORL_GFX_BLEND_FACTOR_ONE):                      return VK_BLEND_FACTOR_ONE;
    case(KORL_GFX_BLEND_FACTOR_SRC_COLOR):                return VK_BLEND_FACTOR_SRC_COLOR;
    case(KORL_GFX_BLEND_FACTOR_ONE_MINUS_SRC_COLOR):      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case(KORL_GFX_BLEND_FACTOR_DST_COLOR):                return VK_BLEND_FACTOR_DST_COLOR;
    case(KORL_GFX_BLEND_FACTOR_ONE_MINUS_DST_COLOR):      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case(KORL_GFX_BLEND_FACTOR_SRC_ALPHA):                return VK_BLEND_FACTOR_SRC_ALPHA;
    case(KORL_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA):      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case(KORL_GFX_BLEND_FACTOR_DST_ALPHA):                return VK_BLEND_FACTOR_DST_ALPHA;
    case(KORL_GFX_BLEND_FACTOR_ONE_MINUS_DST_ALPHA):      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case(KORL_GFX_BLEND_FACTOR_CONSTANT_COLOR):           return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case(KORL_GFX_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR): return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case(KORL_GFX_BLEND_FACTOR_CONSTANT_ALPHA):           return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case(KORL_GFX_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA): return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case(KORL_GFX_BLEND_FACTOR_SRC_ALPHA_SATURATE):       return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    }
    korl_log(ERROR, "Unsupported blend factor: %d", blendFactor);
    return 0;
}
korl_internal void* _korl_vulkan_getStagingPool(VkDeviceSize bytesRequired, VkDeviceSize alignmentRequired, VkBuffer* out_bufferStaging, VkDeviceSize* out_byteOffsetStagingBuffer)
{
    const u$ stagingBufferArenaBytes = korl_math_megabytes(32);// KORL-ISSUE-000-000-134: vulkan: add the ability to dynamically increase staging buffer arena capacity, as well as tune this initial value
    korl_assert(bytesRequired <= stagingBufferArenaBytes);
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* attempt to find a staging buffer that can hold the vertexData */
    _Korl_Vulkan_Buffer*                   validBuffer     = NULL;
    _Korl_Vulkan_DeviceMemory_Alloctation* validAllocation = NULL;
    for(u$ b = 0; b < arrlenu(surfaceContext->stbDaStagingBuffers); b++)
    {
        const u$ bufferIndexOffsetFromLastUsed                 = (b + surfaceContext->stagingBufferIndexLastUsed) % arrlenu(surfaceContext->stbDaStagingBuffers);
        _Korl_Vulkan_Buffer*const                   buffer     = &(surfaceContext->stbDaStagingBuffers[bufferIndexOffsetFromLastUsed]);
        _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryHostVisible, buffer->allocation);
        /* if the buffer has enough room, we can just use it */
        const VkDeviceSize alignedBytesUsed = alignmentRequired 
            ? korl_math_roundUpPowerOf2(buffer->bytesUsed, alignmentRequired) 
            : buffer->bytesUsed;
        const VkDeviceSize bufferRemainingBytes = alignedBytesUsed >= allocation->bytesUsed 
            ? 0 
            : allocation->bytesUsed - alignedBytesUsed;
        if(bufferRemainingBytes >= bytesRequired)
        {
            validBuffer     = buffer;
            validAllocation = allocation;
            buffer->bytesUsed = alignedBytesUsed;
            break;
        }
        /* otherwise, we need to see if the buffer is currently in use; if not, 
            we can just reset the buffer and use it */
        if(buffer->framesSinceLastUsed < surfaceContext->swapChainImagesSize)
            continue;
        #if KORL_DEBUG
        // korl_log(VERBOSE, "resetting staging buffer [%llu]", bufferIndexOffsetFromLastUsed);
        #endif
        buffer->bytesUsed = 0;
        validBuffer     = buffer;
        validAllocation = allocation;
        ///@ASSUMPTION: a reset buffer offset will always satisfy \c alignmentRequired
    }
    /* if we could not find a staging buffer that can hold vertexData, we can 
        just attempt to allocate a new one */
    if(!(validBuffer && validAllocation))
    {
        #if KORL_DEBUG
        // korl_log(VERBOSE, "allocating new staging buffer; arrlenu(surfaceContext->stbDaStagingBuffers)==%llu"
        //                 , arrlenu(surfaceContext->stbDaStagingBuffers));
        #endif
        Korl_Vulkan_DeviceMemory_AllocationHandle newBufferAllocationHandle = _korl_vulkan_deviceMemory_allocateBuffer(&surfaceContext->deviceMemoryHostVisible
                                                                                                                      ,stagingBufferArenaBytes
                                                                                                                      ,  VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                                                                                                       | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                                                                                                       | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                                                                                       | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                                                                                       | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                                                                                      ,VK_SHARING_MODE_EXCLUSIVE
                                                                                                                      ,0// 0 => generate new handle automatically
                                                                                                                      ,&validAllocation);
        KORL_ZERO_STACK(_Korl_Vulkan_Buffer, newBuffer);
        newBuffer.allocation = newBufferAllocationHandle;
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), surfaceContext->stbDaStagingBuffers, newBuffer);
        validBuffer     = &arrlast(surfaceContext->stbDaStagingBuffers);
        ///@ASSUMPTION: a reset buffer offset will always satisfy \c alignmentRequired
    }
    /* at this point, we should have a valid buffer, and if not we are 
        essentially "out of memory" */
    korl_assert(validBuffer && validAllocation);
    korl_assert(validAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER);
    surfaceContext->stagingBufferIndexLastUsed = korl_checkCast_u$_to_u16(validBuffer - surfaceContext->stbDaStagingBuffers);
    *out_byteOffsetStagingBuffer = validBuffer->bytesUsed;
    *out_bufferStaging           = validAllocation->subType.buffer.vulkanBuffer;
    void*const bufferMappedAddress = _korl_vulkan_deviceMemory_allocator_getBufferHostVisibleAddress(&surfaceContext->deviceMemoryHostVisible, validBuffer->allocation);
    u8*const resultAddress = KORL_C_CAST(u8*, bufferMappedAddress) + validBuffer->bytesUsed;
    validBuffer->framesSinceLastUsed = 0;
    validBuffer->bytesUsed          += bytesRequired;
    return resultAddress;
}
//@TODO: useless function
korl_internal void* _korl_vulkan_getVertexStagingPool(const Korl_Vulkan_DrawVertexData* vertexData, VkBuffer* out_bufferStaging, VkDeviceSize* out_byteOffsetStagingBuffer)
{
    const VkDeviceSize bytesRequired = (vertexData->positions         ? vertexData->vertexCount * vertexData->positionDimensions           * sizeof(*vertexData->positions)         : 0)
                                     + (vertexData->colors            ? vertexData->vertexCount                                            * sizeof(*vertexData->colors)            : 0)
                                     + (vertexData->uvs               ? vertexData->vertexCount                                            * sizeof(*vertexData->uvs)               : 0)
                                     + (vertexData->indices           ? vertexData->indexCount                                             * sizeof(*vertexData->indices)           : 0)
                                     + (vertexData->instancePositions ? vertexData->instanceCount * vertexData->instancePositionDimensions * sizeof(*vertexData->instancePositions) : 0)
                                     + (vertexData->instanceUint      ? vertexData->instanceCount                                          * sizeof(*vertexData->instanceUint)      : 0);
    return _korl_vulkan_getStagingPool(bytesRequired, /*alignment*/0, out_bufferStaging, out_byteOffsetStagingBuffer);
}
korl_internal void* _korl_vulkan_getDescriptorStagingPool(VkDeviceSize descriptorBytes, VkBuffer* out_bufferStaging, VkDeviceSize* out_byteOffsetStagingBuffer)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    KORL_ZERO_STACK(VkPhysicalDeviceProperties, physicalDeviceProperties);
    vkGetPhysicalDeviceProperties(context->physicalDevice, &physicalDeviceProperties);
    return _korl_vulkan_getStagingPool(descriptorBytes, physicalDeviceProperties.limits.minUniformBufferOffsetAlignment, out_bufferStaging, out_byteOffsetStagingBuffer);
}
korl_internal VkDescriptorSet _korl_vulkan_newDescriptorSet(VkDescriptorSetLayout descriptorSetLayout)
{
    _Korl_Vulkan_Context*const               context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const        surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    korl_shared_const uint32_t MAX_DESCRIPTOR_SETS_PER_POOL = 512;
    /* iterate over each pool in the swap chain image to see if any have 
        available descriptors of the given type */
    _Korl_Vulkan_DescriptorPool* validDescriptorPool = NULL;
    for(u$ p = 0; p < arrlenu(swapChainImageContext->stbDaDescriptorPools); p++)
    {
        _Korl_Vulkan_DescriptorPool*const descriptorPool = &(swapChainImageContext->stbDaDescriptorPools[p]);
        if(descriptorPool->setsAllocated < MAX_DESCRIPTOR_SETS_PER_POOL)
        {
            validDescriptorPool = descriptorPool;
            break;
        }
    }
    /* if we didn't find an available pool, we need to allocate a new one */
    if(!validDescriptorPool)
    {
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaDescriptorPools, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Vulkan_DescriptorPool));
        validDescriptorPool = &(arrlast(swapChainImageContext->stbDaDescriptorPools));
        KORL_ZERO_STACK_ARRAY(VkDescriptorPoolSize, descriptorPoolSizes, 3);
        descriptorPoolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSizes[0].descriptorCount = MAX_DESCRIPTOR_SETS_PER_POOL;
        descriptorPoolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorPoolSizes[1].descriptorCount = MAX_DESCRIPTOR_SETS_PER_POOL;
        descriptorPoolSizes[2].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorPoolSizes[2].descriptorCount = MAX_DESCRIPTOR_SETS_PER_POOL;
        KORL_ZERO_STACK(VkDescriptorPoolCreateInfo, createInfoDescriptorPool);
        createInfoDescriptorPool.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfoDescriptorPool.maxSets       = korl_arraySize(descriptorPoolSizes)*MAX_DESCRIPTOR_SETS_PER_POOL;
        createInfoDescriptorPool.poolSizeCount = korl_arraySize(descriptorPoolSizes);
        createInfoDescriptorPool.pPoolSizes    = descriptorPoolSizes;
        _KORL_VULKAN_CHECK(
            vkCreateDescriptorPool(context->device, &createInfoDescriptorPool, context->allocator
                                  ,&(validDescriptorPool->vkDescriptorPool)));
    }
    /* at this point, we have a descriptor pool that has space for a new set; 
        allocate & return the set */
    korl_assert(validDescriptorPool);
    KORL_ZERO_STACK(VkDescriptorSet, resultDescriptorSet);
    KORL_ZERO_STACK(VkDescriptorSetAllocateInfo, allocInfoDescriptorSets);
    allocInfoDescriptorSets.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfoDescriptorSets.descriptorPool     = validDescriptorPool->vkDescriptorPool;
    allocInfoDescriptorSets.descriptorSetCount = 1;
    allocInfoDescriptorSets.pSetLayouts        = &descriptorSetLayout;
    _KORL_VULKAN_CHECK(
        vkAllocateDescriptorSets(context->device, &allocInfoDescriptorSets
                                ,&resultDescriptorSet));
    validDescriptorPool->setsAllocated++;
    return resultDescriptorSet;
}
korl_internal void _korl_vulkan_frameBegin(void)
{
    _Korl_Vulkan_Context*const          context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const   surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext* swapChainImageContext = NULL;
    /* it is possible that we do not have the ability to render anything this 
        frame, so we need to check the state of the swap chain image index for 
        each of the exposed rendering APIs - if it remains unchanged from this 
        initial assigned value, then we need to just not do anything in those 
        functions which require a valid swap chain image context!!! */
    surfaceContext->frameSwapChainImageIndex = _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE;
    bool unableToAcquireSwapChainImage = false;
    /* to begin the frame, we need to: 
        - resize the swap chain if any events triggered a deferred resize
        - acquire the next swap chain image
        - begin the new command buffer, including a command to clear it */
    if(surfaceContext->deferredResize)
    {
        // only perform swapchain resize the swapchain if the area is non-zero!
        // swapchain area can be zero in various situations, such as a minimized window
        if(   surfaceContext->deferredResizeX == 0 
           || surfaceContext->deferredResizeY == 0)
            unableToAcquireSwapChainImage = true;
        else
        {
            korl_log(VERBOSE, "resizing to: %ux%u", surfaceContext->deferredResizeX
                                                  , surfaceContext->deferredResizeY);
            /* destroy swap chain & all resources which depend on it implicitly */
            _korl_vulkan_destroySwapChain();
            /* since the device surface meta data has changed, we need to query it */
            const _Korl_Vulkan_DeviceSurfaceMetaData deviceSurfaceMetaData = _korl_vulkan_deviceSurfaceMetaData(context->physicalDevice, surfaceContext->surface);
            /* re-create the swap chain & all resources which depend on it */
            _korl_vulkan_createSwapChain(surfaceContext->deferredResizeX
                                        ,surfaceContext->deferredResizeY
                                        ,&deviceSurfaceMetaData);
            /* re-create pipelines */
            const _Korl_Vulkan_Pipeline*const pipelinesEnd = context->stbDaPipelines + arrlen(context->stbDaPipelines);
            for(_Korl_Vulkan_Pipeline* pipeline = context->stbDaPipelines; pipeline < pipelinesEnd; pipeline++)
            {
                const u$ p = pipeline - context->stbDaPipelines;
                korl_assert(pipeline->pipeline != VK_NULL_HANDLE);
                vkDestroyPipeline(context->device, pipeline->pipeline, context->allocator);
                pipeline->pipeline = VK_NULL_HANDLE;
                _korl_vulkan_createPipeline(p);
            }
            //KORL-ISSUE-000-000-025: re-record command buffers?...
            /* clear the deferred resize flag for the next frame */
            surfaceContext->deferredResize = false;
        }
    }
    /* wait on the next "wip frame" synchronization struct so that we have a set 
        of sync primitives that aren't currently being used by the device */
    if(surfaceContext->wipFrameCount >= surfaceContext->swapChainImagesSize)
        _KORL_VULKAN_CHECK(
            vkWaitForFences(context->device, 1
                           ,&surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fenceFrameComplete
                           ,VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/));
    /* at this point, we know for certain that a frame has been processed, so we 
        can update our memory pools to reflect this history, allowing us to 
        reuse pools that we can deduce _must_ no longer be in use */
    for(u$ b = 0; b < arrlenu(surfaceContext->stbDaStagingBuffers); b++)
    {
        _Korl_Vulkan_Buffer*const buffer = &(surfaceContext->stbDaStagingBuffers[b]);
        if(buffer->framesSinceLastUsed < KORL_U8_MAX)
            buffer->framesSinceLastUsed++;
    }
    /* we can now dequeue deep destruction of device memory objects that we know 
        are no longer being used by the device anymore */
    for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(surfaceContext->deviceLocalAllocationShallowDequeueBatches); )
    {
        _Korl_Vulkan_DeviceLocalAllocationShallowDequeueBatch*const batch = &(surfaceContext->deviceLocalAllocationShallowDequeueBatches[i]);
        if(batch->framesSinceQueued < KORL_U8_MAX)
            batch->framesSinceQueued++;
        if(batch->framesSinceQueued > surfaceContext->swapChainImagesSize)
        {
            _korl_vulkan_deviceMemory_allocator_freeShallowDequeue(&surfaceContext->deviceMemoryDeviceLocal, batch->dequeueCount);
            KORL_MEMORY_POOL_REMOVE(surfaceContext->deviceLocalAllocationShallowDequeueBatches, i);
        }
        else
            i++;
    }
    /* same as with staging buffers, we can now nullify device assets that we 
        know are no longer being used */
    u$ freedDeviceLocalAllocations = 0;
    u8 framesSinceQueuedLast = KORL_U8_MAX;// used to help ensure that the device local free queue does in fact contain monotonically increasing values for each member's framesSinceQueued value
    for(_Korl_Vulkan_QueuedFreeDeviceLocalAllocation* queuedFreeDeviceAllocation = surfaceContext->stbDaDeviceLocalFreeQueue;
        queuedFreeDeviceAllocation < surfaceContext->stbDaDeviceLocalFreeQueue + arrlen(surfaceContext->stbDaDeviceLocalFreeQueue);
        queuedFreeDeviceAllocation++)
    {
        korl_assert(queuedFreeDeviceAllocation->framesSinceQueued <= framesSinceQueuedLast);// items later in the queue are newer, ergo should have <= the # of frames of later items
        framesSinceQueuedLast = queuedFreeDeviceAllocation->framesSinceQueued;
        if(queuedFreeDeviceAllocation->framesSinceQueued < KORL_U8_MAX)
            queuedFreeDeviceAllocation->framesSinceQueued++;
        if(queuedFreeDeviceAllocation->framesSinceQueued > surfaceContext->swapChainImagesSize)
        {
            _korl_vulkan_deviceMemory_allocator_free(&surfaceContext->deviceMemoryDeviceLocal, queuedFreeDeviceAllocation->allocationHandle);
            freedDeviceLocalAllocations++;
        }
    }
    if(freedDeviceLocalAllocations)
        arrdeln(surfaceContext->stbDaDeviceLocalFreeQueue, 0, freedDeviceLocalAllocations);
    /* similarly to the above, we can now destroy shaders that we know are no longer in use */
    {
        const _Korl_Vulkan_ShaderTrash* shaderTrashEnd = context->stbDaShaderTrash + arrlen(context->stbDaShaderTrash);
        for(_Korl_Vulkan_ShaderTrash* shaderTrash = context->stbDaShaderTrash; shaderTrash < shaderTrashEnd; )
        {
            if(shaderTrash->framesSinceQueued < KORL_U8_MAX)
                shaderTrash->framesSinceQueued++;
            if(shaderTrash->framesSinceQueued > surfaceContext->swapChainImagesSize)
            {
                vkDestroyShaderModule(context->device, shaderTrash->shader.shaderModule, context->allocator);
                /* delete all pipelines that use this shader module, since we know they are also no longer in use by the device */
                const _Korl_Vulkan_Pipeline* pipelinesEnd = context->stbDaPipelines + arrlen(context->stbDaPipelines);
                for(_Korl_Vulkan_Pipeline* pipeline = context->stbDaPipelines; pipeline < pipelinesEnd; )
                {
                    if(   pipeline->shaderVertex   == shaderTrash->shader.shaderModule
                       || pipeline->shaderFragment == shaderTrash->shader.shaderModule)
                    {
                        vkDestroyPipeline(context->device, pipeline->pipeline, context->allocator);
                        *pipeline = arrlast(context->stbDaPipelines);
                        pipelinesEnd--;
                        arrsetlen(context->stbDaPipelines, pipelinesEnd - context->stbDaPipelines);
                    }
                    else
                        pipeline++;
                }
                /* delete this ShaderTrash by simply replacing it with the last one */
                *shaderTrash = arrlast(context->stbDaShaderTrash);
                shaderTrashEnd--;
                arrsetlen(context->stbDaShaderTrash, shaderTrashEnd - context->stbDaShaderTrash);
            }
            else
                shaderTrash++;
        }
    }
    #if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
        /* reset the debug pool of device asset indices */
        mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaInUseDeviceAssetIndices, 0);
    #endif
    if(!unableToAcquireSwapChainImage)
    {
        /* acquire the next image from the swap chain */
        const VkResult resultAcquireNextImage = 
            vkAcquireNextImageKHR(context->device, surfaceContext->swapChain
                                 ,UINT64_MAX/*timeout; UINT64_MAX -> disable*/
                                 ,surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreImageAvailable
                                 ,VK_NULL_HANDLE/*fence*/
                                 ,/*return value address*/&surfaceContext->frameSwapChainImageIndex);
        _KORL_VULKAN_CHECK(resultAcquireNextImage);
        swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
        /* reset all descriptor pools for this swap chain image */
        for(u$ d = 0; d < arrlenu(swapChainImageContext->stbDaDescriptorPools); d++)
        {
            _Korl_Vulkan_DescriptorPool*const descriptorPool = &(swapChainImageContext->stbDaDescriptorPools[d]);
            const VkDescriptorPool vkDescriptorPool = descriptorPool->vkDescriptorPool;
            korl_memory_zero(descriptorPool, sizeof(*descriptorPool));
            descriptorPool->vkDescriptorPool = vkDescriptorPool;
            _KORL_VULKAN_CHECK(vkResetDescriptorPool(context->device, vkDescriptorPool, /*flags; reserved*/0));
        }
    }
    /* ----- begin the swap chain command buffer for this frame ----- */
    /* free the primary command buffers from the previous frame, since they are 
        no longer valid */
    vkFreeCommandBuffers(context->device, surfaceContext->commandPool, 1, &surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics);
    vkFreeCommandBuffers(context->device, surfaceContext->commandPool, 1, &surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer);
    /* allocate a command buffer for each of the swap chain frame buffers 
        (graphics & transfer) */
    KORL_ZERO_STACK(VkCommandBufferAllocateInfo, allocateInfoCommandBuffers);
    allocateInfoCommandBuffers.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfoCommandBuffers.commandPool        = surfaceContext->commandPool;
    allocateInfoCommandBuffers.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfoCommandBuffers.commandBufferCount = 1;
    _KORL_VULKAN_CHECK(vkAllocateCommandBuffers(context->device, &allocateInfoCommandBuffers
                                               ,&surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer));
    if(swapChainImageContext)
        _KORL_VULKAN_CHECK(vkAllocateCommandBuffers(context->device, &allocateInfoCommandBuffers
                                                   ,&surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics));
    else
        surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics = VK_NULL_HANDLE;
    KORL_ZERO_STACK(VkCommandBufferBeginInfo, beginInfoCommandBuffer);
    beginInfoCommandBuffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfoCommandBuffer.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    _KORL_VULKAN_CHECK(vkBeginCommandBuffer(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer, &beginInfoCommandBuffer));
    if(swapChainImageContext)
        _KORL_VULKAN_CHECK(vkBeginCommandBuffer(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics, &beginInfoCommandBuffer));
    if(swapChainImageContext)
    {
        // define the color we are going to clear the color attachment with when 
        //    the render pass begins:
        KORL_ZERO_STACK_ARRAY(VkClearValue, clearValues, 2);
        clearValues[0].color.float32[0] = surfaceContext->frameBeginClearColor.elements[0];
        clearValues[0].color.float32[1] = surfaceContext->frameBeginClearColor.elements[1];
        clearValues[0].color.float32[2] = surfaceContext->frameBeginClearColor.elements[2];
        clearValues[0].color.float32[3] = 1.f;
        clearValues[1].depthStencil.depth   = _KORL_VULKAN_DEPTH_CLEAR_VALUE;
        clearValues[1].depthStencil.stencil = 0;
        KORL_ZERO_STACK(VkRenderPassBeginInfo, beginInfoRenderPass);
        beginInfoRenderPass.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfoRenderPass.renderPass        = context->renderPass;
        beginInfoRenderPass.framebuffer       = swapChainImageContext->frameBuffer;
        beginInfoRenderPass.renderArea.extent = surfaceContext->swapChainImageExtent;
        beginInfoRenderPass.clearValueCount   = korl_arraySize(clearValues);
        beginInfoRenderPass.pClearValues      = clearValues;
        vkCmdBeginRenderPass(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                            ,&beginInfoRenderPass, VK_SUBPASS_CONTENTS_INLINE);
    }
    /* clear the vertex batch metrics for the upcoming frame */
    KORL_ZERO_STACK(VkRect2D, scissorDefault);
    scissorDefault.offset = (VkOffset2D){.x = 0, .y = 0};
    scissorDefault.extent = surfaceContext->swapChainImageExtent;
    korl_memory_zero(&surfaceContext->drawState    , sizeof(surfaceContext->drawState));
    korl_memory_zero(&surfaceContext->drawStateLast, sizeof(surfaceContext->drawStateLast));
    surfaceContext->drawState.pushConstants.vertex.m4f32Model    = KORL_MATH_M4F32_IDENTITY;
    surfaceContext->drawState.pushConstants.fragment.uvAabb      = (Korl_Math_V4f32){0,0,1,1};
    surfaceContext->drawState.uboSceneProperties.m4f32View       = KORL_MATH_M4F32_IDENTITY;
    surfaceContext->drawState.uboSceneProperties.m4f32Projection = KORL_MATH_M4F32_IDENTITY;
    surfaceContext->drawState.pipelineConfigurationCache         = _korl_vulkan_pipeline_default();
    surfaceContext->drawState.scissor                            = surfaceContext->drawState.scissor = scissorDefault;
    surfaceContext->drawState.uboMaterialProperties              = korl_gfx_material_defaultUnlit(korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE)).properties;
    surfaceContext->drawState.materialMaps.base                  = surfaceContext->defaultTexture;
    surfaceContext->drawState.materialMaps.specular              = surfaceContext->defaultTexture;
    surfaceContext->drawState.materialMaps.emissive              = surfaceContext->defaultTexture;
    // setting the current pipeline index to be out of bounds effectively sets 
    //  the pipeline produced from _korl_vulkan_pipeline_default to be used
    surfaceContext->drawState.currentPipeline = KORL_U$_MAX;//KORL-ISSUE-000-000-148: vulkan: this is gross, as using an arbitrarily large integer here theoretically still leaves us in the situation where the index can suddenly become valid if new pipelines are created, even if this is basically impossible to reach KORL_U$_MAX pipelines
}
/** This API is platform-specific, and thus must be defined within the code base 
 * of whatever the current target platform is. */
korl_internal void _korl_vulkan_createSurface(void* userData);
korl_internal Korl_Vulkan_VertexIndex korl_vulkan_safeCast_u$_to_vertexIndex(u$ x)
{
    korl_assert(x <= KORL_C_CAST(Korl_Vulkan_VertexIndex, ~0));
    return KORL_C_CAST(Korl_Vulkan_VertexIndex, x);
}
korl_internal void korl_vulkan_construct(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /* sanity check - ensure that the memory is nullified */
    korl_assert(korl_memory_isNull(context, sizeof(*context)));
    /**/
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(8);
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_CRT, L"korl-vulkan", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    /* get a list of VkLayerProperties so we can check of validation layer 
        support if needed */
    KORL_ZERO_STACK(u32, layerCount);
    _KORL_VULKAN_CHECK(
        vkEnumerateInstanceLayerProperties(&layerCount, NULL/*pProperties; NULL->return # of properties in param 1*/));
    VkLayerProperties layerProperties[128];
    korl_assert(layerCount <= korl_arraySize(layerProperties));
    layerCount = korl_arraySize(layerProperties);
    _KORL_VULKAN_CHECK(
        vkEnumerateInstanceLayerProperties(&layerCount, layerProperties));
    korl_log(INFO, "Provided layers supported by this platform "
                   "{\"name\"[specVersion, implementationVersion]}:");
    for(u32 l = 0; l < layerCount; l++)
    {
        korl_log(INFO, "layer[%u]=\"%hs\"[%u, %u]", l, 
                 layerProperties[l].layerName, layerProperties[l].specVersion, 
                 layerProperties[l].implementationVersion);
    }
    /* select layers & check to see if they are supported */
    for(u32 el = 0; el < korl_arraySize(G_KORL_VULKAN_ENABLED_LAYERS); el++)
    {
        bool enabledLayerSupported = false;
        for(u32 l = 0; l < layerCount; l++)
            if(strcmp(G_KORL_VULKAN_ENABLED_LAYERS[el], 
                      layerProperties[l].layerName))
            {
                enabledLayerSupported = true;
                break;
            }
        korl_assert(enabledLayerSupported);
    }
    /* log the list of instance extensions supported by the platform */
    KORL_ZERO_STACK(u32, extensionCount);
    VkExtensionProperties extensionProperties[128];
    _KORL_VULKAN_CHECK(
        vkEnumerateInstanceExtensionProperties(NULL/*pLayerName*/, &extensionCount, 
                                               NULL/*pProperties; NULL->return # of properties in param 2*/));
    korl_assert(extensionCount <= korl_arraySize(extensionProperties));
    extensionCount = korl_arraySize(extensionProperties);
    _KORL_VULKAN_CHECK(
        vkEnumerateInstanceExtensionProperties(NULL/*pLayerName*/, &extensionCount, extensionProperties));
    korl_log(INFO, "Provided extensions supported by this platform "
        "{\"name\"[specVersion]}:");
    for(u32 e = 0; e < extensionCount; e++)
        korl_log(INFO, "extension[%u]=\"%hs\"[%u]", e, 
                 extensionProperties[e].extensionName, 
                 extensionProperties[e].specVersion);
    /* create the VkInstance */
    //KORL-ISSUE-000-000-024: robustness: cross-check this list of extensions
    const char* enabledExtensions[] = 
        { VK_KHR_SURFACE_EXTENSION_NAME
        #if defined(KORL_PLATFORM_WINDOWS)
        , VK_KHR_WIN32_SURFACE_EXTENSION_NAME
        #endif// defined(KORL_PLATFORM_WINDOWS)
        #if KORL_DEBUG
        , VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        #endif// KORL_DEBUG
        };
    KORL_ZERO_STACK(VkApplicationInfo, applicationInfo);
    applicationInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "KORL Application";
    applicationInfo.pEngineName      = "KORL";
    applicationInfo.engineVersion    = 0;
    applicationInfo.apiVersion       = VK_API_VERSION_1_3;
    KORL_ZERO_STACK(VkInstanceCreateInfo, createInfoInstance);
    createInfoInstance.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfoInstance.pApplicationInfo        = &applicationInfo;
    createInfoInstance.enabledExtensionCount   = korl_arraySize(enabledExtensions);
    createInfoInstance.ppEnabledExtensionNames = enabledExtensions;
#if KORL_DEBUG
    createInfoInstance.enabledLayerCount       = korl_arraySize(G_KORL_VULKAN_ENABLED_LAYERS);
    createInfoInstance.ppEnabledLayerNames     = G_KORL_VULKAN_ENABLED_LAYERS;
    KORL_ZERO_STACK(VkDebugUtilsMessengerCreateInfoEXT, createInfoDebugUtilsMessenger);
    createInfoDebugUtilsMessenger.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfoDebugUtilsMessenger.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfoDebugUtilsMessenger.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfoDebugUtilsMessenger.pfnUserCallback = _korl_vulkan_debugUtilsMessengerCallback;
    /* append the debug utils messenger create info to the instance create info 
        so we can get debug info messages in create/destroy instance API */
    createInfoDebugUtilsMessenger.pNext = createInfoInstance.pNext;
    createInfoInstance.pNext = &createInfoDebugUtilsMessenger;
#endif// KORL_DEBUG
    _KORL_VULKAN_CHECK(
        vkCreateInstance(&createInfoInstance, context->allocator, &context->instance));
    /* get instance function pointers */
    _KORL_VULKAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfaceSupportKHR);
    _KORL_VULKAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    _KORL_VULKAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfaceFormatsKHR);
    _KORL_VULKAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfacePresentModesKHR);
#if KORL_DEBUG
    /* get debug extension function pointers */
    // note: there's no need to store this in the context since we're never 
    //  going to call it again //
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = 
        KORL_C_CAST(PFN_vkCreateDebugUtilsMessengerEXT, 
                    vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT"));
    korl_assert(vkCreateDebugUtilsMessengerEXT);
    _KORL_VULKAN_GET_INSTANCE_PROC_ADDR(context, DestroyDebugUtilsMessengerEXT);
    /* attach a VkDebugUtilsMessengerEXT to our instance */
    _KORL_VULKAN_CHECK(
        vkCreateDebugUtilsMessengerEXT(context->instance, &createInfoDebugUtilsMessenger, 
                                       context->allocator, &context->debugMessenger));
#endif// KORL_DEBUG
    context->stringPool = korl_stringPool_create(context->allocatorHandle);
}
korl_internal void korl_vulkan_destroy(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
#if KORL_DEBUG
    context->vkDestroyDebugUtilsMessengerEXT(context->instance, context->debugMessenger, context->allocator);
    vkDestroyInstance(context->instance, context->allocator);
    korl_memory_allocator_destroy(context->allocatorHandle);
    /* nullify the context after cleaning up properly for safety */
    korl_memory_zero(context, sizeof(*context));
#else
    /* we only need to manually destroy the vulkan module if we're running a 
        debug build, since the validation layers require us to properly clean up 
        before the program ends, but for release builds we really don't care at 
        all about wasting time like this if we don't have to */
#endif// KORL_DEBUG
}
korl_internal void korl_vulkan_createSurface(void* createSurfaceUserData, u32 sizeX, u32 sizeY)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* we have to create the OS-specific surface before choosing a physical 
        device to create the logical device on */
    _korl_vulkan_createSurface(createSurfaceUserData);
    /* enumerate & choose a physical device */
    KORL_ZERO_STACK(u32, physicalDeviceCount);
    _KORL_VULKAN_CHECK(
        vkEnumeratePhysicalDevices(context->instance, &physicalDeviceCount, 
                                   NULL/*pPhysicalDevices; NULL->return the count in param 2*/));
    // we need at least one physical device compatible with Vulkan
    korl_assert(physicalDeviceCount > 0);
    VkPhysicalDevice physicalDevices[16];
    korl_assert(physicalDeviceCount <= korl_arraySize(physicalDevices));
    physicalDeviceCount = korl_arraySize(physicalDevices);
    _KORL_VULKAN_CHECK(
        vkEnumeratePhysicalDevices(context->instance, &physicalDeviceCount, physicalDevices));
    KORL_ZERO_STACK(VkPhysicalDeviceProperties, devicePropertiesBest);
    KORL_ZERO_STACK(VkPhysicalDeviceFeatures, deviceFeaturesBest);
    KORL_ZERO_STACK(u32, queueFamilyCountBest);
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceSurfaceMetaData, deviceSurfaceMetaDataBest);
    for(u32 d = 0; d < physicalDeviceCount; d++)
    {
        KORL_ZERO_STACK(u32, queueFamilyCount);
        VkQueueFamilyProperties queueFamilies[32];
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[d], &queueFamilyCount, 
                                                 NULL/*pQueueFamilyProperties; NULL->return the count via param 2*/);
        korl_assert(queueFamilyCount <= korl_arraySize(queueFamilies));
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[d], &queueFamilyCount, queueFamilies);
        _Korl_Vulkan_QueueFamilyMetaData queueFamilyMetaData = 
            _korl_vulkan_queueFamilyMetaData(physicalDevices[d], queueFamilyCount, queueFamilies, 
                                             surfaceContext->surface);
        KORL_ZERO_STACK(VkPhysicalDeviceProperties, deviceProperties);
        KORL_ZERO_STACK(VkPhysicalDeviceFeatures, deviceFeatures);
        vkGetPhysicalDeviceProperties(physicalDevices[d], &deviceProperties);
        vkGetPhysicalDeviceFeatures(physicalDevices[d], &deviceFeatures);
        _Korl_Vulkan_DeviceSurfaceMetaData deviceSurfaceMetaData = 
            _korl_vulkan_deviceSurfaceMetaData(physicalDevices[d], surfaceContext->surface);
        if(_korl_vulkan_isBetterDevice(context->physicalDevice, physicalDevices[d]
                                      ,&queueFamilyMetaData, &deviceSurfaceMetaData
                                      ,&devicePropertiesBest, &deviceProperties
                                      ,&deviceFeaturesBest, &deviceFeatures))
        {
            context->physicalDevice      = physicalDevices[d];
            devicePropertiesBest         = deviceProperties;
            deviceFeaturesBest           = deviceFeatures;
            context->queueFamilyMetaData = queueFamilyMetaData;
            deviceSurfaceMetaDataBest    = deviceSurfaceMetaData;
        }
    }
    korl_assert(context->physicalDevice != VK_NULL_HANDLE);
    const wchar_t* stringDeviceType = L"unknown";
    switch(devicePropertiesBest.deviceType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:         { stringDeviceType = L"OTHER";          break;}
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:{ stringDeviceType = L"INTEGRATED_GPU"; break;}
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:  { stringDeviceType = L"DISCRETE_GPU";   break;}
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:   { stringDeviceType = L"VIRTUAL_GPU";    break;}
    case VK_PHYSICAL_DEVICE_TYPE_CPU:           { stringDeviceType = L"CPU";            break;}
    default:{break;}
    }
    u32 deviceDriverVersionVariant = VK_API_VERSION_VARIANT(devicePropertiesBest.driverVersion);
    u32 deviceDriverVersionMajor   = VK_API_VERSION_MAJOR(devicePropertiesBest.driverVersion);
    u32 deviceDriverVersionMinor   = VK_API_VERSION_MINOR(devicePropertiesBest.driverVersion);
    u32 deviceDriverVersionPatch   = VK_API_VERSION_PATCH(devicePropertiesBest.driverVersion);
    if(devicePropertiesBest.vendorID == 4318)// NVIDIA
    {
        deviceDriverVersionVariant = (devicePropertiesBest.driverVersion >> 22) & 0x3FF;
        deviceDriverVersionMajor   = (devicePropertiesBest.driverVersion >> 14) & 0x0FF;
        deviceDriverVersionMinor   = (devicePropertiesBest.driverVersion >>  6) & 0x0FF;
        deviceDriverVersionPatch   = (devicePropertiesBest.driverVersion >>  0) &  0x3F;
    }
    #ifdef KORL_PLATFORM_WINDOWS//KORL-ISSUE-000-000-060: Vulkan: test & verify driver version decoding on Intel+Windows
    else if(devicePropertiesBest.vendorID == 0x8086)// Intel
    {
        deviceDriverVersionMajor = devicePropertiesBest.driverVersion >> 14;
        deviceDriverVersionMinor = devicePropertiesBest.driverVersion & 0x3FFF;
    }
    #endif
    korl_log(INFO, "chosen physical device: \"%hs\"", devicePropertiesBest.deviceName);
    korl_log(INFO, "vendorID=0x%X|%u, apiVersion=0x%X|%u|%u.%u.%u.%u, driverVersion=0x%X|%u|%u.%u.%u.%u, deviceID=0x%X|%u, deviceType=%ws"
            ,devicePropertiesBest.vendorID, devicePropertiesBest.vendorID
            ,devicePropertiesBest.apiVersion, devicePropertiesBest.apiVersion
            ,VK_API_VERSION_VARIANT(devicePropertiesBest.apiVersion)
            ,VK_API_VERSION_MAJOR(devicePropertiesBest.apiVersion)
            ,VK_API_VERSION_MINOR(devicePropertiesBest.apiVersion)
            ,VK_API_VERSION_PATCH(devicePropertiesBest.apiVersion)
            ,devicePropertiesBest.driverVersion, devicePropertiesBest.driverVersion
            ,deviceDriverVersionVariant
            ,deviceDriverVersionMajor
            ,deviceDriverVersionMinor
            ,deviceDriverVersionPatch
            ,devicePropertiesBest.deviceID, devicePropertiesBest.deviceID
            ,stringDeviceType);
    /* log the list of extensions provided by the chosen device */
    {
        VkExtensionProperties extensionProperties[256];
        KORL_ZERO_STACK(u32, extensionPropertiesSize);
        _KORL_VULKAN_CHECK(
            vkEnumerateDeviceExtensionProperties(context->physicalDevice, NULL/*pLayerName*/, &extensionPropertiesSize, NULL/*pProperties*/));
        korl_assert(extensionPropertiesSize <= korl_arraySize(extensionProperties));
        extensionPropertiesSize = korl_arraySize(extensionProperties);
        _KORL_VULKAN_CHECK(
            vkEnumerateDeviceExtensionProperties(context->physicalDevice, NULL/*pLayerName*/, &extensionPropertiesSize, extensionProperties));
        korl_log(INFO, "physical device extensions:");
        for(u32 e = 0; e < extensionPropertiesSize; e++)
            korl_log(INFO, "[%u]: \"%hs\"", e, extensionProperties[e].extensionName);
    }
    /* determine how many queue families we need, which determines how many 
        VkDeviceQueueCreateInfo structs we will need to create the logical 
        device with our specifications */
    VkDeviceQueueCreateInfo createInfoDeviceQueueFamilies[8];
    KORL_ZERO_STACK(u32, createInfoDeviceQueueFamiliesSize);
    // we're only going to make one queue for each queue family, so this array 
    //  size is just going to be size == 1 for now
    korl_shared_const f32 QUEUE_PRIORITIES[] = {1.f};
    for(u32 f = 0; 
        f < korl_arraySize(context->queueFamilyMetaData.indexQueueUnion.indices); 
        f++)
    {
        KORL_ZERO_STACK(bool, queueFamilyCreateInfoFound);
        for(u32 c = 0; c < createInfoDeviceQueueFamiliesSize; c++)
        {
            if(createInfoDeviceQueueFamilies[c].queueFamilyIndex == context->queueFamilyMetaData.indexQueueUnion.indices[f])
            {
                queueFamilyCreateInfoFound = true;
                break;
            }
        }
        if(queueFamilyCreateInfoFound)
            continue;
        korl_assert(createInfoDeviceQueueFamiliesSize < 
            korl_arraySize(createInfoDeviceQueueFamilies));
        VkDeviceQueueCreateInfo*const createInfo = 
            &createInfoDeviceQueueFamilies[createInfoDeviceQueueFamiliesSize];
        korl_memory_zero(createInfo, sizeof(*createInfo));
        createInfo->sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        createInfo->queueFamilyIndex = context->queueFamilyMetaData.indexQueueUnion.indices[f];
        createInfo->queueCount       = korl_arraySize(QUEUE_PRIORITIES);
        createInfo->pQueuePriorities = QUEUE_PRIORITIES;
        createInfoDeviceQueueFamiliesSize++;
    }
    /* create logical device */
    KORL_ZERO_STACK(VkPhysicalDeviceFeatures, physicalDeviceFeatures);
    physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
    physicalDeviceFeatures.fillModeNonSolid  = VK_TRUE;// allows VkPolygonMode values other than FILL, easier wireframe drawing
    KORL_ZERO_STACK(VkPhysicalDeviceVulkan13Features, physicalDeviceFeatures13);
    physicalDeviceFeatures13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    physicalDeviceFeatures13.synchronization2 = VK_TRUE;
    KORL_ZERO_STACK(VkDeviceCreateInfo, createInfoDevice);
    createInfoDevice.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfoDevice.pNext                   = &physicalDeviceFeatures13;
    createInfoDevice.queueCreateInfoCount    = createInfoDeviceQueueFamiliesSize;
    createInfoDevice.pQueueCreateInfos       = createInfoDeviceQueueFamilies;
    createInfoDevice.pEnabledFeatures        = &physicalDeviceFeatures;
    createInfoDevice.enabledExtensionCount   = korl_arraySize(G_KORL_VULKAN_DEVICE_EXTENSIONS);
    createInfoDevice.ppEnabledExtensionNames = G_KORL_VULKAN_DEVICE_EXTENSIONS;
    #if KORL_DEBUG
    createInfoDevice.enabledLayerCount       = korl_arraySize(G_KORL_VULKAN_ENABLED_LAYERS);
    createInfoDevice.ppEnabledLayerNames     = G_KORL_VULKAN_ENABLED_LAYERS;
    #endif// KORL_DEBUG
    _KORL_VULKAN_CHECK(
        vkCreateDevice(context->physicalDevice, &createInfoDevice
                      ,context->allocator, &context->device));
    /* obtain opaque handles to the queues that we need */
    vkGetDeviceQueue(context->device
                    ,context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics
                    ,0/*queueIndex*/, &context->queueGraphics);
    vkGetDeviceQueue(context->device
                    ,context->queueFamilyMetaData.indexQueueUnion.indexQueues.present
                    ,0/*queueIndex*/, &context->queuePresent);
    /* create the device memory allocator we will require to store certain swap 
        chain dependent resources, such as the depth buffer, stencil buffer, etc. */
    surfaceContext->deviceMemoryRenderResources = _korl_vulkan_deviceMemory_allocator_create(context->allocatorHandle
                                                                                            ,_KORL_VULKAN_DEVICE_MEMORY_ALLOCATOR_TYPE_GENERAL
                                                                                            ,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                                                                            ,/*bufferUsageFlags*/0
                                                                                            ,/*imageUsageFlags*/VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                                                            ,korl_math_megabytes(64));
    /* create device memory allocator used to store host-visible data, such as 
        staging buffers for vertex attributes, textures, etc... */
    surfaceContext->deviceMemoryHostVisible = _korl_vulkan_deviceMemory_allocator_create(context->allocatorHandle
                                                                                        ,_KORL_VULKAN_DEVICE_MEMORY_ALLOCATOR_TYPE_GENERAL
                                                                                        ,  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 
                                                                                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT// KORL-PERFORMANCE-000-000-034: vulkan: we could potentially get more performance here by removing HOST_COHERENT & manually calling vkFlushMappedMemoryRanges & vkInvalidateMappedMemoryRanges on bulk memory ranges; timings necessary
                                                                                        ,  VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                                                                         | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT// KORL-PERFORMANCE-000-000-033: vulkan: potentially better device performance by removing these *_BUFFER bits, then adding a device-local vertex data pool to which we transfer all vertex data to
                                                                                         | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                                                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                                                        ,/*image usage flags*/0
                                                                                        ,korl_math_megabytes(32));
    /* create a device memory allocator used to store device-local data, such as 
        mesh manifolds, SSBOs, textures, etc...  mostly things that persist for 
        many frames and have a high cost associated with data transfers to the 
        device */
    surfaceContext->deviceMemoryDeviceLocal = _korl_vulkan_deviceMemory_allocator_create(context->allocatorHandle
                                                                                        ,_KORL_VULKAN_DEVICE_MEMORY_ALLOCATOR_TYPE_GENERAL
                                                                                        ,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                                                                        ,VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                                                                        ,/*image usage flags*/0
                                                                                        ,korl_math_megabytes(32));
    /* initialize staging buffers collection */
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), surfaceContext->stbDaStagingBuffers, 4);
    /* now that the device is created we can create the swap chain 
        - this also requires the command pool since we need to create the 
          graphics command buffers for each element of the swap chain
        - we also have to do this BEFORE creating the vertex batch memory 
          allocations/buffers, because this is where we determine the value of 
          `surfaceContext->swapChainImagesSize`!*/
    _korl_vulkan_createSwapChain(sizeX, sizeY, &deviceSurfaceMetaDataBest);
    /* create frame synchronization objects, now that we know the value of 
        swapChainImages size; this is separate since these objects don't need to 
        be created/destroyed each time the swap chain is created/destroyed */
    {
        KORL_ZERO_STACK(VkSemaphoreCreateInfo, createInfoSemaphore);
        createInfoSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        KORL_ZERO_STACK(VkFenceCreateInfo, createInfoFence);
        createInfoFence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        createInfoFence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
        {
            _KORL_VULKAN_CHECK(vkCreateSemaphore(context->device, &createInfoSemaphore, context->allocator, &surfaceContext->wipFrames[i].semaphoreTransfersDone));
            _KORL_VULKAN_CHECK(vkCreateSemaphore(context->device, &createInfoSemaphore, context->allocator, &surfaceContext->wipFrames[i].semaphoreImageAvailable));
            _KORL_VULKAN_CHECK(vkCreateSemaphore(context->device, &createInfoSemaphore, context->allocator, &surfaceContext->wipFrames[i].semaphoreRenderDone));
            _KORL_VULKAN_CHECK(vkCreateFence    (context->device, &createInfoFence    , context->allocator, &surfaceContext->wipFrames[i].fenceFrameComplete));
        }
    }
    /* --- create batch descriptor set layouts --- 
        _must_ be created _before_ the pipelines are created */
    {/* SCENE_TRANSFORMS descriptor set layout */
        KORL_ZERO_STACK(VkDescriptorSetLayoutCreateInfo, createInfoDescriptorSetLayout);
        createInfoDescriptorSetLayout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfoDescriptorSetLayout.bindingCount = korl_arraySize(_KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_SCENE_TRANSFORMS);
        createInfoDescriptorSetLayout.pBindings    = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_SCENE_TRANSFORMS;
        _KORL_VULKAN_CHECK(vkCreateDescriptorSetLayout(context->device, &createInfoDescriptorSetLayout, context->allocator
                                                      ,&context->descriptorSetLayouts[_KORL_VULKAN_DESCRIPTOR_SET_INDEX_SCENE]));
    }
    {/* LIGHTS descriptor set layout */
        KORL_ZERO_STACK(VkDescriptorSetLayoutCreateInfo, createInfoDescriptorSetLayout);
        createInfoDescriptorSetLayout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfoDescriptorSetLayout.bindingCount = korl_arraySize(_KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_LIGHTS);
        createInfoDescriptorSetLayout.pBindings    = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_LIGHTS;
        _KORL_VULKAN_CHECK(vkCreateDescriptorSetLayout(context->device, &createInfoDescriptorSetLayout, context->allocator
                                                      ,&context->descriptorSetLayouts[_KORL_VULKAN_DESCRIPTOR_SET_INDEX_LIGHTS]));
    }
    {/* STORAGE descriptor set layout */
        KORL_ZERO_STACK(VkDescriptorSetLayoutCreateInfo, createInfoDescriptorSetLayout);
        createInfoDescriptorSetLayout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfoDescriptorSetLayout.bindingCount = korl_arraySize(_KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_STORAGE);
        createInfoDescriptorSetLayout.pBindings    = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_STORAGE;
        _KORL_VULKAN_CHECK(vkCreateDescriptorSetLayout(context->device, &createInfoDescriptorSetLayout, context->allocator
                                                      ,&context->descriptorSetLayouts[_KORL_VULKAN_DESCRIPTOR_SET_INDEX_STORAGE]));
    }
    {/* MATERIAL descriptor set layout */
        KORL_ZERO_STACK(VkDescriptorSetLayoutCreateInfo, createInfoDescriptorSetLayout);
        createInfoDescriptorSetLayout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfoDescriptorSetLayout.bindingCount = korl_arraySize(_KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL);
        createInfoDescriptorSetLayout.pBindings    = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL;
        _KORL_VULKAN_CHECK(vkCreateDescriptorSetLayout(context->device, &createInfoDescriptorSetLayout, context->allocator
                                                      ,&context->descriptorSetLayouts[_KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL]));
    }
    /* create pipeline layout */
    KORL_ZERO_STACK_ARRAY(VkPushConstantRange, pushConstantRange, 2);
    pushConstantRange[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange[0].size       = sizeof(surfaceContext->drawState.pushConstants.vertex);
    pushConstantRange[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange[1].offset     = sizeof(surfaceContext->drawState.pushConstants.vertex);
    pushConstantRange[1].size       = sizeof(surfaceContext->drawState.pushConstants.fragment);
    KORL_ZERO_STACK(VkPipelineLayoutCreateInfo, createInfoPipelineLayout);
    createInfoPipelineLayout.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfoPipelineLayout.setLayoutCount         = korl_arraySize(context->descriptorSetLayouts);
    createInfoPipelineLayout.pSetLayouts            = context->descriptorSetLayouts;
    createInfoPipelineLayout.pushConstantRangeCount = korl_arraySize(pushConstantRange);
    createInfoPipelineLayout.pPushConstantRanges    = pushConstantRange;
    _KORL_VULKAN_CHECK(
        vkCreatePipelineLayout(context->device, &createInfoPipelineLayout, context->allocator
                              ,&context->pipelineLayout));
    /* load required built-in shader assets */
    Korl_AssetCache_AssetData assetShaderVertex2d;
    Korl_AssetCache_AssetData assetShaderVertex2dUv;
    Korl_AssetCache_AssetData assetShaderVertex2dColor;
    Korl_AssetCache_AssetData assetShaderVertex3d;
    Korl_AssetCache_AssetData assetShaderVertex3dColor;
    Korl_AssetCache_AssetData assetShaderVertex3dUv;
    Korl_AssetCache_AssetData assetShaderVertexText;
    Korl_AssetCache_AssetData assetShaderFragmentColor;
    Korl_AssetCache_AssetData assetShaderFragmentColorTexture;
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-2d.vert.spv"           , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex2d));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-2d-color.vert.spv"     , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex2dColor));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-2d-uv.vert.spv"        , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex2dUv));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-3d.vert.spv"           , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex3d));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-3d-color.vert.spv"     , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex3dColor));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-3d-uv.vert.spv"        , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex3dUv));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-text.vert.spv"         , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertexText));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-color.frag.spv"        , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderFragmentColor));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-color-texture.frag.spv", KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderFragmentColorTexture));
    /* create shader modules */
    KORL_ZERO_STACK(VkShaderModuleCreateInfo, createInfoShader);
    createInfoShader.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfoShader.codeSize = assetShaderVertex2d.dataBytes;
    createInfoShader.pCode    = assetShaderVertex2d.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex2d));
    createInfoShader.codeSize = assetShaderVertex2dColor.dataBytes;
    createInfoShader.pCode    = assetShaderVertex2dColor.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex2dColor));
    createInfoShader.codeSize = assetShaderVertex2dUv.dataBytes;
    createInfoShader.pCode    = assetShaderVertex2dUv.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex2dUv));
    createInfoShader.codeSize = assetShaderVertex3d.dataBytes;
    createInfoShader.pCode    = assetShaderVertex3d.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex3d));
    createInfoShader.codeSize = assetShaderVertex3dColor.dataBytes;
    createInfoShader.pCode    = assetShaderVertex3dColor.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex3dColor));
    createInfoShader.codeSize = assetShaderVertex3dUv.dataBytes;
    createInfoShader.pCode    = assetShaderVertex3dUv.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex3dUv));
    createInfoShader.codeSize = assetShaderVertexText.dataBytes;
    createInfoShader.pCode    = assetShaderVertexText.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertexText));
    createInfoShader.codeSize = assetShaderFragmentColor.dataBytes;
    createInfoShader.pCode    = assetShaderFragmentColor.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderFragmentColor));
    createInfoShader.codeSize = assetShaderFragmentColorTexture.dataBytes;
    createInfoShader.pCode    = assetShaderFragmentColorTexture.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderFragmentColorTexture));
    /* create command pool for graphics queue family */
    KORL_ZERO_STACK(VkCommandPoolCreateInfo, createInfoCommandPool);
    createInfoCommandPool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfoCommandPool.queueFamilyIndex = context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics;
    createInfoCommandPool.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;// we're going to delete each of these command buffers after each frame
    _KORL_VULKAN_CHECK(
        vkCreateCommandPool(context->device, &createInfoCommandPool, context->allocator, 
                            &surfaceContext->commandPool));
    /* initialize the rest of the surface context's host memory structures */
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaPipelines                  , 128);
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaShaders                    , 128);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaShaderTrash                , 128);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), surfaceContext->stbDaDeviceLocalFreeQueue, 128);
    korl_memory_zero(context->stbDaShaders, arrlenu(context->stbDaShaders) * sizeof(*context->stbDaShaders));
    // at this point, the surface context is fully created & ready to begin the first frame //
    /* as soon as the surface is created, we can "kick-start" the first frame; 
        this _must_ be done before attempting to perform any deviceAsset 
        operations, as the command buffers required for memory transfers are 
        managed by each individual frame in the surface context */
    _korl_vulkan_frameBegin();
    /* create default texture */
    {
        KORL_ZERO_STACK(Korl_Vulkan_CreateInfoTexture, createInfoTexture);
        createInfoTexture.sizeX = 1;
        createInfoTexture.sizeY = 1;
        surfaceContext->defaultTexture = korl_vulkan_deviceAsset_createTexture(&createInfoTexture, 0/*0 => generate new handle*/);
        Korl_Vulkan_Color4u8 defaultTextureColor = (Korl_Vulkan_Color4u8){255, 0, 255, 255};
        korl_vulkan_texture_update(surfaceContext->defaultTexture, &defaultTextureColor);
    }
#ifdef _KORL_VULKAN_LOG_REPORTS
    korl_log(INFO, "deviceMemoryHostVisible report:");
    _korl_vulkan_deviceMemory_allocator_logReport(&surfaceContext->deviceMemoryHostVisible);
    korl_log(INFO, "deviceMemoryRenderResources report:");
    _korl_vulkan_deviceMemory_allocator_logReport(&surfaceContext->deviceMemoryRenderResources);
    korl_log(INFO, "deviceMemoryDeviceLocal report:");
    _korl_vulkan_deviceMemory_allocator_logReport(&surfaceContext->deviceMemoryDeviceLocal);
#endif
}
korl_internal void korl_vulkan_destroySurface(void)
{
    _korl_vulkan_destroySwapChain();
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* destroy the surface-specific resources */
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        vkDestroySemaphore(context->device, surfaceContext->wipFrames[i].semaphoreTransfersDone, context->allocator);
        vkDestroySemaphore(context->device, surfaceContext->wipFrames[i].semaphoreImageAvailable, context->allocator);
        vkDestroySemaphore(context->device, surfaceContext->wipFrames[i].semaphoreRenderDone, context->allocator);
        vkDestroyFence(context->device, surfaceContext->wipFrames[i].fenceFrameComplete, context->allocator);
        vkFreeCommandBuffers(context->device, surfaceContext->commandPool, 1, &(surfaceContext->wipFrames[i].commandBufferGraphics));
        vkFreeCommandBuffers(context->device, surfaceContext->commandPool, 1, &(surfaceContext->wipFrames[i].commandBufferTransfer));
    }
    _korl_vulkan_deviceMemory_allocator_destroy(&surfaceContext->deviceMemoryDeviceLocal);
    _korl_vulkan_deviceMemory_allocator_destroy(&surfaceContext->deviceMemoryHostVisible);
    _korl_vulkan_deviceMemory_allocator_destroy(&surfaceContext->deviceMemoryRenderResources);
    vkDestroyCommandPool(context->device, surfaceContext->commandPool, context->allocator);
    /* NOTE: we don't have to free individual device memory allocations in each 
             Buffer, since the entire allocators are being destroyed above! */
    mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), surfaceContext->stbDaDeviceLocalFreeQueue);
    mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), surfaceContext->stbDaStagingBuffers);
    vkDestroySurfaceKHR(context->instance, surfaceContext->surface, context->allocator);
    korl_memory_zero(surfaceContext, sizeof(*surfaceContext));// NOTE: Keep this as the last operation in the surface destructor!
    /* destroy the device-specific resources */
    {
        const _Korl_Vulkan_Pipeline*const pipelinesEnd = context->stbDaPipelines + arrlen(context->stbDaPipelines);
        for(_Korl_Vulkan_Pipeline* pipeline = context->stbDaPipelines; pipeline < pipelinesEnd; pipeline++)
            vkDestroyPipeline(context->device, pipeline->pipeline, context->allocator);
        mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaPipelines);
    }
    {
        const _Korl_Vulkan_Shader*const shadersEnd = context->stbDaShaders + arrlen(context->stbDaShaders);
        for(_Korl_Vulkan_Shader* shader = context->stbDaShaders; shader < shadersEnd; shader++)
            vkDestroyShaderModule(context->device, shader->shaderModule, context->allocator);
        mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaShaders);
    }
    {
        const _Korl_Vulkan_ShaderTrash*const shaderTrashEnd = context->stbDaShaderTrash + arrlen(context->stbDaShaderTrash);
        for(_Korl_Vulkan_ShaderTrash* shaderTrash = context->stbDaShaderTrash; shaderTrash < shaderTrashEnd; shaderTrash++)
            vkDestroyShaderModule(context->device, shaderTrash->shader.shaderModule, context->allocator);
        mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaShaderTrash);
    }
    for(u$ d = 0; d < korl_arraySize(context->descriptorSetLayouts); d++)
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayouts[d], context->allocator);
    vkDestroyPipelineLayout(context->device, context->pipelineLayout, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex2d, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex2dColor, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex2dUv, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex3d, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex3dColor, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex3dUv, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertexText, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderFragmentColor, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderFragmentColorTexture, context->allocator);
    vkDestroyDevice(context->device, context->allocator);
}
korl_internal Korl_Math_V2u32 korl_vulkan_getSurfaceSize(void)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    return (Korl_Math_V2u32){ surfaceContext->swapChainImageExtent.width
                            , surfaceContext->swapChainImageExtent.height };
}
/** Appends each allocator handle to the surface context's device-local free queue. */
korl_internal _KORL_VULKAN_DEVICEMEMORY_ALLOCATOR_FOR_EACH_CALLBACK(_korl_vulkan_clearAllDeviceAllocations_forEachCallback)
{
    _Korl_Vulkan_Context*const                                  context        = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const                           surfaceContext = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_DeviceLocalAllocationShallowDequeueBatch*const dequeueBatch   = KORL_C_CAST(_Korl_Vulkan_DeviceLocalAllocationShallowDequeueBatch*, userData);
    if(allocation->freeQueued)
        return;
    if(allocationHandle == surfaceContext->defaultTexture)
        return;
    // KORL-PERFORMANCE-000-000-038: vulkan: MINOR; this could be faster if we pass the allocation itself, but this isn't really that important right now...
    _korl_vulkan_deviceMemory_allocator_freeShallowQueueDestroyDeviceObjects(allocator, allocationHandle);
    dequeueBatch->dequeueCount++;
}
korl_internal void korl_vulkan_clearAllDeviceAllocations(void)
{
    _Korl_Vulkan_Context*const        context        = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_DeviceLocalAllocationShallowDequeueBatch*const dequeueBatch = KORL_MEMORY_POOL_ADD(surfaceContext->deviceLocalAllocationShallowDequeueBatches);
    korl_memory_zero(dequeueBatch, sizeof(*dequeueBatch));
    _korl_vulkan_deviceMemory_allocator_forEach(&surfaceContext->deviceMemoryDeviceLocal, _korl_vulkan_clearAllDeviceAllocations_forEachCallback, dequeueBatch);
    {
        const _Korl_Vulkan_Shader*const shadersEnd = context->stbDaShaders + arrlen(context->stbDaShaders);
        for(_Korl_Vulkan_Shader* shader = context->stbDaShaders; shader < shadersEnd; shader++)
        {
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaShaderTrash, ((_Korl_Vulkan_ShaderTrash){.shader = *shader, .framesSinceQueued = 0}));
            shader->shaderModule = VK_NULL_HANDLE;
        }
    }
}
korl_internal void korl_vulkan_setSurfaceClearColor(const f32 clearRgb[3])
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    for(u8 i = 0; i < korl_arraySize(surfaceContext->frameBeginClearColor.elements); i++)
        surfaceContext->frameBeginClearColor.elements[i] = clearRgb[i];
}
korl_internal void korl_vulkan_frameEnd(void)
{
    _Korl_Vulkan_Context*const          context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const   surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext* swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    if(   surfaceContext->frameSwapChainImageIndex >= _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE
       || surfaceContext->deferredResize)
        swapChainImageContext = NULL;
    /* now we can finalize the primary command buffers for the current frame */
    _KORL_VULKAN_CHECK(vkEndCommandBuffer(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer));
    if(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics)
    {
        vkCmdEndRenderPass(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics);
        _KORL_VULKAN_CHECK(vkEndCommandBuffer(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics));
    }
    /* close the frame-complete indication fence in preparation to submit 
        commands to the graphics queue for the current WIP frame */
    _KORL_VULKAN_CHECK(
        vkResetFences(context->device, 1, &surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fenceFrameComplete));
    /* submit transfer commands to the graphics queue */
    {
        korl_time_probeStart(submit_xfer_cmds_to_gfx_q);
        KORL_ZERO_STACK_ARRAY(VkCommandBufferSubmitInfo, commandBufferSubmitInfo, 1);
        commandBufferSubmitInfo[0].sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfo[0].commandBuffer = surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer;
        KORL_ZERO_STACK_ARRAY(VkSemaphoreSubmitInfo, semaphoreSubmitInfoSignal, 1);
        semaphoreSubmitInfoSignal[0].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        semaphoreSubmitInfoSignal[0].semaphore = surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreTransfersDone;
        semaphoreSubmitInfoSignal[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        KORL_ZERO_STACK_ARRAY(VkSubmitInfo2, submitInfoGraphics, 1);
        submitInfoGraphics[0].sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfoGraphics[0].commandBufferInfoCount   = korl_arraySize(commandBufferSubmitInfo);
        submitInfoGraphics[0].pCommandBufferInfos      = commandBufferSubmitInfo;
        /* we only want to signal to the graphics command queue submission that 
            transfers are done if there is no deferred resize 
            (we're doing graphics commands this frame) */
        if(!surfaceContext->deferredResize)
        {
            submitInfoGraphics[0].signalSemaphoreInfoCount = korl_arraySize(semaphoreSubmitInfoSignal);
            submitInfoGraphics[0].pSignalSemaphoreInfos    = semaphoreSubmitInfoSignal;
        }
        const VkFence signalFence = surfaceContext->deferredResize 
            /* If we have a deferred resize, we are not going to submit our 
                graphics commands for this frame, so we can signal the frame is 
                complete after just the transfer buffer completes. */
            ? surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fenceFrameComplete 
            : VK_NULL_HANDLE;
        _KORL_VULKAN_CHECK(
            vkQueueSubmit2(context->queueGraphics, korl_arraySize(submitInfoGraphics), submitInfoGraphics, signalFence));
        korl_time_probeStop(submit_xfer_cmds_to_gfx_q);
    }
    /* we shouldn't have to submit any graphics commands if our surface context 
        is in an invalid state */
    if(!surfaceContext->deferredResize)
    {
        /* submit graphics commands to the graphics queue */
        korl_time_probeStart(submit_gfx_cmds_to_gfx_q);
        KORL_ZERO_STACK_ARRAY(VkSemaphoreSubmitInfo, semaphoreSubmitInfoWait, 2);
        semaphoreSubmitInfoWait[0].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        semaphoreSubmitInfoWait[0].semaphore = surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreImageAvailable;
        semaphoreSubmitInfoWait[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        semaphoreSubmitInfoWait[1].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        semaphoreSubmitInfoWait[1].semaphore = surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreTransfersDone;
        semaphoreSubmitInfoWait[1].stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        KORL_ZERO_STACK_ARRAY(VkCommandBufferSubmitInfo, commandBufferSubmitInfo, 1);
        commandBufferSubmitInfo[0].sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfo[0].commandBuffer = surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics;
        KORL_ZERO_STACK_ARRAY(VkSemaphoreSubmitInfo, semaphoreSubmitInfoSignal, 1);
        semaphoreSubmitInfoSignal[0].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        semaphoreSubmitInfoSignal[0].semaphore = surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreRenderDone;
        semaphoreSubmitInfoSignal[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        KORL_ZERO_STACK_ARRAY(VkSubmitInfo2, submitInfoGraphics, 1);
        submitInfoGraphics[0].sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfoGraphics[0].waitSemaphoreInfoCount   = korl_arraySize(semaphoreSubmitInfoWait);
        submitInfoGraphics[0].pWaitSemaphoreInfos      = semaphoreSubmitInfoWait;
        submitInfoGraphics[0].commandBufferInfoCount   = korl_arraySize(commandBufferSubmitInfo);
        submitInfoGraphics[0].pCommandBufferInfos      = commandBufferSubmitInfo;
        submitInfoGraphics[0].signalSemaphoreInfoCount = korl_arraySize(semaphoreSubmitInfoSignal);
        submitInfoGraphics[0].pSignalSemaphoreInfos    = semaphoreSubmitInfoSignal;
        _KORL_VULKAN_CHECK(
            vkQueueSubmit2(context->queueGraphics, korl_arraySize(submitInfoGraphics), submitInfoGraphics
                          ,surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fenceFrameComplete));
        korl_time_probeStop(submit_gfx_cmds_to_gfx_q);
    }
    /* present the swap chain */
    if(swapChainImageContext)
    {
        korl_time_probeStart(present_swap_chain);
        VkSwapchainKHR presentInfoSwapChains[] = { surfaceContext->swapChain };
        KORL_ZERO_STACK(VkPresentInfoKHR, presentInfo);
        presentInfo.sType          = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = korl_arraySize(presentInfoSwapChains);
        presentInfo.pSwapchains    = presentInfoSwapChains;
        presentInfo.pImageIndices  = &surfaceContext->frameSwapChainImageIndex;
        if(!surfaceContext->deferredResize)
        {
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores    = &(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreRenderDone);
        }
        const VkResult resultQueuePresent = vkQueuePresentKHR(context->queuePresent, &presentInfo);
        switch(resultQueuePresent)
        {
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
            korl_assert(surfaceContext->deferredResize);
            break;
        default:
            _KORL_VULKAN_CHECK(resultQueuePresent);
        }
        korl_time_probeStop(present_swap_chain);
    }
    /* advance to the next WIP frame index */
    surfaceContext->wipFrameCurrent = (surfaceContext->wipFrameCurrent + 1) % surfaceContext->swapChainImagesSize;
    surfaceContext->wipFrameCount   = KORL_MATH_MIN(surfaceContext->wipFrameCount + 1, surfaceContext->swapChainImagesSize);
    /* begin the next frame */
    _korl_vulkan_frameBegin();
}
korl_internal void korl_vulkan_deferredResize(u32 sizeX, u32 sizeY)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* help ensure that this code never runs INSIDE of a set of 
        frameBegin/frameEnd calls - we cannot signal a deferred resize once a 
        new frame has begun! */
    surfaceContext->deferredResize  = true;
    surfaceContext->deferredResizeX = sizeX;
    surfaceContext->deferredResizeY = sizeY;
}
korl_internal void korl_vulkan_setDrawState(const Korl_Gfx_DrawState* state)
{
    _Korl_Vulkan_Context*const        context        = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    korl_time_probeStart(set_draw_state);
    /* if the swap chain image context is invalid for this frame for some reason, 
        then just do nothing (this happens during deferred resize for example) */
    if(surfaceContext->frameSwapChainImageIndex == _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE)
        goto done;
    /* all we have to do is configure the pipeline config cache here, as the 
        actual vulkan pipeline will be created & bound when we call draw */
    _Korl_Vulkan_Pipeline*const pipelineCache = &surfaceContext->drawState.pipelineConfigurationCache;
    if(state->modes)
        pipelineCache->modes = *state->modes;
    if(state->blend)
        pipelineCache->blend = *state->blend;
    if(state->sceneProperties)
    {
        surfaceContext->drawState.uboSceneProperties.m4f32Projection = state->sceneProperties->projection;
        surfaceContext->drawState.uboSceneProperties.m4f32View       = state->sceneProperties->view;
        surfaceContext->drawState.uboSceneProperties.seconds         = state->sceneProperties->seconds;
    }
    if(state->model)
    {
        surfaceContext->drawState.pushConstants.vertex.m4f32Model = state->model->transform;
        surfaceContext->drawState.pushConstants.fragment.uvAabb   = (Korl_Math_V4f32){.xy = state->model->uvAabb.min, .zw = state->model->uvAabb.max};
    }
    if(state->scissor)
    {
        surfaceContext->drawState.scissor.offset = (VkOffset2D){.x     = state->scissor->x    , .y      = state->scissor->y};
        surfaceContext->drawState.scissor.extent = (VkExtent2D){.width = state->scissor->width, .height = state->scissor->height};
    }
    if(state->material)
    {
        surfaceContext->drawState.uboMaterialProperties = state->material->properties;
        if(state->material->maps.resourceHandleTextureBase)
        {
            if(!(surfaceContext->drawState.materialMaps.base = korl_resource_getVulkanDeviceMemoryAllocationHandle(state->material->maps.resourceHandleTextureBase)))
                surfaceContext->drawState.materialMaps.base = surfaceContext->defaultTexture;
        }
        else 
            surfaceContext->drawState.materialMaps.base = 0;
        if(!(surfaceContext->drawState.materialMaps.specular = korl_resource_getVulkanDeviceMemoryAllocationHandle(state->material->maps.resourceHandleTextureSpecular)))
            surfaceContext->drawState.materialMaps.specular = surfaceContext->defaultTexture;
        if(!(surfaceContext->drawState.materialMaps.emissive = korl_resource_getVulkanDeviceMemoryAllocationHandle(state->material->maps.resourceHandleTextureEmissive)))
            surfaceContext->drawState.materialMaps.emissive = surfaceContext->defaultTexture;
        surfaceContext->drawState.transientShaderHandleVertex   = korl_resource_shader_getHandle(state->material->shaders.resourceHandleShaderVertex);
        surfaceContext->drawState.transientShaderHandleFragment = korl_resource_shader_getHandle(state->material->shaders.resourceHandleShaderFragment);
    }
    if(state->storageBuffers)
        surfaceContext->drawState.vertexStorageBuffer = korl_resource_getVulkanDeviceMemoryAllocationHandle(state->storageBuffers->resourceHandleVertex);
    if(state->lighting)
    {
        KORL_MEMORY_POOL_RESIZE(surfaceContext->drawState.lights, state->lighting->lightsCount);
        korl_memory_copy(surfaceContext->drawState.lights, state->lighting->lights, state->lighting->lightsCount * sizeof(*state->lighting->lights));
    }
    done:
    korl_time_probeStop(set_draw_state);
}
korl_internal void korl_vulkan_draw(const Korl_Vulkan_DrawVertexData* vertexData)
{
    #if 0//@TODO: do we just deprecate this functionality right now?...
    _Korl_Vulkan_Context*const               context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const        surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* if the swap chain image context is invalid for this frame for some reason, 
        then just do nothing (this happens during deferred resize for example) */
    if(surfaceContext->frameSwapChainImageIndex == _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE)
        return;
    /* it is possible for the graphics command buffer to be not available for 
        this frame (such as a minimized window); do nothing if that happens */
    if(!surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics)
        return;
    /* basic parameter sanity checks */
    if(vertexData->vertexBuffer.type != KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_UNUSED)
    {
        /* if we're using a vertex buffer device asset for vertex data, we can't 
            do sanity checks on vertex data until we obtain the device asset and 
            extract the vertex attribute descriptors from it; do nothing */
    }
    else
    {
        if(vertexData->indexCount || vertexData->indices)
            korl_assert(vertexData->indexCount && vertexData->indices);
        if(vertexData->colorsStride || vertexData->colors)
            korl_assert(vertexData->colorsStride && vertexData->colors);
        if(vertexData->uvsStride || vertexData->uvs)
            korl_assert(vertexData->uvsStride && vertexData->uvs);
        korl_assert(vertexData->vertexCount || vertexData->instanceCount);
        if(vertexData->positions)
        {
            korl_assert(   vertexData->positionDimensions == 2 
                        || vertexData->positionDimensions == 3);
            korl_assert(   vertexData->positionsStride == 2*sizeof(*vertexData->positions) 
                        || vertexData->positionsStride == 3*sizeof(*vertexData->positions));
        }
        if(vertexData->instancePositions)
        {
            korl_assert(vertexData->instancePositionDimensions == 2);
            korl_assert(vertexData->instancePositionsStride == 2*sizeof(*vertexData->instancePositions));
        }
    }
    /* if the user is attempting to draw with a custom vertex buffer, we need to 
        obtain it now to configure the pipeline */
    Korl_Vulkan_DeviceMemory_AllocationHandle deviceMemoryAllocationHandleVertexBuffer = 0;
    _Korl_Vulkan_DeviceMemory_Alloctation* deviceMemoryAllocationVertexBuffer = NULL;
    switch(vertexData->vertexBuffer.type)
    {
    case KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_UNUSED:{break;}
    case KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_RESOURCE:{
        deviceMemoryAllocationHandleVertexBuffer = korl_resource_getVulkanDeviceMemoryAllocationHandle(vertexData->vertexBuffer.subType.handleResource);
        break;}
    case KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_DEVICE_MEMORY_ALLOCATION:{
        deviceMemoryAllocationHandleVertexBuffer = vertexData->vertexBuffer.subType.handleDeviceMemoryAllocation;
        break;}
    default:
        korl_log(ERROR, "unsupported vertex buffer type: %i", vertexData->vertexBuffer.type);
    }
    if(vertexData->vertexBuffer.type != KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_UNUSED)
    {
        deviceMemoryAllocationVertexBuffer = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, deviceMemoryAllocationHandleVertexBuffer);
        korl_assert(deviceMemoryAllocationVertexBuffer);
        korl_assert(!deviceMemoryAllocationVertexBuffer->freeQueued);
        korl_assert(deviceMemoryAllocationVertexBuffer->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER);
    }
    /* prepare the pipeline config cache with the vertex data properties */
    _Korl_Vulkan_Pipeline*const pipelineCache = &surfaceContext->drawState.pipelineConfigurationCache;
    korl_time_probeStart(draw_config_pipeline);
    switch(vertexData->primitiveType)
    {
    case KORL_VULKAN_PRIMITIVETYPE_TRIANGLES: pipelineCache->primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
    case KORL_VULKAN_PRIMITIVETYPE_LINES    : pipelineCache->primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;     break;
    }
    switch(vertexData->polygonMode)
    {
    case KORL_GFX_POLYGON_MODE_FILL: pipelineCache->polygonMode = VK_POLYGON_MODE_FILL; break;
    case KORL_GFX_POLYGON_MODE_LINE: pipelineCache->polygonMode = VK_POLYGON_MODE_LINE; break;
    }
    switch(vertexData->cullMode)
    {
    case KORL_GFX_CULL_MODE_NONE: pipelineCache->cullModeFlags = VK_CULL_MODE_NONE;     break;
    case KORL_GFX_CULL_MODE_BACK: pipelineCache->cullModeFlags = VK_CULL_MODE_BACK_BIT; break;
    }
    korl_memory_zero(pipelineCache->vertexAttributes, sizeof(pipelineCache->vertexAttributes));
    u32 stagingBufferByteOffset = 0;
    if(vertexData->indices)
    {
        //we just implicitly place vertex indices at the start of the staging buffer, so no need to remember this value
        stagingBufferByteOffset += vertexData->indexCount * sizeof(*vertexData->indices);
    }
    if(vertexData->positions)
    {
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffset = stagingBufferByteOffset;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride = vertexData->positionsStride;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].format     = vertexData->positionDimensions == 2 ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32_SFLOAT;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate  = VK_VERTEX_INPUT_RATE_VERTEX;
        stagingBufferByteOffset += vertexData->vertexCount * pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    }
    if(vertexData->colors)
    {
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffset = stagingBufferByteOffset;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride = vertexData->colorsStride;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_COLOR].format     = VK_FORMAT_R8G8B8A8_UNORM;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_COLOR].inputRate  = VK_VERTEX_INPUT_RATE_VERTEX;
        stagingBufferByteOffset += vertexData->vertexCount * pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride;
    }
    if(vertexData->uvs)
    {
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UV].byteOffset = stagingBufferByteOffset;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UV].byteStride = vertexData->uvsStride;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UV].format     = VK_FORMAT_R32G32_SFLOAT;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UV].inputRate  = VK_VERTEX_INPUT_RATE_VERTEX;
        stagingBufferByteOffset += vertexData->vertexCount * pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UV].byteStride;
    }
    if(vertexData->instancePositions)
    {
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffset = stagingBufferByteOffset;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride = vertexData->instancePositionsStride;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].format     = vertexData->instancePositionDimensions == 2 ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32_SFLOAT;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate  = VK_VERTEX_INPUT_RATE_INSTANCE;
        stagingBufferByteOffset += vertexData->vertexCount * pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    }
    if(vertexData->instanceUint)
    {
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UINT].byteOffset = stagingBufferByteOffset;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UINT].byteStride = vertexData->instanceUintStride;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UINT].format     = VK_FORMAT_R32_UINT;
        pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UINT].inputRate  = VK_VERTEX_INPUT_RATE_INSTANCE;
        stagingBufferByteOffset += vertexData->vertexCount * pipelineCache->vertexAttributes[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UINT].byteStride;
    }
    if(vertexData->vertexBuffer.type != KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_UNUSED)
    {
        for(u32 d = 0; d < KORL_VULKAN_VERTEX_ATTRIBUTE_ENUM_COUNT; d++)
        {
            const u32 stride = deviceMemoryAllocationVertexBuffer->subType.buffer.attributeDescriptors[d].stride;
            if(0 == stride)
                continue;
            switch(d)
            {
            case KORL_VULKAN_VERTEX_ATTRIBUTE_INDEX:{
                /* for vertex indices, we _thankfully_ don't have to configure 
                    anything special in the pipeline, as this just uses special 
                    bind/draw commands (as you will see further down) */
                break;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_POSITION_3D:{
                pipelineCache->positionDimensions = 3;
                pipelineCache->positionsStride    = stride;
                break;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_NORMAL_3D:{
                pipelineCache->normalDimensions = 3;
                pipelineCache->normalsStride    = stride;
                break;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_UV:{
                pipelineCache->uvsStride = stride;
                break;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_INSTANCE_POSITION_2D:{
                pipelineCache->instancePositionDimensions = 2;
                pipelineCache->instancePositionStride     = stride;
                break;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_INSTANCE_UINT:{
                pipelineCache->instanceUintStride = stride;
                break;}
            default:
                korl_log(ERROR, "vertex attribute [%u] not implemented", d);
            }
        }
    }
    /* now that we have potentially configured the pipeline config cache to use 
        the vertex data from a vertex buffer, we can do a final round of sanity 
        checks before attempting to configure a pipeline */
    /// It really feels like I should be doing some sanity checks here, but I honestly don't know what I should be checking for...
    /// Maybe if _korl_vulkan_setPipelineMetaData fails in the future we can revisit this validation step.
    /* determine which shader modules this pipeline should use */
    // KORL-ISSUE-000-000-147: vulkan: delete all these shader modules; move the shader management task out to korl-gfx; when the hard-coded context->shaders are removed from korl-vulkan, we can assert that the pipelineCache _must_ be configured with valid shaders
    if(surfaceContext->drawState.transientShaderHandleVertex)
    {
        const u$ shaderIndex = surfaceContext->drawState.transientShaderHandleVertex - 1;
        korl_assert(shaderIndex < arrlenu(context->stbDaShaders));
        pipelineCache->shaderVertex = context->stbDaShaders[shaderIndex].shaderModule;
    }
    else if(pipelineCache->instancePositionStride && pipelineCache->instanceUintStride)
        pipelineCache->shaderVertex = context->shaderVertexText;
    else if(pipelineCache->positionDimensions == 2)
        pipelineCache->shaderVertex = context->shaderVertex2d;
    else if(pipelineCache->positionDimensions == 3)
        if(   !pipelineCache->colorsStride
           &&  pipelineCache->uvsStride)
            pipelineCache->shaderVertex = context->shaderVertex3dUv;
        else if(    pipelineCache->colorsStride
                && !pipelineCache->uvsStride)
            pipelineCache->shaderVertex = context->shaderVertex3dColor;
        else
            pipelineCache->shaderVertex = context->shaderVertex3d;
    if(surfaceContext->drawState.transientShaderHandleFragment)
    {
        const u$ shaderIndex = surfaceContext->drawState.transientShaderHandleFragment - 1;
        korl_assert(shaderIndex < arrlenu(context->stbDaShaders));
        pipelineCache->shaderFragment = context->stbDaShaders[shaderIndex].shaderModule;
    }
    else if(surfaceContext->drawState.materialMaps.base)
        pipelineCache->shaderFragment = context->shaderFragmentColorTexture;
    else
        pipelineCache->shaderFragment = context->shaderFragmentColor;
    /* reset the transient shader handles, requiring the user to re-set them via setDrawState for future draw calls */
    surfaceContext->drawState.transientShaderHandleVertex   = 0;
    surfaceContext->drawState.transientShaderHandleFragment = 0;
    /* now we can actually configure the pipeline */
    const u$ pipelinePrevious = surfaceContext->drawState.currentPipeline;
    _korl_vulkan_setPipelineMetaData(surfaceContext->drawState.pipelineConfigurationCache);
    const bool pipelineChanged = (pipelinePrevious != surfaceContext->drawState.currentPipeline);
    korl_time_probeStop(draw_config_pipeline);
    /* stage uniform data (descriptor set staging);  here we will examine the 
        current draw state & see if it differs from the last used draw state;  
        if it does, we will compose the minimum # of descriptor set stages & 
        writes to update the device draw state appropriately;  only then will we 
        update/bind descriptor sets, since this is a very expensive operation! */
    VkDeviceSize byteOffsetStagingBuffer = 0;
    VkBuffer     bufferStaging           = VK_NULL_HANDLE;
    KORL_ZERO_STACK_ARRAY(VkWriteDescriptorSet           , descriptorSetWrites         , _KORL_VULKAN_DESCRIPTOR_BINDING_TOTAL);
    KORL_ZERO_STACK_ARRAY(_Korl_Vulkan_DescriptorSetIndex, descriptorSetWriteSetIndices, _KORL_VULKAN_DESCRIPTOR_BINDING_TOTAL);
    uint32_t descriptorWriteCount = 0;
    KORL_ZERO_STACK_ARRAY(bool, descriptorSetIndicesChanged, _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT);
    KORL_MEMORY_POOL_DECLARE(VkDescriptorBufferInfo, descriptorBufferInfos, _KORL_VULKAN_DESCRIPTOR_BINDING_TOTAL);
    KORL_MEMORY_POOL_ZERO(descriptorBufferInfos);
    KORL_MEMORY_POOL_DECLARE(VkDescriptorImageInfo, descriptorImageInfos, _KORL_VULKAN_DESCRIPTOR_BINDING_TOTAL);
    KORL_MEMORY_POOL_ZERO(descriptorImageInfos);
    // _KORL_VULKAN_DESCRIPTOR_SET_INDEX_SCENE //
    if(0 != korl_memory_compare(&surfaceContext->drawState.uboSceneProperties// only ever get new VP uniform staging memory if the VP state has changed!
                               ,&surfaceContext->drawStateLast.uboSceneProperties
                               ,sizeof(surfaceContext->drawState.uboSceneProperties)))
    {
        /* stage the UBO transforms */
        _Korl_Vulkan_Uniform_SceneProperties*const stagingMemoryUniformTransforms = 
            _korl_vulkan_getDescriptorStagingPool(sizeof(*stagingMemoryUniformTransforms), &bufferStaging, &byteOffsetStagingBuffer);
        *stagingMemoryUniformTransforms = surfaceContext->drawState.uboSceneProperties;
        /* prepare a descriptor set write with the staged UBO */
        VkDescriptorBufferInfo*const descriptorBufferInfo = KORL_MEMORY_POOL_ADD(descriptorBufferInfos);
        descriptorBufferInfo->buffer = bufferStaging;
        descriptorBufferInfo->range  = sizeof(*stagingMemoryUniformTransforms);
        descriptorBufferInfo->offset = byteOffsetStagingBuffer;
        const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_SCENE_TRANSFORMS 
                                                                            + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_SCENE_PROPERTIES_UBO;
        descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
        descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
        descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
        descriptorSetWrites[descriptorWriteCount].pBufferInfo     = descriptorBufferInfo;
        // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
        descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_SCENE;
        descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
        descriptorWriteCount++;
    }
    // _KORL_VULKAN_DESCRIPTOR_SET_INDEX_LIGHTS //
    if(   KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights) != KORL_MEMORY_POOL_SIZE(surfaceContext->drawStateLast.lights)
       || 0 != korl_memory_compare(&surfaceContext->drawState.lights
                                  ,&surfaceContext->drawStateLast.lights
                                  ,KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights) * sizeof(*surfaceContext->drawState.lights)))
    {
        /* stage the Lighting SSBO */
        typedef struct _Korl_Vulkan_ShaderBuffer_Lights
        {
            u32 lightsSize;
            u32 _padding_0[3];
            Korl_Gfx_Light lights[1];// array size == `lightsSize`
        } _Korl_Vulkan_ShaderBuffer_Lights;
        const VkDeviceSize bufferBytes = sizeof(_Korl_Vulkan_ShaderBuffer_Lights) - sizeof(Korl_Gfx_Light) + KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights) * sizeof(Korl_Gfx_Light);
        _Korl_Vulkan_ShaderBuffer_Lights*const stagingMemoryBufferLights = 
            _korl_vulkan_getDescriptorStagingPool(bufferBytes, &bufferStaging, &byteOffsetStagingBuffer);
        stagingMemoryBufferLights->lightsSize = KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights);
        korl_memory_copy(stagingMemoryBufferLights->lights, surfaceContext->drawState.lights, KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights) * sizeof(*surfaceContext->drawState.lights));
        for(Korl_MemoryPool_Size i = 0; i < KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights); i++)
        {
            Korl_Gfx_Light*const light = surfaceContext->drawState.lights + i;
            stagingMemoryBufferLights->lights[i].position  = korl_math_m4f32_multiplyV4f32Copy(&surfaceContext->drawState.uboSceneProperties.m4f32View, (Korl_Math_V4f32){.xyz = light->position, 1}).xyz;
            stagingMemoryBufferLights->lights[i].direction = korl_math_m4f32_multiplyV4f32Copy(&surfaceContext->drawState.uboSceneProperties.m4f32View, (Korl_Math_V4f32){.xyz = light->direction, 0}).xyz;
        }
        /* prepare a descriptor set write with the staged UBO */
        VkDescriptorBufferInfo*const descriptorBufferInfo = KORL_MEMORY_POOL_ADD(descriptorBufferInfos);
        descriptorBufferInfo->buffer = bufferStaging;
        descriptorBufferInfo->range  = bufferBytes;
        descriptorBufferInfo->offset = byteOffsetStagingBuffer;
        const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_LIGHTS 
                                                                            + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_LIGHTS_SSBO;
        descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
        descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
        descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
        descriptorSetWrites[descriptorWriteCount].pBufferInfo     = descriptorBufferInfo;
        // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
        descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_LIGHTS;
        descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
        descriptorWriteCount++;
    }
    // _KORL_VULKAN_DESCRIPTOR_SET_INDEX_STORAGE //
    if(   surfaceContext->drawState.vertexStorageBuffer
       && surfaceContext->drawState.vertexStorageBuffer != surfaceContext->drawStateLast.vertexStorageBuffer)
    {
        _Korl_Vulkan_DeviceMemory_Alloctation* deviceMemoryAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, surfaceContext->drawState.vertexStorageBuffer);
        korl_assert(!deviceMemoryAllocation->freeQueued);
        VkDescriptorBufferInfo*const descriptorBufferInfo = KORL_MEMORY_POOL_ADD(descriptorBufferInfos);
        descriptorBufferInfo->buffer = deviceMemoryAllocation->subType.buffer.vulkanBuffer;
        descriptorBufferInfo->range  = deviceMemoryAllocation->bytesUsed;
        #if 0 // already set to default value
        descriptorBufferInfo->offset = 0;
        #endif
        const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_STORAGE 
                                                                            + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_VERTEX_STORAGE_SSBO;
        descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
        descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
        descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
        descriptorSetWrites[descriptorWriteCount].pBufferInfo     = descriptorBufferInfo;
        // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
        descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_STORAGE;
        descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
        descriptorWriteCount++;
    }
    // _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL //
    if(   // write material UBO uniform if it has changed //
          0 != korl_memory_compare(&surfaceContext->drawState.uboMaterialProperties
                                  ,&surfaceContext->drawStateLast.uboMaterialProperties
                                  ,sizeof(surfaceContext->drawState.uboMaterialProperties))
          // conditionally write the texture sampler if we require it //
       || (surfaceContext->drawState.materialMaps.base && surfaceContext->drawState.materialMaps.base != surfaceContext->drawStateLast.materialMaps.base)
       || (surfaceContext->drawState.materialMaps.specular && surfaceContext->drawState.materialMaps.specular != surfaceContext->drawStateLast.materialMaps.specular)
       || (surfaceContext->drawState.materialMaps.emissive && surfaceContext->drawState.materialMaps.emissive != surfaceContext->drawStateLast.materialMaps.emissive))
    {
        {
            /* stage the UBO */
            Korl_Gfx_Material_Properties*const stagingMemoryUboMaterial = 
                _korl_vulkan_getDescriptorStagingPool(sizeof(*stagingMemoryUboMaterial), &bufferStaging, &byteOffsetStagingBuffer);
            *stagingMemoryUboMaterial = surfaceContext->drawState.uboMaterialProperties;
            /* prepare a descriptor set write with the staged UBO */
            VkDescriptorBufferInfo*const descriptorBufferInfo = KORL_MEMORY_POOL_ADD(descriptorBufferInfos);
            descriptorBufferInfo->buffer = bufferStaging;
            descriptorBufferInfo->range  = sizeof(*stagingMemoryUboMaterial);
            descriptorBufferInfo->offset = byteOffsetStagingBuffer;
            const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL 
                                                                                + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_UBO;
            descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
            descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
            descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
            descriptorSetWrites[descriptorWriteCount].pBufferInfo     = descriptorBufferInfo;
            // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
            descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL;
            descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
            descriptorWriteCount++;
        }
        if(surfaceContext->drawState.materialMaps.base)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const textureAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, surfaceContext->drawState.materialMaps.base);
            korl_assert(!textureAllocation->freeQueued);
            #if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
            const _Korl_Vulkan_DeviceAssetHandle_Unpacked deviceAssetHandleUnpacked = _korl_vulkan_deviceAssetHandle_unpack(surfaceContext->drawState.materialMaps.resourceHandleTextureBase);
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaInUseDeviceAssetIndices, deviceAssetHandleUnpacked.databaseIndex);
            #endif
            korl_assert(textureAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
            VkDescriptorImageInfo*const descriptorImageInfo = KORL_MEMORY_POOL_ADD(descriptorImageInfos);
            descriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo->imageView   = textureAllocation->subType.texture.imageView;
            descriptorImageInfo->sampler     = textureAllocation->subType.texture.sampler;
            korl_assert(descriptorWriteCount < korl_arraySize(descriptorSetWrites));
            const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL 
                                                                                + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_BASE;
            descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
            descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
            descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
            descriptorSetWrites[descriptorWriteCount].pImageInfo      = descriptorImageInfo;
            // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
            descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL;
            descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
            descriptorWriteCount++;
        }
        if(surfaceContext->drawState.materialMaps.specular)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const textureAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, surfaceContext->drawState.materialMaps.specular);
            korl_assert(!textureAllocation->freeQueued);
            #if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
            const _Korl_Vulkan_DeviceAssetHandle_Unpacked deviceAssetHandleUnpacked = _korl_vulkan_deviceAssetHandle_unpack(surfaceContext->drawState.materialMaps.resourceHandleTextureSpecular);
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaInUseDeviceAssetIndices, deviceAssetHandleUnpacked.databaseIndex);
            #endif
            korl_assert(textureAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
            VkDescriptorImageInfo*const descriptorImageInfo = KORL_MEMORY_POOL_ADD(descriptorImageInfos);
            descriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo->imageView   = textureAllocation->subType.texture.imageView;
            descriptorImageInfo->sampler     = textureAllocation->subType.texture.sampler;
            korl_assert(descriptorWriteCount < korl_arraySize(descriptorSetWrites));
            const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL 
                                                                                + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_SPECULAR;
            descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
            descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
            descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
            descriptorSetWrites[descriptorWriteCount].pImageInfo      = descriptorImageInfo;
            // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
            descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL;
            descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
            descriptorWriteCount++;
        }
        if(surfaceContext->drawState.materialMaps.emissive)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const textureAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, surfaceContext->drawState.materialMaps.emissive);
            korl_assert(!textureAllocation->freeQueued);
            #if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
            const _Korl_Vulkan_DeviceAssetHandle_Unpacked deviceAssetHandleUnpacked = _korl_vulkan_deviceAssetHandle_unpack(surfaceContext->drawState.materialMaps.resourceHandleTextureEmissive);
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaInUseDeviceAssetIndices, deviceAssetHandleUnpacked.databaseIndex);
            #endif
            korl_assert(textureAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
            VkDescriptorImageInfo*const descriptorImageInfo = KORL_MEMORY_POOL_ADD(descriptorImageInfos);
            descriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo->imageView   = textureAllocation->subType.texture.imageView;
            descriptorImageInfo->sampler     = textureAllocation->subType.texture.sampler;
            korl_assert(descriptorWriteCount < korl_arraySize(descriptorSetWrites));
            const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL 
                                                                                + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_EMISSIVE;
            descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
            descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
            descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
            descriptorSetWrites[descriptorWriteCount].pImageInfo      = descriptorImageInfo;
            // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
            descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL;
            descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
            descriptorWriteCount++;
        }
    }
    korl_assert(descriptorWriteCount <= korl_arraySize(descriptorSetWrites));
    /* allocate a new descriptor set conditionally, and configure the composed 
        descriptor set writes to modify it */
    KORL_ZERO_STACK_ARRAY(VkDescriptorSet, descriptorSets, _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT);
    if(descriptorWriteCount)
    {
        korl_time_probeStart(draw_descriptor_set_alloc); {
            for(u32 d = 0; d < _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT; d++)
                if(descriptorSetIndicesChanged[d])
                    descriptorSets[d] = _korl_vulkan_newDescriptorSet(context->descriptorSetLayouts[d]);
            for(uint32_t d = 0; d < descriptorWriteCount; d++)
                descriptorSetWrites[d].dstSet = descriptorSets[descriptorSetWriteSetIndices[d]];
        } korl_time_probeStop(draw_descriptor_set_alloc);
        vkUpdateDescriptorSets(context->device, descriptorWriteCount, descriptorSetWrites, 0/*copyCount*/, NULL/*copies*/);
    }
    /* stage the vertex index/attribute data */
    // KORL-PERFORMANCE-000-000-035: vulkan: we could potentially get more performance here by transferring this data to device-local memory
    korl_time_probeStart(draw_stage_vertices);
    byteOffsetStagingBuffer = 0;
    bufferStaging           = VK_NULL_HANDLE;
    u8* vertexStagingMemory = _korl_vulkan_getVertexStagingPool(vertexData, &bufferStaging, &byteOffsetStagingBuffer);
    VkDeviceSize byteOffsetStagingBufferIndices           = 0
               , byteOffsetStagingBufferPositions         = 0
               , byteOffsetStagingBufferNormals           = 0
               , byteOffsetStagingBufferColors            = 0
               , byteOffsetStagingBufferUvs               = 0
               , byteOffsetStagingBufferInstancePositions = 0
               , byteOffsetStagingBufferInstanceU32s      = 0;
    // KORL-PERFORMANCE-000-000-036: vulkan: if we allow interleaved vertex data, we can copy _everything_ to the staging buffer in a single memcopy operation!
    if(vertexData->indices)
    {
        const u$ stageDataBytes = vertexData->indexCount*sizeof(*vertexData->indices);
        // we can safely assume all vertex index data is never interleaved with anything for now:
        korl_memory_copy(vertexStagingMemory, vertexData->indices, stageDataBytes);
        byteOffsetStagingBufferIndices = byteOffsetStagingBuffer;
        vertexStagingMemory     += stageDataBytes;
        byteOffsetStagingBuffer += stageDataBytes;
    }
    if(vertexData->positions)
    {
        const u$ positionSize   = vertexData->positionDimensions*sizeof(*vertexData->positions);
        const u$ stageDataBytes = vertexData->vertexCount*positionSize;
        korl_assert(vertexData->positionsStride == positionSize);// for now, interleaved vertex data is not supported
        korl_memory_copy(vertexStagingMemory, vertexData->positions, stageDataBytes);
        byteOffsetStagingBufferPositions = byteOffsetStagingBuffer;
        vertexStagingMemory     += stageDataBytes;
        byteOffsetStagingBuffer += stageDataBytes;
    }
    //KORL-ISSUE-000-000-150: vulkan: incomplete normals vertex attribute support
    if(vertexData->colors)
    {
        const u$ stageDataBytes = vertexData->vertexCount*sizeof(*vertexData->colors);
        korl_assert(vertexData->colorsStride == sizeof(*vertexData->colors));// for now, interleaved vertex data is not supported
        korl_memory_copy(vertexStagingMemory, vertexData->colors, stageDataBytes);
        byteOffsetStagingBufferColors = byteOffsetStagingBuffer;
        vertexStagingMemory     += stageDataBytes;
        byteOffsetStagingBuffer += stageDataBytes;
    }
    if(vertexData->uvs)
    {
        const u$ stageDataBytes = vertexData->vertexCount*sizeof(*vertexData->uvs);
        korl_assert(vertexData->uvsStride == sizeof(*vertexData->uvs));// for now, interleaved vertex data is not supported
        korl_memory_copy(vertexStagingMemory, vertexData->uvs, stageDataBytes);
        byteOffsetStagingBufferUvs = byteOffsetStagingBuffer;
        vertexStagingMemory     += stageDataBytes;
        byteOffsetStagingBuffer += stageDataBytes;
    }
    if(vertexData->instancePositions)
    {
        const u$ positionSize   = vertexData->instancePositionDimensions*sizeof(*vertexData->instancePositions);
        const u$ stageDataBytes = vertexData->instanceCount*positionSize;
        korl_assert(vertexData->instancePositionsStride == positionSize);// for now, interleaved vertex data is not supported
        korl_memory_copy(vertexStagingMemory, vertexData->instancePositions, stageDataBytes);
        byteOffsetStagingBufferInstancePositions = byteOffsetStagingBuffer;
        vertexStagingMemory     += stageDataBytes;
        byteOffsetStagingBuffer += stageDataBytes;
    }
    if(vertexData->instanceUint)
    {
        const u$ stageDataBytes = vertexData->instanceCount*sizeof(*vertexData->instanceUint);
        korl_assert(vertexData->instanceUintStride == sizeof(*vertexData->instanceUint));// for now, interleaved vertex data is not supported
        korl_memory_copy(vertexStagingMemory, vertexData->instanceUint, stageDataBytes);
        byteOffsetStagingBufferInstanceU32s = byteOffsetStagingBuffer;
        vertexStagingMemory     += stageDataBytes;
        byteOffsetStagingBuffer += stageDataBytes;
    }
    korl_time_probeStop(draw_stage_vertices);
    VkBuffer     batchIndexBuffer       = bufferStaging;
    VkDeviceSize batchIndexBufferOffset = byteOffsetStagingBufferIndices;
    VkBuffer batchVertexBuffers[] = 
        { bufferStaging
        , bufferStaging
        , bufferStaging
        , bufferStaging
        , bufferStaging
        , bufferStaging };
    VkDeviceSize batchVertexBufferOffsets[] = 
        { byteOffsetStagingBufferPositions
        , byteOffsetStagingBufferColors
        , byteOffsetStagingBufferUvs
        , byteOffsetStagingBufferInstancePositions
        , byteOffsetStagingBufferInstanceU32s 
        , byteOffsetStagingBufferNormals
        /* NOTE: keep these in the same order as they appear in the 
                 `_KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_*` defines!
                 failure to do so will result in vertex attribute corruptions */};
    korl_assert(korl_arraySize(batchVertexBuffers) == korl_arraySize(batchVertexBufferOffsets));
    /* if we're passing a vertex buffer in the vertex data, that means we have 
        to bind to a user-managed device asset buffer instead of a staging 
        buffer for a non-zero number of vertex (instance) attributes */
    if(vertexData->vertexBuffer.type != KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_UNUSED)
    {
        for(u32 d = 0; d < KORL_VULKAN_VERTEX_ATTRIBUTE_ENUM_COUNT; d++)
        {
            if(0 == deviceMemoryAllocationVertexBuffer->subType.buffer.attributeDescriptors[d].stride)
                continue;
            u32 attributeBinding = KORL_U32_MAX;
            switch(d)
            {
            case KORL_VULKAN_VERTEX_ATTRIBUTE_INDEX:{
                /* special case; modify the batch index buffer instead of vertex buffers */
                batchIndexBuffer       = deviceMemoryAllocationVertexBuffer->subType.buffer.vulkanBuffer;
                batchIndexBufferOffset = deviceMemoryAllocationVertexBuffer->subType.buffer.attributeDescriptors[d].offset + vertexData->vertexBuffer.byteOffset;
                continue;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_POSITION_3D:{
                attributeBinding = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_POSITION;
                break;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_UV:{
                attributeBinding = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_UV;
                break;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_INSTANCE_POSITION_2D:{
                attributeBinding = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_INSTANCE_POSITION;
                break;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_INSTANCE_UINT:{
                attributeBinding = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_INSTANCE_UINT;
                break;}
            case KORL_VULKAN_VERTEX_ATTRIBUTE_NORMAL_3D:{
                attributeBinding = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_NORMAL;
                break;}
            default:
                korl_log(ERROR, "vertex attribute [%u] not implemented", d);
            }
            batchVertexBuffers      [attributeBinding] = deviceMemoryAllocationVertexBuffer->subType.buffer.vulkanBuffer;
            batchVertexBufferOffsets[attributeBinding] = deviceMemoryAllocationVertexBuffer->subType.buffer.attributeDescriptors[d].offset + vertexData->vertexBuffer.byteOffset;
        }
    }
    /* compose the draw commands */
    korl_assert(surfaceContext->drawState.currentPipeline < arrlenu(context->stbDaPipelines) && arrlenu(context->stbDaPipelines) > 0);//try to make sure we have selected a valid pipeline before going further
    korl_time_probeStart(draw_compose_gfx_commands);
    if(pipelineChanged)
        vkCmdBindPipeline(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                         ,VK_PIPELINE_BIND_POINT_GRAPHICS
                         ,context->stbDaPipelines[surfaceContext->drawState.currentPipeline].pipeline);
    for(u32 d = 0; d < _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT; d++)
    {
        if(descriptorSets[d] == VK_NULL_HANDLE)
            continue;
        /* in an effort to minimize the total number of calls to vkCmdBindDescriptorSets, 
            we try to accumulate the largest possible number of contiguous descriptor sets 
            which are waiting to be bound: */
        const uint32_t firstSet = d;
        uint32_t setCount = 1;
        while(d + 1 < _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT)
        {
            if(descriptorSets[d + 1] == VK_NULL_HANDLE)
                break;
            setCount++;
            d++;
        }
        vkCmdBindDescriptorSets(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                               ,VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipelineLayout
                               ,firstSet
                               ,setCount/*set count */, &(descriptorSets[firstSet])
                               ,/*dynamic offset count*/0, /*pDynamicOffsets*/NULL);
    }
    vkCmdPushConstants(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics, context->pipelineLayout
                      ,VK_SHADER_STAGE_VERTEX_BIT
                      ,/*offset*/0, sizeof(surfaceContext->drawState.pushConstants.vertex)
                      ,&surfaceContext->drawState.pushConstants.vertex);
    vkCmdPushConstants(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics, context->pipelineLayout
                      ,VK_SHADER_STAGE_FRAGMENT_BIT
                      ,/*offset*/sizeof(surfaceContext->drawState.pushConstants.vertex), sizeof(surfaceContext->drawState.pushConstants.fragment)
                      ,&surfaceContext->drawState.pushConstants.fragment);
    vkCmdSetScissor(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                   ,0/*firstScissor*/, 1/*scissorCount*/, &surfaceContext->drawState.scissor);
    vkCmdBindVertexBuffers(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                          ,0/*first binding*/, korl_arraySize(batchVertexBuffers)
                          ,batchVertexBuffers, batchVertexBufferOffsets);
    const uint32_t instanceCount = vertexData->instanceCount ? vertexData->instanceCount : 1;
    if(vertexData->indexCount)
    {
        vkCmdBindIndexBuffer(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                            ,batchIndexBuffer, batchIndexBufferOffset
                            ,_korl_vulkan_vertexIndexType());
        vkCmdDrawIndexed(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                        ,vertexData->indexCount
                        ,instanceCount, /*firstIndex*/0, /*vertexOffset*/0
                        ,/*firstInstance*/0);
    }
    else
        vkCmdDraw(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                 ,vertexData->vertexCount
                 ,instanceCount, /*firstVertex*/0, /*firstInstance*/0);
    korl_time_probeStop(draw_compose_gfx_commands);
    surfaceContext->drawStateLast = surfaceContext->drawState;
    #endif
}
korl_internal VkDeviceSize _korl_vulkan_vertexAttribute_vectorBytes(const Korl_Gfx_VertexAttributeDescriptor* vertexAttributeDescriptor)
{
    VkDeviceSize elementBytes = 0;
    switch(vertexAttributeDescriptor->elementType)
    {
    case KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID:                             break;
    case KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32    : elementBytes = sizeof(f32); break;
    case KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8     : elementBytes = sizeof( u8); break;
    case KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32    : elementBytes = sizeof(u32); break;
    }
    return elementBytes * vertexAttributeDescriptor->vectorSize;
}
korl_internal VkFormat _korl_vulkan_vertexAttribute_format(const Korl_Gfx_VertexAttributeDescriptor* vertexAttributeDescriptor)
{
    switch(vertexAttributeDescriptor->elementType)
    {
    case KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8:
        switch(vertexAttributeDescriptor->vectorSize)
        {
        case  4: return VK_FORMAT_R8G8B8A8_UNORM;
        default: return VK_FORMAT_UNDEFINED;
        }
    case KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32:
        switch(vertexAttributeDescriptor->vectorSize)
        {
        case  1: return VK_FORMAT_R32_UINT;
        default: return VK_FORMAT_UNDEFINED;
        }
    case KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32:
        switch(vertexAttributeDescriptor->vectorSize)
        {
        case  2: return VK_FORMAT_R32G32_SFLOAT;
        case  3: return VK_FORMAT_R32G32B32_SFLOAT;
        default: return VK_FORMAT_UNDEFINED;
        }
    default:
        // korl_log(WARNING, "undefined vertex attribute element type: %u", vertexAttributeDescriptor->elementType);
        return VK_FORMAT_UNDEFINED;
    }
}
korl_internal VkDeviceSize _korl_vulkan_vertexStagingMeta_bytes(const Korl_Gfx_VertexStagingMeta* stagingMeta)
{
    VkDeviceSize vertexByteStart = VK_WHOLE_SIZE;
    VkDeviceSize vertexByteEnd   = 0;
    if(stagingMeta->indexType != KORL_GFX_VERTEX_INDEX_TYPE_INVALID)
    {
        VkDeviceSize indexElementBytes = 0;
        switch(stagingMeta->indexType)
        {
        case KORL_GFX_VERTEX_INDEX_TYPE_U16: indexElementBytes = 2; break;
        case KORL_GFX_VERTEX_INDEX_TYPE_U32: indexElementBytes = 4; break;
        default:
            korl_log(ERROR, "invalid index type: %u", stagingMeta->indexType);
        }
        const VkDeviceSize indexByteStart = stagingMeta->indexByteOffsetBuffer;
        const VkDeviceSize indexByteEnd   = stagingMeta->indexByteOffsetBuffer + stagingMeta->indexCount * indexElementBytes;
        KORL_MATH_ASSIGN_CLAMP_MAX(vertexByteStart, indexByteStart);
        KORL_MATH_ASSIGN_CLAMP_MIN(vertexByteEnd  , indexByteEnd);
    }
    for(u8 i = 0; i < KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT; i++)
    {
        const Korl_Gfx_VertexAttributeDescriptor*const vertexAttributeDescriptor = stagingMeta->vertexAttributeDescriptors + i;
        if(vertexAttributeDescriptor->elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            continue;
        u32 attributeCount = 0;
        switch(vertexAttributeDescriptor->inputRate)
        {
        case VK_VERTEX_INPUT_RATE_VERTEX:   attributeCount = stagingMeta->vertexCount;   break;
        case VK_VERTEX_INPUT_RATE_INSTANCE: attributeCount = stagingMeta->instanceCount; break;
        }
        korl_assert(attributeCount);
        const VkDeviceSize attributeVectorBytes = _korl_vulkan_vertexAttribute_vectorBytes(vertexAttributeDescriptor);
        const VkDeviceSize attributeByteStart   = vertexAttributeDescriptor->byteOffsetBuffer;
        const VkDeviceSize attributeByteEnd     = vertexAttributeDescriptor->byteOffsetBuffer + (attributeCount * vertexAttributeDescriptor->byteStride) + attributeVectorBytes;
        KORL_MATH_ASSIGN_CLAMP_MAX(vertexByteStart, attributeByteStart);
        KORL_MATH_ASSIGN_CLAMP_MIN(vertexByteEnd  , attributeByteEnd);
    }
    return vertexByteEnd > vertexByteStart ? vertexByteEnd - vertexByteStart : 0;
}
korl_internal Korl_Gfx_StagingAllocation korl_vulkan_stagingAllocate(const Korl_Gfx_VertexStagingMeta* stagingMeta)
{
    KORL_ZERO_STACK(Korl_Gfx_StagingAllocation, result);
    result.bytes  = _korl_vulkan_vertexStagingMeta_bytes(stagingMeta);
    result.buffer = _korl_vulkan_getStagingPool(result.bytes, /*alignment*/0, KORL_C_CAST(VkBuffer*, &result.deviceBuffer), &result.deviceBufferOffset);
    return result;
}
korl_internal void _korl_vulkan_flushPipelineState(const Korl_Gfx_VertexStagingMeta* stagingMeta)
{
    _Korl_Vulkan_Context*const               context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const        surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* prepare the pipeline config cache with the vertex data properties */
    _Korl_Vulkan_Pipeline*const pipelineCache = &surfaceContext->drawState.pipelineConfigurationCache;
    for(u32 i = 0; i < KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT; i++)
    {
        pipelineCache->vertexAttributes[i].byteOffset = 0;// we will _always_ make sure to bind the vertex attribute to the byte offset of the first element!
        pipelineCache->vertexAttributes[i].byteStride = stagingMeta->vertexAttributeDescriptors[i].byteStride;
        pipelineCache->vertexAttributes[i].format     = _korl_vulkan_vertexAttribute_format(stagingMeta->vertexAttributeDescriptors + i);
        switch(stagingMeta->vertexAttributeDescriptors[i].inputRate)
        {
        case KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX:   pipelineCache->vertexAttributes[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;   break;
        case KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE: pipelineCache->vertexAttributes[i].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE; break;
        }
    }
    /* determine which shader modules this pipeline should use */
    // KORL-ISSUE-000-000-147: vulkan: delete all these shader modules; move the shader management task out to korl-gfx; when the hard-coded context->shaders are removed from korl-vulkan, we can assert that the pipelineCache _must_ be configured with valid shaders
    if(surfaceContext->drawState.transientShaderHandleVertex)
    {
        const u$ shaderIndex = surfaceContext->drawState.transientShaderHandleVertex - 1;
        korl_assert(shaderIndex < arrlenu(context->stbDaShaders));
        pipelineCache->shaderVertex = context->stbDaShaders[shaderIndex].shaderModule;
    }
    else if(   pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].format != VK_FORMAT_UNDEFINED
            && pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT].format     != VK_FORMAT_UNDEFINED
            && pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate == VK_VERTEX_INPUT_RATE_INSTANCE
            && pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT].inputRate     == VK_VERTEX_INPUT_RATE_INSTANCE)
        pipelineCache->shaderVertex = context->shaderVertexText;
    else if(pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].format == VK_FORMAT_R32G32_SFLOAT)
        if(   pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].format == VK_FORMAT_UNDEFINED
           && pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].format != VK_FORMAT_UNDEFINED)
            pipelineCache->shaderVertex = context->shaderVertex2dColor;
        else if(   pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].format != VK_FORMAT_UNDEFINED
                && pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].format == VK_FORMAT_UNDEFINED)
            pipelineCache->shaderVertex = context->shaderVertex2dUv;
        else
            pipelineCache->shaderVertex = context->shaderVertex2d;
    else if(pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].format == VK_FORMAT_R32G32B32_SFLOAT)
        if(   pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].format == VK_FORMAT_UNDEFINED
           && pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].format != VK_FORMAT_UNDEFINED)
            pipelineCache->shaderVertex = context->shaderVertex3dUv;
        else if(   pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].format != VK_FORMAT_UNDEFINED
                && pipelineCache->vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV   ].format == VK_FORMAT_UNDEFINED)
            pipelineCache->shaderVertex = context->shaderVertex3dColor;
        else
            pipelineCache->shaderVertex = context->shaderVertex3d;
    if(surfaceContext->drawState.transientShaderHandleFragment)
    {
        const u$ shaderIndex = surfaceContext->drawState.transientShaderHandleFragment - 1;
        korl_assert(shaderIndex < arrlenu(context->stbDaShaders));
        pipelineCache->shaderFragment = context->stbDaShaders[shaderIndex].shaderModule;
    }
    else if(surfaceContext->drawState.materialMaps.base)
        pipelineCache->shaderFragment = context->shaderFragmentColorTexture;
    else
        pipelineCache->shaderFragment = context->shaderFragmentColor;
    /* reset the transient shader handles, requiring the user to re-set them via setDrawState for future draw calls */
    surfaceContext->drawState.transientShaderHandleVertex   = 0;
    surfaceContext->drawState.transientShaderHandleFragment = 0;
    /* now we can actually configure the pipeline */
    const u$ pipelinePrevious = surfaceContext->drawState.currentPipeline;
    _korl_vulkan_setPipelineMetaData(surfaceContext->drawState.pipelineConfigurationCache);
    korl_assert(surfaceContext->drawState.currentPipeline < arrlenu(context->stbDaPipelines) && arrlenu(context->stbDaPipelines) > 0);//try to make sure we have selected a valid pipeline before going further
    if(pipelinePrevious != surfaceContext->drawState.currentPipeline)
        vkCmdBindPipeline(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                         ,VK_PIPELINE_BIND_POINT_GRAPHICS
                         ,context->stbDaPipelines[surfaceContext->drawState.currentPipeline].pipeline);
}
korl_internal void _korl_vulkan_flushDescriptors(void)
{
    _Korl_Vulkan_Context*const               context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const        surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* stage uniform data (descriptor set staging);  here we will examine the 
        current draw state & see if it differs from the last used draw state;  
        if it does, we will compose the minimum # of descriptor set stages & 
        writes to update the device draw state appropriately;  only then will we 
        update/bind descriptor sets, since this is a very expensive operation! */
    VkDeviceSize byteOffsetStagingBuffer = 0;
    VkBuffer     bufferStaging           = VK_NULL_HANDLE;
    KORL_ZERO_STACK_ARRAY(VkWriteDescriptorSet           , descriptorSetWrites         , _KORL_VULKAN_DESCRIPTOR_BINDING_TOTAL);
    KORL_ZERO_STACK_ARRAY(_Korl_Vulkan_DescriptorSetIndex, descriptorSetWriteSetIndices, _KORL_VULKAN_DESCRIPTOR_BINDING_TOTAL);
    uint32_t descriptorWriteCount = 0;
    KORL_ZERO_STACK_ARRAY(bool, descriptorSetIndicesChanged, _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT);
    KORL_MEMORY_POOL_DECLARE(VkDescriptorBufferInfo, descriptorBufferInfos, _KORL_VULKAN_DESCRIPTOR_BINDING_TOTAL);
    KORL_MEMORY_POOL_ZERO(descriptorBufferInfos);
    KORL_MEMORY_POOL_DECLARE(VkDescriptorImageInfo, descriptorImageInfos, _KORL_VULKAN_DESCRIPTOR_BINDING_TOTAL);
    KORL_MEMORY_POOL_ZERO(descriptorImageInfos);
    // _KORL_VULKAN_DESCRIPTOR_SET_INDEX_SCENE //
    if(0 != korl_memory_compare(&surfaceContext->drawState.uboSceneProperties// only ever get new VP uniform staging memory if the VP state has changed!
                               ,&surfaceContext->drawStateLast.uboSceneProperties
                               ,sizeof(surfaceContext->drawState.uboSceneProperties)))
    {
        /* stage the UBO transforms */
        _Korl_Vulkan_Uniform_SceneProperties*const stagingMemoryUniformTransforms = 
            _korl_vulkan_getDescriptorStagingPool(sizeof(*stagingMemoryUniformTransforms), &bufferStaging, &byteOffsetStagingBuffer);
        *stagingMemoryUniformTransforms = surfaceContext->drawState.uboSceneProperties;
        /* prepare a descriptor set write with the staged UBO */
        VkDescriptorBufferInfo*const descriptorBufferInfo = KORL_MEMORY_POOL_ADD(descriptorBufferInfos);
        descriptorBufferInfo->buffer = bufferStaging;
        descriptorBufferInfo->range  = sizeof(*stagingMemoryUniformTransforms);
        descriptorBufferInfo->offset = byteOffsetStagingBuffer;
        const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_SCENE_TRANSFORMS 
                                                                            + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_SCENE_PROPERTIES_UBO;
        descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
        descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
        descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
        descriptorSetWrites[descriptorWriteCount].pBufferInfo     = descriptorBufferInfo;
        // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
        descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_SCENE;
        descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
        descriptorWriteCount++;
    }
    // _KORL_VULKAN_DESCRIPTOR_SET_INDEX_LIGHTS //
    if(   KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights) != KORL_MEMORY_POOL_SIZE(surfaceContext->drawStateLast.lights)
       || 0 != korl_memory_compare(&surfaceContext->drawState.lights
                                  ,&surfaceContext->drawStateLast.lights
                                  ,KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights) * sizeof(*surfaceContext->drawState.lights)))
    {
        /* stage the Lighting SSBO */
        typedef struct _Korl_Vulkan_ShaderBuffer_Lights
        {
            u32 lightsSize;
            u32 _padding_0[3];
            Korl_Gfx_Light lights[1];// array size == `lightsSize`
        } _Korl_Vulkan_ShaderBuffer_Lights;
        const VkDeviceSize bufferBytes = sizeof(_Korl_Vulkan_ShaderBuffer_Lights) - sizeof(Korl_Gfx_Light) + KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights) * sizeof(Korl_Gfx_Light);
        _Korl_Vulkan_ShaderBuffer_Lights*const stagingMemoryBufferLights = 
            _korl_vulkan_getDescriptorStagingPool(bufferBytes, &bufferStaging, &byteOffsetStagingBuffer);
        stagingMemoryBufferLights->lightsSize = KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights);
        korl_memory_copy(stagingMemoryBufferLights->lights, surfaceContext->drawState.lights, KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights) * sizeof(*surfaceContext->drawState.lights));
        for(Korl_MemoryPool_Size i = 0; i < KORL_MEMORY_POOL_SIZE(surfaceContext->drawState.lights); i++)
        {
            Korl_Gfx_Light*const light = surfaceContext->drawState.lights + i;
            stagingMemoryBufferLights->lights[i].position  = korl_math_m4f32_multiplyV4f32Copy(&surfaceContext->drawState.uboSceneProperties.m4f32View, (Korl_Math_V4f32){.xyz = light->position, 1}).xyz;
            stagingMemoryBufferLights->lights[i].direction = korl_math_m4f32_multiplyV4f32Copy(&surfaceContext->drawState.uboSceneProperties.m4f32View, (Korl_Math_V4f32){.xyz = light->direction, 0}).xyz;
        }
        /* prepare a descriptor set write with the staged UBO */
        VkDescriptorBufferInfo*const descriptorBufferInfo = KORL_MEMORY_POOL_ADD(descriptorBufferInfos);
        descriptorBufferInfo->buffer = bufferStaging;
        descriptorBufferInfo->range  = bufferBytes;
        descriptorBufferInfo->offset = byteOffsetStagingBuffer;
        const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_LIGHTS 
                                                                            + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_LIGHTS_SSBO;
        descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
        descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
        descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
        descriptorSetWrites[descriptorWriteCount].pBufferInfo     = descriptorBufferInfo;
        // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
        descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_LIGHTS;
        descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
        descriptorWriteCount++;
    }
    // _KORL_VULKAN_DESCRIPTOR_SET_INDEX_STORAGE //
    if(   surfaceContext->drawState.vertexStorageBuffer
       && surfaceContext->drawState.vertexStorageBuffer != surfaceContext->drawStateLast.vertexStorageBuffer)
    {
        _Korl_Vulkan_DeviceMemory_Alloctation* deviceMemoryAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, surfaceContext->drawState.vertexStorageBuffer);
        korl_assert(!deviceMemoryAllocation->freeQueued);
        VkDescriptorBufferInfo*const descriptorBufferInfo = KORL_MEMORY_POOL_ADD(descriptorBufferInfos);
        descriptorBufferInfo->buffer = deviceMemoryAllocation->subType.buffer.vulkanBuffer;
        descriptorBufferInfo->range  = deviceMemoryAllocation->bytesUsed;
        #if 0 // already set to default value
        descriptorBufferInfo->offset = 0;
        #endif
        const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_STORAGE 
                                                                            + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_VERTEX_STORAGE_SSBO;
        descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
        descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
        descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
        descriptorSetWrites[descriptorWriteCount].pBufferInfo     = descriptorBufferInfo;
        // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
        descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_STORAGE;
        descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
        descriptorWriteCount++;
    }
    // _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL //
    if(   // write material UBO uniform if it has changed //
          0 != korl_memory_compare(&surfaceContext->drawState.uboMaterialProperties
                                  ,&surfaceContext->drawStateLast.uboMaterialProperties
                                  ,sizeof(surfaceContext->drawState.uboMaterialProperties))
          // conditionally write the texture sampler if we require it //
       || (surfaceContext->drawState.materialMaps.base && surfaceContext->drawState.materialMaps.base != surfaceContext->drawStateLast.materialMaps.base)
       || (surfaceContext->drawState.materialMaps.specular && surfaceContext->drawState.materialMaps.specular != surfaceContext->drawStateLast.materialMaps.specular)
       || (surfaceContext->drawState.materialMaps.emissive && surfaceContext->drawState.materialMaps.emissive != surfaceContext->drawStateLast.materialMaps.emissive))
    {
        {
            /* stage the UBO */
            Korl_Gfx_Material_Properties*const stagingMemoryUboMaterial = 
                _korl_vulkan_getDescriptorStagingPool(sizeof(*stagingMemoryUboMaterial), &bufferStaging, &byteOffsetStagingBuffer);
            *stagingMemoryUboMaterial = surfaceContext->drawState.uboMaterialProperties;
            /* prepare a descriptor set write with the staged UBO */
            VkDescriptorBufferInfo*const descriptorBufferInfo = KORL_MEMORY_POOL_ADD(descriptorBufferInfos);
            descriptorBufferInfo->buffer = bufferStaging;
            descriptorBufferInfo->range  = sizeof(*stagingMemoryUboMaterial);
            descriptorBufferInfo->offset = byteOffsetStagingBuffer;
            const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL 
                                                                                + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_UBO;
            descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
            descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
            descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
            descriptorSetWrites[descriptorWriteCount].pBufferInfo     = descriptorBufferInfo;
            // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
            descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL;
            descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
            descriptorWriteCount++;
        }
        if(surfaceContext->drawState.materialMaps.base)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const textureAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, surfaceContext->drawState.materialMaps.base);
            korl_assert(!textureAllocation->freeQueued);
            #if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
            const _Korl_Vulkan_DeviceAssetHandle_Unpacked deviceAssetHandleUnpacked = _korl_vulkan_deviceAssetHandle_unpack(surfaceContext->drawState.materialMaps.resourceHandleTextureBase);
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaInUseDeviceAssetIndices, deviceAssetHandleUnpacked.databaseIndex);
            #endif
            korl_assert(textureAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
            VkDescriptorImageInfo*const descriptorImageInfo = KORL_MEMORY_POOL_ADD(descriptorImageInfos);
            descriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo->imageView   = textureAllocation->subType.texture.imageView;
            descriptorImageInfo->sampler     = textureAllocation->subType.texture.sampler;
            korl_assert(descriptorWriteCount < korl_arraySize(descriptorSetWrites));
            const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL 
                                                                                + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_BASE;
            descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
            descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
            descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
            descriptorSetWrites[descriptorWriteCount].pImageInfo      = descriptorImageInfo;
            // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
            descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL;
            descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
            descriptorWriteCount++;
        }
        if(surfaceContext->drawState.materialMaps.specular)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const textureAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, surfaceContext->drawState.materialMaps.specular);
            korl_assert(!textureAllocation->freeQueued);
            #if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
            const _Korl_Vulkan_DeviceAssetHandle_Unpacked deviceAssetHandleUnpacked = _korl_vulkan_deviceAssetHandle_unpack(surfaceContext->drawState.materialMaps.resourceHandleTextureSpecular);
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaInUseDeviceAssetIndices, deviceAssetHandleUnpacked.databaseIndex);
            #endif
            korl_assert(textureAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
            VkDescriptorImageInfo*const descriptorImageInfo = KORL_MEMORY_POOL_ADD(descriptorImageInfos);
            descriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo->imageView   = textureAllocation->subType.texture.imageView;
            descriptorImageInfo->sampler     = textureAllocation->subType.texture.sampler;
            korl_assert(descriptorWriteCount < korl_arraySize(descriptorSetWrites));
            const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL 
                                                                                + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_SPECULAR;
            descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
            descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
            descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
            descriptorSetWrites[descriptorWriteCount].pImageInfo      = descriptorImageInfo;
            // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
            descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL;
            descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
            descriptorWriteCount++;
        }
        if(surfaceContext->drawState.materialMaps.emissive)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const textureAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, surfaceContext->drawState.materialMaps.emissive);
            korl_assert(!textureAllocation->freeQueued);
            #if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
            const _Korl_Vulkan_DeviceAssetHandle_Unpacked deviceAssetHandleUnpacked = _korl_vulkan_deviceAssetHandle_unpack(surfaceContext->drawState.materialMaps.resourceHandleTextureEmissive);
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), swapChainImageContext->stbDaInUseDeviceAssetIndices, deviceAssetHandleUnpacked.databaseIndex);
            #endif
            korl_assert(textureAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
            VkDescriptorImageInfo*const descriptorImageInfo = KORL_MEMORY_POOL_ADD(descriptorImageInfos);
            descriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo->imageView   = textureAllocation->subType.texture.imageView;
            descriptorImageInfo->sampler     = textureAllocation->subType.texture.sampler;
            korl_assert(descriptorWriteCount < korl_arraySize(descriptorSetWrites));
            const VkDescriptorSetLayoutBinding*const descriptorSetLayoutBinding = _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL 
                                                                                + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_EMISSIVE;
            descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorSetWrites[descriptorWriteCount].dstBinding      = descriptorSetLayoutBinding->binding;
            descriptorSetWrites[descriptorWriteCount].descriptorType  = descriptorSetLayoutBinding->descriptorType;
            descriptorSetWrites[descriptorWriteCount].descriptorCount = descriptorSetLayoutBinding->descriptorCount;
            descriptorSetWrites[descriptorWriteCount].pImageInfo      = descriptorImageInfo;
            // defer writing a destination set, since we don't want to allocate a descriptor set unless we need to; descriptor set allocation/binding is _EXPENSIVE_!
            descriptorSetWriteSetIndices[descriptorWriteCount] = _KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL;
            descriptorSetIndicesChanged[descriptorSetWriteSetIndices[descriptorWriteCount]] = true;
            descriptorWriteCount++;
        }
    }
    korl_assert(descriptorWriteCount <= korl_arraySize(descriptorSetWrites));
    /* allocate a new descriptor set conditionally, and configure the composed 
        descriptor set writes to modify it */
    KORL_ZERO_STACK_ARRAY(VkDescriptorSet, descriptorSets, _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT);
    if(descriptorWriteCount)
    {
        korl_time_probeStart(draw_descriptor_set_alloc); {
            for(u32 d = 0; d < _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT; d++)
                if(descriptorSetIndicesChanged[d])
                    descriptorSets[d] = _korl_vulkan_newDescriptorSet(context->descriptorSetLayouts[d]);
            for(uint32_t d = 0; d < descriptorWriteCount; d++)
                descriptorSetWrites[d].dstSet = descriptorSets[descriptorSetWriteSetIndices[d]];
        } korl_time_probeStop(draw_descriptor_set_alloc);
        vkUpdateDescriptorSets(context->device, descriptorWriteCount, descriptorSetWrites, 0/*copyCount*/, NULL/*copies*/);
    }
    for(u32 d = 0; d < _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT; d++)
    {
        if(descriptorSets[d] == VK_NULL_HANDLE)
            continue;
        /* in an effort to minimize the total number of calls to vkCmdBindDescriptorSets, 
            we try to accumulate the largest possible number of contiguous descriptor sets 
            which are waiting to be bound: */
        const uint32_t firstSet = d;
        uint32_t setCount = 1;
        while(d + 1 < _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT)
        {
            if(descriptorSets[d + 1] == VK_NULL_HANDLE)
                break;
            setCount++;
            d++;
        }
        vkCmdBindDescriptorSets(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                               ,VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipelineLayout
                               ,firstSet
                               ,setCount/*set count */, &(descriptorSets[firstSet])
                               ,/*dynamic offset count*/0, /*pDynamicOffsets*/NULL);
    }
}
korl_internal void _korl_vulkan_draw(VkBuffer buffer, VkDeviceSize bufferByteOffset, const Korl_Gfx_VertexStagingMeta* stagingMeta)
{
    _Korl_Vulkan_Context*const               context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const        surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* if the swap chain image context is invalid for this frame for some reason, 
        then just do nothing (this happens during deferred resize for example) */
    if(surfaceContext->frameSwapChainImageIndex == _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE)
        return;
    /* it is possible for the graphics command buffer to be not available for 
        this frame (such as a minimized window); do nothing if that happens */
    if(!surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics)
        return;
    _korl_vulkan_flushPipelineState(stagingMeta);// calls vkCmdBindPipeline
    _korl_vulkan_flushDescriptors();// calls vkUpdateDescriptorSets & vkCmdBindDescriptorSets
    /* compose the draw commands */
    vkCmdPushConstants(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics, context->pipelineLayout
                      ,VK_SHADER_STAGE_VERTEX_BIT
                      ,/*offset*/0, sizeof(surfaceContext->drawState.pushConstants.vertex)
                      ,&surfaceContext->drawState.pushConstants.vertex);
    vkCmdPushConstants(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics, context->pipelineLayout
                      ,VK_SHADER_STAGE_FRAGMENT_BIT
                      ,/*offset*/sizeof(surfaceContext->drawState.pushConstants.vertex), sizeof(surfaceContext->drawState.pushConstants.fragment)
                      ,&surfaceContext->drawState.pushConstants.fragment);
    vkCmdSetScissor(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                   ,0/*firstScissor*/, 1/*scissorCount*/, &surfaceContext->drawState.scissor);
    VkBuffer     vertexBuffers          [KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT];
    VkDeviceSize vertexBufferByteOffsets[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT];
    for(u8 i = 0; i < KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT; i++)
    {
        vertexBuffers[i]           = buffer;
        vertexBufferByteOffsets[i] = bufferByteOffset + stagingMeta->vertexAttributeDescriptors[i].byteOffsetBuffer;
    }
    vkCmdBindVertexBuffers(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                          ,0/*first binding; we're just going to re-bind all our bindings*/, korl_arraySize(vertexBuffers)
                          ,vertexBuffers, vertexBufferByteOffsets);
    const uint32_t instanceCount = stagingMeta->instanceCount ? stagingMeta->instanceCount : 1;// allow user to pass 0 for instanceCount, to implicitly draw one instance
    if(stagingMeta->indexCount)
    {
        VkIndexType indexType = VK_INDEX_TYPE_NONE_KHR;
        switch(stagingMeta->indexType)
        {
        case KORL_GFX_VERTEX_INDEX_TYPE_U16    : indexType = VK_INDEX_TYPE_UINT16;   break;
        case KORL_GFX_VERTEX_INDEX_TYPE_U32    : indexType = VK_INDEX_TYPE_UINT32;   break;
        case KORL_GFX_VERTEX_INDEX_TYPE_INVALID: indexType = VK_INDEX_TYPE_NONE_KHR; break;
        }
        vkCmdBindIndexBuffer(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                            ,buffer
                            ,bufferByteOffset + stagingMeta->indexByteOffsetBuffer
                            ,indexType);
        vkCmdDrawIndexed(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                        ,stagingMeta->indexCount, instanceCount, /*firstIndex*/0, /*vertexOffset*/0, /*firstInstance*/0);
    }
    else
        vkCmdDraw(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferGraphics
                 ,stagingMeta->vertexCount, instanceCount, /*firstVertex*/0, /*firstInstance*/0);
    surfaceContext->drawStateLast = surfaceContext->drawState;
}
korl_internal void korl_vulkan_drawStagingAllocation(const Korl_Gfx_StagingAllocation* stagingAllocation, const Korl_Gfx_VertexStagingMeta* stagingMeta)
{
    _korl_vulkan_draw(stagingAllocation->deviceBuffer, stagingAllocation->deviceBufferOffset, stagingMeta);
}
korl_internal void korl_vulkan_drawVertexBuffer(Korl_Vulkan_DeviceMemory_AllocationHandle vertexBuffer, const Korl_Gfx_VertexStagingMeta* stagingMeta)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* attempt to obtain the vertex buffer device memory allocation; we can't do anything if the handle is invalid */
    _Korl_Vulkan_DeviceMemory_Alloctation*const bufferAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, vertexBuffer);
    if(!bufferAllocation || bufferAllocation->freeQueued)
        return;
    korl_assert(bufferAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER);// sanity check; make sure user is using a Buffer allocation
    /**/
    _korl_vulkan_draw(bufferAllocation->subType.buffer.vulkanBuffer, 0/*bufferByteOffset*/, stagingMeta);
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createTexture(const Korl_Vulkan_CreateInfoTexture* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    return _korl_vulkan_deviceMemory_allocateTexture(&surfaceContext->deviceMemoryDeviceLocal
                                                    ,createInfo->sizeX, createInfo->sizeY
                                                    ,  VK_IMAGE_USAGE_TRANSFER_DST_BIT 
                                                     | VK_IMAGE_USAGE_SAMPLED_BIT
                                                    ,requiredHandle
                                                    ,NULL/*out_allocation*/);
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createVertexBuffer(const Korl_Vulkan_CreateInfoVertexBuffer* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* determine the buffer usage flags based on the buffer's attribute descriptors */
    VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT/* in order allow the user to "resize" a buffer, we need this to allow us to transfer data from the old buffer to the new buffer*/;
    if(createInfo->usageFlags & KORL_VULKAN_BUFFER_USAGE_FLAG_INDEX)
        bufferUsageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if(createInfo->usageFlags & KORL_VULKAN_BUFFER_USAGE_FLAG_VERTEX)
        bufferUsageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if(createInfo->usageFlags & KORL_VULKAN_BUFFER_USAGE_FLAG_STORAGE)
        bufferUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    /* create the buffer */
    _Korl_Vulkan_DeviceMemory_Alloctation* deviceMemoryAllocation = NULL;
    Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle = 
        _korl_vulkan_deviceMemory_allocateBuffer(&surfaceContext->deviceMemoryDeviceLocal
                                                ,createInfo->bytes
                                                ,bufferUsageFlags
                                                ,VK_SHARING_MODE_EXCLUSIVE
                                                ,requiredHandle
                                                ,&deviceMemoryAllocation);
    /**/
    return allocationHandle;
}
korl_internal void korl_vulkan_deviceAsset_destroy(Korl_Vulkan_DeviceMemory_AllocationHandle deviceAssetHandle)
{
    _Korl_Vulkan_Context*const        context        = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_DeviceMemory_Alloctation*const deviceMemoryAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, deviceAssetHandle);
    korl_assert(deviceMemoryAllocation);
    korl_assert(!deviceMemoryAllocation->freeQueued);
    deviceMemoryAllocation->freeQueued = true;
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), surfaceContext->stbDaDeviceLocalFreeQueue, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Vulkan_QueuedFreeDeviceLocalAllocation));
    arrlast(surfaceContext->stbDaDeviceLocalFreeQueue).allocationHandle = deviceAssetHandle;
}
korl_internal void korl_vulkan_texture_update(Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle, const Korl_Vulkan_Color4u8* pixelData)
{
    _Korl_Vulkan_Context*const        context        = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_DeviceMemory_Alloctation*const deviceMemoryAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, textureHandle);
    korl_assert(deviceMemoryAllocation);
    korl_assert(deviceMemoryAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
    /* allocate staging buffer space to store the pixel data, since we can only 
        transfer from staging => device memory */
    const VkDeviceSize imageBytes = deviceMemoryAllocation->subType.texture.sizeX
                                  * deviceMemoryAllocation->subType.texture.sizeY
                                  * sizeof(*pixelData);
    VkBuffer bufferStaging;
    VkDeviceSize bufferStagingOffset;
    void*const pixelDataStagingMemory = _korl_vulkan_getStagingPool(imageBytes, 0/*alignment*/, &bufferStaging, &bufferStagingOffset);
    /* copy the pixel data to the staging buffer */
    korl_memory_copy(pixelDataStagingMemory, pixelData, imageBytes);
    /* compose the transfer commands to upload from staging => device-local memory */
    // transition the image layout from undefined => transfer_destination_optimal //
    KORL_ZERO_STACK(VkImageMemoryBarrier, barrierImageMemory);
    barrierImageMemory.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierImageMemory.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierImageMemory.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierImageMemory.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrierImageMemory.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrierImageMemory.image                           = deviceMemoryAllocation->subType.texture.image;
    barrierImageMemory.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierImageMemory.subresourceRange.levelCount     = 1;
    barrierImageMemory.subresourceRange.layerCount     = 1;
    barrierImageMemory.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    #if 0// already defaulted to 0
    barrierImageMemory.subresourceRange.baseMipLevel   = 0;
    barrierImageMemory.subresourceRange.baseArrayLayer = 0;
    barrierImageMemory.srcAccessMask                   = 0;
    #endif
    vkCmdPipelineBarrier(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer
                        ,/*srcStageMask*/VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                        ,/*dstStageMask*/VK_PIPELINE_STAGE_TRANSFER_BIT
                        ,/*dependencyFlags*/0
                        ,/*memoryBarrierCount*/0, /*pMemoryBarriers*/NULL
                        ,/*bufferBarrierCount*/0, /*bufferBarriers*/NULL
                        ,/*imageBarrierCount*/1, &barrierImageMemory);
    // copy the buffer from staging buffer => device-local image //
    KORL_ZERO_STACK(VkBufferImageCopy, imageCopyRegion);
    // default zero values indicate no buffer padding, copy to image origin, copy to base mip level & array layer
    imageCopyRegion.bufferOffset                = bufferStagingOffset;
    imageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.imageSubresource.layerCount = 1;
    imageCopyRegion.imageExtent.width           = deviceMemoryAllocation->subType.texture.sizeX;
    imageCopyRegion.imageExtent.height          = deviceMemoryAllocation->subType.texture.sizeY;
    imageCopyRegion.imageExtent.depth           = 1;
    vkCmdCopyBufferToImage(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer
                          ,bufferStaging, deviceMemoryAllocation->subType.texture.image
                          ,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /*regionCount*/1, &imageCopyRegion);
    // transition the image layout from tranfer_destination_optimal => shader_read_only_optimal //
    // we can recycle the VkImageMemoryBarrier from the first transition here:  
    barrierImageMemory.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierImageMemory.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierImageMemory.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierImageMemory.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer
                        ,/*srcStageMask*/VK_PIPELINE_STAGE_TRANSFER_BIT
                        ,/*dstStageMask*/VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                        ,/*dependencyFlags*/0
                        ,/*memoryBarrierCount*/0, /*pMemoryBarriers*/NULL
                        ,/*bufferBarrierCount*/0, /*bufferBarriers*/NULL
                        ,/*imageBarrierCount*/1, &barrierImageMemory);
    // KORL-ISSUE-000-000-092: vulkan: even though this technique _should_ make sure that the upcoming frame will have the correct pixel data, we are still not properly isolating texture memory from frames that are still WIP
}
korl_internal Korl_Math_V2u32 korl_vulkan_texture_getSize(const Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_DeviceMemory_Alloctation*const deviceMemoryAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, textureHandle);
    korl_assert(deviceMemoryAllocation);
    korl_assert(deviceMemoryAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
    return (Korl_Math_V2u32){.x = deviceMemoryAllocation->subType.texture.sizeX
                            ,.y = deviceMemoryAllocation->subType.texture.sizeY};
}
korl_internal void korl_vulkan_vertexBuffer_resize(Korl_Vulkan_DeviceMemory_AllocationHandle* in_out_bufferHandle, u$ bytes)
{
    _Korl_Vulkan_Context*const        context        = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* if we're resizing to 0 for some reason, just delete the old buffer & return an invalid handle */
    if(!bytes)
    {
        korl_vulkan_deviceAsset_destroy(*in_out_bufferHandle);
        *in_out_bufferHandle = 0;
        return;
    }
    /* get the buffer that already exists */
    _Korl_Vulkan_DeviceMemory_Alloctation*const deviceMemoryAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, *in_out_bufferHandle);
    korl_assert(deviceMemoryAllocation);
    korl_assert(deviceMemoryAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER);
    /* create a new buffer of the new size */
    _Korl_Vulkan_DeviceMemory_Alloctation* resizedAllocation = NULL;
    Korl_Vulkan_DeviceMemory_AllocationHandle resizedAllocationHandle = 
        _korl_vulkan_deviceMemory_allocateBuffer(&surfaceContext->deviceMemoryDeviceLocal
                                                ,bytes
                                                ,deviceMemoryAllocation->subType.buffer.bufferUsageFlags
                                                ,VK_SHARING_MODE_EXCLUSIVE
                                                ,0// 0 => generate new handle automatically
                                                ,&resizedAllocation);
    /* compose memory copy commands to copy from the old device memory allocation to the new one */
    KORL_ZERO_STACK_ARRAY(VkBufferCopy, copyRegions, 1);
    copyRegions[0].size = KORL_MATH_MIN(bytes, deviceMemoryAllocation->bytesUsed);
    vkCmdCopyBuffer(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer
                   ,deviceMemoryAllocation->subType.buffer.vulkanBuffer
                   ,resizedAllocation->subType.buffer.vulkanBuffer
                   ,korl_arraySize(copyRegions), copyRegions);
    /* queue the old buffer for nullification, and give the user the new handle */
    korl_vulkan_deviceAsset_destroy(*in_out_bufferHandle);
    *in_out_bufferHandle = resizedAllocationHandle;
}
korl_internal void korl_vulkan_vertexBuffer_update(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle, const void* data, u$ dataBytes, u$ deviceLocalBufferOffset)
{
    _Korl_Vulkan_Context*const        context        = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_DeviceMemory_Alloctation*const deviceMemoryAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, bufferHandle);
    korl_assert(deviceMemoryAllocation);
    korl_assert(deviceMemoryAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER);
    /* allocate staging buffer space for the updated data */
    VkBuffer bufferStaging;
    VkDeviceSize bufferStagingOffset;
    void*const stagingMemory = _korl_vulkan_getStagingPool(dataBytes, 0/*alignment*/, &bufferStaging, &bufferStagingOffset);
    /* copy the data to the staging buffer */
    korl_memory_copy(stagingMemory, data, dataBytes);
    /* compose the transfer commands to upload from staging => device-local memory */
    KORL_ZERO_STACK_ARRAY(VkBufferCopy, copyRegions, 1);
    copyRegions[0].srcOffset = bufferStagingOffset;
    copyRegions[0].dstOffset = deviceLocalBufferOffset;
    copyRegions[0].size      = KORL_MATH_MIN(dataBytes, deviceMemoryAllocation->bytesUsed);
    vkCmdCopyBuffer(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer
                   ,bufferStaging
                   ,deviceMemoryAllocation->subType.buffer.vulkanBuffer
                   ,korl_arraySize(copyRegions), copyRegions);
}
korl_internal void* korl_vulkan_vertexBuffer_getStagingBuffer(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle, u$ bytes, u$ bufferByteOffset)
{
    _Korl_Vulkan_Context*const        context        = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* get device memory allocation; perform sanity checks */
    _Korl_Vulkan_DeviceMemory_Alloctation*const deviceMemoryAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryDeviceLocal, bufferHandle);
    korl_assert(deviceMemoryAllocation);
    korl_assert(bufferByteOffset + bytes <= deviceMemoryAllocation->bytesUsed);
    /* allocate staging buffer space for the updated data */
    VkBuffer     bufferStaging;
    VkDeviceSize bufferStagingOffset;
    void*const stagingMemory = _korl_vulkan_getStagingPool(bytes, 0/*alignment*/, &bufferStaging, &bufferStagingOffset);
    /* compose the transfer commands to upload from staging => device-local memory */
    KORL_ZERO_STACK_ARRAY(VkBufferCopy, copyRegions, 1);
    copyRegions[0].srcOffset = bufferStagingOffset;
    copyRegions[0].dstOffset = bufferByteOffset;
    copyRegions[0].size      = bytes;
    vkCmdCopyBuffer(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].commandBufferTransfer
                   ,bufferStaging
                   ,deviceMemoryAllocation->subType.buffer.vulkanBuffer
                   ,korl_arraySize(copyRegions), copyRegions);
    /**/
    return stagingMemory;
}
korl_internal Korl_Vulkan_ShaderHandle korl_vulkan_shader_create(const Korl_Vulkan_CreateInfoShader* createInfo, Korl_Vulkan_ShaderHandle requiredHandle)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /* find an unoccupied database index */
    Korl_Vulkan_ShaderHandle shaderIndex = arrlenu(context->stbDaShaders);
    _Korl_Vulkan_Shader*     shader      = NULL;
    if(requiredHandle)
    {
        shaderIndex = requiredHandle - 1;
        shader      = context->stbDaShaders + shaderIndex;
        korl_assert(shader->shaderModule == VK_NULL_HANDLE);// the caller _must_ know that this handle is unoccupied via some other korl-vulkan API call
    }
    else
    {
        const _Korl_Vulkan_Shader*const shadersEnd = context->stbDaShaders + arrlen(context->stbDaShaders);
        for(_Korl_Vulkan_Shader* s = context->stbDaShaders; s < shadersEnd && !shader; s++)
        {
            if(s->shaderModule != VK_NULL_HANDLE)
                continue;
            shader      = s;
            shaderIndex = s - context->stbDaShaders;
        }
    }
    /* if we couldn't find an unoccupied database entry, we need to expand the database */
    if(!shader)
    {
        shaderIndex = arrlenu(context->stbDaShaders);
        mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaShaders, 2 * arrlen(context->stbDaShaders));
        shader = context->stbDaShaders + shaderIndex;
        korl_memory_zero(shader, shaderIndex * sizeof(*shader));// zero out the newly-allocated shader slots
    }
    /* create the vulkan shader module & add to the database */
    KORL_ZERO_STACK(VkShaderModuleCreateInfo, vCreateInfo);
    vCreateInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vCreateInfo.pCode    = createInfo->data;
    vCreateInfo.codeSize = createInfo->dataBytes;
    vkCreateShaderModule(context->device, &vCreateInfo, context->allocator, &shader->shaderModule);
    /* construct & return the handle */
    return shaderIndex + 1;
}
korl_internal void korl_vulkan_shader_destroy(Korl_Vulkan_ShaderHandle shaderHandle)
{
    if(!shaderHandle)
        return;
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    const u$ shaderIndex = shaderHandle - 1;
    /* for now, just treat the shaderHandle as a direct index */
    korl_assert(shaderHandle < arrlenu(context->stbDaShaders));
    _Korl_Vulkan_Shader*const shader = context->stbDaShaders + shaderIndex;
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaShaderTrash, ((_Korl_Vulkan_ShaderTrash){.shader = *shader, .framesSinceQueued = 0}));
    shader->shaderModule = VK_NULL_HANDLE;
}
