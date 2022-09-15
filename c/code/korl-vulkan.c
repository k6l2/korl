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
#if defined(KORL_PLATFORM_WINDOWS)
#include <vulkan/vulkan_win32.h>
#endif// defined(KORL_PLATFORM_WINDOWS)
#define _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_UBO     0/// @TODO: rename to _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_UNIFORM_TRANSFORMS
#define _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_TEXTURE 1
#define _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_POSITION 0
#define _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_COLOR    1
#define _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_UV       2
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
#define _KORL_VULKAN_CHECK(operation) korl_assert((operation) == VK_SUCCESS);
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
korl_internal bool _korl_vulkan_isBetterDevice(
    const VkPhysicalDevice deviceOld, 
    const VkPhysicalDevice deviceNew, 
    const _Korl_Vulkan_QueueFamilyMetaData*const queueFamilyMetaData, 
    const _Korl_Vulkan_DeviceSurfaceMetaData*const deviceSurfaceMetaData, 
    const VkPhysicalDeviceProperties*const propertiesOld, 
    const VkPhysicalDeviceProperties*const propertiesNew, 
    const VkPhysicalDeviceFeatures*const featuresOld, 
    const VkPhysicalDeviceFeatures*const featuresNew)
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
    // surfaceContext->hasStencilComponent = formatDepthBuffer == VK_FORMAT_D32_SFLOAT_S8_UINT
    //                                    || formatDepthBuffer == VK_FORMAT_D24_UNORM_S8_UINT;
    // now that we have selected a format, we can create the depth buffer image //
    surfaceContext->depthStencilImageBuffer = _korl_vulkan_deviceMemory_allocateImageBuffer(&surfaceContext->deviceMemoryRenderResources, 
                                                                                            surfaceContext->swapChainImageExtent.width, 
                                                                                            surfaceContext->swapChainImageExtent.height, 
                                                                                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
                                                                                            VK_IMAGE_ASPECT_DEPTH_BIT, formatDepthBuffer, depthBufferTiling);
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
        _Korl_Vulkan_DeviceMemory_Alloctation*const allocationDepthStencilImageBuffer = 
            _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryRenderResources, surfaceContext->depthStencilImageBuffer);
        VkImageView frameBufferAttachments[] = 
            { surfaceContext->swapChainImageContexts[i].imageView
            , allocationDepthStencilImageBuffer->deviceObject.imageBuffer.imageView };
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
        /* create a separate transient command pool for each frame */
        KORL_ZERO_STACK(VkCommandPoolCreateInfo, createInfoCommandPool);
        createInfoCommandPool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfoCommandPool.queueFamilyIndex = context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics;
        createInfoCommandPool.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        _KORL_VULKAN_CHECK(
            vkCreateCommandPool(context->device, &createInfoCommandPool, context->allocator, 
                                &surfaceContext->swapChainImageContexts[i].commandPool));
        /* initialize pool of descriptor pools */
        mcarrsetcap(KORL_C_CAST(void*, context->allocatorHandle), surfaceContext->swapChainImageContexts[i].stbDaDescriptorPools, 8);
    }
#if 0///@TODO: delete/recycle
    /* allocate a command buffer for each of the swap chain frame buffers */
    KORL_ZERO_STACK(VkCommandBufferAllocateInfo, allocateInfoCommandBuffers);
    allocateInfoCommandBuffers.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfoCommandBuffers.commandPool        = context->commandPoolGraphics;
    allocateInfoCommandBuffers.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfoCommandBuffers.commandBufferCount = surfaceContext->swapChainImagesSize;
    _KORL_VULKAN_CHECK(
        vkAllocateCommandBuffers(context->device, &allocateInfoCommandBuffers, 
                                 surfaceContext->swapChainCommandBuffers));
#endif
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
        vkFreeCommandBuffers(context->device,swapChainImageContext->commandPool, 1, &(swapChainImageContext->commandBufferGraphics));
        vkFreeCommandBuffers(context->device,swapChainImageContext->commandPool, 1, &(swapChainImageContext->commandBufferTransfer));
        vkDestroyCommandPool(context->device,swapChainImageContext->commandPool, context->allocator);
        for(u$ d = 0; d < arrlenu(swapChainImageContext->stbDaDescriptorPools); d++)
        {
            vkResetDescriptorPool(context->device, swapChainImageContext->stbDaDescriptorPools[d].vkDescriptorPool, /*flags; reserved*/0);
            vkDestroyDescriptorPool(context->device, swapChainImageContext->stbDaDescriptorPools[d].vkDescriptorPool, context->allocator);
        }
        mcarrfree(KORL_C_CAST(void*, context->allocatorHandle), swapChainImageContext->stbDaDescriptorPools);
    }
    korl_memory_zero(surfaceContext->swapChainImageContexts, sizeof(surfaceContext->swapChainImageContexts));
    vkDestroySwapchainKHR(context->device, surfaceContext->swapChain, context->allocator);
}
#if 0///@TODO: delete/recycle
/**
 * move all the vertices in the batch buffer to the vulkan device
 */
korl_internal void _korl_vulkan_flushBatchStaging(void)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* don't do any work if there's no work to do! */
    if(    surfaceContext->batchState.vertexCountStaging      <= 0
        && surfaceContext->batchState.vertexIndexCountStaging <= 0)
        return;
    /* make sure that we don't overflow the device memory! */
    ///@TODO: instead of having a hard-cap of one device-local buffer per swap chain image, we should allow unlimited buffers (maybe with a linked list or something), and continuously allocate more buffers as needed
    korl_assert(
          surfaceContext->batchState.vertexCountStaging 
        + surfaceContext->batchState.vertexCountDevice 
            <= _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE);
    korl_assert(
          surfaceContext->batchState.vertexIndexCountStaging
        + surfaceContext->batchState.vertexIndexCountDevice
            <= _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_DEVICE);
    /* KORL-PERFORMANCE-000-000-023: likely suboptimal use of Vulkan synchronization primitives;
        This section of code here is a hack, or at least seems very hacky.  We 
        shouldn't have to be forced to destroy and re-create synchronization 
        primitives, especially considering that this operation experimentally 
        costs around 20 microseconds.  Unfortunately, we cannot use 
        vkSignalSemaphore for VK_SEMAPHORE_TYPE_BINARY semaphores, so there 
        doesn't really seem like there is a way to re-use these semaphores.  
        We need the ability to re-use semaphores because we will likely need to 
        re-submit the same staging buffers multiple times per frame.  Is there a 
        way for us to wait on our own semaphore that we are going to signal in 
        the same call to vkQueueSubmit below?  If that is possible, or if there 
        is some other synchronization primitive we can use instead of 
        VK_SEMAPHORE_TYPE_BINARY, then this code is no longer necessary. */
    if(VK_NULL_HANDLE == swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex])
    {
        korl_time_probeStart(re_create_semaphore);
        vkDestroySemaphore(context->device, swapChainImageContext->semaphoreStagingBuffers[swapChainImageContext->stagingBufferIndex], context->allocator);
        KORL_ZERO_STACK(VkSemaphoreCreateInfo, createInfoSemaphore);
        createInfoSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        _KORL_VULKAN_CHECK(
            vkCreateSemaphore(context->device, &createInfoSemaphore, context->allocator, 
                              &swapChainImageContext->semaphoreStagingBuffers[swapChainImageContext->stagingBufferIndex]));
        korl_time_probeStop(re_create_semaphore);
    }
    /* record commands to transfer the data from staging to device-local memory */
    korl_time_probeStart(record_transfer_commands);
    KORL_ZERO_STACK(VkCommandBufferAllocateInfo, commandBufferAllocateInfo);
    commandBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool        = context->commandPoolTransfer;
    commandBufferAllocateInfo.commandBufferCount = 1;
    _KORL_VULKAN_CHECK(
        vkAllocateCommandBuffers(context->device, &commandBufferAllocateInfo, 
                                 &swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex]));
    KORL_ZERO_STACK(VkCommandBufferBeginInfo, commandBufferBeginInfo);
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    _KORL_VULKAN_CHECK(
        vkBeginCommandBuffer(swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex], &commandBufferBeginInfo));
    if(surfaceContext->batchState.vertexIndexCountStaging > 0)
    {
        KORL_ZERO_STACK(VkBufferCopy, bufferCopyIndices);
        bufferCopyIndices.srcOffset = 0;
        bufferCopyIndices.dstOffset = surfaceContext->batchState.vertexIndexCountDevice *sizeof(Korl_Vulkan_VertexIndex);
        bufferCopyIndices.size      = surfaceContext->batchState.vertexIndexCountStaging*sizeof(Korl_Vulkan_VertexIndex);
        vkCmdCopyBuffer(
            swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex], 
            swapChainImageContext->bufferStagingBatchIndices[swapChainImageContext->stagingBufferIndex]->deviceObject.buffer, 
            swapChainImageContext->bufferDeviceBatchIndices                                            ->deviceObject.buffer, 
            1, &bufferCopyIndices);
    }
    if(surfaceContext->batchState.vertexCountStaging > 0)
    {
        KORL_ZERO_STACK(VkBufferCopy, bufferCopyPositions);
        bufferCopyPositions.srcOffset = 0;
        bufferCopyPositions.dstOffset = surfaceContext->batchState.vertexCountDevice *sizeof(Korl_Vulkan_Position);
        bufferCopyPositions.size      = surfaceContext->batchState.vertexCountStaging*sizeof(Korl_Vulkan_Position);
        vkCmdCopyBuffer(
            swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex], 
            swapChainImageContext->bufferStagingBatchPositions[swapChainImageContext->stagingBufferIndex]->deviceObject.buffer, 
            swapChainImageContext->bufferDeviceBatchPositions                                            ->deviceObject.buffer, 
            1, &bufferCopyPositions);
        KORL_ZERO_STACK(VkBufferCopy, bufferCopyColors);
        bufferCopyColors.srcOffset = 0;
        bufferCopyColors.dstOffset = surfaceContext->batchState.vertexCountDevice *sizeof(Korl_Vulkan_Color4u8);
        bufferCopyColors.size      = surfaceContext->batchState.vertexCountStaging*sizeof(Korl_Vulkan_Color4u8);
        vkCmdCopyBuffer(
            swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex], 
            swapChainImageContext->bufferStagingBatchColors[swapChainImageContext->stagingBufferIndex]->deviceObject.buffer, 
            swapChainImageContext->bufferDeviceBatchColors                                            ->deviceObject.buffer, 
            1, &bufferCopyColors);
        KORL_ZERO_STACK(VkBufferCopy, bufferCopyUvs);
        bufferCopyUvs.srcOffset = 0;
        bufferCopyUvs.dstOffset = surfaceContext->batchState.vertexCountDevice *sizeof(Korl_Vulkan_Uv);
        bufferCopyUvs.size      = surfaceContext->batchState.vertexCountStaging*sizeof(Korl_Vulkan_Uv);
        vkCmdCopyBuffer(
            swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex], 
            swapChainImageContext->bufferStagingBatchUvs[swapChainImageContext->stagingBufferIndex]->deviceObject.buffer, 
            swapChainImageContext->bufferDeviceBatchUvs                                            ->deviceObject.buffer, 
            1, &bufferCopyUvs);
    }
    _KORL_VULKAN_CHECK(vkEndCommandBuffer(swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex]));
    korl_time_probeStop(record_transfer_commands);
    /* submit the memory transfer commands to the device */
    korl_time_probeStart(queue_submit);
    _KORL_VULKAN_CHECK(vkResetFences(context->device, 1, 
                                     &swapChainImageContext->fenceStagingBuffers[swapChainImageContext->stagingBufferIndex]));
    KORL_ZERO_STACK(VkSubmitInfo, queueSubmitInfo);
    queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    queueSubmitInfo.commandBufferCount   = 1;
    queueSubmitInfo.pCommandBuffers      = &swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex];
    queueSubmitInfo.signalSemaphoreCount = 1;
    queueSubmitInfo.pSignalSemaphores    = &swapChainImageContext->semaphoreStagingBuffers[swapChainImageContext->stagingBufferIndex];
    _KORL_VULKAN_CHECK(vkQueueSubmit(context->queueGraphics, 1, &queueSubmitInfo, 
                                     swapChainImageContext->fenceStagingBuffers[swapChainImageContext->stagingBufferIndex]));
    korl_time_probeStop(queue_submit);
    /* finish up; update our book-keeping */
    surfaceContext->batchState.vertexIndexCountDevice += surfaceContext->batchState.vertexIndexCountStaging;
    surfaceContext->batchState.vertexCountDevice      += surfaceContext->batchState.vertexCountStaging;
    surfaceContext->batchState.vertexIndexCountStaging = 0;
    surfaceContext->batchState.vertexCountStaging      = 0;
    swapChainImageContext->stagingBufferIndex = (swapChainImageContext->stagingBufferIndex + 1) % _KORL_VULKAN_SWAPCHAIN_IMAGE_CONTEXT_STAGING_BUFFER_COUNT;
    /* After this function completes, the caller is expecting to be able to 
        write to memory in the staging buffer _SAFELY_.  Because of this, we 
        must wait on the fence of the next staging buffer to be absolutely sure 
        that we aren't about to modify data that is being read from: */
    korl_time_probeStart(wait_for_new_buffer_fence);
    _KORL_VULKAN_CHECK(vkWaitForFences(context->device, 1, 
                                       &swapChainImageContext->fenceStagingBuffers[swapChainImageContext->stagingBufferIndex], 
                                       VK_TRUE, UINT64_MAX));//KORL-PERFORMANCE-000-000-021: this is (hopefully) better than what we were doing before, but optimally we would have no wait here at all
    korl_time_probeStop(wait_for_new_buffer_fence);
    /* Finally we can release the previous command buffer back to the pool, 
        because at this point we know for sure that the previous queue 
        submission has completed.  Note that these stored command buffer handles 
        should be initialized to VK_NULL_HANDLE, and it is _okay_ to pass this 
        NULL value to vkFreeCommandBuffers 
        (vk spec VUID-vkFreeCommandBuffers-pCommandBuffers-00048). */
    vkFreeCommandBuffers(context->device, context->commandPoolTransfer, 1, 
                         &swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex]);
    swapChainImageContext->commandBufferStagingBuffers[swapChainImageContext->stagingBufferIndex] = VK_NULL_HANDLE;
}
#endif
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
    pipeline.primitiveTopology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline.positionDimensions       = 3;
    pipeline.positionsStride          = pipeline.positionDimensions*sizeof(f32);
    pipeline.colorsStride             = 4*sizeof(u8);// default to RGBA; @TODO: should this be more well-defined somewhere, such as a Korl_Vulkan_ColorChannel datatype or something?
    pipeline.uvsStride                = 0;
    pipeline.features.enableBlend     = true;
    pipeline.features.enableDepthTest = true;
    pipeline.blend                    = (Korl_Vulkan_DrawState_Blend)
                                        {.opColor = VK_BLEND_OP_ADD
                                        ,.factorColorSource = VK_BLEND_FACTOR_SRC_ALPHA
                                        ,.factorColorTarget = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
                                        ,.opAlpha = VK_BLEND_OP_ADD
                                        ,.factorAlphaSource = VK_BLEND_FACTOR_ONE
                                        ,.factorAlphaTarget = VK_BLEND_FACTOR_ZERO};
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
    KORL_ZERO_STACK_ARRAY(VkVertexInputBindingDescription, vertexInputBindings, 3);
    uint32_t vertexBindingDescriptionCount = 1;
    vertexInputBindings[0].binding   = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_POSITION;
    vertexInputBindings[0].stride    = pipeline->positionsStride;
    vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    if(pipeline->colorsStride)
    {
        vertexInputBindings[vertexBindingDescriptionCount].binding   = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_COLOR;
        vertexInputBindings[vertexBindingDescriptionCount].stride    = pipeline->colorsStride;
        vertexInputBindings[vertexBindingDescriptionCount].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingDescriptionCount++;
    }
    if(pipeline->uvsStride)
    {
        vertexInputBindings[vertexBindingDescriptionCount].binding   = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_UV;
        vertexInputBindings[vertexBindingDescriptionCount].stride    = pipeline->uvsStride;
        vertexInputBindings[vertexBindingDescriptionCount].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingDescriptionCount++;
    }
    KORL_ZERO_STACK_ARRAY(VkVertexInputAttributeDescription, vertexAttributes, 3);
    uint32_t vertexAttributeDescriptionCount = 1;
    vertexAttributes[0].binding  = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_POSITION;
    vertexAttributes[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].location = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_POSITION;
    vertexAttributes[0].offset   = 0;// we're not using interleaved vertex data
    if(pipeline->colorsStride)
    {
        vertexAttributes[vertexAttributeDescriptionCount].binding  = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_COLOR;
        vertexAttributes[vertexAttributeDescriptionCount].format   = VK_FORMAT_R8G8B8A8_UNORM;
        vertexAttributes[vertexAttributeDescriptionCount].location = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_COLOR;
        vertexAttributes[vertexAttributeDescriptionCount].offset   = 0;// we're not using interleaved vertex data
        vertexAttributeDescriptionCount++;
    }
    if(pipeline->uvsStride)
    {
        vertexAttributes[vertexAttributeDescriptionCount].binding  = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_UV;
        vertexAttributes[vertexAttributeDescriptionCount].format   = VK_FORMAT_R32G32_SFLOAT;
        vertexAttributes[vertexAttributeDescriptionCount].location = _KORL_VULKAN_BATCH_VERTEXATTRIBUTE_BINDING_UV;
        vertexAttributes[vertexAttributeDescriptionCount].offset   = 0;// we're not using interleaved vertex data
        vertexAttributeDescriptionCount++;
    }
    KORL_ZERO_STACK(VkPipelineVertexInputStateCreateInfo, createInfoVertexInput);
    createInfoVertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfoVertexInput.pVertexBindingDescriptions      = vertexInputBindings;
    createInfoVertexInput.vertexBindingDescriptionCount   = vertexBindingDescriptionCount;
    createInfoVertexInput.pVertexAttributeDescriptions    = vertexAttributes;
    createInfoVertexInput.vertexAttributeDescriptionCount = vertexAttributeDescriptionCount;
    KORL_ZERO_STACK(VkPipelineInputAssemblyStateCreateInfo, createInfoInputAssembly);
    createInfoInputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    createInfoInputAssembly.topology = pipeline->primitiveTopology;
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
    createInfoRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    createInfoRasterizer.lineWidth   = 1.f;
    createInfoRasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;//default: VK_CULL_MODE_NONE
    //createInfoRasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;//right-handed triangles! (default)
    KORL_ZERO_STACK(VkPipelineMultisampleStateCreateInfo, createInfoMultisample);
    createInfoMultisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    createInfoMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    createInfoMultisample.minSampleShading     = 1.f;
    // we have only one framebuffer, so we only need one attachment:
    // enable alpha blending
    KORL_ZERO_STACK(VkPipelineColorBlendAttachmentState, colorBlendAttachment);
    colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = 0 != pipeline->features.enableBlend;
    colorBlendAttachment.srcColorBlendFactor = pipeline->blend.factorColorSource;
    colorBlendAttachment.dstColorBlendFactor = pipeline->blend.factorColorTarget;
    colorBlendAttachment.colorBlendOp        = pipeline->blend.opColor;
    colorBlendAttachment.srcAlphaBlendFactor = pipeline->blend.factorAlphaSource;
    colorBlendAttachment.dstAlphaBlendFactor = pipeline->blend.factorAlphaTarget;
    colorBlendAttachment.alphaBlendOp        = pipeline->blend.opAlpha;
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
    KORL_ZERO_STACK_ARRAY(
        VkPipelineShaderStageCreateInfo, createInfoShaderStages, 2);
    createInfoShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfoShaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    if(pipeline->positionDimensions == 2)
        createInfoShaderStages[0].module = context->shaderVertex2d;
    else if(pipeline->positionDimensions == 3)
        if(   !pipeline->colorsStride
           &&  pipeline->uvsStride)
            createInfoShaderStages[0].module = context->shaderVertex3dUv;
        else if(    pipeline->colorsStride
                && !pipeline->uvsStride)
            createInfoShaderStages[0].module = context->shaderVertex3dColor;
        else
            createInfoShaderStages[0].module = context->shaderVertex3d;
#if 0///@TODO: recycle/delete
    if(    pipeline->colorsStride
       && !pipeline->uvsStride)
        createInfoShaderStages[0].module = context->shaderBatchVertColor;
    else if(    pipeline->uvsStride
            && !pipeline->colorsStride)
        createInfoShaderStages[0].module = context->shaderBatchVertTexture;
    else if(   pipeline->uvsStride
            && pipeline->colorsStride)
        createInfoShaderStages[0].module = context->shaderBatchVertColorTexture;
#endif
    createInfoShaderStages[0].pName  = "main";
    createInfoShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfoShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    if(pipeline->uvsStride)
        createInfoShaderStages[1].module = context->shaderFragmentColorTexture;
    else
        createInfoShaderStages[1].module = context->shaderFragmentColor;
#if 0///@TODO: recycle/delete
    if(     pipeline->colorsStride
        && !pipeline->uvsStride)
        createInfoShaderStages[1].module = context->shaderBatchFragColor;
    else if(    pipeline->uvsStride
            && !pipeline->colorsStride)
        createInfoShaderStages[1].module = context->shaderBatchFragTexture;
    else if(   pipeline->colorsStride
            && pipeline->uvsStride)
        createInfoShaderStages[1].module = context->shaderBatchFragColorTexture;
#endif
    createInfoShaderStages[1].pName  = "main";
    KORL_ZERO_STACK(VkPipelineDepthStencilStateCreateInfo, createInfoDepthStencil);
    createInfoDepthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    createInfoDepthStencil.depthTestEnable  = 0 != pipeline->features.enableDepthTest ? VK_TRUE : VK_FALSE;
    createInfoDepthStencil.depthWriteEnable = 0 != pipeline->features.enableDepthTest ? VK_TRUE : VK_FALSE;
    createInfoDepthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;
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
        vkCreateGraphicsPipelines(
            context->device, VK_NULL_HANDLE/*pipeline cache*/, 
            1, &createInfoPipeline, context->allocator, &pipeline->pipeline));
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
        mcarrpush(KORL_C_CAST(void*, context->allocatorHandle), context->stbDaPipelines, pipeline);
        arrlast(context->stbDaPipelines).pipeline = VK_NULL_HANDLE;
        _korl_vulkan_createPipeline(arrlenu(context->stbDaPipelines) - 1);
    }
    return pipelineIndex;
}
korl_internal VkIndexType _korl_vulkan_vertexIndexType(void)
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
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
#if 0///@TODO: delete/recycle
    bool pipelineAboutToChange = false;
#endif
    u$ newPipelineIndex = arrlenu(context->stbDaPipelines);
    if(   surfaceContext->batchState.currentPipeline < arrlenu(context->stbDaPipelines)
       && _korl_vulkan_pipeline_isMetaDataSame(pipeline, context->stbDaPipelines[surfaceContext->batchState.currentPipeline]))
    {
        // do nothing; just leave the current pipeline selected
        newPipelineIndex = surfaceContext->batchState.currentPipeline;
    }
    else
    {
        /* search for a pipeline that can handle the data we're trying to batch */
        u$ pipelineIndex = _korl_vulkan_addPipeline(pipeline);
        korl_assert(pipelineIndex < arrlenu(context->stbDaPipelines));
#if 0///@TODO: delete/recycle
        pipelineAboutToChange = (pipelineIndex != surfaceContext->batchState.currentPipeline);
#endif
        newPipelineIndex      = pipelineIndex;
    }
#if 0///@TODO: delete/recycle
    /* flush the pipeline batch if we're about to change pipelines */
    if(pipelineAboutToChange && surfaceContext->batchState.pipelineVertexCount > 0)
        _korl_vulkan_flushBatchPipeline();
#endif
    /* then, actually change to a new pipeline for the next batch */
    korl_assert(newPipelineIndex < arrlenu(context->stbDaPipelines));//sanity check!
    surfaceContext->batchState.currentPipeline = newPipelineIndex;
}
#if 0///@TODO: delete/recycle
/**
 * Calling this ensures that the current descriptor set index of the surface 
 * context batch state is pointing to unused batch descriptor sets, flushing the 
 * batch pipeline if necessary.
 */
korl_internal void _korl_vulkan_batchDescriptorSetFlush(void)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    if(!surfaceContext->batchState.descriptorSetIsUsed)
        return;
    korl_time_probeStart(batch_pipeline_flush); _korl_vulkan_flushBatchPipeline(); korl_time_probeStop(batch_pipeline_flush);
    /* copy the state of the non-UBO descriptors of the current batch 
        descriptor set to the next one */
    korl_time_probeStart(copy_nonUBO_descriptors);
    korl_assert(surfaceContext->batchState.descriptorSetIndexCurrent < _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS);
    KORL_ZERO_STACK_ARRAY(VkCopyDescriptorSet, copyDescriptorSets, 1);
    copyDescriptorSets[0].sType           = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    copyDescriptorSets[0].srcSet          = surfaceContext->batchDescriptorSets[surfaceContext->frameSwapChainImageIndex][surfaceContext->batchState.descriptorSetIndexCurrent];
    copyDescriptorSets[0].srcBinding      = _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_TEXTURE;
    copyDescriptorSets[0].dstSet          = surfaceContext->batchDescriptorSets[surfaceContext->frameSwapChainImageIndex][surfaceContext->batchState.descriptorSetIndexCurrent + 1];
    copyDescriptorSets[0].dstBinding      = _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_TEXTURE;
    copyDescriptorSets[0].descriptorCount = 1;
    vkUpdateDescriptorSets(context->device, 0, NULL, korl_arraySize(copyDescriptorSets), copyDescriptorSets);
    /* calculate the stride of each batch descriptor set UBO within the buffer */
    KORL_ZERO_STACK(VkPhysicalDeviceProperties, physicalDeviceProperties);
    vkGetPhysicalDeviceProperties(context->physicalDevice, &physicalDeviceProperties);
    const VkDeviceSize batchUboStride = korl_math_roundUpPowerOf2(sizeof(_Korl_Vulkan_SwapChainImageBatchUbo), physicalDeviceProperties.limits.minUniformBufferOffsetAlignment);
    korl_time_probeStop(copy_nonUBO_descriptors);
    /* copy the previous UBO to the buffer slot occupied by the next one */
    korl_time_probeStart(copy_previous_UBO);
    KORL_ZERO_STACK(void*, mappedDeviceMemory);
    _KORL_VULKAN_CHECK(
        vkMapMemory(
            context->device, surfaceContext->deviceMemoryHostVisible.deviceMemory, 
            swapChainImageContext->bufferStagingUbo->byteOffset + surfaceContext->batchState.descriptorSetIndexCurrent*batchUboStride, 
            /*bytes*/2*batchUboStride, 0/*flags*/, &mappedDeviceMemory));
    _Korl_Vulkan_SwapChainImageBatchUbo*const ubo     = KORL_C_CAST(_Korl_Vulkan_SwapChainImageBatchUbo*, mappedDeviceMemory);
    _Korl_Vulkan_SwapChainImageBatchUbo*const uboNext = KORL_C_CAST(_Korl_Vulkan_SwapChainImageBatchUbo*, KORL_C_CAST(u8*, mappedDeviceMemory) + batchUboStride);
    *uboNext = *ubo;
    vkUnmapMemory(context->device, surfaceContext->deviceMemoryHostVisible.deviceMemory);
    korl_time_probeStop(copy_previous_UBO);
    /* advance to the copied batch descriptor set */
    surfaceContext->batchState.descriptorSetIndexCurrent++;
    surfaceContext->batchState.descriptorSetIsUsed = false;
}
#endif
#if 0///@TODO: delete/recycle
korl_internal void _korl_vulkan_transferImageBufferToTexture(
    void* sourceImageR8G8B8A8, uint32_t sourceImageSizeX, uint32_t sourceImageSizeY, 
    _Korl_Vulkan_DeviceMemory_Alloctation* deviceImage)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    const u$ imageBytes = sizeof(Korl_Vulkan_Color4u8) * sourceImageSizeX * sourceImageSizeY;
    /* create a staging object to store the pixel data */
    _Korl_Vulkan_DeviceMemory_Alloctation* stagingPixelBuffer = 
        _korl_vulkan_deviceMemoryLinear_allocateBuffer(
            &context->deviceMemoryLinearAssetsStaging, imageBytes, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
    /* copy the pixel data to the staging object */
    KORL_ZERO_STACK(void*, mappedStagingMemory);
    _KORL_VULKAN_CHECK(
        vkMapMemory(
            context->device, context->deviceMemoryLinearAssetsStaging.deviceMemory, 
            stagingPixelBuffer->byteOffset, imageBytes, 
            0/*flags*/, &mappedStagingMemory));
    korl_memory_copy(mappedStagingMemory, sourceImageR8G8B8A8, imageBytes);
    vkUnmapMemory(context->device, context->deviceMemoryLinearAssetsStaging.deviceMemory);
    //KORL-PERFORMANCE-000-000-014: bandwidth: batch these buffer transfers
    /* transfer the staging memory to the device-local texture memory */
    KORL_ZERO_STACK(VkCommandBufferAllocateInfo, commandBufferAllocateInfo);
    commandBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool        = context->commandPoolTransfer;
    commandBufferAllocateInfo.commandBufferCount = 1;
    KORL_ZERO_STACK(VkCommandBuffer, commandBuffer);
    _KORL_VULKAN_CHECK(vkAllocateCommandBuffers(context->device, &commandBufferAllocateInfo, &commandBuffer));
    KORL_ZERO_STACK(VkCommandBufferBeginInfo, commandBufferBeginInfo);
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    _KORL_VULKAN_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
    // transition the image layout from undefined => transfer_destination_optimal //
    KORL_ZERO_STACK(VkImageMemoryBarrier, barrierImageMemory);
    barrierImageMemory.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierImageMemory.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierImageMemory.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierImageMemory.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrierImageMemory.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrierImageMemory.image                           = deviceImage->deviceObject.texture.image;
    barrierImageMemory.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierImageMemory.subresourceRange.levelCount     = 1;
    barrierImageMemory.subresourceRange.layerCount     = 1;
    barrierImageMemory.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    //barrierImageMemory.subresourceRange.baseMipLevel   = 0;
    //barrierImageMemory.subresourceRange.baseArrayLayer = 0;
    //barrierImageMemory.srcAccessMask                   = 0;
    vkCmdPipelineBarrier(
        commandBuffer, /*srcStageMask*/VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
        /*dstStageMask*/VK_PIPELINE_STAGE_TRANSFER_BIT, /*dependencyFlags*/0, 
        /*memoryBarrierCount*/0, /*pMemoryBarriers*/NULL, 
        /*bufferBarrierCount*/0, /*bufferBarriers*/NULL, 
        /*imageBarrierCount*/1, &barrierImageMemory);
    // copy the buffer from staging buffer => device-local image //
    KORL_ZERO_STACK(VkBufferImageCopy, imageCopyRegion);
    // default zero values indicate no buffer padding, copy to image origin, copy to base mip level & array layer
    imageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.imageSubresource.layerCount = 1;
    imageCopyRegion.imageExtent.width           = sourceImageSizeX;
    imageCopyRegion.imageExtent.height          = sourceImageSizeY;
    imageCopyRegion.imageExtent.depth           = 1;
    vkCmdCopyBufferToImage(commandBuffer, stagingPixelBuffer->deviceObject.buffer, deviceImage->deviceObject.texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /*regionCount*/1, &imageCopyRegion);
    // transition the image layout from tranfer_destination_optimal => shader_read_only_optimal //
    // we can recycle the VkImageMemoryBarrier from the first transition here:  
    barrierImageMemory.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierImageMemory.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierImageMemory.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierImageMemory.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
        commandBuffer, /*srcStageMask*/VK_PIPELINE_STAGE_TRANSFER_BIT, 
        /*dstStageMask*/VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, /*dependencyFlags*/0, 
        /*memoryBarrierCount*/0, /*pMemoryBarriers*/NULL, 
        /*bufferBarrierCount*/0, /*bufferBarriers*/NULL, 
        /*imageBarrierCount*/1, &barrierImageMemory);
    /* end & submit the memory transfer commands to the device */
    _KORL_VULKAN_CHECK(vkEndCommandBuffer(commandBuffer));
    KORL_ZERO_STACK(VkSubmitInfo, queueSubmitInfo);
    queueSubmitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    queueSubmitInfo.commandBufferCount = 1;
    queueSubmitInfo.pCommandBuffers    = &commandBuffer;
    _KORL_VULKAN_CHECK(vkQueueSubmit(context->queueGraphics, 1, &queueSubmitInfo, VK_NULL_HANDLE/*fence*/));
    //KORL-PERFORMANCE-000-000-012: bandwidth: heavy-handed locking mechanism
    _KORL_VULKAN_CHECK(vkQueueWaitIdle(context->queueGraphics));
    vkFreeCommandBuffers(context->device, context->commandPoolTransfer, 1, &commandBuffer);
    /* now that the staging assets have been moved to the device-local memory, 
        we can completely clear out the staging asset allocator */
    _korl_vulkan_deviceMemoryLinear_clear(&context->deviceMemoryLinearAssetsStaging);
}
#endif
korl_internal VkBlendOp _korl_vulkan_blendOperation_to_vulkan(Korl_Vulkan_BlendOperation blendOp)
{
    switch(blendOp)
    {
    case(KORL_BLEND_OP_ADD):              return VK_BLEND_OP_ADD;
    case(KORL_BLEND_OP_SUBTRACT):         return VK_BLEND_OP_SUBTRACT;
    case(KORL_BLEND_OP_REVERSE_SUBTRACT): return VK_BLEND_OP_REVERSE_SUBTRACT;
    case(KORL_BLEND_OP_MIN):              return VK_BLEND_OP_MIN;
    case(KORL_BLEND_OP_MAX):              return VK_BLEND_OP_MAX;
    }
    korl_log(ERROR, "Unsupported blend operation: %d", blendOp);
    return 0;
}
korl_internal VkBlendFactor _korl_vulkan_blendFactor_to_vulkan(Korl_Vulkan_BlendFactor blendFactor)
{
    switch(blendFactor)
    {
    case(KORL_BLEND_FACTOR_ZERO):                     return VK_BLEND_FACTOR_ZERO;
    case(KORL_BLEND_FACTOR_ONE):                      return VK_BLEND_FACTOR_ONE;
    case(KORL_BLEND_FACTOR_SRC_COLOR):                return VK_BLEND_FACTOR_SRC_COLOR;
    case(KORL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR):      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case(KORL_BLEND_FACTOR_DST_COLOR):                return VK_BLEND_FACTOR_DST_COLOR;
    case(KORL_BLEND_FACTOR_ONE_MINUS_DST_COLOR):      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case(KORL_BLEND_FACTOR_SRC_ALPHA):                return VK_BLEND_FACTOR_SRC_ALPHA;
    case(KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA):      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case(KORL_BLEND_FACTOR_DST_ALPHA):                return VK_BLEND_FACTOR_DST_ALPHA;
    case(KORL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA):      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case(KORL_BLEND_FACTOR_CONSTANT_COLOR):           return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case(KORL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR): return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case(KORL_BLEND_FACTOR_CONSTANT_ALPHA):           return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case(KORL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA): return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case(KORL_BLEND_FACTOR_SRC_ALPHA_SATURATE):       return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    }
    korl_log(ERROR, "Unsupported blend factor: %d", blendFactor);
    return 0;
}
#if 0///@TODO: delete/recycle
korl_internal void _korl_vulkan_selectTexture(VkImageView imageView, VkSampler sampler)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* if the descriptor set state we are attempting to set is identical to the 
        last known state, then we don't need to do anything */
    if(    surfaceContext->batchState.textureImageView == imageView
        && surfaceContext->batchState.textureSampler   == sampler)
        return;
    surfaceContext->batchState.textureImageView = imageView;
    surfaceContext->batchState.textureSampler   = sampler;
    /* we're about to modify the batch descriptor set state, so let's make sure 
        the batch pipeline doesn't have any pending geometry for the current 
        descriptor set index */
    korl_time_probeStart(batch_descriptor_set_flush);
    _korl_vulkan_batchDescriptorSetFlush();
    korl_time_probeStop(batch_descriptor_set_flush);
    /* select the loaded texture device asset for any future textured draw operations */
    KORL_ZERO_STACK(VkDescriptorImageInfo, descriptorImageInfo);
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorImageInfo.imageView   = imageView;
    descriptorImageInfo.sampler     = sampler;
    KORL_ZERO_STACK(VkWriteDescriptorSet, writeDescriptorSetUbo);
    writeDescriptorSetUbo.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSetUbo.dstSet          = surfaceContext->batchDescriptorSets[surfaceContext->frameSwapChainImageIndex][surfaceContext->batchState.descriptorSetIndexCurrent];
    writeDescriptorSetUbo.dstBinding      = _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_TEXTURE;
    writeDescriptorSetUbo.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSetUbo.descriptorCount = 1;
    writeDescriptorSetUbo.pImageInfo      = &descriptorImageInfo;
    vkUpdateDescriptorSets(context->device, 1, &writeDescriptorSetUbo, 0, NULL);
}
#endif
korl_internal void* _korl_vulkan_getStagingPool(VkDeviceSize bytesRequired, VkDeviceSize alignmentRequired, VkBuffer* out_bufferStaging, VkDeviceSize* out_byteOffsetStagingBuffer)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* attempt to find a staging buffer that can hold the vertexData */
    _Korl_Vulkan_Buffer*                   validBuffer     = NULL;
    _Korl_Vulkan_DeviceMemory_Alloctation* validAllocation = NULL;
    for(u$ b = 0; b < arrlenu(surfaceContext->stbDaStagingBuffers); b++)
    {
        /// @TODO: remember what buffer index we used the last call to this function; 
        ///        currently what will happen is that we will prefer to reset the earlier buffers more often, 
        ///        which will cause all buffers to slowly accumulate data, 
        ///        which I can easily imagine putting us in the sad position down the line where we will occasionally get a frame "hitch" or something caused by all our buffers resetting or something; 
        ///        filling our staging pools in a round-robin order seems much more robust against these types of performance hitches, and regardless just seems a lot more cleaner to me...
        _Korl_Vulkan_Buffer*const                   buffer     = &(surfaceContext->stbDaStagingBuffers[b]);
        _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryHostVisible, buffer->allocation);
        /* if the buffer has enough room, we can just use it */
        const VkDeviceSize alignedBytesUsed = alignmentRequired 
            ? korl_math_roundUpPowerOf2(buffer->bytesUsed, alignmentRequired) 
            : buffer->bytesUsed;
        const VkDeviceSize bufferRemainingBytes = alignedBytesUsed >= allocation->byteSize 
            ? 0 
            : allocation->byteSize - alignedBytesUsed;
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
        korl_log(VERBOSE, "resetting staging buffer [%llu]", b);
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
        korl_log(VERBOSE, "allocating new staging buffer; arrlenu(surfaceContext->stbDaStagingBuffers)==%llu"
                        , arrlenu(surfaceContext->stbDaStagingBuffers));
#endif
        _Korl_Vulkan_DeviceMemory_AllocationHandle newBufferAllocationHandle = _korl_vulkan_deviceMemory_allocateBuffer(&surfaceContext->deviceMemoryHostVisible
                                                                                                                       ,korl_math_megabytes(1)
                                                                                                                       ,  VK_BUFFER_USAGE_TRANSFER_SRC_BIT 
                                                                                                                        | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                                                                                                        | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                                                                                        | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                                                                                       ,VK_SHARING_MODE_EXCLUSIVE);
        KORL_ZERO_STACK(_Korl_Vulkan_Buffer, newBuffer);
        newBuffer.allocation = newBufferAllocationHandle;
        mcarrpush(KORL_C_CAST(void*, context->allocatorHandle), surfaceContext->stbDaStagingBuffers, newBuffer);
        validBuffer     = &arrlast(surfaceContext->stbDaStagingBuffers);
        validAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(&surfaceContext->deviceMemoryHostVisible, newBufferAllocationHandle);
        ///@ASSUMPTION: a reset buffer offset will always satisfy \c alignmentRequired
    }
    /* at this point, we should have a valid buffer, and if not we are 
        essentially "out of memory" */
    korl_assert(validBuffer && validAllocation);
    korl_assert(validAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER);
    *out_byteOffsetStagingBuffer = validBuffer->bytesUsed;
    *out_bufferStaging           = validAllocation->deviceObject.buffer.vulkanBuffer;
    void*const bufferMappedAddress = _korl_vulkan_deviceMemory_allocator_getBufferHostVisibleAddress(&surfaceContext->deviceMemoryHostVisible
                                                                                                    ,validBuffer->allocation, validAllocation);
    u8*const resultAddress = KORL_C_CAST(u8*, bufferMappedAddress) + validBuffer->bytesUsed;
    validBuffer->framesSinceLastUsed = 0;
    validBuffer->bytesUsed          += bytesRequired;
    return resultAddress;
}
korl_internal void* _korl_vulkan_getVertexStagingPool(const Korl_Vulkan_DrawVertexData* vertexData, VkBuffer* out_bufferStaging, VkDeviceSize* out_byteOffsetStagingBuffer)
{
    const VkDeviceSize bytesRequired = vertexData->vertexCount*vertexData->positionDimensions*sizeof(*vertexData->positions)
                                     + (vertexData->colors  ? vertexData->vertexCount*sizeof(*vertexData->colors) : 0)
                                     + (vertexData->uvs     ? vertexData->vertexCount*sizeof(*vertexData->uvs)    : 0)
                                     + (vertexData->indices ? vertexData->indexCount*sizeof(*vertexData->indices) : 0);
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
        mcarrpush(KORL_C_CAST(void*, context->allocatorHandle), swapChainImageContext->stbDaDescriptorPools, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Vulkan_DescriptorPool));
        validDescriptorPool = &(arrlast(swapChainImageContext->stbDaDescriptorPools));
        KORL_ZERO_STACK_ARRAY(VkDescriptorPoolSize, descriptorPoolSizes, 2);
        descriptorPoolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSizes[0].descriptorCount = MAX_DESCRIPTOR_SETS_PER_POOL;
        descriptorPoolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorPoolSizes[1].descriptorCount = MAX_DESCRIPTOR_SETS_PER_POOL;
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
    allocInfoDescriptorSets.pSetLayouts        = &context->batchDescriptorSetLayout;
    _KORL_VULKAN_CHECK(
        vkAllocateDescriptorSets(context->device, &allocInfoDescriptorSets
                                ,&resultDescriptorSet));
    validDescriptorPool->setsAllocated++;
    return resultDescriptorSet;
}
korl_internal _Korl_Vulkan_DeviceAssetDatabase _korl_vulkan_deviceAssetDatabase_create(Korl_Memory_AllocatorHandle allocatorHandle, _Korl_Vulkan_DeviceMemory_Allocator* deviceMemoryAllocator)
{
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceAssetDatabase, result);
    result.memoryContext         = KORL_C_CAST(void*, allocatorHandle);
    result.deviceMemoryAllocator = deviceMemoryAllocator;
    mcarrsetcap(result.memoryContext, result.stbDaAssets, 1024);
    return result;
}
korl_internal void _korl_vulkan_deviceAssetDatabase_destroy(_Korl_Vulkan_DeviceAssetDatabase* deviceAssetDatabase)
{
    for(u$ i = 0; i < arrlenu(deviceAssetDatabase->stbDaAssets); i++)
        _korl_vulkan_deviceMemory_allocator_free(deviceAssetDatabase->deviceMemoryAllocator, deviceAssetDatabase->stbDaAssets[i].deviceAllocation);
    mcarrfree(deviceAssetDatabase->memoryContext, deviceAssetDatabase->stbDaAssets);
    korl_memory_zero(deviceAssetDatabase, sizeof(*deviceAssetDatabase));
}
korl_internal void _korl_vulkan_deviceAssetDatabase_cycleAssetLifetimes(_Korl_Vulkan_DeviceAssetDatabase* deviceAssetDatabase)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    for(u$ i = 0; i < arrlenu(deviceAssetDatabase->stbDaAssets); i++)
    {
        _Korl_Vulkan_DeviceAsset*const asset = &(deviceAssetDatabase->stbDaAssets[i]);
        if(asset->deviceAllocation == 0)
            continue;
        if(asset->framesSinceLastUsed < KORL_U8_MAX)
            asset->framesSinceLastUsed++;
        if(asset->nullify && asset->framesSinceLastUsed >= surfaceContext->swapChainImagesSize)
        {
            _korl_vulkan_deviceMemory_allocator_free(deviceAssetDatabase->deviceMemoryAllocator, asset->deviceAllocation);
            asset->deviceAllocation = 0;
        }
    }
}
typedef struct _Korl_Vulkan_DeviceAssetHandle_Unpacked
{
    _Korl_Vulkan_DeviceMemory_Allocation_Type type;
    u16                                       databaseIndex;
    u8                                        salt;
} _Korl_Vulkan_DeviceAssetHandle_Unpacked;
korl_internal Korl_Vulkan_DeviceAssetHandle _korl_vulkan_deviceAssetHandle_pack(_Korl_Vulkan_DeviceAssetHandle_Unpacked unpackedHandle)
{
    return (KORL_C_CAST(Korl_Vulkan_DeviceAssetHandle, unpackedHandle.type         ) << 24)
         | (KORL_C_CAST(Korl_Vulkan_DeviceAssetHandle, unpackedHandle.salt         ) << 16)
         |  KORL_C_CAST(Korl_Vulkan_DeviceAssetHandle, unpackedHandle.databaseIndex);
}
korl_internal _Korl_Vulkan_DeviceAssetHandle_Unpacked _korl_vulkan_deviceAssetHandle_unpack(Korl_Vulkan_DeviceAssetHandle handle)
{
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceAssetHandle_Unpacked, handleUnpacked);
    handleUnpacked.type          = KORL_C_CAST(_Korl_Vulkan_DeviceMemory_Allocation_Type, handle >> 24);
    handleUnpacked.salt          = KORL_C_CAST(u8                                       , handle >> 16);
    handleUnpacked.databaseIndex = KORL_C_CAST(u16                                      , handle);
    return handleUnpacked;
}
korl_internal Korl_Vulkan_DeviceAssetHandle _korl_vulkan_deviceAssetDatabase_add(_Korl_Vulkan_DeviceAssetDatabase* deviceAssetDatabase, _Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceAssetHandle_Unpacked, handleUnpacked);
    /* find a device asset slot that is not in use */
    handleUnpacked.databaseIndex = korl_checkCast_u$_to_u16(arrlenu(deviceAssetDatabase->stbDaAssets));
    for(u$ a = 0; a < arrlenu(deviceAssetDatabase->stbDaAssets); a++)
    {
        _Korl_Vulkan_DeviceAsset*const asset = &(deviceAssetDatabase->stbDaAssets[a]);
        if(asset->deviceAllocation == 0)
        {
            handleUnpacked.databaseIndex = korl_checkCast_u$_to_u16(a);
            korl_memory_zero(asset, sizeof(*asset));
            break;
        }
    }
    /* if there is no available slot, add a new slot */
    if(handleUnpacked.databaseIndex >= arrlenu(deviceAssetDatabase->stbDaAssets))
    {
        mcarrpush(deviceAssetDatabase->memoryContext, deviceAssetDatabase->stbDaAssets, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Vulkan_DeviceAsset));
        handleUnpacked.databaseIndex = korl_checkCast_u$_to_u16(arrlenu(deviceAssetDatabase->stbDaAssets) - 1);
    }
    /* populate the new asset in the database */
    _Korl_Vulkan_DeviceAsset*const asset = &arrlast(deviceAssetDatabase->stbDaAssets);
    asset->deviceAllocation = allocationHandle;
    asset->salt             = deviceAssetDatabase->nextSalt++;
    _Korl_Vulkan_DeviceMemory_Alloctation*const assetAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(deviceAssetDatabase->deviceMemoryAllocator, asset->deviceAllocation);
    handleUnpacked.salt = asset->salt;
    handleUnpacked.type = assetAllocation->type;
    return _korl_vulkan_deviceAssetHandle_pack(handleUnpacked);
}
korl_internal _Korl_Vulkan_DeviceAsset* _korl_vulkan_deviceAssetDatabase_get(_Korl_Vulkan_DeviceAssetDatabase* deviceAssetDatabase, Korl_Vulkan_DeviceAssetHandle deviceAssetHandle, _Korl_Vulkan_DeviceMemory_Alloctation** out_deviceMemoryAllocation)
{
    _Korl_Vulkan_DeviceAssetHandle_Unpacked handleUnpacked = _korl_vulkan_deviceAssetHandle_unpack(deviceAssetHandle);
    korl_assert(handleUnpacked.databaseIndex < arrlenu(deviceAssetDatabase->stbDaAssets));
    _Korl_Vulkan_DeviceAsset*const asset = &(deviceAssetDatabase->stbDaAssets[handleUnpacked.databaseIndex]);
    korl_assert(asset->salt == handleUnpacked.salt);
    _Korl_Vulkan_DeviceMemory_Alloctation*const assetAllocation = _korl_vulkan_deviceMemory_allocator_getAllocation(deviceAssetDatabase->deviceMemoryAllocator, asset->deviceAllocation);
    korl_assert(assetAllocation->type == handleUnpacked.type);
    if(out_deviceMemoryAllocation)
        *out_deviceMemoryAllocation = assetAllocation;
    return asset;
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
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_megabytes(8), L"korl-vulkan", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL/*auto-choose start address*/);
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
        if(_korl_vulkan_isBetterDevice(context->physicalDevice, physicalDevices[d], 
                                       &queueFamilyMetaData, &deviceSurfaceMetaData, 
                                       &devicePropertiesBest, &deviceProperties, 
                                       &deviceFeaturesBest, &deviceFeatures))
        {
            context->physicalDevice = physicalDevices[d];
            devicePropertiesBest = deviceProperties;
            deviceFeaturesBest = deviceFeatures;
            context->queueFamilyMetaData = queueFamilyMetaData;
            deviceSurfaceMetaDataBest = deviceSurfaceMetaData;
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
    korl_log(INFO, "vendorID=0x%X|%u, apiVersion=0x%X|%u|%u.%u.%u.%u, driverVersion=0x%X|%u|%u.%u.%u.%u, deviceID=0x%X|%u, deviceType=%ws", 
             devicePropertiesBest.vendorID, devicePropertiesBest.vendorID, 
             devicePropertiesBest.apiVersion, devicePropertiesBest.apiVersion, 
             VK_API_VERSION_VARIANT(devicePropertiesBest.apiVersion), 
             VK_API_VERSION_MAJOR(devicePropertiesBest.apiVersion), 
             VK_API_VERSION_MINOR(devicePropertiesBest.apiVersion), 
             VK_API_VERSION_PATCH(devicePropertiesBest.apiVersion), 
             devicePropertiesBest.driverVersion, devicePropertiesBest.driverVersion, 
             deviceDriverVersionVariant, 
             deviceDriverVersionMajor, 
             deviceDriverVersionMinor, 
             deviceDriverVersionPatch, 
             devicePropertiesBest.deviceID, devicePropertiesBest.deviceID, 
             stringDeviceType);
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
            if(createInfoDeviceQueueFamilies[c].queueFamilyIndex == 
                context->queueFamilyMetaData.indexQueueUnion.indices[f])
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
        vkCreateDevice(context->physicalDevice, &createInfoDevice, 
                       context->allocator, &context->device));
    /* obtain opaque handles to the queues that we need */
    vkGetDeviceQueue(context->device, 
                     context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics, 
                     0/*queueIndex*/, &context->queueGraphics);
    vkGetDeviceQueue(context->device, 
                     context->queueFamilyMetaData.indexQueueUnion.indexQueues.present, 
                     0/*queueIndex*/, &context->queuePresent);
#if 0///@TODO: delete/recycle
    /* create graphics command pool */
    KORL_ZERO_STACK(VkCommandPoolCreateInfo, createInfoCommandPool);
    createInfoCommandPool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfoCommandPool.queueFamilyIndex = context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics;
    createInfoCommandPool.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT/*we want to be able to reset the same primary command buffers in this pool once at the beginning of each frame*/;
    _KORL_VULKAN_CHECK(
        vkCreateCommandPool(
            context->device, &createInfoCommandPool, context->allocator, 
            &context->commandPoolGraphics));
    /* create memory transfer command pool */
    /* We DO, in fact, need to have transient command buffers created because 
        memory has to move from staging to device memory potentially multiple 
        times in the same frame.  Then, the staging buffer memory potentially 
        will be reused to batch more vertices, which are subsequently flushed 
        again to device memory.  This is logical speculation based on the fact 
        that it is very likely for us to be working with a SMALL staging buffer, 
        and a LARGE device buffer! */
    createInfoCommandPool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT/*give a hint to the implementation that the command buffers allocated by this command pool will be short-lived*/;
    _KORL_VULKAN_CHECK(
        vkCreateCommandPool(
            context->device, &createInfoCommandPool, context->allocator, 
            &context->commandPoolTransfer));
#endif
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
                                                                                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT///@TODO: we could potentially get more performance here by removing HOST_COHERENT & manually calling vkFlushMappedMemoryRanges & vkInvalidateMappedMemoryRanges on bulk memory ranges; timings necessary
                                                                                        ,  VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                                                                         | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT///@TODO: potentially better device performance by removing these *_BUFFER bits, then adding a device-local vertex data pool to which we transfer all vertex data to
                                                                                         | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                                                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                                                        ,/*image usage flags*/0
                                                                                        ,korl_math_megabytes(8));
    /* create a device memory allocator used to store device-local data, such as 
        mesh manifolds, SSBOs, textures, etc...  mostly things that persist for 
        many frames and have a high cost associated with data transfers to the 
        device */
    surfaceContext->deviceMemoryDeviceLocal = _korl_vulkan_deviceMemory_allocator_create(context->allocatorHandle
                                                                                        ,_KORL_VULKAN_DEVICE_MEMORY_ALLOCATOR_TYPE_GENERAL
                                                                                        ,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                                                                        ,VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                                                                        ,/*image usage flags*/0
                                                                                        ,korl_math_megabytes(8));
    /* initialize staging buffers collection */
    mcarrsetcap(KORL_C_CAST(void*, context->allocatorHandle), surfaceContext->stbDaStagingBuffers, 4);
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
            _KORL_VULKAN_CHECK(vkCreateSemaphore(context->device, &createInfoSemaphore, context->allocator, 
                                                 &surfaceContext->wipFrames[i].semaphoreTransfersDone));
            _KORL_VULKAN_CHECK(vkCreateSemaphore(context->device, &createInfoSemaphore, context->allocator, 
                                                 &surfaceContext->wipFrames[i].semaphoreImageAvailable));
            _KORL_VULKAN_CHECK(vkCreateSemaphore(context->device, &createInfoSemaphore, context->allocator, 
                                                 &surfaceContext->wipFrames[i].semaphoreRenderDone));
            _KORL_VULKAN_CHECK(vkCreateFence(context->device, &createInfoFence, context->allocator, 
                                             &surfaceContext->wipFrames[i].fenceFrameComplete));
        }
    }
#if 0///@TODO: delete/recycle
    /* --- create device memory managers --- */
    _korl_vulkan_deviceMemoryLinear_create(
        &surfaceContext->deviceMemoryHostVisible, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        /*image usage flags*/0, 
        korl_math_megabytes(8));
    _korl_vulkan_deviceMemoryLinear_create(
        &surfaceContext->deviceMemoryDeviceLocal, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        /*image usage flags*/0, 
        korl_math_megabytes(8));
    /* --- create buffers on the device to store various resources in ---
        We have two distinct usages: staging buffers which will only be the 
        source of transfer operations to the device, and device buffers which 
        will be the destination of transfer operations to the device & also be 
        used as vertex buffers. 
        - Why are we not doing this inside `_korl_vulkan_createSwapChain`?
          Because these memory allocations should (hopefully) be able to persist 
          between swap chain creation/destruction! */
    KORL_ZERO_STACK(VkPhysicalDeviceProperties, physicalDeviceProperties);
    vkGetPhysicalDeviceProperties(context->physicalDevice, &physicalDeviceProperties);
    const VkDeviceSize batchUboStride = korl_math_roundUpPowerOf2(sizeof(_Korl_Vulkan_SwapChainImageBatchUbo), physicalDeviceProperties.limits.minUniformBufferOffsetAlignment);
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[i];
        for(u8 s = 0; s < _KORL_VULKAN_SWAPCHAIN_IMAGE_CONTEXT_STAGING_BUFFER_COUNT; s++)
        {
            KORL_ZERO_STACK(VkSemaphoreCreateInfo, createInfoSemaphore);
            createInfoSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            _KORL_VULKAN_CHECK(
                vkCreateSemaphore(context->device, &createInfoSemaphore, context->allocator, 
                                  &swapChainImageContext->semaphoreStagingBuffers[s]));
            //KORL-PERFORMANCE-000-000-023: if I could re-use the same semaphore, 
            //  I would have called vkSignalSemaphore here or something to 
            //  initialize them, but it doesn't matter anymore since I added a 
            //  hack to destroy & re-create semaphores when we flush a staging buffer...
            //  This comment can just be deleted if I come up with a different solution
            KORL_ZERO_STACK(VkFenceCreateInfo, createInfoFence);
            createInfoFence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            createInfoFence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            _KORL_VULKAN_CHECK(
                vkCreateFence(context->device, &createInfoFence, context->allocator, 
                              &swapChainImageContext->fenceStagingBuffers[s]));
            /* --- allocate vertex batch staging buffers --- */
            swapChainImageContext->bufferStagingBatchIndices[s] = 
                _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                    &surfaceContext->deviceMemoryHostVisible, 
                    _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_STAGING*sizeof(Korl_Vulkan_VertexIndex), 
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
            swapChainImageContext->bufferStagingBatchPositions[s] = 
                _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                    &surfaceContext->deviceMemoryHostVisible, 
                    _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*sizeof(Korl_Vulkan_Position), 
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
            swapChainImageContext->bufferStagingBatchColors[s] = 
                _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                    &surfaceContext->deviceMemoryHostVisible, 
                    _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*sizeof(Korl_Vulkan_Color4u8), 
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
            swapChainImageContext->bufferStagingBatchUvs[s] = 
                _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                    &surfaceContext->deviceMemoryHostVisible, 
                    _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*sizeof(Korl_Vulkan_Uv), 
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
        }
        /* --- allocate UBO staging buffer --- */
        swapChainImageContext->bufferStagingUbo = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &surfaceContext->deviceMemoryHostVisible, 
                batchUboStride * _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS, 
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
        /* --- allocate device-local vertex batch buffers --- */
        swapChainImageContext->bufferDeviceBatchIndices = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &surfaceContext->deviceMemoryDeviceLocal, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_DEVICE*sizeof(Korl_Vulkan_VertexIndex), 
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE);
        swapChainImageContext->bufferDeviceBatchPositions = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &surfaceContext->deviceMemoryDeviceLocal, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*sizeof(Korl_Vulkan_Position), 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE);
        swapChainImageContext->bufferDeviceBatchColors = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &surfaceContext->deviceMemoryDeviceLocal, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*sizeof(Korl_Vulkan_Color4u8), 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE);
        swapChainImageContext->bufferDeviceBatchUvs = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &surfaceContext->deviceMemoryDeviceLocal, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*sizeof(Korl_Vulkan_Uv), 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE);
    }
    korl_log(INFO, "log reports for batch allocators:");
    korl_log(INFO, "----- host-visible -----");
    _korl_vulkan_deviceMemoryLinear_logReport(&surfaceContext->deviceMemoryHostVisible);
    korl_log(INFO, "----- device-local -----");
    _korl_vulkan_deviceMemoryLinear_logReport(&surfaceContext->deviceMemoryDeviceLocal);
    /* create the semaphores we will use to sync rendering operations of the 
        swap chain */
    KORL_ZERO_STACK(VkSemaphoreCreateInfo, createInfoSemaphore);
    createInfoSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    KORL_ZERO_STACK(VkFenceCreateInfo, createInfoFence);
    createInfoFence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfoFence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(size_t f = 0; f < _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES; f++)
    {
        _KORL_VULKAN_CHECK(
            vkCreateSemaphore(
                context->device, &createInfoSemaphore, context->allocator, 
                &surfaceContext->wipFrames[f].semaphoreImageAvailable));
        _KORL_VULKAN_CHECK(
            vkCreateSemaphore(
                context->device, &createInfoSemaphore, context->allocator, 
                &surfaceContext->wipFrames[f].semaphoreRenderDone));
        _KORL_VULKAN_CHECK(
            vkCreateFence(
                context->device, &createInfoFence, context->allocator, 
                &surfaceContext->wipFrames[f].fence));
    }
#endif
#if 0///@TODO: delete/recycle
    /* --- create descriptor pool ---
        We shouldn't have to do this inside createSwapChain for similar reasons 
        to the internal memory buffers; we should know a priori what the maximum 
        # of images in the swap chain will be.  Ergo, we can just create a pool 
        which can support this # of descriptors.  For now, this is perfectly 
        fine since the only descriptors we will have will be internal (shared 
        UBOs, for example), but this might change in the future.  
        Support for multiple batch descriptor sets per frame:
            - instead of having ONE descriptor set (containing one of each type 
              of descriptor) per frame, we can allow MANY such sets to allow 
              functionality such as:
                - multiple VP xforms per frame
                - multiple textures per frame
            - we keep a rolling index of the current batch descriptor set that 
              the current working pipeline batch will use
            - when we flush the pipeline batch, we will bind to this descriptor 
              set, and then move on to the next descriptor set for the next 
              pipeline batch
            - this means we need the ability to do one of the following things: 
                - store the current "descriptor set state" for the pipeline 
                  batch so that when the pipeline batch is flushed, we can copy 
                  the current descriptor set state to the next working batch 
                  descriptor set
                - pull the current descriptor set data back into host memory, so 
                  we can copy this data to the next batch descriptor set
                - copy descriptor set data from one set to another
            - At what time do we move from one batch descriptor set to the next?  
                - if we choose to do so after each pipeline batch flush, we're 
                  essentially putting a hard cap on the # of times we can do a 
                  pipeline batch flush per frame
                - in reality though, it's likely that we can have many batch 
                  pipeline flushes using the exact same batch descriptor set 
                - it makes more sense to ONLY flush the batch descriptor set 
                  when we have a situation where one or more batch pipeline 
                  flushes have occurred since the last time the batch descriptor 
                  set has been updated
                - for implementation of this, we can have a FLAG, which is only 
                  raised when a batch pipeline is flushed.  Then, whenever any 
                  korl-vulkan API tries to modify the batch descriptor set, if 
                  this flag was raised we advance to the next batch descriptor 
                  set and modify that instead */
    KORL_ZERO_STACK_ARRAY(VkDescriptorPoolSize, descriptorPoolSizes, 2);
    descriptorPoolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorPoolSizes[0].descriptorCount = _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE*_KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS;
    descriptorPoolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSizes[1].descriptorCount = _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE*_KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS;
    KORL_ZERO_STACK(VkDescriptorPoolCreateInfo, createInfoDescriptorPool);
    createInfoDescriptorPool.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfoDescriptorPool.maxSets       = _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE*_KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS;
    createInfoDescriptorPool.poolSizeCount = korl_arraySize(descriptorPoolSizes);
    createInfoDescriptorPool.pPoolSizes    = descriptorPoolSizes;
    _KORL_VULKAN_CHECK(
        vkCreateDescriptorPool(
            context->device, &createInfoDescriptorPool, context->allocator, 
            &surfaceContext->batchDescriptorPool));
#endif
    /* --- create batch descriptor set layout --- */
    // not really sure where this should go, but I know that it just needs to 
    //    be created BEFORE the pipelines are created, since they are used there //
    //KORL-PERFORMANCE-000-000-015: split this into multiple descriptor sets based on change frequency
    KORL_ZERO_STACK_ARRAY(VkDescriptorSetLayoutBinding, descriptorSetLayoutBindings, 2);
    descriptorSetLayoutBindings[0].binding         = _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_UBO;
    descriptorSetLayoutBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorSetLayoutBindings[0].descriptorCount = 1;//size of the array in the shader
    descriptorSetLayoutBindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
    descriptorSetLayoutBindings[1].binding         = _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_TEXTURE;
    descriptorSetLayoutBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBindings[1].descriptorCount = 1;//size of the array in the shader
    descriptorSetLayoutBindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    KORL_ZERO_STACK(VkDescriptorSetLayoutCreateInfo, createInfoDescriptorSetLayout);
    createInfoDescriptorSetLayout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfoDescriptorSetLayout.bindingCount = korl_arraySize(descriptorSetLayoutBindings);
    createInfoDescriptorSetLayout.pBindings    = descriptorSetLayoutBindings;
    _KORL_VULKAN_CHECK(
        vkCreateDescriptorSetLayout(context->device, &createInfoDescriptorSetLayout, context->allocator
                                   ,&context->batchDescriptorSetLayout));
#if 0///@TODO: delete/recycle
    /* --- allocate descriptor sets --- 
        This must happen AFTER the associated descriptorSetLayout(s) get 
        created, since creation of a set depends on the layout! */
    VkDescriptorSetLayout batchDescriptorSetLayouts[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE*_KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS];
    for(unsigned i = 0; i < _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE*_KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS; i++)
        batchDescriptorSetLayouts[i] = context->batchDescriptorSetLayout;
    KORL_ZERO_STACK(VkDescriptorSetAllocateInfo, allocInfoDescriptorSets);
    allocInfoDescriptorSets.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfoDescriptorSets.descriptorPool     = surfaceContext->batchDescriptorPool;
    allocInfoDescriptorSets.descriptorSetCount = korl_arraySize(batchDescriptorSetLayouts);
    allocInfoDescriptorSets.pSetLayouts        = batchDescriptorSetLayouts;
    _KORL_VULKAN_CHECK(
        vkAllocateDescriptorSets(
            context->device, &allocInfoDescriptorSets, 
            &surfaceContext->batchDescriptorSets[0][0]));
    /* because the batch descriptor set UBOs are always going to point to the 
        same buffer memory locations for the duration of the program, we can 
        just set them all right now! */
    for(u32 s = 0; s < surfaceContext->swapChainImagesSize; s++)
    {
        _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[s];
        KORL_ZERO_STACK_ARRAY(VkDescriptorBufferInfo, descriptorBufferInfoUbos, _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS);
        KORL_ZERO_STACK_ARRAY(VkWriteDescriptorSet, writeDescriptorSetUbos, _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS);
        for(u$ i = 0; i < _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS; i++)
        {
            descriptorBufferInfoUbos[i].buffer = swapChainImageContext->bufferStagingUbo->deviceObject.buffer;
            descriptorBufferInfoUbos[i].range  = sizeof(_Korl_Vulkan_SwapChainImageBatchUbo);
            descriptorBufferInfoUbos[i].offset = i*batchUboStride;
            writeDescriptorSetUbos[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSetUbos[i].dstSet          = surfaceContext->batchDescriptorSets[s][i];
            writeDescriptorSetUbos[i].dstBinding      = _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_UBO;
            writeDescriptorSetUbos[i].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeDescriptorSetUbos[i].descriptorCount = 1;
            writeDescriptorSetUbos[i].pBufferInfo     = &descriptorBufferInfoUbos[i];
        }
        vkUpdateDescriptorSets(context->device, korl_arraySize(writeDescriptorSetUbos), writeDescriptorSetUbos, 0, NULL);
    }
#endif
    /* create pipeline layout */
    KORL_ZERO_STACK(VkPushConstantRange, pushConstantRange);
    pushConstantRange.size       = sizeof(_Korl_Vulkan_DrawPushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    KORL_ZERO_STACK(VkPipelineLayoutCreateInfo, createInfoPipelineLayout);
    createInfoPipelineLayout.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfoPipelineLayout.setLayoutCount         = 1;
    createInfoPipelineLayout.pSetLayouts            = &context->batchDescriptorSetLayout;
    createInfoPipelineLayout.pushConstantRangeCount = 1;
    createInfoPipelineLayout.pPushConstantRanges    = &pushConstantRange;
    _KORL_VULKAN_CHECK(
        vkCreatePipelineLayout(context->device, &createInfoPipelineLayout, context->allocator
                              ,&context->pipelineLayout));
    /* load required built-in shader assets */
    Korl_AssetCache_AssetData assetShaderBatchVertexColor;
    Korl_AssetCache_AssetData assetShaderBatchVertexTexture;
    Korl_AssetCache_AssetData assetShaderBatchVertexColorTexture;
    Korl_AssetCache_AssetData assetShaderBatchFragmentColor;
    Korl_AssetCache_AssetData assetShaderBatchFragmentTexture;
    Korl_AssetCache_AssetData assetShaderBatchFragmentColorTexture;
    Korl_AssetCache_AssetData assetShaderVertex2d;
    Korl_AssetCache_AssetData assetShaderVertex3d;
    Korl_AssetCache_AssetData assetShaderVertex3dColor;
    Korl_AssetCache_AssetData assetShaderVertex3dUv;
    Korl_AssetCache_AssetData assetShaderFragmentColor;
    Korl_AssetCache_AssetData assetShaderFragmentColorTexture;
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-batch-color.vert.spv"        , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderBatchVertexColor));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-batch-texture.vert.spv"      , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderBatchVertexTexture));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-batch-color-texture.vert.spv", KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderBatchVertexColorTexture));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-batch-color.frag.spv"        , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderBatchFragmentColor));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-batch-texture.frag.spv"      , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderBatchFragmentTexture));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-batch-color-texture.frag.spv", KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderBatchFragmentColorTexture));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-2d.vert.spv"                 , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex2d));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-3d.vert.spv"                 , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex3d));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-3d-color.vert.spv"           , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex3dColor));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-3d-uv.vert.spv"              , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderVertex3dUv));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-color.frag.spv"              , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderFragmentColor));
    korl_assert(KORL_ASSETCACHE_GET_RESULT_LOADED == korl_assetCache_get(L"build/shaders/korl-color-texture.frag.spv"      , KORL_ASSETCACHE_GET_FLAGS_NONE, &assetShaderFragmentColorTexture));
    /* create shader modules */
    KORL_ZERO_STACK(VkShaderModuleCreateInfo, createInfoShader);
    createInfoShader.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfoShader.codeSize = assetShaderBatchVertexColor.dataBytes;
    createInfoShader.pCode    = assetShaderBatchVertexColor.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderBatchVertColor));
    createInfoShader.codeSize = assetShaderBatchVertexTexture.dataBytes;
    createInfoShader.pCode    = assetShaderBatchVertexTexture.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderBatchVertTexture));
    createInfoShader.codeSize = assetShaderBatchVertexColorTexture.dataBytes;
    createInfoShader.pCode    = assetShaderBatchVertexColorTexture.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderBatchVertColorTexture));
    createInfoShader.codeSize = assetShaderBatchFragmentColor.dataBytes;
    createInfoShader.pCode    = assetShaderBatchFragmentColor.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderBatchFragColor));
    createInfoShader.codeSize = assetShaderBatchFragmentTexture.dataBytes;
    createInfoShader.pCode    = assetShaderBatchFragmentTexture.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderBatchFragTexture));
    createInfoShader.codeSize = assetShaderBatchFragmentColorTexture.dataBytes;
    createInfoShader.pCode    = assetShaderBatchFragmentColorTexture.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderBatchFragColorTexture));
    createInfoShader.codeSize = assetShaderVertex2d.dataBytes;
    createInfoShader.pCode    = assetShaderVertex2d.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex2d));
    createInfoShader.codeSize = assetShaderVertex3d.dataBytes;
    createInfoShader.pCode    = assetShaderVertex3d.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex3d));
    createInfoShader.codeSize = assetShaderVertex3dColor.dataBytes;
    createInfoShader.pCode    = assetShaderVertex3dColor.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex3dColor));
    createInfoShader.codeSize = assetShaderVertex3dUv.dataBytes;
    createInfoShader.pCode    = assetShaderVertex3dUv.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderVertex3dUv));
    createInfoShader.codeSize = assetShaderFragmentColor.dataBytes;
    createInfoShader.pCode    = assetShaderFragmentColor.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderFragmentColor));
    createInfoShader.codeSize = assetShaderFragmentColorTexture.dataBytes;
    createInfoShader.pCode    = assetShaderFragmentColorTexture.data;
    _KORL_VULKAN_CHECK(vkCreateShaderModule(context->device, &createInfoShader, context->allocator, &context->shaderFragmentColorTexture));
#if 0///@TODO: delete/recycle
    /* create memory allocators to stage & store persistent assets, like images */
    _korl_vulkan_deviceMemoryLinear_create(
        &context->deviceMemoryLinearAssetsStaging, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        /*image usage flags*/0, 
        korl_math_megabytes(8));
    _korl_vulkan_deviceMemoryLinear_create(
        &context->deviceMemoryLinearAssets, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
        korl_math_megabytes(64));
    /* create a default texture asset to be used whenever we fail to get texture 
        assets for example 
        @create-default-texture-copypasta */
    Korl_Vulkan_Color4u8 defaultTextureColor = (Korl_Vulkan_Color4u8){255, 0, 255, 255};
    context->textureHandleDefaultTexture = korl_vulkan_textureCreate(1, 1, &defaultTextureColor);
#endif
    surfaceContext->deviceAssetDatabase = _korl_vulkan_deviceAssetDatabase_create(context->allocatorHandle, &(surfaceContext->deviceMemoryDeviceLocal));
    _korl_vulkan_deviceMemory_allocator_logReport(&surfaceContext->deviceMemoryHostVisible);
    _korl_vulkan_deviceMemory_allocator_logReport(&surfaceContext->deviceMemoryRenderResources);
    mcarrsetcap(KORL_C_CAST(void*, context->allocatorHandle), context->stbDaPipelines, 128);
}
korl_internal void korl_vulkan_destroySurface(void)
{
    _korl_vulkan_destroySwapChain();
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* destroy the surface-specific resources */
    _korl_vulkan_deviceAssetDatabase_destroy(&surfaceContext->deviceAssetDatabase);
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        vkDestroySemaphore(context->device, surfaceContext->wipFrames[i].semaphoreTransfersDone, context->allocator);
        vkDestroySemaphore(context->device, surfaceContext->wipFrames[i].semaphoreImageAvailable, context->allocator);
        vkDestroySemaphore(context->device, surfaceContext->wipFrames[i].semaphoreRenderDone, context->allocator);
        vkDestroyFence(context->device, surfaceContext->wipFrames[i].fenceFrameComplete, context->allocator);
    }
    vkDestroySurfaceKHR(context->instance, surfaceContext->surface, context->allocator);
#if 0///@TODO: delete/recycle
    vkDestroyDescriptorPool(context->device, surfaceContext->batchDescriptorPool, context->allocator);
    _korl_vulkan_deviceMemoryLinear_destroy(&surfaceContext->deviceMemoryHostVisible);
    _korl_vulkan_deviceMemoryLinear_destroy(&surfaceContext->deviceMemoryDeviceLocal);
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[i];
        for(u8 s = 0; s < _KORL_VULKAN_SWAPCHAIN_IMAGE_CONTEXT_STAGING_BUFFER_COUNT; s++)
        {
            vkDestroyFence(context->device, swapChainImageContext->fenceStagingBuffers[s], context->allocator);
            vkDestroySemaphore(context->device, swapChainImageContext->semaphoreStagingBuffers[s], context->allocator);
        }
    }
#endif
    _korl_vulkan_deviceMemory_allocator_destroy(&surfaceContext->deviceMemoryDeviceLocal);
    _korl_vulkan_deviceMemory_allocator_destroy(&surfaceContext->deviceMemoryHostVisible);
    _korl_vulkan_deviceMemory_allocator_destroy(&surfaceContext->deviceMemoryRenderResources);
    /* NOTE: we don't have to free individual device memory allocations in each 
             Buffer, since the entire allocators are being destroyed above! */
    mcarrfree(KORL_C_CAST(void*, context->allocatorHandle), surfaceContext->stbDaStagingBuffers);
    korl_memory_zero(surfaceContext, sizeof(*surfaceContext));// NOTE: Keep this as the last operation in the surface destructor!
    /* destroy the device-specific resources */
#if 0///@TODO: delete/recycle
    _korl_vulkan_deviceMemoryLinear_destroy(&context->deviceMemoryLinearAssetsStaging);
    _korl_vulkan_deviceMemoryLinear_destroy(&context->deviceMemoryLinearAssets);
    _korl_vulkan_deviceMemoryLinear_destroy(&context->deviceMemoryLinearRenderResources);
#endif
    for(u$ p = 0; p < arrlenu(context->stbDaPipelines); p++)
        vkDestroyPipeline(context->device, context->stbDaPipelines[p].pipeline, context->allocator);
    mcarrfree(KORL_C_CAST(void*, context->allocatorHandle), context->stbDaPipelines);
    vkDestroyDescriptorSetLayout(context->device, context->batchDescriptorSetLayout, context->allocator);
    vkDestroyPipelineLayout(context->device, context->pipelineLayout, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderBatchVertColor, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderBatchVertTexture, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderBatchVertColorTexture, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderBatchFragColor, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderBatchFragTexture, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderBatchFragColorTexture, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex2d, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex3d, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex3dColor, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderVertex3dUv, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderFragmentColor, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderFragmentColorTexture, context->allocator);
#if 0///@TODO: delete/recycle
    vkDestroyCommandPool(context->device, context->commandPoolTransfer, context->allocator);
    vkDestroyCommandPool(context->device, context->commandPoolGraphics, context->allocator);
#endif
    vkDestroyDevice(context->device, context->allocator);
}
korl_internal Korl_Math_V2u32 korl_vulkan_getSurfaceSize(void)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    if(surfaceContext->deferredResize)
        return (Korl_Math_V2u32){ surfaceContext->deferredResizeX
                                , surfaceContext->deferredResizeY };
    return (Korl_Math_V2u32){ surfaceContext->swapChainImageExtent.width
                            , surfaceContext->swapChainImageExtent.height };
}
korl_internal void korl_vulkan_clearAllDeviceAssets(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    ///@TODO: no need to be lazy/dumb & wait on the device; just mark all the internal device assets for deletion
    ///@TODO: if we end up with a default texture, just skip over it instead of clearing everything & re-making it :|
#if 0///@TODO: delete/recycle
    _KORL_VULKAN_CHECK(vkDeviceWaitIdle(context->device));
    _korl_vulkan_deviceMemoryLinear_clear(&context->deviceMemoryLinearAssetsStaging);
    _korl_vulkan_deviceMemoryLinear_clear(&context->deviceMemoryLinearAssets);
    for(Korl_MemoryPool_Size da = 0; da < KORL_MEMORY_POOL_SIZE(context->deviceAssets); da++)
    {
        switch(context->deviceAssets[da].type)
        {
        case _KORL_VULKAN_DEVICEASSET_TYPE_ASSET_TEXTURE:{
            korl_stringPool_free(context->deviceAssets[da].subType.assetTexture.name);
            break;}
        case _KORL_VULKAN_DEVICEASSET_TYPE_TEXTURE:{
            break;}
        }
    }
    KORL_MEMORY_POOL_EMPTY(context->deviceAssets);
    /* create a default texture asset to be used whenever we fail to get texture 
        assets for example 
        @create-default-texture-copypasta */
    Korl_Vulkan_Color4u8 defaultTextureColor = (Korl_Vulkan_Color4u8){255, 0, 255, 255};
    context->textureHandleDefaultTexture = korl_vulkan_textureCreate(1, 1, &defaultTextureColor);
#endif
}
korl_internal void korl_vulkan_setSurfaceClearColor(const f32 clearRgb[3])
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    for(u8 i = 0; i < korl_arraySize(surfaceContext->frameBeginClearColor.elements); i++)
        surfaceContext->frameBeginClearColor.elements[i] = clearRgb[i];
}
korl_internal void korl_vulkan_frameBegin(void)
{
    _Korl_Vulkan_Context*const context                        = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext          = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext* swapChainImageContext = NULL;
    /* it is possible that we do not have the ability to render anything this 
        frame, so we need to check the state of the swap chain image index for 
        each of the exposed rendering APIs - if it remains unchanged from this 
        initial assigned value, then we need to just not do anything in those 
        functions which require a valid swap chain image context!!! */
    surfaceContext->frameSwapChainImageIndex = _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE;
    /* to begin the frame, we need to: 
        - resize the swap chain if any events triggered a deferred resize
        - acquire the next swap chain image
        - begin the new command buffer, including a command to clear it */
    if(surfaceContext->deferredResize)
    {
        /* if the resize area is zero, then we can't do anything because that is 
            an illegal vulkan state anyways!  This happens when the window gets 
            minimized or the user makes it really small for some reason... */
        if(   surfaceContext->deferredResizeX == 0 
           || surfaceContext->deferredResizeY == 0)
            goto done;
        korl_log(VERBOSE, "resizing to: %ux%u", surfaceContext->deferredResizeX
                                              , surfaceContext->deferredResizeY);
        /* destroy swap chain & all resources which depend on it implicitly*/
        _korl_vulkan_destroySwapChain();
        /* since the device surface meta data has changed, we need to query it */
        _Korl_Vulkan_DeviceSurfaceMetaData deviceSurfaceMetaData = 
            _korl_vulkan_deviceSurfaceMetaData(context->physicalDevice, 
                                               surfaceContext->surface);
        /* re-create the swap chain & all resources which depend on it */
        _korl_vulkan_createSwapChain(surfaceContext->deferredResizeX, 
                                     surfaceContext->deferredResizeY, 
                                     &deviceSurfaceMetaData);
        /* re-create pipelines */
        for(u$ p = 0; p < arrlenu(context->stbDaPipelines); p++)
        {
            korl_assert(context->stbDaPipelines[p].pipeline != VK_NULL_HANDLE);
            vkDestroyPipeline(context->device, context->stbDaPipelines[p].pipeline, context->allocator);
            context->stbDaPipelines[p].pipeline = VK_NULL_HANDLE;
            _korl_vulkan_createPipeline(p);
        }
        //KORL-ISSUE-000-000-025: re-record command buffers?...
        /* clear the deferred resize flag for the next frame */
        surfaceContext->deferredResize = false;
    }
#if 0///@TODO: delete/recycle
    /* wait on the fence for the current WIP frame */
    _KORL_VULKAN_CHECK(
        vkWaitForFences(context->device, 1, 
                        &surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fence, 
                        VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/));
#endif
    /* wait on the next "wip frame" synchronization struct so that we have a set 
        of sync primitives that aren't currently being used by the device */
    if(surfaceContext->wipFrameCount >= surfaceContext->swapChainImagesSize)
        _KORL_VULKAN_CHECK(
            vkWaitForFences(context->device, 1, 
                            &surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fenceFrameComplete, 
                            VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/));
    /* at this point, we know for certain that a frame has been processed, so we 
        can update our memory pools to reflect this history, allowing us to 
        reuse pools that we can deduce _must_ no longer be in use */
    for(u$ b = 0; b < arrlenu(surfaceContext->stbDaStagingBuffers); b++)
    {
        _Korl_Vulkan_Buffer*const buffer = &(surfaceContext->stbDaStagingBuffers[b]);
        if(buffer->framesSinceLastUsed < KORL_U8_MAX)
            buffer->framesSinceLastUsed++;
    }
    /* same as with staging buffers, we can now nullify device assets that we 
        know are no longer being used */
    _korl_vulkan_deviceAssetDatabase_cycleAssetLifetimes(&(surfaceContext->deviceAssetDatabase));
    /* acquire the next image from the swap chain */
    _KORL_VULKAN_CHECK(
        vkAcquireNextImageKHR(context->device, surfaceContext->swapChain, 
                              UINT64_MAX/*timeout; UINT64_MAX -> disable*/, 
                              surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreImageAvailable, 
                              VK_NULL_HANDLE/*fence*/, 
                              /*return value address*/&surfaceContext->frameSwapChainImageIndex));
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
#if 0///@TODO: delete/recycle
    if(swapChainImageContext->fenceWipFrame != VK_NULL_HANDLE)
    {
        _KORL_VULKAN_CHECK(
            vkWaitForFences(context->device, 1, 
                            &swapChainImageContext->fenceWipFrame, 
                            VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/));
    }
    swapChainImageContext->fenceWipFrame = 
        surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fence;
#endif
#if 0///@TODO: delete/recycle
    /* We can, and _should_, free & invalidate the staging=>device transfer 
        command buffer handles at the beginning of each frame, since we 
        definitely know that they are no longer in use! */
    vkFreeCommandBuffers(context->device, context->commandPoolTransfer, 
                         _KORL_VULKAN_SWAPCHAIN_IMAGE_CONTEXT_STAGING_BUFFER_COUNT, 
                         swapChainImageContext->commandBufferStagingBuffers);
    for(u8 s = 0; s < _KORL_VULKAN_SWAPCHAIN_IMAGE_CONTEXT_STAGING_BUFFER_COUNT; s++)
        swapChainImageContext->commandBufferStagingBuffers[s] = VK_NULL_HANDLE;
#endif
    /* ----- begin the swap chain command buffer for this frame ----- */
    /* free the primary command buffers from the previous frame, since they are 
        no longer valid */
    vkFreeCommandBuffers(context->device, swapChainImageContext->commandPool, 1, &swapChainImageContext->commandBufferGraphics);
    vkFreeCommandBuffers(context->device, swapChainImageContext->commandPool, 1, &swapChainImageContext->commandBufferTransfer);
    /* allocate a command buffer for each of the swap chain frame buffers 
        (graphics & transfer) */
    KORL_ZERO_STACK(VkCommandBufferAllocateInfo, allocateInfoCommandBuffers);
    allocateInfoCommandBuffers.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfoCommandBuffers.commandPool        = swapChainImageContext->commandPool;
    allocateInfoCommandBuffers.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfoCommandBuffers.commandBufferCount = 1;
    _KORL_VULKAN_CHECK(
        vkAllocateCommandBuffers(context->device, &allocateInfoCommandBuffers, 
                                 &swapChainImageContext->commandBufferGraphics));
    _KORL_VULKAN_CHECK(
        vkAllocateCommandBuffers(context->device, &allocateInfoCommandBuffers, 
                                 &swapChainImageContext->commandBufferTransfer));
#if 0///@TODO: delete/recycle
    _KORL_VULKAN_CHECK(
        vkResetCommandBuffer(surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 
                             0/*flags*/));
#endif
    KORL_ZERO_STACK(VkCommandBufferBeginInfo, beginInfoCommandBuffer);
    beginInfoCommandBuffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfoCommandBuffer.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    _KORL_VULKAN_CHECK(vkBeginCommandBuffer(swapChainImageContext->commandBufferGraphics, &beginInfoCommandBuffer));
    _KORL_VULKAN_CHECK(vkBeginCommandBuffer(swapChainImageContext->commandBufferTransfer, &beginInfoCommandBuffer));
    // define the color we are going to clear the color attachment with when 
    //    the render pass begins:
    KORL_ZERO_STACK_ARRAY(VkClearValue, clearValues, 2);
    clearValues[0].color.float32[0] = surfaceContext->frameBeginClearColor.elements[0];
    clearValues[0].color.float32[1] = surfaceContext->frameBeginClearColor.elements[1];
    clearValues[0].color.float32[2] = surfaceContext->frameBeginClearColor.elements[2];
    clearValues[0].color.float32[3] = 1.f;
    clearValues[1].depthStencil.depth = 1.f;
    clearValues[1].depthStencil.stencil = 0;
    KORL_ZERO_STACK(VkRenderPassBeginInfo, beginInfoRenderPass);
    beginInfoRenderPass.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfoRenderPass.renderPass        = context->renderPass;
    beginInfoRenderPass.framebuffer       = swapChainImageContext->frameBuffer;
    beginInfoRenderPass.renderArea.extent = surfaceContext->swapChainImageExtent;
    beginInfoRenderPass.clearValueCount   = korl_arraySize(clearValues);
    beginInfoRenderPass.pClearValues      = clearValues;
    vkCmdBeginRenderPass(swapChainImageContext->commandBufferGraphics, 
                         &beginInfoRenderPass, VK_SUBPASS_CONTENTS_INLINE);
#if 0///@TODO: delete/recycle
    /* clear the state of the first batch descriptor set */
    /* calculate the stride of each batch descriptor set UBO within the buffer */
    KORL_ZERO_STACK(VkPhysicalDeviceProperties, physicalDeviceProperties);
    vkGetPhysicalDeviceProperties(context->physicalDevice, &physicalDeviceProperties);
    const VkDeviceSize batchUboStride = 
        korl_math_roundUpPowerOf2(sizeof(_Korl_Vulkan_SwapChainImageBatchUbo), 
                                  physicalDeviceProperties.limits.minUniformBufferOffsetAlignment);
    /* now we can set the UBO in the buffer at the current batch descriptor set 
        index to be a known default set of values */
    KORL_ZERO_STACK(void*, mappedDeviceMemory);
    _KORL_VULKAN_CHECK(
        vkMapMemory(context->device, surfaceContext->deviceMemoryHostVisible.deviceMemory, 
                    swapChainImageContext->bufferStagingUbo->byteOffset + surfaceContext->batchState.descriptorSetIndexCurrent*batchUboStride, 
                    /*bytes*/sizeof(_Korl_Vulkan_SwapChainImageBatchUbo), 
                    0/*flags*/, &mappedDeviceMemory));
    _Korl_Vulkan_SwapChainImageBatchUbo*const ubo = KORL_C_CAST(_Korl_Vulkan_SwapChainImageBatchUbo*, mappedDeviceMemory);
    ubo->m4f32Projection = KORL_MATH_M4F32_IDENTITY;
    ubo->m4f32View       = KORL_MATH_M4F32_IDENTITY;
    ubo->m4f32Model      = KORL_MATH_M4F32_IDENTITY;
    vkUnmapMemory(context->device, surfaceContext->deviceMemoryHostVisible.deviceMemory);
#endif
done:
    /* help to ensure the user is calling frameBegin & frameEnd in the correct 
        order & the correct frequency */
    korl_assert(surfaceContext->frameStackCounter == 0);
    surfaceContext->frameStackCounter++;
    /* clear the vertex batch metrics for the upcoming frame */
    KORL_ZERO_STACK(VkRect2D, scissorDefault);
    scissorDefault.offset = (VkOffset2D){.x = 0, .y = 0};
    scissorDefault.extent = surfaceContext->swapChainImageExtent;
    korl_memory_zero(&surfaceContext->batchState, sizeof(surfaceContext->batchState));
    surfaceContext->batchState.pushConstants.m4f32Model   = KORL_MATH_M4F32_IDENTITY;
    surfaceContext->batchState.m4f32View                  = KORL_MATH_M4F32_IDENTITY;
    surfaceContext->batchState.m4f32Projection            = KORL_MATH_M4F32_IDENTITY;
    surfaceContext->batchState.pipelineConfigurationCache = _korl_vulkan_pipeline_default();
    surfaceContext->batchState.scissor                    = surfaceContext->batchState.scissor = scissorDefault;
#if 0///@TODO: delete/recycle
    /* Select a known valid internal texture by default.  
        NOTE: This is done because it is possible for the initial descriptor set 
              to be configured to use image/samplers for a texture asset which 
              is no longer loaded, which will eventually cause a validation error.  */
    korl_vulkan_useTexture(context->textureHandleDefaultTexture);
    // setting the current pipeline index to be out of bounds effectively sets 
    //  the pipeline produced from _korl_vulkan_pipeline_default to be used
    surfaceContext->batchState.currentPipeline = context->pipelinesCount;
#endif
}
korl_internal void korl_vulkan_frameEnd(void)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* if we got an invalid next swapchain image index, just do nothing */
    if(surfaceContext->frameSwapChainImageIndex >= _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE)
        goto done;
#if 0///@TODO: delete/recycle
    korl_time_probeStart(flush_batch_pipeline);
    _korl_vulkan_flushBatchPipeline();
    korl_time_probeStop(flush_batch_pipeline);
    korl_time_probeStart(flush_batch_staging);
    _korl_vulkan_flushBatchStaging();//KORL-PERFORMANCE-000-000-022: this function currently assumes that the caller is expecting to be able to safely write to the new staging buffer RIGHT AWAY, but in this special case we do NOT need to wait for the alternate staging buffer to be available!!!  We should add a parameter to this function to optionally ignore the `wait_for_new_buffer_fence` code that runs at the end of this function.
    korl_time_probeStop(flush_batch_staging);
#endif
    vkCmdEndRenderPass(swapChainImageContext->commandBufferGraphics);
    _KORL_VULKAN_CHECK(vkEndCommandBuffer(swapChainImageContext->commandBufferGraphics));
    _KORL_VULKAN_CHECK(vkEndCommandBuffer(swapChainImageContext->commandBufferTransfer));
    /* submit transfer commands to the graphics queue */
    {
        korl_time_probeStart(submit_xfer_cmds_to_gfx_q);
        KORL_ZERO_STACK_ARRAY(VkCommandBufferSubmitInfo, commandBufferSubmitInfo, 1);
        commandBufferSubmitInfo[0].sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfo[0].commandBuffer = swapChainImageContext->commandBufferTransfer;
        KORL_ZERO_STACK_ARRAY(VkSemaphoreSubmitInfo, semaphoreSubmitInfoSignal, 1);
        semaphoreSubmitInfoSignal[0].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        semaphoreSubmitInfoSignal[0].semaphore = surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreTransfersDone;
        semaphoreSubmitInfoSignal[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        KORL_ZERO_STACK_ARRAY(VkSubmitInfo2, submitInfoGraphics, 1);
        submitInfoGraphics[0].sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfoGraphics[0].commandBufferInfoCount   = korl_arraySize(commandBufferSubmitInfo);
        submitInfoGraphics[0].pCommandBufferInfos      = commandBufferSubmitInfo;
        submitInfoGraphics[0].signalSemaphoreInfoCount = korl_arraySize(semaphoreSubmitInfoSignal);
        submitInfoGraphics[0].pSignalSemaphoreInfos    = semaphoreSubmitInfoSignal;
        _KORL_VULKAN_CHECK(
            vkQueueSubmit2(context->queueGraphics, korl_arraySize(submitInfoGraphics), submitInfoGraphics, /*signal fence*/VK_NULL_HANDLE));
        korl_time_probeStop(submit_xfer_cmds_to_gfx_q);
    }
    /* submit graphics commands to the graphics queue */
#if 0///@TODO: delete/recycle
    uint32_t submitGraphicsWaitSemaphoreCount = 0;
    VkSemaphore submitGraphicsWaitSemaphores[1/*semaphoreImageAvailable*/];
    submitGraphicsWaitSemaphores[submitGraphicsWaitSemaphoreCount++] = 
        surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreImageAvailable;
#if 0///@TODO: delete/recycle
    // ONLY wait on staging buffer semaphores that we KNOW have been used to submit a memory transfer command buffer //
    for(uint32_t i = 0; i < _KORL_VULKAN_SWAPCHAIN_IMAGE_CONTEXT_STAGING_BUFFER_COUNT; i++)
        if(swapChainImageContext->commandBufferStagingBuffers[i] != VK_NULL_HANDLE)
            submitGraphicsWaitSemaphores[submitGraphicsWaitSemaphoreCount++] = swapChainImageContext->semaphoreStagingBuffers[i];
#endif
    VkPipelineStageFlags submitGraphicsWaitStages[1/*semaphoreImageAvailable*/];
    submitGraphicsWaitStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
#if 0///@TODO: delete/recycle
    for(uint32_t i = 1; i < submitGraphicsWaitSemaphoreCount; i++)
        submitGraphicsWaitStages[i] = VK_PIPELINE_STAGE_TRANSFER_BIT;
#endif
    VkSemaphore submitGraphicsSignalSemaphores[] = 
        { surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreRenderDone };
    korl_time_probeStart(submit_gfx_cmds_to_gfx_q);
    KORL_ZERO_STACK(VkSubmitInfo, submitInfoGraphics);
    submitInfoGraphics.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfoGraphics.waitSemaphoreCount   = submitGraphicsWaitSemaphoreCount;
    submitInfoGraphics.pWaitSemaphores      = submitGraphicsWaitSemaphores;
    submitInfoGraphics.pWaitDstStageMask    = submitGraphicsWaitStages;
    submitInfoGraphics.commandBufferCount   = 1;
    submitInfoGraphics.pCommandBuffers      = &surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex];
    submitInfoGraphics.signalSemaphoreCount = korl_arraySize(submitGraphicsSignalSemaphores);
    submitInfoGraphics.pSignalSemaphores    = submitGraphicsSignalSemaphores;
#else
    korl_time_probeStart(submit_gfx_cmds_to_gfx_q);
#endif
    // close the fence in preparation to submit commands to the graphics queue
    _KORL_VULKAN_CHECK(
        vkResetFences(context->device, 1, 
                      &surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fenceFrameComplete));
#if 0///@TODO: delete/recycle
    _KORL_VULKAN_CHECK(
        vkQueueSubmit(context->queueGraphics, 1, &submitInfoGraphics, 
                      surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fenceFrameComplete));
#endif
    KORL_ZERO_STACK_ARRAY(VkSemaphoreSubmitInfo, semaphoreSubmitInfoWait, 2);
    semaphoreSubmitInfoWait[0].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    semaphoreSubmitInfoWait[0].semaphore = surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreImageAvailable;
    semaphoreSubmitInfoWait[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    semaphoreSubmitInfoWait[1].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    semaphoreSubmitInfoWait[1].semaphore = surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreTransfersDone;
    semaphoreSubmitInfoWait[1].stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    KORL_ZERO_STACK_ARRAY(VkCommandBufferSubmitInfo, commandBufferSubmitInfo, 1);
    commandBufferSubmitInfo[0].sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferSubmitInfo[0].commandBuffer = swapChainImageContext->commandBufferGraphics;
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
        vkQueueSubmit2(context->queueGraphics, korl_arraySize(submitInfoGraphics), submitInfoGraphics, 
                       surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fenceFrameComplete));
    korl_time_probeStop(submit_gfx_cmds_to_gfx_q);
    /* present the swap chain */
    korl_time_probeStart(present_swap_chain);
    VkSwapchainKHR presentInfoSwapChains[] = { surfaceContext->swapChain };
    KORL_ZERO_STACK(VkPresentInfoKHR, presentInfo);
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &(surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreRenderDone);
    presentInfo.swapchainCount     = korl_arraySize(presentInfoSwapChains);
    presentInfo.pSwapchains        = presentInfoSwapChains;
    presentInfo.pImageIndices      = &surfaceContext->frameSwapChainImageIndex;
    _KORL_VULKAN_CHECK(vkQueuePresentKHR(context->queuePresent, &presentInfo));
    korl_time_probeStop(present_swap_chain);
    /* advance to the next WIP frame index */
    surfaceContext->wipFrameCurrent = (surfaceContext->wipFrameCurrent + 1) % surfaceContext->swapChainImagesSize;
    surfaceContext->wipFrameCount++;
    KORL_MATH_ASSIGN_CLAMP_MAX(surfaceContext->wipFrameCount, surfaceContext->swapChainImagesSize);
done:
    /* help to ensure the user is calling frameBegin & frameEnd in the correct 
        order & the correct frequency */
    korl_assert(surfaceContext->frameStackCounter == 1);
    surfaceContext->frameStackCounter--;
}
korl_internal void korl_vulkan_deferredResize(u32 sizeX, u32 sizeY)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* help ensure that this code never runs INSIDE of a set of 
        frameBegin/frameEnd calls - we cannot signal a deferred resize once a 
        new frame has begun! */
    korl_assert(surfaceContext->frameStackCounter == 0);
    surfaceContext->deferredResize = true;
    surfaceContext->deferredResizeX = sizeX;
    surfaceContext->deferredResizeY = sizeY;
}
korl_internal void korl_vulkan_setDrawState(const Korl_Vulkan_DrawState* state)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    korl_time_probeStart(set_draw_state);
    /* help ensure that this code never runs outside of a set of 
        frameBegin/frameEnd calls */
    if(surfaceContext->frameStackCounter != 1)
        goto done;
    /* if the swap chain image context is invalid for this frame for some reason, 
        then just do nothing (this happens during deferred resize for example) */
    if(surfaceContext->frameSwapChainImageIndex == _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE)
        goto done;
    /* all we have to do is configure the pipeline config cache here, as the 
        actual vulkan pipeline will be created & bound when we call draw */
    if(state->features)
    {
        surfaceContext->batchState.pipelineConfigurationCache.features.enableBlend     = 0 != state->features->enableBlend;
        surfaceContext->batchState.pipelineConfigurationCache.features.enableDepthTest = 0 != state->features->enableDepthTest;
    }
    if(state->blend)
        surfaceContext->batchState.pipelineConfigurationCache.blend = *state->blend;
    if(state->projection)
        switch(state->projection->type)
        {
        case KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_FOV:{
            const f32 viewportWidthOverHeight = surfaceContext->swapChainImageExtent.height == 0 
                ? 1.f 
                :  KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.width)
                 / KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.height);
            surfaceContext->batchState.m4f32Projection = korl_math_m4f32_projectionFov(state->projection->subType.fov.horizontalFovDegrees
                                                                                      ,viewportWidthOverHeight
                                                                                      ,state->projection->subType.fov.clipNear
                                                                                      ,state->projection->subType.fov.clipFar);
            break;}
        case KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_ORTHOGRAPHIC:{
            const f32 left   = 0.f - state->projection->subType.orthographic.originRatioX*KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.width );
            const f32 bottom = 0.f - state->projection->subType.orthographic.originRatioY*KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.height);
            const f32 right  = KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.width ) - state->projection->subType.orthographic.originRatioX*KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.width );
            const f32 top    = KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.height) - state->projection->subType.orthographic.originRatioY*KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.height);
            const f32 far    = -state->projection->subType.orthographic.depth;
            const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
            surfaceContext->batchState.m4f32Projection = korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);
            break;}
        case KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
            const f32 viewportWidthOverHeight = surfaceContext->swapChainImageExtent.height == 0 
                ? 1.f 
                :  KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.width) 
                 / KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.height);
            /* w / fixedHeight == windowAspectRatio */
            const f32 width  = state->projection->subType.orthographic.fixedHeight * viewportWidthOverHeight;
            const f32 left   = 0.f - state->projection->subType.orthographic.originRatioX*width;
            const f32 bottom = 0.f - state->projection->subType.orthographic.originRatioY*state->projection->subType.orthographic.fixedHeight;
            const f32 right  = width       - state->projection->subType.orthographic.originRatioX*width;
            const f32 top    = state->projection->subType.orthographic.fixedHeight - state->projection->subType.orthographic.originRatioY*state->projection->subType.orthographic.fixedHeight;
            const f32 far    = -state->projection->subType.orthographic.depth;
            const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
            surfaceContext->batchState.m4f32Projection = korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);
            break;}
        default:{
            korl_log(ERROR, "invalid projection type: %i", state->projection->type);
            break;}
        }
    if(state->view)
        surfaceContext->batchState.m4f32View = korl_math_m4f32_lookAt(&state->view->positionEye, &state->view->positionTarget, &state->view->worldUpNormal);
    if(state->model)
        surfaceContext->batchState.pushConstants.m4f32Model = korl_math_makeM4f32_rotateScaleTranslate(state->model->rotation, state->model->scale, state->model->translation);
    if(state->scissor)
    {
        surfaceContext->batchState.scissor.offset = (VkOffset2D){.x     = state->scissor->x    , .y      = state->scissor->y};
        surfaceContext->batchState.scissor.extent = (VkExtent2D){.width = state->scissor->width, .height = state->scissor->height};
    }
    if(state->samplers)
        surfaceContext->batchState.texture = state->samplers->texture;
    done:
    korl_time_probeStop(set_draw_state);
}
korl_internal void korl_vulkan_draw(const Korl_Vulkan_DrawVertexData* vertexData)
{
    _Korl_Vulkan_Context*const               context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const        surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* help ensure that this code never runs outside of a set of 
        frameBegin/frameEnd calls */
    if(surfaceContext->frameStackCounter != 1)
        return;
    /* if the swap chain image context is invalid for this frame for some reason, 
        then just do nothing (this happens during deferred resize for example) */
    if(surfaceContext->frameSwapChainImageIndex == _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE)
        return;
    /* basic parameter sanity checks */
    if(vertexData->indexCount || vertexData->indices)
        korl_assert(vertexData->indexCount && vertexData->indices);
    if(vertexData->colorsStride || vertexData->colors)
        korl_assert(vertexData->colorsStride && vertexData->colors);
    if(vertexData->uvsStride || vertexData->uvs)
        korl_assert(vertexData->uvsStride && vertexData->uvs);
    korl_assert(vertexData->vertexCount);
    korl_assert(vertexData->positionDimensions == 2 || vertexData->positionDimensions == 3);
    korl_assert(vertexData->positions);
    korl_assert(vertexData->positionsStride == 2*sizeof(f32) || vertexData->positionsStride == 3*sizeof(f32));
    /* configure the pipeline config cache with the vertex data properties */
    korl_time_probeStart(draw_config_pipeline);
    switch(vertexData->primitiveType)
    {
    case KORL_VULKAN_PRIMITIVETYPE_TRIANGLES:{surfaceContext->batchState.pipelineConfigurationCache.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;}
    case KORL_VULKAN_PRIMITIVETYPE_LINES:    {surfaceContext->batchState.pipelineConfigurationCache.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;     break;}
    }
    surfaceContext->batchState.pipelineConfigurationCache.positionDimensions = vertexData->positionDimensions;
    surfaceContext->batchState.pipelineConfigurationCache.positionsStride    = vertexData->positionsStride;
    surfaceContext->batchState.pipelineConfigurationCache.colorsStride       = vertexData->colorsStride;
    surfaceContext->batchState.pipelineConfigurationCache.uvsStride          = vertexData->uvsStride;
    _korl_vulkan_setPipelineMetaData(surfaceContext->batchState.pipelineConfigurationCache);
    korl_time_probeStop(draw_config_pipeline);
    /* stage uniform data */
    korl_time_probeStart(draw_stage_descriptors);
    VkDeviceSize byteOffsetStagingBuffer = 0;
    VkBuffer     bufferStaging           = VK_NULL_HANDLE;
    _Korl_Vulkan_SwapChainImageUniformTransforms* stagingMemoryUniformTransforms = 
        _korl_vulkan_getDescriptorStagingPool(sizeof(*stagingMemoryUniformTransforms), &bufferStaging, &byteOffsetStagingBuffer);
    stagingMemoryUniformTransforms->m4f32Projection = surfaceContext->batchState.m4f32Projection;
    stagingMemoryUniformTransforms->m4f32View       = surfaceContext->batchState.m4f32View;
    korl_time_probeStop(draw_stage_descriptors);
    /* allocate & configure descriptor set(s) for this draw operation */
    korl_time_probeStart(draw_descriptor_set_alloc);
    VkDescriptorSet descriptorSetUniformTransforms = _korl_vulkan_newDescriptorSet(context->batchDescriptorSetLayout);
    KORL_ZERO_STACK_ARRAY(VkWriteDescriptorSet, descriptorSetWrites, 2);
    uint32_t descriptorWriteCount = 0;
    // write the uniform transform buffer (view-projection transforms) //
    KORL_ZERO_STACK(VkDescriptorBufferInfo, descriptorBufferInfoUniformTransforms);
    descriptorBufferInfoUniformTransforms.buffer = bufferStaging;
    descriptorBufferInfoUniformTransforms.range  = sizeof(*stagingMemoryUniformTransforms);
    descriptorBufferInfoUniformTransforms.offset = byteOffsetStagingBuffer;
    descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorSetWrites[descriptorWriteCount].dstSet          = descriptorSetUniformTransforms;
    descriptorSetWrites[descriptorWriteCount].dstBinding      = _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_UBO;
    descriptorSetWrites[descriptorWriteCount].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorSetWrites[descriptorWriteCount].descriptorCount = 1;
    descriptorSetWrites[descriptorWriteCount].pBufferInfo     = &descriptorBufferInfoUniformTransforms;
    descriptorWriteCount++;
    // conditionally write the texture sampler if we require it //
    KORL_ZERO_STACK(VkDescriptorImageInfo, descriptorImageInfo);
    if(vertexData->uvs && surfaceContext->batchState.texture)
    {
        _Korl_Vulkan_DeviceMemory_Alloctation* textureAllocation = NULL;
        _Korl_Vulkan_DeviceAsset*const deviceAsset =
             _korl_vulkan_deviceAssetDatabase_get(&(surfaceContext->deviceAssetDatabase), surfaceContext->batchState.texture, &textureAllocation);
        deviceAsset->framesSinceLastUsed = 0;// reset the frames since the asset has last been used, to prevent the asset database from destroying this data while it's potentially still being used to render
        korl_assert(textureAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
        descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        descriptorImageInfo.imageView   = textureAllocation->deviceObject.texture.imageView;
        descriptorImageInfo.sampler     = textureAllocation->deviceObject.texture.sampler;
        korl_assert(descriptorWriteCount < korl_arraySize(descriptorSetWrites));
        descriptorSetWrites[descriptorWriteCount].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorSetWrites[descriptorWriteCount].dstSet          = descriptorSetUniformTransforms;
        descriptorSetWrites[descriptorWriteCount].dstBinding      = _KORL_VULKAN_BATCH_DESCRIPTORSET_BINDING_TEXTURE;
        descriptorSetWrites[descriptorWriteCount].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorSetWrites[descriptorWriteCount].descriptorCount = 1;
        descriptorSetWrites[descriptorWriteCount].pImageInfo      = &descriptorImageInfo;
        descriptorWriteCount++;
    }
    vkUpdateDescriptorSets(context->device, descriptorWriteCount, descriptorSetWrites, 0/*copyCount*/, NULL/*copies*/);
    korl_time_probeStop(draw_descriptor_set_alloc);
    /* stage the vertex index/attribute data */
    ///@TODO: we could potentially get more performance here by transferring this data to device-local memory
    korl_time_probeStart(draw_stage_vertices);
    byteOffsetStagingBuffer = 0;
    bufferStaging           = VK_NULL_HANDLE;
    u8* vertexStagingMemory = _korl_vulkan_getVertexStagingPool(vertexData, &bufferStaging, &byteOffsetStagingBuffer);
    VkDeviceSize byteOffsetStagingBufferIndices   = 0
               , byteOffsetStagingBufferPositions = 0
               , byteOffsetStagingBufferColors    = 0
               , byteOffsetStagingBufferUvs       = 0;
    ///@TODO: if we allow interleaved vertex data, we can copy _everything_ to the staging buffer in a single memcopy operation!
    if(vertexData->indices)
    {
        const u$ stageDataBytes = vertexData->indexCount*sizeof(*vertexData->indices);
        // we can safely assume all vertex index data is never interleaved with anything for now:
        korl_memory_copy(vertexStagingMemory, vertexData->indices, stageDataBytes);
        byteOffsetStagingBufferIndices = byteOffsetStagingBuffer;
        vertexStagingMemory     += stageDataBytes;
        byteOffsetStagingBuffer += stageDataBytes;
    }
    {
        const u$ positionSize   = vertexData->positionDimensions*sizeof(*vertexData->positions);
        const u$ stageDataBytes = vertexData->vertexCount*positionSize;
        korl_assert(vertexData->positionsStride == positionSize);// for now, interleaved vertex data is not supported
        korl_memory_copy(vertexStagingMemory, vertexData->positions, stageDataBytes);
        byteOffsetStagingBufferPositions = byteOffsetStagingBuffer;
        vertexStagingMemory     += stageDataBytes;
        byteOffsetStagingBuffer += stageDataBytes;
    }
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
    korl_time_probeStop(draw_stage_vertices);
    /* compose the draw commands */
    korl_assert(surfaceContext->batchState.currentPipeline < arrlenu(context->stbDaPipelines) && arrlenu(context->stbDaPipelines) > 0);//try to make sure we have selected a valid pipeline before going further
    korl_time_probeStart(draw_compose_gfx_commands);
    vkCmdBindPipeline(swapChainImageContext->commandBufferGraphics
                     ,VK_PIPELINE_BIND_POINT_GRAPHICS
                     ,context->stbDaPipelines[surfaceContext->batchState.currentPipeline].pipeline);
    VkBuffer batchVertexBuffers[] = 
        { bufferStaging
        , bufferStaging
        , bufferStaging };
    const VkDeviceSize batchVertexBufferOffsets[] = 
        { byteOffsetStagingBufferPositions
        , byteOffsetStagingBufferColors
        , byteOffsetStagingBufferUvs };
    korl_assert(korl_arraySize(batchVertexBuffers) == korl_arraySize(batchVertexBufferOffsets));
    vkCmdBindVertexBuffers(swapChainImageContext->commandBufferGraphics
                          ,0/*first binding*/, korl_arraySize(batchVertexBuffers)
                          ,batchVertexBuffers, batchVertexBufferOffsets);
    //KORL-PERFORMANCE-000-000-013: speed: only bind descriptor sets when needed
    vkCmdBindDescriptorSets(swapChainImageContext->commandBufferGraphics
                           ,VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipelineLayout
                           ,0/*first set*/, 1/*set count */
                           ,&descriptorSetUniformTransforms
                           ,/*dynamic offset count*/0, /*pDynamicOffsets*/NULL);
    vkCmdPushConstants(swapChainImageContext->commandBufferGraphics, context->pipelineLayout
                      ,VK_SHADER_STAGE_VERTEX_BIT
                      ,/*offset*/0, sizeof(surfaceContext->batchState.pushConstants)
                      ,&surfaceContext->batchState.pushConstants);
    vkCmdSetScissor(swapChainImageContext->commandBufferGraphics
                   ,0/*firstScissor*/, 1/*scissorCount*/, &surfaceContext->batchState.scissor);
    if(vertexData->indices)
    {
        vkCmdBindIndexBuffer(swapChainImageContext->commandBufferGraphics
                            ,bufferStaging, byteOffsetStagingBufferIndices
                            ,_korl_vulkan_vertexIndexType());
        vkCmdDrawIndexed(swapChainImageContext->commandBufferGraphics
                        ,vertexData->indexCount
                        ,/*instance count*/1, /*firstIndex*/0, /*vertexOffset*/0
                        ,/*firstInstance*/0);
    }
    else
        vkCmdDraw(swapChainImageContext->commandBufferGraphics
                 ,vertexData->vertexCount
                 ,/*instance count*/1, /*firstIndex*/0, /*firstInstance*/0);
    korl_time_probeStop(draw_compose_gfx_commands);
}
korl_internal void korl_vulkan_useImageAssetAsTexture(const wchar_t* assetName)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* help ensure that this code never runs outside of a set of 
        frameBegin/frameEnd calls */
    if(surfaceContext->frameStackCounter != 1)
        return;
    /* if the swap chain image context is invalid for this frame for some reason, 
        then just do nothing (this happens during deferred resize for example) */
    if(surfaceContext->frameSwapChainImageIndex == _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE)
        return;
#if 0///@TODO: delete/recycle
    /* check and see if the asset is already loaded as a device texture */
    u$ deviceAssetIndexLoaded = 0;
    for(; deviceAssetIndexLoaded < KORL_MEMORY_POOL_SIZE(context->deviceAssets); ++deviceAssetIndexLoaded)
    {
        if(context->deviceAssets[deviceAssetIndexLoaded].type != _KORL_VULKAN_DEVICEASSET_TYPE_ASSET_TEXTURE)
            continue;
        if(korl_stringPool_equalsUtf16(context->deviceAssets[deviceAssetIndexLoaded].subType.assetTexture.name, 
                                       assetName))
            break;
    }
    /* if it is, select this texture for later use and return */
    if(deviceAssetIndexLoaded < KORL_MEMORY_POOL_SIZE(context->deviceAssets))
        goto done_conditionallySelectLoadedAsset;
    /* request the image asset from the asset manager; if the asset isn't loaded 
        we can stop here */
    KORL_ZERO_STACK(Korl_AssetCache_AssetData, assetData);
    if(KORL_ASSETCACHE_GET_RESULT_LOADED != korl_assetCache_get(assetName, KORL_ASSETCACHE_GET_FLAG_LAZY, &assetData))
    {
        /* locate the internal "default texture" asset so we can actually render something; 
            not doing so would result in invalid rendering operation (validation layer error) */
        deviceAssetIndexLoaded = 0;
        for(; deviceAssetIndexLoaded < KORL_MEMORY_POOL_SIZE(context->deviceAssets); ++deviceAssetIndexLoaded)
        {
            if(context->deviceAssets[deviceAssetIndexLoaded].type != _KORL_VULKAN_DEVICEASSET_TYPE_TEXTURE)
                continue;
            if(context->deviceAssets[deviceAssetIndexLoaded].subType.texture.handle == context->textureHandleDefaultTexture)
                break;
        }
        korl_assert(deviceAssetIndexLoaded < KORL_MEMORY_POOL_SIZE(context->deviceAssets));
        goto done_conditionallySelectLoadedAsset;
    }
    /* decode the raw image data from the asset */
    int imageSizeX = 0, imageSizeY = 0, imageChannels = 0;
    stbi_uc*const imagePixels = stbi_load_from_memory(assetData.data, assetData.dataBytes, &imageSizeX, &imageSizeY, &imageChannels, STBI_rgb_alpha);
    if(!imagePixels)
    {
        korl_log(ERROR, "stbi_load_from_memory failed! (%ls)", assetName);
        /* locate the internal "default texture" asset so we can actually render something; 
            not doing so would result in invalid rendering operation (validation layer error) */
        deviceAssetIndexLoaded = 0;
        for(; deviceAssetIndexLoaded < KORL_MEMORY_POOL_SIZE(context->deviceAssets); ++deviceAssetIndexLoaded)
        {
            if(context->deviceAssets[deviceAssetIndexLoaded].type != _KORL_VULKAN_DEVICEASSET_TYPE_TEXTURE)
                continue;
            if(context->deviceAssets[deviceAssetIndexLoaded].subType.texture.handle == context->textureHandleDefaultTexture)
                break;
        }
        korl_assert(deviceAssetIndexLoaded < KORL_MEMORY_POOL_SIZE(context->deviceAssets));
        goto done_conditionallySelectLoadedAsset;
    }
    /* allocate a device-local image object for the texture */
    _Korl_Vulkan_DeviceMemory_Alloctation* deviceImage = 
        _korl_vulkan_deviceMemoryLinear_allocateTexture(
            &context->deviceMemoryLinearAssets, imageSizeX, imageSizeY, 
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    /* transfer the staging image buffer to the device-local texture object */
    _korl_vulkan_transferImageBufferToTexture(imagePixels, imageSizeX, imageSizeY, deviceImage);
    /* free the raw image data */
    stbi_image_free(imagePixels);
    /* add the device asset to the `deviceAssets` database */
    _Korl_Vulkan_DeviceAsset*const asset = KORL_MEMORY_POOL_ADD(context->deviceAssets);
    asset->type                                  = _KORL_VULKAN_DEVICEASSET_TYPE_ASSET_TEXTURE;
    asset->subType.assetTexture.deviceAllocation = deviceImage;
    asset->subType.assetTexture.name             = korl_stringNewUtf16(&context->stringPool, assetName);
    korl_assert(asset->subType.assetTexture.name.handle);
    deviceAssetIndexLoaded = KORL_MEMORY_POOL_SIZE(context->deviceAssets) - 1;
done_conditionallySelectLoadedAsset:
    /* if we do not have a valid index for a loaded device asset, just do nothing */
    if(deviceAssetIndexLoaded >= KORL_MEMORY_POOL_SIZE(context->deviceAssets))
        return;
    /* handle the TEXTURE device asset type, in case we couldn't get an 
        ASSET_TEXTURE for some reason (not loaded yet, etc...) */
    if(context->deviceAssets[deviceAssetIndexLoaded].type == _KORL_VULKAN_DEVICEASSET_TYPE_TEXTURE)
    {
        korl_assert(context->deviceAssets[deviceAssetIndexLoaded].subType.texture.deviceAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
        _korl_vulkan_selectTexture(context->deviceAssets[deviceAssetIndexLoaded].subType.texture.deviceAllocation->deviceObject.texture.imageView, 
                                   context->deviceAssets[deviceAssetIndexLoaded].subType.texture.deviceAllocation->deviceObject.texture.sampler);
        return;
    }
    /* we have a valid ASSET_TEXTURE device asset, so we can just select it */
    korl_assert(context->deviceAssets[deviceAssetIndexLoaded].type == _KORL_VULKAN_DEVICEASSET_TYPE_ASSET_TEXTURE);
    korl_assert(context->deviceAssets[deviceAssetIndexLoaded].subType.assetTexture.deviceAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
    _korl_vulkan_selectTexture(context->deviceAssets[deviceAssetIndexLoaded].subType.assetTexture.deviceAllocation->deviceObject.texture.imageView, 
                               context->deviceAssets[deviceAssetIndexLoaded].subType.assetTexture.deviceAllocation->deviceObject.texture.sampler);
#endif
}
korl_internal KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(korl_vulkan_onAssetHotReload)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
#if 0///@TODO: delete/recycle
    /* check to see if the asset is loaded in our database */
    _Korl_Vulkan_DeviceAsset* deviceAsset = NULL;
    for(u$ dai = 0; dai < KORL_MEMORY_POOL_SIZE(context->deviceAssets); ++dai)
    {
        Korl_StringPool_String stringDeviceAssetName;
        stringDeviceAssetName.handle = 0;

        switch(context->deviceAssets[dai].type)
        {
        case _KORL_VULKAN_DEVICEASSET_TYPE_ASSET_TEXTURE:{
            stringDeviceAssetName = context->deviceAssets[dai].subType.assetTexture.name;
            break;}
        case _KORL_VULKAN_DEVICEASSET_TYPE_TEXTURE:{
            break;}
        }
        if(0 == stringDeviceAssetName.handle)
            continue;// device asset type not supported
        if(korl_stringPool_equalsUtf16(stringDeviceAssetName, rawUtf16AssetName))
        {
            deviceAsset = &(context->deviceAssets[dai]);
            break;
        }
    }
    if(!deviceAsset)
        /* if the asset name isn't found in the device asset database, then we 
            don't have to do anything */
        return;
    /* perform asset hot-reloading logic */
    switch(deviceAsset->type)
    {
    case _KORL_VULKAN_DEVICEASSET_TYPE_ASSET_TEXTURE:{
        _KORL_VULKAN_CHECK(vkDeviceWaitIdle(context->device));
        _korl_vulkan_deviceMemoryLinear_free(&context->deviceMemoryLinearAssets, deviceAsset->subType.assetTexture.deviceAllocation);
        int imageSizeX = 0, imageSizeY = 0, imageChannels = 0;
        stbi_uc*const imagePixels = stbi_load_from_memory(assetData.data, assetData.dataBytes, &imageSizeX, &imageSizeY, &imageChannels, STBI_rgb_alpha);
        if(!imagePixels)
            korl_log(ERROR, "stbi_load_from_memory failed! assetName=\"%ws\"", rawUtf16AssetName);
        _Korl_Vulkan_DeviceMemory_Alloctation*const deviceImage = 
            _korl_vulkan_deviceMemoryLinear_allocateTexture(
                &context->deviceMemoryLinearAssets, imageSizeX, imageSizeY, 
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        _korl_vulkan_transferImageBufferToTexture(imagePixels, imageSizeX, imageSizeY, deviceImage);
        stbi_image_free(imagePixels);
        deviceAsset->subType.assetTexture.deviceAllocation = deviceImage;
        break;}
    default:{
        korl_log(ERROR, "device asset type %i not implemented", deviceAsset->type);
        break;}
    }
#endif
}
korl_internal Korl_Vulkan_DeviceAssetHandle korl_vulkan_deviceAsset_createTexture(u32 sizeX, u32 sizeY)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle = 
        _korl_vulkan_deviceMemory_allocateTexture(surfaceContext->deviceAssetDatabase.deviceMemoryAllocator
                                                 ,sizeX, sizeY
                                                 ,  VK_IMAGE_USAGE_TRANSFER_DST_BIT 
                                                  | VK_IMAGE_USAGE_SAMPLED_BIT);
    return _korl_vulkan_deviceAssetDatabase_add(&(surfaceContext->deviceAssetDatabase), allocationHandle);
}
korl_internal void korl_vulkan_deviceAsset_destroy(Korl_Vulkan_DeviceAssetHandle deviceAssetHandle)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    _korl_vulkan_deviceAssetDatabase_get(&(surfaceContext->deviceAssetDatabase), deviceAssetHandle, NULL/*out_deviceMemoryAllocation*/)->nullify = true;
}
korl_internal void korl_vulkan_deviceAsset_updateTexture(Korl_Vulkan_DeviceAssetHandle textureHandle, const Korl_Vulkan_Color4u8* pixelData)
{
    _Korl_Vulkan_SurfaceContext*const        surfaceContext        = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /// @TODO: what happens if this occurs with an "invalid" swap chain image context??? (minimized window); perhaps we need to be able to guarantee that at least transfer commands will be submitted each frame???
    _Korl_Vulkan_DeviceMemory_Alloctation* deviceMemoryAllocation = NULL;
    _Korl_Vulkan_DeviceAsset*const deviceAsset = _korl_vulkan_deviceAssetDatabase_get(&(surfaceContext->deviceAssetDatabase), textureHandle, &deviceMemoryAllocation);
    korl_assert(deviceMemoryAllocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE);
    /* allocate staging buffer space to store the pixel data, since we can only 
        transfer from staging => device memory */
    const VkDeviceSize imageBytes = deviceMemoryAllocation->deviceObject.texture.sizeX
                                  * deviceMemoryAllocation->deviceObject.texture.sizeY
                                  * sizeof(*pixelData);
    VkBuffer bufferStaging;
    VkDeviceSize bufferStagingOffset;
    void*const pixelDataStagingMemory = _korl_vulkan_getStagingPool(imageBytes, 0/*alignment*/, &bufferStaging, &bufferStagingOffset);
    /* copy the pixel data to the staging buffer */
    korl_memory_copy(pixelDataStagingMemory, pixelData, imageBytes);
    /* compose the commands to transfer the staging buffer pixel data to the 
        device-local memory occupied by the device asset */
    // transition the image layout from undefined => transfer_destination_optimal //
    KORL_ZERO_STACK(VkImageMemoryBarrier, barrierImageMemory);
    barrierImageMemory.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierImageMemory.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierImageMemory.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierImageMemory.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrierImageMemory.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrierImageMemory.image                           = deviceMemoryAllocation->deviceObject.texture.image;
    barrierImageMemory.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierImageMemory.subresourceRange.levelCount     = 1;
    barrierImageMemory.subresourceRange.layerCount     = 1;
    barrierImageMemory.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    #if 0// already defaulted to 0
    barrierImageMemory.subresourceRange.baseMipLevel   = 0;
    barrierImageMemory.subresourceRange.baseArrayLayer = 0;
    barrierImageMemory.srcAccessMask                   = 0;
    #endif
    vkCmdPipelineBarrier(swapChainImageContext->commandBufferTransfer
                        ,/*srcStageMask*/VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                        ,/*dstStageMask*/VK_PIPELINE_STAGE_TRANSFER_BIT
                        ,/*dependencyFlags*/0
                        ,/*memoryBarrierCount*/0, /*pMemoryBarriers*/NULL
                        ,/*bufferBarrierCount*/0, /*bufferBarriers*/NULL
                        ,/*imageBarrierCount*/1, &barrierImageMemory);
    // copy the buffer from staging buffer => device-local image //
    KORL_ZERO_STACK(VkBufferImageCopy, imageCopyRegion);
    // default zero values indicate no buffer padding, copy to image origin, copy to base mip level & array layer
    imageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.imageSubresource.layerCount = 1;
    imageCopyRegion.imageExtent.width           = deviceMemoryAllocation->deviceObject.texture.sizeX;
    imageCopyRegion.imageExtent.height          = deviceMemoryAllocation->deviceObject.texture.sizeY;
    imageCopyRegion.imageExtent.depth           = 1;
    vkCmdCopyBufferToImage(swapChainImageContext->commandBufferTransfer
                          ,bufferStaging, deviceMemoryAllocation->deviceObject.texture.image
                          ,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /*regionCount*/1, &imageCopyRegion);
    // transition the image layout from tranfer_destination_optimal => shader_read_only_optimal //
    // we can recycle the VkImageMemoryBarrier from the first transition here:  
    barrierImageMemory.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierImageMemory.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierImageMemory.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierImageMemory.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(swapChainImageContext->commandBufferTransfer
                        ,/*srcStageMask*/VK_PIPELINE_STAGE_TRANSFER_BIT
                        ,/*dstStageMask*/VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                        ,/*dependencyFlags*/0
                        ,/*memoryBarrierCount*/0, /*pMemoryBarriers*/NULL
                        ,/*bufferBarrierCount*/0, /*bufferBarriers*/NULL
                        ,/*imageBarrierCount*/1, &barrierImageMemory);
    ///@TODO: even though this technique _should_ make sure that the upcoming frame will have the correct pixel data, we are still not properly isolating texture memory from frames that are still WIP
}
