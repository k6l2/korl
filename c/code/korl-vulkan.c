#include "korl-vulkan.h"
#include "korl-io.h"
#include "korl-assert.h"
#include "korl-assetCache.h"
#include "korl-vulkan-common.h"
#include "korl-stb-image.h"
#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
#endif// defined(_WIN32)
korl_global_const char* G_KORL_VULKAN_ENABLED_LAYERS[] = 
    { "VK_LAYER_KHRONOS_validation" };
korl_global_const char* G_KORL_VULKAN_DEVICE_EXTENSIONS[] = 
    { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
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
        korl_log(ERROR  , "{%s} %s", messageTypeString, pCallbackData->pMessage);
    else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        korl_log(WARNING, "{%s} %s", messageTypeString, pCallbackData->pMessage);
    else
        korl_log(INFO   , "{%s} %s", messageTypeString, pCallbackData->pMessage);
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
    const VkPhysicalDeviceProperties*const propertiesNew/*, 
    const VkPhysicalDeviceFeatures*const featuresOld, 
    const VkPhysicalDeviceFeatures*const featuresNew*/)
{
    korl_assert(deviceNew != VK_NULL_HANDLE);
    /* don't even consider devices that don't support all the extensions we 
        need */
    VkExtensionProperties extensionProperties[256];
    KORL_ZERO_STACK(u32, extensionPropertiesSize);
    _KORL_VULKAN_CHECK(
        vkEnumerateDeviceExtensionProperties(
            deviceNew, NULL/*pLayerName*/, &extensionPropertiesSize, 
            NULL/*pProperties*/));
    korl_assert(extensionPropertiesSize <= korl_arraySize(extensionProperties));
    extensionPropertiesSize = korl_arraySize(extensionProperties);
    _KORL_VULKAN_CHECK(
        vkEnumerateDeviceExtensionProperties(
            deviceNew, NULL/*pLayerName*/, &extensionPropertiesSize, 
            extensionProperties));
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
    /* if the old device hasn't been selected, just use the new device */
    if(deviceOld == VK_NULL_HANDLE)
        return true;
    /* always attempt to use a discrete GPU first */
    if(    propertiesNew->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && propertiesOld->deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return true;
    return false;
}
korl_internal _Korl_Vulkan_DeviceSurfaceMetaData _korl_vulkan_deviceSurfaceMetaData(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceSurfaceMetaData, result);
    /* query for device-surface capabilities */
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice, surface, &result.capabilities));
    /* query for device-surface formats */
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &result.formatsSize, 
            NULL/*pSurfaceFormats*/));
    korl_assert(result.formatsSize <= korl_arraySize(result.formats));
    result.formatsSize = korl_arraySize(result.formats);
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &result.formatsSize, result.formats));
    /* query for device-surface present modes */
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surface, &result.presentModesSize, 
            NULL/*pPresentModes*/));
    korl_assert(result.presentModesSize <= korl_arraySize(result.presentModes));
    result.presentModesSize = korl_arraySize(result.presentModes);
    _KORL_VULKAN_CHECK(
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surface, &result.presentModesSize, 
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
korl_internal void _korl_vulkan_createSwapChain(
    u32 sizeX, u32 sizeY, 
    const _Korl_Vulkan_DeviceSurfaceMetaData*const deviceSurfaceMetaData)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* given what we know about the device & the surface, select the best 
        settings for the swap chain */
    korl_assert(deviceSurfaceMetaData->formatsSize > 0);
    surfaceContext->swapChainSurfaceFormat = deviceSurfaceMetaData->formats[0];
    for(u32 f = 0; f < deviceSurfaceMetaData->formatsSize; f++)
        if(    deviceSurfaceMetaData->formats[f].format == VK_FORMAT_B8G8R8A8_SRGB
            && deviceSurfaceMetaData->formats[f].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surfaceContext->swapChainSurfaceFormat = deviceSurfaceMetaData->formats[f];
            break;
        }
    // the only present mode REQUIRED by the spec to be supported
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
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
    /* create the swap chain */
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
    if(context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics != 
        context->queueFamilyMetaData.indexQueueUnion.indexQueues.present)
    {
        createInfoSwapChain.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfoSwapChain.queueFamilyIndexCount = 2;
        createInfoSwapChain.pQueueFamilyIndices   = context->queueFamilyMetaData.indexQueueUnion.indices;
    }
    _KORL_VULKAN_CHECK(
        vkCreateSwapchainKHR(
            context->device, &createInfoSwapChain, 
            context->allocator, &surfaceContext->swapChain));
    /* create render pass */
    KORL_ZERO_STACK(VkAttachmentDescription, colorAttachment);
    colorAttachment.format         = surfaceContext->swapChainSurfaceFormat.format;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    KORL_ZERO_STACK(VkAttachmentReference, attachmentReference);
    attachmentReference.attachment = 0;
    attachmentReference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    KORL_ZERO_STACK(VkSubpassDescription, subPass);
    subPass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPass.colorAttachmentCount = 1;
    subPass.pColorAttachments    = &attachmentReference;
    KORL_ZERO_STACK(VkSubpassDependency, subpassDependency);
    subpassDependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass    = 0;
    subpassDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    KORL_ZERO_STACK(VkRenderPassCreateInfo, createInfoRenderPass);
    createInfoRenderPass.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfoRenderPass.attachmentCount = 1;
    createInfoRenderPass.pAttachments    = &colorAttachment;
    createInfoRenderPass.subpassCount    = 1;
    createInfoRenderPass.pSubpasses      = &subPass;
    createInfoRenderPass.dependencyCount = 1;
    createInfoRenderPass.pDependencies   = &subpassDependency;
    _KORL_VULKAN_CHECK(
        vkCreateRenderPass(
            context->device, &createInfoRenderPass, context->allocator, 
            &context->renderPass));
    /* get swap chain images */
    _KORL_VULKAN_CHECK(
        vkGetSwapchainImagesKHR(
            context->device, surfaceContext->swapChain, 
            &surfaceContext->swapChainImagesSize, NULL/*pSwapchainImages*/));
    korl_assert(surfaceContext->swapChainImagesSize <= 
        korl_arraySize(surfaceContext->swapChainImages));
    surfaceContext->swapChainImagesSize = 
        korl_arraySize(surfaceContext->swapChainImages);
    _KORL_VULKAN_CHECK(
        vkGetSwapchainImagesKHR(
            context->device, surfaceContext->swapChain, 
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
            vkCreateImageView(
                context->device, &createInfoImageView, context->allocator, 
                &surfaceContext->swapChainImageContexts[i].imageView));
        /* create a frame buffer for all the swap chain image views */
        VkImageView frameBufferAttachments[] = 
            { surfaceContext->swapChainImageContexts[i].imageView };
        KORL_ZERO_STACK(VkFramebufferCreateInfo, createInfoFrameBuffer);
        createInfoFrameBuffer.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfoFrameBuffer.renderPass      = context->renderPass;
        createInfoFrameBuffer.attachmentCount = korl_arraySize(frameBufferAttachments);
        createInfoFrameBuffer.pAttachments    = frameBufferAttachments;
        createInfoFrameBuffer.width           = surfaceContext->swapChainImageExtent.width;
        createInfoFrameBuffer.height          = surfaceContext->swapChainImageExtent.height;
        createInfoFrameBuffer.layers          = 1;
        _KORL_VULKAN_CHECK(
            vkCreateFramebuffer(
                context->device, &createInfoFrameBuffer, context->allocator, 
                &surfaceContext->swapChainImageContexts[i].frameBuffer));
        /* initialize the swap chain fence references to nothing */
        surfaceContext->swapChainImageContexts[i].fence = VK_NULL_HANDLE;
    }
    /* allocate a command buffer for each of the swap chain frame buffers */
    KORL_ZERO_STACK(VkCommandBufferAllocateInfo, allocateInfoCommandBuffers);
    allocateInfoCommandBuffers.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfoCommandBuffers.commandPool        = context->commandPoolGraphics;
    allocateInfoCommandBuffers.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfoCommandBuffers.commandBufferCount = surfaceContext->swapChainImagesSize;
    _KORL_VULKAN_CHECK(
        vkAllocateCommandBuffers(
            context->device, &allocateInfoCommandBuffers, 
            surfaceContext->swapChainCommandBuffers));
}
korl_internal void _korl_vulkan_destroySwapChainDependencies(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    _KORL_VULKAN_CHECK(vkDeviceWaitIdle(context->device));
    vkDestroyRenderPass(context->device, context->renderPass, context->allocator);
}
korl_internal void _korl_vulkan_destroySwapChain(void)
{
    _korl_vulkan_destroySwapChainDependencies();
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    vkFreeCommandBuffers(
        context->device, context->commandPoolGraphics, 
        surfaceContext->swapChainImagesSize, 
        surfaceContext->swapChainCommandBuffers);
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        vkDestroyFramebuffer(
            context->device, surfaceContext->swapChainImageContexts[i].frameBuffer, 
            context->allocator);
        vkDestroyImageView(
            context->device, surfaceContext->swapChainImageContexts[i].imageView, 
            context->allocator);
    }
    vkDestroySwapchainKHR(
        context->device, surfaceContext->swapChain, context->allocator);
}
/**
 * move all the vertices in the batch buffer to the vulkan device
 */
korl_internal void _korl_vulkan_flushBatchStaging(void)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* don't do any work if there's no work to do! */
    if(    swapChainImageContext->batchVertexCountStaging      <= 0
        && swapChainImageContext->batchVertexIndexCountStaging <= 0)
        return;
    /* make sure that we don't overflow the device memory! */
    korl_assert(
          swapChainImageContext->batchVertexCountStaging 
        + swapChainImageContext->batchVertexCountDevice 
            <= _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE);
    korl_assert(
          swapChainImageContext->batchVertexIndexCountStaging
        + swapChainImageContext->batchVertexIndexCountDevice
            <= _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_DEVICE);
    /* record commands to transfer the data from staging to device-local memory */
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
    if(swapChainImageContext->batchVertexIndexCountStaging > 0)
    {
        KORL_ZERO_STACK(VkBufferCopy, bufferCopyIndices);
        bufferCopyIndices.srcOffset = 0;
        bufferCopyIndices.dstOffset = swapChainImageContext->batchVertexIndexCountDevice *sizeof(Korl_Vulkan_VertexIndex);
        bufferCopyIndices.size      = swapChainImageContext->batchVertexIndexCountStaging*sizeof(Korl_Vulkan_VertexIndex);
        vkCmdCopyBuffer(
            commandBuffer, 
            swapChainImageContext->bufferStagingBatchIndices->deviceObject.buffer, 
            swapChainImageContext->bufferDeviceBatchIndices->deviceObject.buffer, 
            1, &bufferCopyIndices);
    }
    if(swapChainImageContext->batchVertexCountStaging > 0)
    {
        KORL_ZERO_STACK(VkBufferCopy, bufferCopyPositions);
        bufferCopyPositions.srcOffset = 0;
        bufferCopyPositions.dstOffset = swapChainImageContext->batchVertexCountDevice *sizeof(Korl_Vulkan_Position);
        bufferCopyPositions.size      = swapChainImageContext->batchVertexCountStaging*sizeof(Korl_Vulkan_Position);
        vkCmdCopyBuffer(
            commandBuffer, 
            swapChainImageContext->bufferStagingBatchPositions->deviceObject.buffer, 
            swapChainImageContext->bufferDeviceBatchPositions->deviceObject.buffer, 
            1, &bufferCopyPositions);
        KORL_ZERO_STACK(VkBufferCopy, bufferCopyColors);
        bufferCopyColors.srcOffset = 0;
        bufferCopyColors.dstOffset = swapChainImageContext->batchVertexCountDevice *sizeof(Korl_Vulkan_Color);
        bufferCopyColors.size      = swapChainImageContext->batchVertexCountStaging*sizeof(Korl_Vulkan_Color);
        vkCmdCopyBuffer(
            commandBuffer, 
            swapChainImageContext->bufferStagingBatchColors->deviceObject.buffer, 
            swapChainImageContext->bufferDeviceBatchColors->deviceObject.buffer, 
            1, &bufferCopyColors);
    }
    _KORL_VULKAN_CHECK(vkEndCommandBuffer(commandBuffer));
    /* submit the memory transfer commands to the device */
    KORL_ZERO_STACK(VkSubmitInfo, queueSubmitInfo);
    queueSubmitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    queueSubmitInfo.commandBufferCount = 1;
    queueSubmitInfo.pCommandBuffers    = &commandBuffer;
    _KORL_VULKAN_CHECK(vkQueueSubmit(context->queueGraphics, 1, &queueSubmitInfo, VK_NULL_HANDLE/*fence*/));
    /** @bandwidth: this is very heavy handed!  It would be better if this 
     * process could happen in the background while we wait on a fence or 
     * something so that the graphicsQueue can keep going unimpeded and program 
     * execution can continue batching more vertices. */
    _KORL_VULKAN_CHECK(vkQueueWaitIdle(context->queueGraphics));
    /* release the command buffer memory back to the pool now that we're done */
    vkFreeCommandBuffers(context->device, context->commandPoolTransfer, 1, &commandBuffer);
    /* finish up; update our book-keeping */
    swapChainImageContext->batchVertexIndexCountDevice += swapChainImageContext->batchVertexIndexCountStaging;
    swapChainImageContext->batchVertexCountDevice      += swapChainImageContext->batchVertexCountStaging;
    swapChainImageContext->batchVertexIndexCountStaging = 0;
    swapChainImageContext->batchVertexCountStaging      = 0;
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

korl_internal void _korl_vulkan_deviceMemory_allocation_destroy(_Korl_Vulkan_DeviceMemory_Alloctation*const allocation)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    switch(allocation->type)
    {
    case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED:{
        // just do nothing - the allocation is already destroyed
        } break;
    case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER:{
        vkDestroyBuffer(context->device, allocation->deviceObject.buffer, context->allocator);
        } break;
    }
    korl_memory_nullify(allocation, sizeof(*allocation));
}
korl_internal void _korl_vulkan_deviceMemoryLinear_create(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, 
    VkMemoryPropertyFlagBits memoryPropertyFlags, 
    VkBufferUsageFlags bufferUsageFlags, 
    VkDeviceSize bytes)
{
    /* sanity check - ensure the deviceMemoryLinear is already nullified */
    korl_assert(korl_memory_isNull(deviceMemoryLinear, sizeof(*deviceMemoryLinear)));
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /* Create a dummy buffer using the buffer usage flags.  According to sources 
        I've read on this subject, this should allow us to allocate device 
        memory that can accomodate buffers made with any subset of these usage 
        flags in the future.  This Vulkan API idiom is, in my opinion, very 
        obtuse/unintuitive, but whatever!  
        Source: https://stackoverflow.com/a/55456540 */
    VkBuffer dummyBuffer;
    KORL_ZERO_STACK(VkBufferCreateInfo, bufferCreateInfo);
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size  = 1;
    bufferCreateInfo.usage = bufferUsageFlags;
    _KORL_VULKAN_CHECK(
        vkCreateBuffer(
            context->device, &bufferCreateInfo, context->allocator, &dummyBuffer));
    /* query the dummy device objects for their memory requirements */
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirementsDummyBuffer);
    vkGetBufferMemoryRequirements(context->device, dummyBuffer, &memoryRequirementsDummyBuffer);
    /* now we can create the device memory used in the allocator */
    KORL_ZERO_STACK(VkMemoryAllocateInfo, allocateInfo);
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize  = bytes;
    allocateInfo.memoryTypeIndex = _korl_vulkan_findMemoryType(memoryRequirementsDummyBuffer.memoryTypeBits, memoryPropertyFlags);
    _KORL_VULKAN_CHECK(
        vkAllocateMemory(
            context->device, &allocateInfo, context->allocator, &deviceMemoryLinear->deviceMemory));
    /* clean up dummy device objects */
    vkDestroyBuffer(context->device, dummyBuffer, context->allocator);
    /* initialize the rest of the allocator */
    deviceMemoryLinear->byteSize            = bytes;
    deviceMemoryLinear->memoryPropertyFlags = memoryPropertyFlags;
    deviceMemoryLinear->memoryTypeBits      = memoryRequirementsDummyBuffer.memoryTypeBits;
}
korl_internal void _korl_vulkan_deviceMemoryLinear_destroy(_Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    for(unsigned a = 0; a < KORL_MEMORY_POOL_SIZE(deviceMemoryLinear->allocations); a++)
        _korl_vulkan_deviceMemory_allocation_destroy(&deviceMemoryLinear->allocations[a]);
    vkFreeMemory(context->device, deviceMemoryLinear->deviceMemory, context->allocator);
    korl_memory_nullify(deviceMemoryLinear, sizeof(*deviceMemoryLinear));
}
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemoryLinear_allocateBuffer(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, VkDeviceSize bytes, 
    VkBufferUsageFlags bufferUsageFlags, VkSharingMode sharingMode)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    korl_assert(!KORL_MEMORY_POOL_FULL(deviceMemoryLinear->allocations));
    _Korl_Vulkan_DeviceMemory_Alloctation* result = KORL_MEMORY_POOL_ADD(deviceMemoryLinear->allocations);
    result->type = _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER;
    /* create the buffer with the given parameters */
    KORL_ZERO_STACK(VkBufferCreateInfo, createInfoBuffer);
    createInfoBuffer.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfoBuffer.size        = bytes;
    createInfoBuffer.usage       = bufferUsageFlags;
    createInfoBuffer.sharingMode = sharingMode;
    _KORL_VULKAN_CHECK(
        vkCreateBuffer(
            context->device, &createInfoBuffer, context->allocator, 
            &result->deviceObject.buffer));
    /* obtain the device memory requirements for the buffer */
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirements);
    vkGetBufferMemoryRequirements(context->device, result->deviceObject.buffer, &memoryRequirements);
    /* bind the buffer to the device memory, making sure to obey the device 
        memory requirements 
        For some more details, see: https://stackoverflow.com/a/45459196 */
    /* @robustness: ensure that device objects respect `bufferImageGranularity`, 
                    obtained from `VkPhysicalDeviceLimits`, if we ever have NON-
                    LINEAR device objects in the same allocation (in addition to 
                    respecting memory alignment requirements, of course).  */
    korl_assert(memoryRequirements.alignment > 0);
    const VkDeviceSize alignedOffset = 
        (deviceMemoryLinear->bytesAllocated + (memoryRequirements.alignment - 1)) & ~(memoryRequirements.alignment - 1);
    // ensure the allocation can fit in the allocator!  Maybe handle this gracefully in the future?
    korl_assert(alignedOffset + memoryRequirements.size <= deviceMemoryLinear->byteSize);
    _KORL_VULKAN_CHECK(
        vkBindBufferMemory(
            context->device, result->deviceObject.buffer, 
            deviceMemoryLinear->deviceMemory, alignedOffset));
    /* update the rest of the allocations meta data */
    result->byteOffset = alignedOffset;
    result->byteSize   = memoryRequirements.size;
    /* update allocator book keeping */
    deviceMemoryLinear->bytesAllocated = alignedOffset + memoryRequirements.size;
    return result;
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
    pipeline.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline.useIndexBuffer    = true;
    return pipeline;
}
korl_internal void _korl_vulkan_createPipeline(u32 pipelineIndex)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    korl_assert(pipelineIndex < context->pipelinesCount);
    korl_assert(context->pipelines[pipelineIndex].pipeline == VK_NULL_HANDLE);
    /* set fixed functions & other pipeline parameters */
    KORL_ZERO_STACK_ARRAY(VkVertexInputBindingDescription, vertexInputBindings, 2);
    vertexInputBindings[0].binding   = 0;
    vertexInputBindings[0].stride    = sizeof(Korl_Vulkan_Position);
    vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputBindings[1].binding   = 1;
    vertexInputBindings[1].stride    = sizeof(Korl_Vulkan_Color);
    vertexInputBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    KORL_ZERO_STACK_ARRAY(VkVertexInputAttributeDescription, vertexAttributes, 2);
    vertexAttributes[0].binding  = 0;
    vertexAttributes[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].offset   = 0;// we're not using interleaved vertex data
    vertexAttributes[1].binding  = 1;
    vertexAttributes[1].format   = VK_FORMAT_R8G8B8_UNORM;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].offset   = 0;// we're not using interleaved vertex data
    KORL_ZERO_STACK(VkPipelineVertexInputStateCreateInfo, createInfoVertexInput);
    createInfoVertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfoVertexInput.pVertexBindingDescriptions      = vertexInputBindings;
    createInfoVertexInput.vertexBindingDescriptionCount   = korl_arraySize(vertexInputBindings);
    createInfoVertexInput.pVertexAttributeDescriptions    = vertexAttributes;
    createInfoVertexInput.vertexAttributeDescriptionCount = korl_arraySize(vertexAttributes);
    KORL_ZERO_STACK(VkPipelineInputAssemblyStateCreateInfo, createInfoInputAssembly);
    createInfoInputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    createInfoInputAssembly.topology = context->pipelines[pipelineIndex].primitiveTopology;
    VkViewport viewPort;
    viewPort.x        = 0.f;
    viewPort.y        = 0.f;
    viewPort.width    = KORL_C_CAST(float, surfaceContext->swapChainImageExtent.width);
    viewPort.height   = KORL_C_CAST(float, surfaceContext->swapChainImageExtent.height);
    viewPort.minDepth = 0.f;
    viewPort.maxDepth = 1.f;
    VkRect2D scissor;
    scissor.extent   = surfaceContext->swapChainImageExtent;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    KORL_ZERO_STACK(VkPipelineViewportStateCreateInfo, createInfoViewport);
    createInfoViewport.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    createInfoViewport.viewportCount = 1;
    createInfoViewport.pViewports    = &viewPort;
    createInfoViewport.scissorCount  = 1;
    createInfoViewport.pScissors     = &scissor;
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
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    KORL_ZERO_STACK(VkPipelineColorBlendStateCreateInfo, createInfoColorBlend);
    createInfoColorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    createInfoColorBlend.attachmentCount = 1;
    createInfoColorBlend.pAttachments    = &colorBlendAttachment;
    VkDynamicState dynamicStates[] = 
        { VK_DYNAMIC_STATE_VIEWPORT
        , VK_DYNAMIC_STATE_LINE_WIDTH };
    KORL_ZERO_STACK(VkPipelineDynamicStateCreateInfo, createInfoDynamicState);
    createInfoDynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    createInfoDynamicState.dynamicStateCount = korl_arraySize(dynamicStates);
    createInfoDynamicState.pDynamicStates    = dynamicStates;
    /* create pipeline */
    KORL_ZERO_STACK_ARRAY(
        VkPipelineShaderStageCreateInfo, createInfoShaderStages, 2);
    createInfoShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfoShaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    createInfoShaderStages[0].module = context->shaderImmediateColorVert;
    createInfoShaderStages[0].pName  = "main";
    createInfoShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfoShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    createInfoShaderStages[1].module = context->shaderImmediateFrag;
    createInfoShaderStages[1].pName  = "main";
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
    createInfoPipeline.layout              = context->pipelineLayout;
    createInfoPipeline.renderPass          = context->renderPass;
    createInfoPipeline.subpass             = 0;
    _KORL_VULKAN_CHECK(
        vkCreateGraphicsPipelines(
            context->device, VK_NULL_HANDLE/*pipeline cache*/, 
            1, &createInfoPipeline, context->allocator, 
            &context->pipelines[pipelineIndex].pipeline));
}
/**
 * \return The index of the pipeline in \c context->pipelines , or 
 *     \c context->pipelineCount if we failed to create a pipeline.
 */
korl_internal u32 _korl_vulkan_addPipeline(_Korl_Vulkan_Pipeline pipeline)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    if(context->pipelinesCount >= korl_arraySize(context->pipelines))
        return context->pipelinesCount;
    /* iterate over all pipelines and ensure that there is, in fact, no other 
        pipeline with the same meta data */
    u32 pipelineIndex = context->pipelinesCount;
    for(u32 p = 0; p < context->pipelinesCount; p++)
        if(_korl_vulkan_pipeline_isMetaDataSame(pipeline, context->pipelines[p]))
        {
            pipelineIndex = p;
            break;
        }
    /* if no pipeline found, add a new pipeline to the list */
    if(pipelineIndex >= context->pipelinesCount)
    {
        context->pipelines[context->pipelinesCount] = pipeline;
        context->pipelines[context->pipelinesCount].pipeline = VK_NULL_HANDLE;
        context->pipelinesCount++;
        _korl_vulkan_createPipeline(context->pipelinesCount - 1);
    }
    return pipelineIndex;
}
/**
 * Here we will actually compose the draw commands for the internal batch 
 * pipelines.
 */
korl_internal void _korl_vulkan_flushBatchPipeline(void)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    _korl_vulkan_flushBatchStaging();
    if(swapChainImageContext->pipelineBatchVertexCount <= 0)
        return;// do nothing if we haven't batched anything yet
    /* try to make sure we have selected a valid pipeline before going further */
    korl_assert(surfaceContext->batchCurrentPipeline < context->pipelinesCount && context->pipelinesCount > 0);
    /* add batch draw commands to the command buffer */
    vkCmdBindPipeline(
        surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 
        VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipelines[surfaceContext->batchCurrentPipeline].pipeline);
    VkBuffer batchVertexBuffers[] = 
        { swapChainImageContext->bufferDeviceBatchPositions->deviceObject.buffer
        , swapChainImageContext->bufferDeviceBatchColors   ->deviceObject.buffer };
    /* we can calculate the pipeline batch offsets here, because we know how 
        many vertex attribs/indices are in the pipeline batch, and we also know 
        the total number of attribs/indices in the buffer! */
    const VkDeviceSize batchVertexBufferOffsets[] = 
        { swapChainImageContext->batchVertexCountDevice*sizeof(Korl_Vulkan_Position) - swapChainImageContext->pipelineBatchVertexCount*sizeof(Korl_Vulkan_Position)
        , swapChainImageContext->batchVertexCountDevice*sizeof(Korl_Vulkan_Color)    - swapChainImageContext->pipelineBatchVertexCount*sizeof(Korl_Vulkan_Color) };
    vkCmdBindVertexBuffers(
        surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 
        0/*first binding*/, 
        korl_arraySize(batchVertexBuffers), batchVertexBuffers, 
        batchVertexBufferOffsets);
    vkCmdBindDescriptorSets(
        surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 
        VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipelineLayout, 
        0/*first set*/, 1/*set count */, 
        &surfaceContext->descriptorSets[surfaceContext->frameSwapChainImageIndex], 
        /*dynamic offset count*/0, /*pDynamicOffsets*/NULL);
    if(context->pipelines[surfaceContext->batchCurrentPipeline].useIndexBuffer)
    {
        korl_assert(swapChainImageContext->pipelineBatchVertexIndexCount > 0);
        const VkDeviceSize batchIndexOffset = swapChainImageContext->batchVertexIndexCountDevice*sizeof(Korl_Vulkan_VertexIndex) - swapChainImageContext->pipelineBatchVertexIndexCount*sizeof(Korl_Vulkan_VertexIndex);
        vkCmdBindIndexBuffer(
            surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 
            swapChainImageContext->bufferDeviceBatchIndices->deviceObject.buffer, batchIndexOffset, 
            VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(
            surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 
            swapChainImageContext->pipelineBatchVertexIndexCount, 
            /*instance count*/1, /*firstIndex*/0, /*vertexOffset*/0, 
            /*firstInstance*/0);
    }
    else
    {
        vkCmdDraw(
            surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 
            swapChainImageContext->pipelineBatchVertexCount, 
            /*instance count*/1, /*firstIndex*/0, /*firstInstance*/0);
    }
   /* update book keeping */
   swapChainImageContext->pipelineBatchVertexCount      = 0;
   swapChainImageContext->pipelineBatchVertexIndexCount = 0;
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
    bool pipelineAboutToChange = false;
    u32 newPipelineIndex = korl_arraySize(context->pipelines);
    if(surfaceContext->batchCurrentPipeline < context->pipelinesCount
        && _korl_vulkan_pipeline_isMetaDataSame(pipeline, context->pipelines[surfaceContext->batchCurrentPipeline]))
    {
        // do nothing; just leave the current pipeline selected
        newPipelineIndex = surfaceContext->batchCurrentPipeline;
    }
    else
    {
        /* search for a pipeline that can handle the data we're trying to batch */
        u32 pipelineIndex = _korl_vulkan_addPipeline(pipeline);
        korl_assert(pipelineIndex < context->pipelinesCount);
        pipelineAboutToChange = (pipelineIndex != surfaceContext->batchCurrentPipeline);
        newPipelineIndex      = pipelineIndex;
    }
    /* flush the pipeline batch if we're about to change pipelines */
    if(pipelineAboutToChange && swapChainImageContext->pipelineBatchVertexCount > 0)
        _korl_vulkan_flushBatchPipeline();
    /* then, actually change to a new pipeline for the next batch */
    korl_assert(newPipelineIndex < context->pipelinesCount);//sanity check!
    surfaceContext->batchCurrentPipeline = newPipelineIndex;
}
korl_internal void _korl_vulkan_batchVertexIndices(u32 vertexIndexCount, const Korl_Vulkan_VertexIndex* vertexIndices)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* simplification - just make sure that the vertex index data can at least 
        fit in the staging buffer */
    korl_assert(vertexIndexCount <= _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_STAGING);
    /* if the staging buffer has too much in it, we need to flush it */
    if(vertexIndexCount + swapChainImageContext->batchVertexIndexCountStaging > _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_STAGING)
        _korl_vulkan_flushBatchStaging();
    /* copy all the vertex indices to the memory region which is occupied by the 
        vertex index staging buffer */
    KORL_ZERO_STACK(void*, mappedDeviceMemory);
    _KORL_VULKAN_CHECK(
        vkMapMemory(
            context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory, 
            /*offset*/swapChainImageContext->bufferStagingBatchIndices->byteOffset, 
            /*bytes*/vertexIndexCount * sizeof(Korl_Vulkan_VertexIndex), 
            0/*flags*/, &mappedDeviceMemory));
    memcpy(
        mappedDeviceMemory, vertexIndices, 
        vertexIndexCount * sizeof(Korl_Vulkan_VertexIndex));
    /* loop over all the vertex indices we just added to staging and modify the 
        indices to match the offset of the current total vertices we have 
        batched so far in staging + device memory for the current pipeline batch */
    Korl_Vulkan_VertexIndex* copyVertexIndices = 
        KORL_C_CAST(Korl_Vulkan_VertexIndex*, mappedDeviceMemory);
    for(u32 i = 0; i < vertexIndexCount; i++)
        copyVertexIndices[i] += 
            korl_checkCast_u32_to_u16(swapChainImageContext->pipelineBatchVertexCount);
    vkUnmapMemory(context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory);
    /* update book keeping */
    swapChainImageContext->batchVertexIndexCountStaging  += vertexIndexCount;
    swapChainImageContext->pipelineBatchVertexIndexCount += vertexIndexCount;
}
korl_internal void _korl_vulkan_batchVertexAttributes(u32 vertexCount, const Korl_Vulkan_Position* positions, const Korl_Vulkan_Color* colors)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* let's simplify things here and make sure that the vertex data we're 
        trying to batch is guaranteed to fit inside the empty staging buffer at 
        least */
    korl_assert(vertexCount <= _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING);
    /* if the staging buffer has too much in it, we need to flush it */
    if(vertexCount + swapChainImageContext->batchVertexCountStaging > _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING)
        _korl_vulkan_flushBatchStaging();
    /* copy all the vertex data to staging */
    KORL_ZERO_STACK(void*, mappedDeviceMemory);
    // copy the positions in mapped staging memory //
    _KORL_VULKAN_CHECK(
        vkMapMemory(
            context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory, 
            /*offset*/swapChainImageContext->bufferStagingBatchPositions->byteOffset, 
            /*bytes*/vertexCount * sizeof(Korl_Vulkan_Position), 
            0/*flags*/, &mappedDeviceMemory));
    memcpy(
        mappedDeviceMemory, positions, 
        vertexCount * sizeof(Korl_Vulkan_Position));
    vkUnmapMemory(context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory);
    // stage the colors in mapped staging memory //
    _KORL_VULKAN_CHECK(
        vkMapMemory(
            context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory, 
            /*offset*/swapChainImageContext->bufferStagingBatchColors->byteOffset, 
            /*bytes*/vertexCount * sizeof(Korl_Vulkan_Color), 
            0/*flags*/, &mappedDeviceMemory));
    memcpy(
        mappedDeviceMemory, colors, 
        vertexCount * sizeof(Korl_Vulkan_Color));
    vkUnmapMemory(context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory);
    /* update the staging metrics */
    swapChainImageContext->batchVertexCountStaging  += vertexCount;
    swapChainImageContext->pipelineBatchVertexCount += vertexCount;
}
/** This API is platform-specific, and thus must be defined within the code base 
 * of whatever the current target platform is. */
korl_internal void _korl_vulkan_createSurface(void* userData);
korl_internal void korl_vulkan_construct(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /* sanity check - ensure that the memory is nullified */
    korl_assert(korl_memory_isNull(context, sizeof(*context)));
    /* get a list of VkLayerProperties so we can check of validation layer 
        support if needed */
    KORL_ZERO_STACK(u32, layerCount);
    _KORL_VULKAN_CHECK(
        vkEnumerateInstanceLayerProperties(
            &layerCount, 
            NULL/*pProperties; NULL->return # of properties in param 1*/));
    VkLayerProperties layerProperties[128];
    korl_assert(layerCount <= korl_arraySize(layerProperties));
    layerCount = korl_arraySize(layerProperties);
    _KORL_VULKAN_CHECK(
        vkEnumerateInstanceLayerProperties(&layerCount, layerProperties));
    korl_log(INFO, "Provided layers supported by this platform "
        "{\"name\"[specVersion, implementationVersion]}:");
    for(u32 l = 0; l < layerCount; l++)
    {
        korl_log(INFO, "layer[%u]=\"%s\"[%u, %u]", l, 
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
        vkEnumerateInstanceExtensionProperties(
            NULL/*pLayerName*/, &extensionCount, 
            NULL/*pProperties; NULL->return # of properties in param 2*/));
    korl_assert(extensionCount <= korl_arraySize(extensionProperties));
    extensionCount = korl_arraySize(extensionProperties);
    _KORL_VULKAN_CHECK(
        vkEnumerateInstanceExtensionProperties(
            NULL/*pLayerName*/, &extensionCount, extensionProperties));
    korl_log(INFO, "Provided extensions supported by this platform "
        "{\"name\"[specVersion]}:");
    for(u32 e = 0; e < extensionCount; e++)
        korl_log(INFO, "extension[%u]=\"%s\"[%u]", e, 
            extensionProperties[e].extensionName, 
            extensionProperties[e].specVersion);
    /* create the VkInstance */
    /* @robustness: cross-check this list of extension names with the 
        extensionProperties queried above to check if they are all provided by 
        the platform */
    const char* enabledExtensions[] = 
        { VK_KHR_SURFACE_EXTENSION_NAME
#if defined(_WIN32)
        , VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif// defined(_WIN32)
#if KORL_DEBUG
        , VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif// KORL_DEBUG
        };
    KORL_ZERO_STACK(VkApplicationInfo, applicationInfo);
    applicationInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "KORL Application";
    applicationInfo.pEngineName      = "KORL";
    applicationInfo.engineVersion    = 0;
    applicationInfo.apiVersion       = VK_API_VERSION_1_2;
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
        vkCreateInstance(
            &createInfoInstance, context->allocator, &context->instance));
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
            vkGetInstanceProcAddr(
                context->instance, "vkCreateDebugUtilsMessengerEXT"));
    korl_assert(vkCreateDebugUtilsMessengerEXT);
    _KORL_VULKAN_GET_INSTANCE_PROC_ADDR(context, DestroyDebugUtilsMessengerEXT);
    /* attach a VkDebugUtilsMessengerEXT to our instance */
    _KORL_VULKAN_CHECK(
        vkCreateDebugUtilsMessengerEXT(
            context->instance, &createInfoDebugUtilsMessenger, 
            context->allocator, &context->debugMessenger));
#endif// KORL_DEBUG
}
korl_internal void korl_vulkan_destroy(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
#if KORL_DEBUG
    context->vkDestroyDebugUtilsMessengerEXT(
        context->instance, context->debugMessenger, context->allocator);
#else
    /* we only need to manually destroy the vulkan module if we're running a 
        debug build, since the validation layers require us to properly clean up 
        before the program ends, but for release builds we really don't care at 
        all about wasting time like this if we don't have to */
    return;
#endif// KORL_DEBUG
    vkDestroyInstance(context->instance, context->allocator);
    /* nullify the context after cleaning up properly for safety */
    korl_memory_nullify(context, sizeof(*context));
}
korl_internal void korl_vulkan_createDevice(
    void* createSurfaceUserData, u32 sizeX, u32 sizeY)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* we have to create the OS-specific surface before choosing a physical 
        device to create the logical device on */
    _korl_vulkan_createSurface(createSurfaceUserData);
    /* enumerate & choose a physical device */
    KORL_ZERO_STACK(u32, physicalDeviceCount);
    _KORL_VULKAN_CHECK(
        vkEnumeratePhysicalDevices(
            context->instance, &physicalDeviceCount, 
            NULL/*pPhysicalDevices; NULL->return the count in param 2*/));
    // we need at least one physical device compatible with Vulkan
    korl_assert(physicalDeviceCount > 0);
    VkPhysicalDevice physicalDevices[16];
    korl_assert(physicalDeviceCount <= korl_arraySize(physicalDevices));
    physicalDeviceCount = korl_arraySize(physicalDevices);
    _KORL_VULKAN_CHECK(
        vkEnumeratePhysicalDevices(
            context->instance, &physicalDeviceCount, physicalDevices));
    KORL_ZERO_STACK(VkPhysicalDeviceProperties, devicePropertiesBest);
    KORL_ZERO_STACK(VkPhysicalDeviceFeatures, deviceFeaturesBest);
    KORL_ZERO_STACK(u32, queueFamilyCountBest);
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceSurfaceMetaData, deviceSurfaceMetaDataBest);
    for(u32 d = 0; d < physicalDeviceCount; d++)
    {
        KORL_ZERO_STACK(u32, queueFamilyCount);
        VkQueueFamilyProperties queueFamilies[32];
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevices[d], &queueFamilyCount, 
            NULL/*pQueueFamilyProperties; NULL->return the count via param 2*/);
        korl_assert(queueFamilyCount <= korl_arraySize(queueFamilies));
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevices[d], &queueFamilyCount, queueFamilies);
        _Korl_Vulkan_QueueFamilyMetaData queueFamilyMetaData = 
            _korl_vulkan_queueFamilyMetaData(
                physicalDevices[d], queueFamilyCount, queueFamilies, 
                surfaceContext->surface);
        KORL_ZERO_STACK(VkPhysicalDeviceProperties, deviceProperties);
        KORL_ZERO_STACK(VkPhysicalDeviceFeatures, deviceFeatures);
        vkGetPhysicalDeviceProperties(physicalDevices[d], &deviceProperties);
        vkGetPhysicalDeviceFeatures(physicalDevices[d], &deviceFeatures);
        _Korl_Vulkan_DeviceSurfaceMetaData deviceSurfaceMetaData = 
            _korl_vulkan_deviceSurfaceMetaData(
                physicalDevices[d], surfaceContext->surface);
        if(_korl_vulkan_isBetterDevice(
                context->physicalDevice, physicalDevices[d], 
                &queueFamilyMetaData, &deviceSurfaceMetaData, 
                &devicePropertiesBest, &deviceProperties/*, 
                &deviceFeaturesBest, &deviceFeatures*/))
        {
            context->physicalDevice = physicalDevices[d];
            devicePropertiesBest = deviceProperties;
            deviceFeaturesBest = deviceFeatures;
            context->queueFamilyMetaData = queueFamilyMetaData;
            deviceSurfaceMetaDataBest = deviceSurfaceMetaData;
        }
    }
    korl_assert(context->physicalDevice != VK_NULL_HANDLE);
    korl_log(INFO, "chosen physical device: \"%s\"", 
        devicePropertiesBest.deviceName);
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
        korl_memory_nullify(createInfo, sizeof(*createInfo));
        createInfo->sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        createInfo->queueFamilyIndex = context->queueFamilyMetaData.indexQueueUnion.indices[f];
        createInfo->queueCount       = korl_arraySize(QUEUE_PRIORITIES);
        createInfo->pQueuePriorities = QUEUE_PRIORITIES;
        createInfoDeviceQueueFamiliesSize++;
    }
    /* create logical device */
    KORL_ZERO_STACK(VkPhysicalDeviceFeatures, physicalDeviceFeatures);
    // leave all device features turned off for now~
    KORL_ZERO_STACK(VkDeviceCreateInfo, createInfoDevice);
    createInfoDevice.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
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
        vkCreateDevice(
            context->physicalDevice, &createInfoDevice, 
            context->allocator, &context->device));
    /* obtain opaque handles to the queues that we need */
    vkGetDeviceQueue(
        context->device, 
        context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics, 
        0/*queueIndex*/, &context->queueGraphics);
    vkGetDeviceQueue(
        context->device, 
        context->queueFamilyMetaData.indexQueueUnion.indexQueues.present, 
        0/*queueIndex*/, &context->queuePresent);
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
    /* it kinda feels like we don't, since we can just use a single command 
        buffer which is reset once per frame.......................... */
    /* NO!  Actually that is not correct.  We DO, in fact, need to have 
        transient command buffers created because memory has to move from 
        staging to device memory potentially multiple times in the same frame.  
        Then, the staging buffer memory potentially will be reused to batch more 
        vertices, which are subsequently flushed again to device memory.  This 
        is logical speculation based on the fact that it is very likely for us 
        to be working with a SMALL staging buffer, and a LARGE device buffer! */
    createInfoCommandPool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT/*give a hint to the implementation that the command buffers allocated by this command pool will be short-lived*/;
    _KORL_VULKAN_CHECK(
        vkCreateCommandPool(
            context->device, &createInfoCommandPool, context->allocator, 
            &context->commandPoolTransfer));
    /* now that the device is created we can create the swap chain 
        - this also requires the command pool since we need to create the 
          graphics command buffers for each element of the swap chain
        - we also have to do this BEFORE creating the vertex batch memory 
          allocations/buffers, because this is where we determine the value of 
          `surfaceContext->swapChainImagesSize`!*/
    _korl_vulkan_createSwapChain(sizeX, sizeY, &deviceSurfaceMetaDataBest);
    /* --- create descriptor pool ---
        We shouldn't have to do this inside createSwapChain for similar reasons 
        to the internal memory buffers; we should know a priori what the maximum 
        # of images in the swap chain will be.  Ergo, we can just create a pool 
        which can support this # of descriptors.  For now, this is perfectly 
        fine since the only descriptors we will have will be internal (shared 
        UBOs, for example), but this might change in the future. */
    KORL_ZERO_STACK(VkDescriptorPoolSize, descriptorPoolSize);
    descriptorPoolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorPoolSize.descriptorCount = _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE;
    KORL_ZERO_STACK(VkDescriptorPoolCreateInfo, createInfoDescriptorPool);
    createInfoDescriptorPool.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfoDescriptorPool.maxSets       = _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE;
    createInfoDescriptorPool.poolSizeCount = 1;
    createInfoDescriptorPool.pPoolSizes    = &descriptorPoolSize;
    _KORL_VULKAN_CHECK(
        vkCreateDescriptorPool(
            context->device, &createInfoDescriptorPool, context->allocator, 
            &surfaceContext->descriptorPool));
    /* --- create buffers on the device to store various resources in ---
        We have two distinct usages: staging buffers which will only be the 
        source of transfer operations to the device, and device buffers which 
        will be the destination of transfer operations to the device & also be 
        used as vertex buffers. 
        - Why are we not doing this inside `_korl_vulkan_createSwapChain`?
          Because these memory allocations should (hopefully) be able to persist 
          between swap chain creation/destruction! */
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[i];
        /* --- create device memory managers --- */
        _korl_vulkan_deviceMemoryLinear_create(
            &swapChainImageContext->deviceMemoryLinearHostVisible, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
            korl_math_kilobytes(256));
        _korl_vulkan_deviceMemoryLinear_create(
            &swapChainImageContext->deviceMemoryLinearDeviceLocal, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
            korl_math_kilobytes(256));
        /* --- allocate vertex batch staging buffers --- */
        swapChainImageContext->bufferStagingBatchIndices = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &swapChainImageContext->deviceMemoryLinearHostVisible, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_STAGING*sizeof(Korl_Vulkan_VertexIndex), 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
        swapChainImageContext->bufferStagingBatchPositions = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &swapChainImageContext->deviceMemoryLinearHostVisible, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*sizeof(Korl_Vulkan_Position), 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
        swapChainImageContext->bufferStagingBatchColors = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &swapChainImageContext->deviceMemoryLinearHostVisible, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*sizeof(Korl_Vulkan_Color), 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
        /* --- allocate UBO staging buffer --- */
        swapChainImageContext->bufferStagingUbo = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &swapChainImageContext->deviceMemoryLinearHostVisible, 
                sizeof(_Korl_Vulkan_SwapChainImageBatchUbo), 
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
        /* --- allocate device-local vertex batch buffers --- */
        swapChainImageContext->bufferDeviceBatchIndices = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &swapChainImageContext->deviceMemoryLinearDeviceLocal, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_DEVICE*sizeof(Korl_Vulkan_VertexIndex), 
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE);
        swapChainImageContext->bufferDeviceBatchPositions = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &swapChainImageContext->deviceMemoryLinearDeviceLocal, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*sizeof(Korl_Vulkan_Position), 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE);
        swapChainImageContext->bufferDeviceBatchColors = 
            _korl_vulkan_deviceMemoryLinear_allocateBuffer(
                &swapChainImageContext->deviceMemoryLinearDeviceLocal, 
                _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*sizeof(Korl_Vulkan_Color), 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE);
    }
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
    /* create descriptor set layout */
    // not really sure where this should go, but I know that it just needs to 
    //    be created BEFORE the pipelines are created, since they are used there //
    KORL_ZERO_STACK(VkDescriptorSetLayoutBinding, descriptorSetLayoutBinding); 
    descriptorSetLayoutBinding.binding         = 0;
    descriptorSetLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorSetLayoutBinding.descriptorCount = 1;
    descriptorSetLayoutBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
    KORL_ZERO_STACK(VkDescriptorSetLayoutCreateInfo, createInfoDescriptorSetLayout);
    createInfoDescriptorSetLayout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfoDescriptorSetLayout.bindingCount = 1;
    createInfoDescriptorSetLayout.pBindings    = &descriptorSetLayoutBinding;
    _KORL_VULKAN_CHECK(
        vkCreateDescriptorSetLayout(
            context->device, &createInfoDescriptorSetLayout, context->allocator, 
            &context->descriptorSetLayout));
    /* --- allocate descriptor sets --- 
        This must happen AFTER the associated descriptorSetLayout(s) get 
        created, since creation of a set depends on the layout! */
    VkDescriptorSetLayout internalDescriptorSetLayouts[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    for(unsigned i = 0; i < _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE; i++)
        internalDescriptorSetLayouts[i] = context->descriptorSetLayout;
    KORL_ZERO_STACK(VkDescriptorSetAllocateInfo, allocInfoDescriptorSets);
    allocInfoDescriptorSets.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfoDescriptorSets.descriptorPool     = surfaceContext->descriptorPool;
    allocInfoDescriptorSets.descriptorSetCount = korl_arraySize(internalDescriptorSetLayouts);
    allocInfoDescriptorSets.pSetLayouts        = internalDescriptorSetLayouts;
    _KORL_VULKAN_CHECK(
        vkAllocateDescriptorSets(
            context->device, &allocInfoDescriptorSets, 
            surfaceContext->descriptorSets));
    /* create pipeline layout */
    KORL_ZERO_STACK(VkPipelineLayoutCreateInfo, createInfoPipelineLayout);
    createInfoPipelineLayout.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfoPipelineLayout.setLayoutCount = 1;
    createInfoPipelineLayout.pSetLayouts    = &context->descriptorSetLayout;
    _KORL_VULKAN_CHECK(
        vkCreatePipelineLayout(
            context->device, &createInfoPipelineLayout, context->allocator, 
            &context->pipelineLayout));
    /* load required built-in shader assets */
    Korl_AssetCache_AssetData assetShaderBatchVertexColor = korl_assetCache_get(L"build/korl-immediate-color.vert.spv", KORL_ASSETCACHE_GET_FLAGS_NONE);
    Korl_AssetCache_AssetData assetShaderBatchFragment    = korl_assetCache_get(L"build/korl-immediate.frag.spv"      , KORL_ASSETCACHE_GET_FLAGS_NONE);
    /* create shader modules */
    KORL_ZERO_STACK(VkShaderModuleCreateInfo, createInfoShaderVert);
    createInfoShaderVert.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfoShaderVert.codeSize = assetShaderBatchVertexColor.dataBytes;
    createInfoShaderVert.pCode    = assetShaderBatchVertexColor.data;
    _KORL_VULKAN_CHECK(
        vkCreateShaderModule(
            context->device, &createInfoShaderVert, context->allocator, 
            &context->shaderImmediateColorVert));
    KORL_ZERO_STACK(VkShaderModuleCreateInfo, createInfoShaderFrag);
    createInfoShaderFrag.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfoShaderFrag.codeSize = assetShaderBatchFragment.dataBytes;
    createInfoShaderFrag.pCode    = assetShaderBatchFragment.data;
    _KORL_VULKAN_CHECK(
        vkCreateShaderModule(
            context->device, &createInfoShaderFrag, context->allocator, 
            &context->shaderImmediateFrag));
    /* create memory allocators to stage & store persistent assets, like images */
    _korl_vulkan_deviceMemoryLinear_create(
        &context->deviceMemoryLinearAssetsStaging, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        korl_math_megabytes(1));
    _korl_vulkan_deviceMemoryLinear_create(
        &context->deviceMemoryLinearAssets, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
        korl_math_megabytes(64));
}
korl_internal void korl_vulkan_destroyDevice(void)
{
    _korl_vulkan_destroySwapChain();
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* destroy the surface-specific resources */
    vkDestroyDescriptorPool(context->device, surfaceContext->descriptorPool, context->allocator);
    for(size_t f = 0; f < _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES; f++)
    {
        vkDestroySemaphore(context->device, surfaceContext->wipFrames[f].semaphoreImageAvailable, context->allocator);
        vkDestroySemaphore(context->device, surfaceContext->wipFrames[f].semaphoreRenderDone, context->allocator);
        vkDestroyFence(context->device, surfaceContext->wipFrames[f].fence, context->allocator);
    }
    vkDestroySurfaceKHR(context->instance, surfaceContext->surface, context->allocator);
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[i];
        _korl_vulkan_deviceMemoryLinear_destroy(&swapChainImageContext->deviceMemoryLinearHostVisible);
        _korl_vulkan_deviceMemoryLinear_destroy(&swapChainImageContext->deviceMemoryLinearDeviceLocal);
    }
    korl_memory_nullify(surfaceContext, sizeof(*surfaceContext));
    /* destroy the device-specific resources */
    _korl_vulkan_deviceMemoryLinear_destroy(&context->deviceMemoryLinearAssetsStaging);
    _korl_vulkan_deviceMemoryLinear_destroy(&context->deviceMemoryLinearAssets);
    for(size_t p = 0; p < context->pipelinesCount; p++)
        vkDestroyPipeline(context->device, context->pipelines[p].pipeline, context->allocator);
    vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, context->allocator);
    vkDestroyPipelineLayout(context->device, context->pipelineLayout, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderImmediateColorVert, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderImmediateFrag, context->allocator);
    vkDestroyCommandPool(context->device, context->commandPoolTransfer, context->allocator);
    vkDestroyCommandPool(context->device, context->commandPoolGraphics, context->allocator);
    vkDestroyDevice(context->device, context->allocator);
}
korl_internal void korl_vulkan_frameBegin(const f32 clearRgb[3])
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    KORL_ZERO_STACK(_Korl_Vulkan_SwapChainImageContext*, swapChainImageContext);
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
        if(    surfaceContext->deferredResizeX == 0 
            || surfaceContext->deferredResizeY == 0)
            goto done;
        korl_log(VERBOSE, "resizing to: %ux%u", 
            surfaceContext->deferredResizeX, surfaceContext->deferredResizeY);
        /* destroy swap chain & all resources which depend on it implicitly*/
        _korl_vulkan_destroySwapChain();
        /* since the device surface meta data has changed, we need to query it */
        _Korl_Vulkan_DeviceSurfaceMetaData deviceSurfaceMetaData = 
            _korl_vulkan_deviceSurfaceMetaData(
                context->physicalDevice, surfaceContext->surface);
        /* re-create the swap chain & all resources which depend on it */
        _korl_vulkan_createSwapChain(
            surfaceContext->deferredResizeX, surfaceContext->deferredResizeY, 
            &deviceSurfaceMetaData);
        /* re-create pipelines */
        for(u32 p = 0; p < context->pipelinesCount; p++)
        {
            if(context->pipelines[p].pipeline == VK_NULL_HANDLE)
                continue;
            vkDestroyPipeline(context->device, context->pipelines[p].pipeline, context->allocator);
            context->pipelines[p].pipeline = VK_NULL_HANDLE;
            _korl_vulkan_createPipeline(p);
        }
        /** @todo: re-record command buffers?... (waste of time probably) */
        /* clear the deferred resize flag for the next frame */
        surfaceContext->deferredResize = false;
    }
    /* wait on the fence for the current WIP frame */
    _KORL_VULKAN_CHECK(
        vkWaitForFences(
            context->device, 1, 
            &surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fence, 
            VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/));
    /* acquire the next image from the swap chain */
    _KORL_VULKAN_CHECK(
        vkAcquireNextImageKHR(
            context->device, surfaceContext->swapChain, 
            UINT64_MAX/*timeout; UINT64_MAX -> disable*/, 
            surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreImageAvailable, 
            VK_NULL_HANDLE/*fence*/, &surfaceContext->frameSwapChainImageIndex));
    swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    if(swapChainImageContext->fence != VK_NULL_HANDLE)
    {
        _KORL_VULKAN_CHECK(
            vkWaitForFences(
                context->device, 1, 
                &swapChainImageContext->fence, 
                VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/));
    }
    swapChainImageContext->fence = 
        surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fence;
    /* ----- begin the swap chain command buffer for this frame ----- */
    _KORL_VULKAN_CHECK(
        vkResetCommandBuffer(
            surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 0/*flags*/));
    KORL_ZERO_STACK(VkCommandBufferBeginInfo, beginInfoCommandBuffer);
    beginInfoCommandBuffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfoCommandBuffer.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    _KORL_VULKAN_CHECK(
        vkBeginCommandBuffer(
            surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 
            &beginInfoCommandBuffer));
    // define the color we are going to clear the color attachment with when 
    //    the render pass begins:
    KORL_ZERO_STACK(VkClearValue, clearValue);
    clearValue.color.float32[0] = clearRgb[0];
    clearValue.color.float32[1] = clearRgb[1];
    clearValue.color.float32[2] = clearRgb[2];
    clearValue.color.float32[3] = 1.f;
    KORL_ZERO_STACK(VkRenderPassBeginInfo, beginInfoRenderPass);
    beginInfoRenderPass.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfoRenderPass.renderPass        = context->renderPass;
    beginInfoRenderPass.framebuffer       = swapChainImageContext->frameBuffer;
    beginInfoRenderPass.renderArea.extent = surfaceContext->swapChainImageExtent;
    beginInfoRenderPass.clearValueCount   = 1;
    beginInfoRenderPass.pClearValues      = &clearValue;
    vkCmdBeginRenderPass(
        surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex], 
        &beginInfoRenderPass, VK_SUBPASS_CONTENTS_INLINE);
done:
    /* help to ensure the user is calling frameBegin & frameEnd in the correct 
        order & the correct frequency */
    korl_assert(surfaceContext->frameStackCounter == 0);
    surfaceContext->frameStackCounter++;
    /* clear the vertex batch metrics for the upcoming frame */
    swapChainImageContext->batchVertexCountStaging       = 0;
    swapChainImageContext->batchVertexCountDevice        = 0;
    swapChainImageContext->batchVertexIndexCountStaging  = 0;
    swapChainImageContext->batchVertexIndexCountDevice   = 0;
    swapChainImageContext->pipelineBatchVertexIndexCount = 0;
    swapChainImageContext->pipelineBatchVertexCount      = 0;
}
korl_internal void korl_vulkan_frameEnd(void)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    /* if we got an invalid next swapchain image index, just do nothing */
    if(surfaceContext->frameSwapChainImageIndex >= _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE)
        return;
    _korl_vulkan_flushBatchPipeline();
    vkCmdEndRenderPass(surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex]);
    _KORL_VULKAN_CHECK(
        vkEndCommandBuffer(surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex]));
    /* submit graphics commands to the graphics queue */
    VkSemaphore submitGraphicsWaitSemaphores[] = 
        { surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreImageAvailable };
    VkPipelineStageFlags submitGraphicsWaitStages[] = 
        { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore submitGraphicsSignalSemaphores[] = 
        { surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].semaphoreRenderDone };
    KORL_ZERO_STACK(VkSubmitInfo, submitInfoGraphics);
    submitInfoGraphics.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfoGraphics.waitSemaphoreCount   = korl_arraySize(submitGraphicsWaitSemaphores);
    submitInfoGraphics.pWaitSemaphores      = submitGraphicsWaitSemaphores;
    submitInfoGraphics.pWaitDstStageMask    = submitGraphicsWaitStages;
    submitInfoGraphics.commandBufferCount   = 1;
    submitInfoGraphics.pCommandBuffers      = &surfaceContext->swapChainCommandBuffers[surfaceContext->frameSwapChainImageIndex];
    submitInfoGraphics.signalSemaphoreCount = korl_arraySize(submitGraphicsSignalSemaphores);
    submitInfoGraphics.pSignalSemaphores    = submitGraphicsSignalSemaphores;
    // close the fence in preparation to submit commands to the graphics queue
    _KORL_VULKAN_CHECK(
        vkResetFences(
            context->device, 1, 
            &surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fence));
    _KORL_VULKAN_CHECK(
        vkQueueSubmit(
            context->queueGraphics, 1, &submitInfoGraphics, 
            surfaceContext->wipFrames[surfaceContext->wipFrameCurrent].fence));
    /* present the swap chain */
    VkSwapchainKHR presentInfoSwapChains[] = { surfaceContext->swapChain };
    KORL_ZERO_STACK(VkPresentInfoKHR, presentInfo);
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = submitGraphicsSignalSemaphores;
    presentInfo.swapchainCount     = korl_arraySize(presentInfoSwapChains);
    presentInfo.pSwapchains        = presentInfoSwapChains;
    presentInfo.pImageIndices      = &surfaceContext->frameSwapChainImageIndex;
    _KORL_VULKAN_CHECK(vkQueuePresentKHR(context->queuePresent, &presentInfo));
    /* advance to the next WIP frame index */
    surfaceContext->wipFrameCurrent = 
        (surfaceContext->wipFrameCurrent + 1) % 
        _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES;
    /* help to ensure the user is calling frameBegin & frameEnd in the correct 
        order & the correct frequency */
    korl_assert(surfaceContext->frameStackCounter == 1);
    surfaceContext->frameStackCounter--;
}
korl_internal void korl_vulkan_deferredResize(u32 sizeX, u32 sizeY)
{
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    surfaceContext->deferredResize = true;
    surfaceContext->deferredResizeX = sizeX;
    surfaceContext->deferredResizeY = sizeY;
}
korl_internal void korl_vulkan_batchTriangles_color(
    u32 vertexIndexCount, const Korl_Vulkan_VertexIndex* vertexIndices, 
    u32 vertexCount, const Korl_Vulkan_Position* positions, 
    const Korl_Vulkan_Color* colors)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* if there is no pipeline selected, create a default pipeline meta data, 
        modify it to have the desired render state for this call, then set the 
        current pipeline meta data to this value */
    _Korl_Vulkan_Pipeline pipelineMetaData;
    if(surfaceContext->batchCurrentPipeline >= context->pipelinesCount)
        pipelineMetaData = _korl_vulkan_pipeline_default();
    /* otherwise, we want to just modify the current selected pipeline state 
        to have the desired render state for this call */
    else
        pipelineMetaData = context->pipelines[surfaceContext->batchCurrentPipeline];
    pipelineMetaData.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineMetaData.useIndexBuffer    = true;
    _korl_vulkan_setPipelineMetaData(pipelineMetaData);
    /* now that the batch pipeline is setup, we can batch the vertices */
    _korl_vulkan_batchVertexIndices(vertexIndexCount, vertexIndices);
    _korl_vulkan_batchVertexAttributes(vertexCount, positions, colors);
}
korl_internal void korl_vulkan_batchTriangles_uv(
    u32 vertexIndexCount, const Korl_Vulkan_VertexIndex* vertexIndices, 
    u32 vertexCount, const Korl_Vulkan_Position* positions, 
    const Korl_Vulkan_Uv* vertexTextureUvs)
{
    korl_assert(!"@todo");
}
korl_internal void korl_vulkan_batchLines_color(
    u32 vertexCount, const Korl_Vulkan_Position* positions, 
    const Korl_Vulkan_Color* colors)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* if there is no pipeline selected, create a default pipeline meta data, 
        modify it to have the desired render state for this call, then set the 
        current pipeline meta data to this value */
    _Korl_Vulkan_Pipeline pipelineMetaData;
    if(surfaceContext->batchCurrentPipeline >= context->pipelinesCount)
        pipelineMetaData = _korl_vulkan_pipeline_default();
    /* otherwise, we want to just modify the current selected pipeline state 
        to have the desired render state for this call */
    else
        pipelineMetaData = context->pipelines[surfaceContext->batchCurrentPipeline];
    pipelineMetaData.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    pipelineMetaData.useIndexBuffer    = false;
    _korl_vulkan_setPipelineMetaData(pipelineMetaData);
    /* now that the batch pipeline is setup, we can batch the vertices */
    _korl_vulkan_batchVertexAttributes(vertexCount, positions, colors);
}
korl_internal void korl_vulkan_setProjectionFov(
    f32 horizontalFovDegrees, f32 clipNear, f32 clipFar)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    const f32 viewportWidthOverHeight = 
        KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.width) / 
        KORL_C_CAST(f32, surfaceContext->swapChainImageExtent.height);
    const Korl_Math_M4f32 m4f32Projection = 
        korl_math_m4f32_projectionFov(horizontalFovDegrees, viewportWidthOverHeight, clipNear, clipFar);
    /** @todo: for now, we'll just support a single UBO set per frame because it's 
        simpler - later, we could make an array of UBO sets and tell each render 
        batch to use a specific UBO set */
    /* send the data for the matrix into the staging buffer memory */
    KORL_ZERO_STACK(void*, mappedDeviceMemory);
    _KORL_VULKAN_CHECK(
        vkMapMemory(
            context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory, 
            swapChainImageContext->bufferStagingUbo->byteOffset, 
            /*bytes*/sizeof(_Korl_Vulkan_SwapChainImageBatchUbo), 
            0/*flags*/, &mappedDeviceMemory));
    _Korl_Vulkan_SwapChainImageBatchUbo*const ubo = KORL_C_CAST(_Korl_Vulkan_SwapChainImageBatchUbo*, mappedDeviceMemory);
    ubo->m4f32Projection = m4f32Projection;
    vkUnmapMemory(context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory);
    /* update our shared UBO descriptor set */
    /** @todo: support multiple UBO descriptors per frame */
    KORL_ZERO_STACK(VkDescriptorBufferInfo, descriptorBufferInfoUbo);
    descriptorBufferInfoUbo.buffer = surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex].bufferStagingUbo->deviceObject.buffer;
    descriptorBufferInfoUbo.range  = VK_WHOLE_SIZE;
    KORL_ZERO_STACK(VkWriteDescriptorSet, writeDescriptorSetUbo);
    writeDescriptorSetUbo.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSetUbo.dstSet          = surfaceContext->descriptorSets[surfaceContext->frameSwapChainImageIndex];
    writeDescriptorSetUbo.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSetUbo.descriptorCount = 1;
    writeDescriptorSetUbo.pBufferInfo     = &descriptorBufferInfoUbo;
    vkUpdateDescriptorSets(context->device, 1, &writeDescriptorSetUbo, 0, NULL);
}
korl_internal void korl_vulkan_lookAt(
    const Korl_Math_V3f32*const positionEye, 
    const Korl_Math_V3f32*const positionTarget, 
    const Korl_Math_V3f32*const worldUpNormal)
{
    _Korl_Vulkan_Context*const context                             = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext               = &g_korl_vulkan_surfaceContext;
    _Korl_Vulkan_SwapChainImageContext*const swapChainImageContext = &surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex];
    const Korl_Math_M4f32 m4f32View = korl_math_m4f32_lookAt(positionEye, positionTarget, worldUpNormal);
    /* for now, we'll just support a single UBO set per frame because it's 
        simpler - later, we could make an array of UBO sets and tell each render 
        batch to use a specific UBO set */
    /* send the data for the matrix into the staging buffer memory */
    KORL_ZERO_STACK(void*, mappedDeviceMemory);
    _KORL_VULKAN_CHECK(
        vkMapMemory(
            context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory, 
            swapChainImageContext->bufferStagingUbo->byteOffset, 
            /*bytes*/sizeof(_Korl_Vulkan_SwapChainImageBatchUbo), 
            0/*flags*/, &mappedDeviceMemory));
    _Korl_Vulkan_SwapChainImageBatchUbo*const ubo = KORL_C_CAST(_Korl_Vulkan_SwapChainImageBatchUbo*, mappedDeviceMemory);
    ubo->m4f32View = m4f32View;
    vkUnmapMemory(context->device, swapChainImageContext->deviceMemoryLinearHostVisible.deviceMemory);
    /* update our shared UBO descriptor set */
    /** @todo: support multiple UBO descriptors per frame */
    KORL_ZERO_STACK(VkDescriptorBufferInfo, descriptorBufferInfoUbo);
    descriptorBufferInfoUbo.buffer = surfaceContext->swapChainImageContexts[surfaceContext->frameSwapChainImageIndex].bufferStagingUbo->deviceObject.buffer;
    descriptorBufferInfoUbo.range  = VK_WHOLE_SIZE;
    KORL_ZERO_STACK(VkWriteDescriptorSet, writeDescriptorSetUbo);
    writeDescriptorSetUbo.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSetUbo.dstSet          = surfaceContext->descriptorSets[surfaceContext->frameSwapChainImageIndex];
    writeDescriptorSetUbo.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSetUbo.descriptorCount = 1;
    writeDescriptorSetUbo.pBufferInfo     = &descriptorBufferInfoUbo;
    vkUpdateDescriptorSets(context->device, 1, &writeDescriptorSetUbo, 0, NULL);
}
korl_internal void korl_vulkan_useImageAssetAsTexture(const wchar_t* assetName)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /* check and see if the asset is already loaded as a device texture */
    u$ deviceAssetIndexLoaded = 0;
    for(; deviceAssetIndexLoaded < KORL_MEMORY_POOL_SIZE(context->deviceAssets); ++deviceAssetIndexLoaded)
        if(korl_memory_stringCompare(context->deviceAssets[deviceAssetIndexLoaded].name, assetName) == 0)
            break;
    /* if it is, select this texture for later use and return */
    if(deviceAssetIndexLoaded < KORL_MEMORY_POOL_SIZE(context->deviceAssets))
    {
        korl_assert(!"@todo: set this texture device asset for drawing");
        return;
    }
    /* request the image asset from the asset manager */
    Korl_AssetCache_AssetData assetData = korl_assetCache_get(assetName, KORL_ASSETCACHE_GET_FLAGS_LAZY);
    /* if the asset isn't loaded we can stop here */
    if(assetData.data == NULL)
        return;
    /* decode the raw image data from the asset */
    int imageSizeX = 0, imageSizeY = 0, imageChannels = 0;
    stbi_uc*const imagePixels = stbi_load_from_memory(assetData.data, assetData.dataBytes, &imageSizeX, &imageSizeY, &imageChannels, STBI_rgb_alpha);
    if(!imagePixels)
    {
        korl_log(ERROR, "stbi_load_from_memory failed! (%S)", assetName);
        return;
    }
    /* allocate a device texture object */
    korl_assert(!"@todo");
    /* upload the raw image data to the device texture object */
    korl_assert(!"@todo");
    /* free the raw image data */
    stbi_image_free(imagePixels);
}
