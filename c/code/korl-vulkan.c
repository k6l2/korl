#include "korl-io.h"
#include "korl-assert.h"
#include "korl-vulkan-common.h"
#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
#endif// defined(_WIN32)
korl_global_const char* G_KORL_VULKAN_ENABLED_LAYERS[] = 
    { "VK_LAYER_KHRONOS_validation" };
korl_global_const char* G_KORL_VULKAN_DEVICE_EXTENSIONS[] = 
    { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#define KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, proc)                      \
    {                                                                          \
        context->vk##proc =                                                    \
            (PFN_vk##proc)vkGetInstanceProcAddr(context->instance, "vk"#proc); \
        korl_assert(context->vk##proc);                                        \
    }
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
    VkResult vkResult = VK_SUCCESS;
    /* don't even consider devices that don't support all the extensions we 
        need */
    VkExtensionProperties extensionProperties[256];
    KORL_ZERO_STACK(u32, extensionPropertiesSize);
    vkResult = 
        vkEnumerateDeviceExtensionProperties(
            deviceNew, NULL/*pLayerName*/, &extensionPropertiesSize, 
            NULL/*pProperties*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(extensionPropertiesSize <= korl_arraySize(extensionProperties));
    extensionPropertiesSize = korl_arraySize(extensionProperties);
    vkResult = 
        vkEnumerateDeviceExtensionProperties(
            deviceNew, NULL/*pLayerName*/, &extensionPropertiesSize, 
            extensionProperties);
    korl_assert(vkResult == VK_SUCCESS);
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
    VkResult vkResult = VK_SUCCESS;
    /* query for device-surface capabilities */
    vkResult = 
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice, surface, &result.capabilities);
    korl_assert(vkResult == VK_SUCCESS);
    /* query for device-surface formats */
    vkResult = 
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &result.formatsSize, 
            NULL/*pSurfaceFormats*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(result.formatsSize <= korl_arraySize(result.formats));
    result.formatsSize = korl_arraySize(result.formats);
    vkResult = 
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &result.formatsSize, result.formats);
    korl_assert(vkResult == VK_SUCCESS);
    /* query for device-surface present modes */
    vkResult = 
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surface, &result.presentModesSize, 
            NULL/*pPresentModes*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(result.presentModesSize <= korl_arraySize(result.presentModes));
    result.presentModesSize = korl_arraySize(result.presentModes);
    vkResult = 
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surface, &result.presentModesSize, 
            result.presentModes);
    korl_assert(vkResult == VK_SUCCESS);
    return result;
}
korl_internal _Korl_Vulkan_QueueFamilyMetaData _korl_vulkan_queueFamilyMetaData(
    VkPhysicalDevice physicalDevice, 
    const u32 queueFamilyCount, 
    const VkQueueFamilyProperties*const queueFamilies, 
    VkSurfaceKHR surface)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    VkResult vkResult = VK_SUCCESS;
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
            vkResult = 
                context->vkGetPhysicalDeviceSurfaceSupportKHR(
                    physicalDevice, q, surface, &surfaceSupport);
            korl_assert(vkResult == VK_SUCCESS);
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
    VkResult vkResult = VK_SUCCESS;
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
    vkResult = 
        vkCreateSwapchainKHR(
            context->device, &createInfoSwapChain, 
            context->allocator, &surfaceContext->swapChain);
    korl_assert(vkResult == VK_SUCCESS);
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
    vkResult = 
        vkCreateRenderPass(
            context->device, &createInfoRenderPass, context->allocator, 
            &context->renderPass);
    korl_assert(vkResult == VK_SUCCESS);
    /* get swap chain images */
    vkResult = 
        vkGetSwapchainImagesKHR(
            context->device, surfaceContext->swapChain, 
            &surfaceContext->swapChainImagesSize, NULL/*pSwapchainImages*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(surfaceContext->swapChainImagesSize <= 
        korl_arraySize(surfaceContext->swapChainImages));
    surfaceContext->swapChainImagesSize = 
        korl_arraySize(surfaceContext->swapChainImages);
    vkResult = 
        vkGetSwapchainImagesKHR(
            context->device, surfaceContext->swapChain, 
            &surfaceContext->swapChainImagesSize, 
            surfaceContext->swapChainImages);
    korl_assert(vkResult == VK_SUCCESS);
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
        vkResult = 
            vkCreateImageView(
                context->device, &createInfoImageView, context->allocator, 
                &surfaceContext->swapChainImageViews[i]);
        korl_assert(vkResult == VK_SUCCESS);
        /* create a frame buffer for all the swap chain image views */
        VkImageView frameBufferAttachments[] = 
            { surfaceContext->swapChainImageViews[i] };
        KORL_ZERO_STACK(VkFramebufferCreateInfo, createInfoFrameBuffer);
        createInfoFrameBuffer.sType      = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfoFrameBuffer.renderPass = context->renderPass;
        createInfoFrameBuffer.attachmentCount = korl_arraySize(frameBufferAttachments);
        createInfoFrameBuffer.pAttachments    = frameBufferAttachments;
        createInfoFrameBuffer.width           = surfaceContext->swapChainImageExtent.width;
        createInfoFrameBuffer.height          = surfaceContext->swapChainImageExtent.height;
        createInfoFrameBuffer.layers          = 1;
        vkResult = 
            vkCreateFramebuffer(
                context->device, &createInfoFrameBuffer, context->allocator, 
                &surfaceContext->swapChainFrameBuffers[i]);
        korl_assert(vkResult == VK_SUCCESS);
        /* initialize the swap chain fence references to nothing */
        surfaceContext->swapChainFences[i] = VK_NULL_HANDLE;
    }
    /* allocate a command buffer for each of the swap chain frame buffers */
    KORL_ZERO_STACK(VkCommandBufferAllocateInfo, allocateInfoCommandBuffers);
    allocateInfoCommandBuffers.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfoCommandBuffers.commandPool        = context->commandPoolGraphics;
    allocateInfoCommandBuffers.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfoCommandBuffers.commandBufferCount = surfaceContext->swapChainImagesSize;
    vkResult = 
        vkAllocateCommandBuffers(
            context->device, &allocateInfoCommandBuffers, 
            surfaceContext->swapChainCommandBuffers);
    korl_assert(vkResult == VK_SUCCESS);
}
korl_internal void _korl_vulkan_destroySwapChainDependencies(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    VkResult vkResult = vkDeviceWaitIdle(context->device);
    korl_assert(vkResult == VK_SUCCESS);
    vkDestroyPipeline(context->device, context->pipelineImmediateColor, context->allocator);
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
            context->device, surfaceContext->swapChainFrameBuffers[i], 
            context->allocator);
        vkDestroyImageView(
            context->device, surfaceContext->swapChainImageViews[i], 
            context->allocator);
    }
    vkDestroySwapchainKHR(
        context->device, surfaceContext->swapChain, context->allocator);
}
/**
 * move all the vertices in the batch buffer to the vulkan device
 */
korl_internal void _korl_vulkan_batchFlush(void)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    /* don't do any work if there's no work to do! */
    if(surfaceContext->batchVertexCountStaging <= 0)
        return;
    /* make sure that we don't overflow the device memory! */
    korl_assert(
        surfaceContext->batchVertexCountStaging 
        + surfaceContext->batchVertexCountDevice 
            <= _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE);
    /* record commands to transfer the data from staging to device-local memory */
    KORL_ZERO_STACK(VkCommandBufferAllocateInfo, commandBufferAllocateInfo);
    commandBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool        = context->commandPoolTransfer;
    commandBufferAllocateInfo.commandBufferCount = 1;
    KORL_ZERO_STACK(VkCommandBuffer, commandBuffer);
    vkResult = vkAllocateCommandBuffers(context->device, &commandBufferAllocateInfo, &commandBuffer);
    korl_assert(vkResult == VK_SUCCESS);
    KORL_ZERO_STACK(VkCommandBufferBeginInfo, commandBufferBeginInfo);
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResult = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    korl_assert(vkResult == VK_SUCCESS);
    KORL_ZERO_STACK(VkBufferCopy, bufferCopyPositions);
    bufferCopyPositions.srcOffset = 0;
    bufferCopyPositions.dstOffset = surfaceContext->batchVertexCountDevice *sizeof(Korl_Math_V3f32);
    bufferCopyPositions.size      = surfaceContext->batchVertexCountStaging*sizeof(Korl_Math_V3f32);
    vkCmdCopyBuffer(
        commandBuffer, 
        surfaceContext->bufferVertexBatchStagingPositions, 
        surfaceContext->bufferVertexBatchDevicePositions, 
        1, &bufferCopyPositions);
    KORL_ZERO_STACK(VkBufferCopy, bufferCopyColors);
    bufferCopyColors.srcOffset = 0;
    bufferCopyColors.dstOffset = _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*sizeof(Korl_Math_V3f32) + surfaceContext->batchVertexCountDevice*sizeof(Korl_Math_V3u8);
    bufferCopyColors.size      = surfaceContext->batchVertexCountStaging*sizeof(Korl_Math_V3u8);
    vkCmdCopyBuffer(
        commandBuffer, 
        surfaceContext->bufferVertexBatchStagingColors, 
        surfaceContext->bufferVertexBatchDeviceColors, 
        1, &bufferCopyPositions);
    vkResult = vkEndCommandBuffer(commandBuffer);
    korl_assert(vkResult == VK_SUCCESS);
    /* submit the memory transfer commands to the device */
    KORL_ZERO_STACK(VkSubmitInfo, queueSubmitInfo);
    queueSubmitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    queueSubmitInfo.commandBufferCount = 1;
    queueSubmitInfo.pCommandBuffers    = &commandBuffer;
    vkResult = vkQueueSubmit(context->queueGraphics, 1, &queueSubmitInfo, VK_NULL_HANDLE/*fence*/);
    korl_assert(vkResult == VK_SUCCESS);
    /** @bandwidth: this is very heavy handed!  It would be better if this 
     * process could happen in the background while we wait on a fence or 
     * something so that the graphicsQueue can keep going unimpeded and program 
     * execution can continue batching more vertices. */
    vkResult = vkQueueWaitIdle(context->queueGraphics);
    korl_assert(vkResult == VK_SUCCESS);
    /* release the command buffer memory back to the pool now that we're done */
    vkFreeCommandBuffers(context->device, context->commandPoolTransfer, 1, &commandBuffer);
    /* finish up; update our book-keeping */
    surfaceContext->batchVertexCountDevice += surfaceContext->batchVertexCountStaging;
    surfaceContext->batchVertexCountStaging = 0;
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
/** This API is platform-specific, and thus must be defined within the code base 
 * of whatever the current target platform is. */
korl_internal void _korl_vulkan_createSurface(void* userData);
korl_internal void korl_vulkan_construct(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    VkResult vkResult = VK_SUCCESS;
    /* nullify the context memory before doing anything for safety */
    memset(context, 0, sizeof(*context));
    /* get a list of VkLayerProperties so we can check of validation layer 
        support if needed */
    KORL_ZERO_STACK(u32, layerCount);
    vkResult = 
        vkEnumerateInstanceLayerProperties(
            &layerCount, 
            NULL/*pProperties; NULL->return # of properties in param 1*/);
    korl_assert(vkResult == VK_SUCCESS);
    VkLayerProperties layerProperties[128];
    korl_assert(layerCount <= korl_arraySize(layerProperties));
    layerCount = korl_arraySize(layerProperties);
    vkResult = 
        vkEnumerateInstanceLayerProperties(&layerCount, layerProperties);
    korl_assert(vkResult == VK_SUCCESS);
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
    vkResult = 
        vkEnumerateInstanceExtensionProperties(
            NULL/*pLayerName*/, &extensionCount, 
            NULL/*pProperties; NULL->return # of properties in param 2*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(extensionCount <= korl_arraySize(extensionProperties));
    extensionCount = korl_arraySize(extensionProperties);
    vkResult = 
        vkEnumerateInstanceExtensionProperties(
            NULL/*pLayerName*/, &extensionCount, extensionProperties);
    korl_assert(vkResult == VK_SUCCESS);
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
    vkResult = 
        vkCreateInstance(
            &createInfoInstance, context->allocator, &context->instance);
    korl_assert(vkResult == VK_SUCCESS);
    /* get instance function pointers */
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfaceSupportKHR);
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfaceFormatsKHR);
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfacePresentModesKHR);
#if KORL_DEBUG
    /* get debug extension function pointers */
    // note: there's no need to store this in the context since we're never 
    //  going to call it again //
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = 
        KORL_C_CAST(PFN_vkCreateDebugUtilsMessengerEXT, 
            vkGetInstanceProcAddr(
                context->instance, "vkCreateDebugUtilsMessengerEXT"));
    korl_assert(vkCreateDebugUtilsMessengerEXT);
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, DestroyDebugUtilsMessengerEXT);
    /* attach a VkDebugUtilsMessengerEXT to our instance */
    vkResult = 
        vkCreateDebugUtilsMessengerEXT(
            context->instance, &createInfoDebugUtilsMessenger, 
            context->allocator, &context->debugMessenger);
    korl_assert(vkResult == VK_SUCCESS);
#endif// KORL_DEBUG
}
korl_internal void korl_vulkan_destroy(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
#if KORL_DEBUG
    context->vkDestroyDebugUtilsMessengerEXT(
        context->instance, context->debugMessenger, context->allocator);
#endif// KORL_DEBUG
    vkDestroyInstance(context->instance, context->allocator);
    /* nullify the context after cleaning up properly for safety */
    memset(context, 0, sizeof(*context));
}
korl_internal void korl_vulkan_createDevice(
    void* createSurfaceUserData, u32 sizeX, u32 sizeY)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    /* we have to create the OS-specific surface before choosing a physical 
        device to create the logical device on */
    _korl_vulkan_createSurface(createSurfaceUserData);
    /* enumerate & choose a physical device */
    KORL_ZERO_STACK(u32, physicalDeviceCount);
    vkResult = 
        vkEnumeratePhysicalDevices(
            context->instance, &physicalDeviceCount, 
            NULL/*pPhysicalDevices; NULL->return the count in param 2*/);
    korl_assert(vkResult == VK_SUCCESS);
    // we need at least one physical device compatible with Vulkan
    korl_assert(physicalDeviceCount > 0);
    VkPhysicalDevice physicalDevices[16];
    korl_assert(physicalDeviceCount <= korl_arraySize(physicalDevices));
    physicalDeviceCount = korl_arraySize(physicalDevices);
    vkResult = 
        vkEnumeratePhysicalDevices(
            context->instance, &physicalDeviceCount, physicalDevices);
    korl_assert(vkResult == VK_SUCCESS);
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
        memset(createInfo, 0, sizeof(*createInfo));
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
    vkResult = 
        vkCreateDevice(
            context->physicalDevice, &createInfoDevice, 
            context->allocator, &context->device);
    korl_assert(vkResult == VK_SUCCESS);
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
    vkResult = 
        vkCreateCommandPool(
            context->device, &createInfoCommandPool, context->allocator, 
            &context->commandPoolGraphics);
    korl_assert(vkResult == VK_SUCCESS);
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
    vkResult = 
        vkCreateCommandPool(
            context->device, &createInfoCommandPool, context->allocator, 
            &context->commandPoolTransfer);
    korl_assert(vkResult == VK_SUCCESS);
    /* --- create buffers on the device to store various resources in ---
        We have two distinct usages: staging buffers which will only be the 
        source of transfer operations to the device, and device buffers which 
        will be the destination of transfer operations to the device & also be 
        used as vertex buffers. */
    // first let's declare the staging buffers //
    KORL_ZERO_STACK(VkBufferCreateInfo, createInfoBuffer);
    createInfoBuffer.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfoBuffer.size        = _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*sizeof(Korl_Math_V3f32);
    createInfoBuffer.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    createInfoBuffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkResult = 
        vkCreateBuffer(
            context->device, &createInfoBuffer, context->allocator, 
            &surfaceContext->bufferVertexBatchStagingPositions);
    korl_assert(vkResult == VK_SUCCESS);
    createInfoBuffer.size = _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*sizeof(Korl_Math_V3u8);
    vkResult = 
        vkCreateBuffer(
            context->device, &createInfoBuffer, context->allocator, 
            &surfaceContext->bufferVertexBatchStagingColors);
    korl_assert(vkResult == VK_SUCCESS);
    // now we can create the device buffers //
    createInfoBuffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    createInfoBuffer.size  = _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*sizeof(Korl_Math_V3f32);
    vkResult = 
        vkCreateBuffer(
            context->device, &createInfoBuffer, context->allocator, 
            &surfaceContext->bufferVertexBatchDevicePositions);
    korl_assert(vkResult == VK_SUCCESS);
    createInfoBuffer.size = _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*sizeof(Korl_Math_V3u8);
    vkResult = 
        vkCreateBuffer(
            context->device, &createInfoBuffer, context->allocator, 
            &surfaceContext->bufferVertexBatchDeviceColors);
    korl_assert(vkResult == VK_SUCCESS);
    /** --- allocate memory on the device to bind buffers to ---
        We need to make two allocations here now: one for the staging buffer, 
        and one for the device buffer.  We need to do this because in general 
        the most optimal memory for assets used for rendering are not directly 
        accessible (the ability to map this memory) by the host.  To ensure this 
        happens, we have to specifically query 
        \c VkPhysicalDeviceMemoryProperties for a memory type which is coherent 
        & visible to the host, as well as a memory type which is local to the 
        device. */
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirementsBufferVertexBatchStagingPositions);
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirementsBufferVertexBatchStagingColors);
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirementsBufferVertexBatchDevicePositions);
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirementsBufferVertexBatchDeviceColors);
    vkGetBufferMemoryRequirements(context->device, surfaceContext->bufferVertexBatchStagingColors   , &memoryRequirementsBufferVertexBatchStagingColors);
    vkGetBufferMemoryRequirements(context->device, surfaceContext->bufferVertexBatchStagingPositions, &memoryRequirementsBufferVertexBatchStagingPositions);
    vkGetBufferMemoryRequirements(context->device, surfaceContext->bufferVertexBatchDeviceColors    , &memoryRequirementsBufferVertexBatchDeviceColors);
    vkGetBufferMemoryRequirements(context->device, surfaceContext->bufferVertexBatchDevicePositions , &memoryRequirementsBufferVertexBatchDevicePositions);
    const uint32_t allocationMemoryTypeBitsStaging = 
        memoryRequirementsBufferVertexBatchStagingPositions.memoryTypeBits | 
        memoryRequirementsBufferVertexBatchStagingColors   .memoryTypeBits;
    const uint32_t allocationMemoryTypeBitsDevice = 
        memoryRequirementsBufferVertexBatchDevicePositions.memoryTypeBits | 
        memoryRequirementsBufferVertexBatchDeviceColors   .memoryTypeBits;
    const VkMemoryPropertyFlags allocationMemoryPropertiesStaging = 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const VkMemoryPropertyFlags allocationMemoryPropertiesDevice = 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    // first, let's allocate the host-visible STAGING device memory //
    KORL_ZERO_STACK(VkMemoryAllocateInfo, allocateInfo);
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize  = _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*(sizeof(Korl_Math_V3f32) + sizeof(Korl_Math_V3u8));
    allocateInfo.memoryTypeIndex = _korl_vulkan_findMemoryType(allocationMemoryTypeBitsStaging, allocationMemoryPropertiesStaging);
    vkResult = 
        vkAllocateMemory(
            context->device, &allocateInfo, context->allocator, 
            &surfaceContext->deviceMemoryVertexBatchStaging);
    korl_assert(vkResult == VK_SUCCESS);
    // now let's allocate the DEVICE-local device memory //
    allocateInfo.allocationSize  = _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*(sizeof(Korl_Math_V3f32) + sizeof(Korl_Math_V3u8));
    allocateInfo.memoryTypeIndex = _korl_vulkan_findMemoryType(allocationMemoryTypeBitsDevice, allocationMemoryPropertiesDevice);
    vkResult = 
        vkAllocateMemory(
            context->device, &allocateInfo, context->allocator, 
            &surfaceContext->deviceMemoryVertexBatchDevice);
    korl_assert(vkResult == VK_SUCCESS);
    /* --- bind all the vertex batch buffers to their respective memory --- */
    // bind the positions buffer at the beginning of the allocation //
    vkResult = 
        vkBindBufferMemory(
            context->device, surfaceContext->bufferVertexBatchStagingPositions, 
            surfaceContext->deviceMemoryVertexBatchStaging, 0);
    korl_assert(vkResult == VK_SUCCESS);
    // bind the color buffer immediately after the positions buffer //
    vkResult = 
        vkBindBufferMemory(
            context->device, surfaceContext->bufferVertexBatchStagingColors, 
            surfaceContext->deviceMemoryVertexBatchStaging, 
            // note that we probably should respect the VkMemoryRequirements of 
            //  this buffer (specifically alignment), but I'm lazy ;)
            _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING*sizeof(Korl_Math_V3f32));
    korl_assert(vkResult == VK_SUCCESS);
    // bind the positions buffer at the beginning of the allocation //
    vkResult = 
        vkBindBufferMemory(
            context->device, surfaceContext->bufferVertexBatchDevicePositions, 
            surfaceContext->deviceMemoryVertexBatchDevice, 0);
    korl_assert(vkResult == VK_SUCCESS);
    // bind the color buffer immediately after the positions buffer //
    vkResult = 
        vkBindBufferMemory(
            context->device, surfaceContext->bufferVertexBatchDeviceColors, 
            surfaceContext->deviceMemoryVertexBatchDevice, 
            // note that we probably should respect the VkMemoryRequirements of 
            //  this buffer (specifically alignment), but I'm lazy ;)
            _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE*sizeof(Korl_Math_V3f32));
    korl_assert(vkResult == VK_SUCCESS);
    /* now that the device is created we can create the swap chain - this also 
        requires the command pool since we need to create the graphics command 
        buffers for each element of the swap chain */
    _korl_vulkan_createSwapChain(sizeX, sizeY, &deviceSurfaceMetaDataBest);
    /* create the semaphores we will use to sync rendering operations of the 
        swap chain */
    KORL_ZERO_STACK(VkSemaphoreCreateInfo, createInfoSemaphore);
    createInfoSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    KORL_ZERO_STACK(VkFenceCreateInfo, createInfoFence);
    createInfoFence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfoFence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(size_t f = 0; f < _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES; f++)
    {
        vkResult = 
            vkCreateSemaphore(
                context->device, &createInfoSemaphore, context->allocator, 
                &surfaceContext->wipFramesSemaphoreImageAvailable[f]);
        korl_assert(vkResult == VK_SUCCESS);
        vkResult = 
            vkCreateSemaphore(
                context->device, &createInfoSemaphore, context->allocator, 
                &surfaceContext->wipFramesSemaphoreRenderDone[f]);
        korl_assert(vkResult == VK_SUCCESS);
        vkResult = 
            vkCreateFence(
                context->device, &createInfoFence, context->allocator, 
                &surfaceContext->wipFramesFence[f]);
        korl_assert(vkResult == VK_SUCCESS);
    }
    /* create pipeline layout */
    KORL_ZERO_STACK(VkPipelineLayoutCreateInfo, createInfoPipelineLayout);
    createInfoPipelineLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vkResult = 
        vkCreatePipelineLayout(
            context->device, &createInfoPipelineLayout, context->allocator, 
            &context->pipelineLayout);
    korl_assert(vkResult == VK_SUCCESS);
    /* @hack: just load shader files into memory right here; handle file IO 
        asynchronously at some point, maybe using some kind of asset manager */
    Korl_File_Result spirvImmediateColorVertex   = korl_readEntireFile(L"korl-immediate-color.vert.spv");
    Korl_File_Result spirvImmediateFragment = korl_readEntireFile(L"korl-immediate.frag.spv");
    /* create shader modules */
    KORL_ZERO_STACK(VkShaderModuleCreateInfo, createInfoShaderVert);
    createInfoShaderVert.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfoShaderVert.codeSize = spirvImmediateColorVertex.dataSize;
    createInfoShaderVert.pCode    = spirvImmediateColorVertex.data;
    vkResult = 
        vkCreateShaderModule(
            context->device, &createInfoShaderVert, context->allocator, 
            &context->shaderImmediateColorVert);
    korl_assert(vkResult == VK_SUCCESS);
    KORL_ZERO_STACK(VkShaderModuleCreateInfo, createInfoShaderFrag);
    createInfoShaderFrag.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfoShaderFrag.codeSize = spirvImmediateFragment.dataSize;
    createInfoShaderFrag.pCode    = spirvImmediateFragment.data;
    vkResult = 
        vkCreateShaderModule(
            context->device, &createInfoShaderFrag, context->allocator, 
            &context->shaderImmediateFrag);
    korl_assert(vkResult == VK_SUCCESS);
    /* and now we can just free the shader file data */
    korl_freeEntireFile(&spirvImmediateColorVertex);
    korl_freeEntireFile(&spirvImmediateFragment);
}
korl_internal void korl_vulkan_destroyDevice(void)
{
    _korl_vulkan_destroySwapChain();
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    /* destroy the surface-specific resources */
    for(size_t f = 0; f < _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES; f++)
    {
        vkDestroySemaphore(context->device, surfaceContext->wipFramesSemaphoreImageAvailable[f], context->allocator);
        vkDestroySemaphore(context->device, surfaceContext->wipFramesSemaphoreRenderDone[f], context->allocator);
        vkDestroyFence(context->device, surfaceContext->wipFramesFence[f], context->allocator);
    }
    vkDestroySurfaceKHR(context->instance, surfaceContext->surface, context->allocator);
    vkDestroyBuffer(context->device, surfaceContext->bufferVertexBatchStagingColors, context->allocator);
    vkDestroyBuffer(context->device, surfaceContext->bufferVertexBatchStagingPositions, context->allocator);
    vkDestroyBuffer(context->device, surfaceContext->bufferVertexBatchDeviceColors, context->allocator);
    vkDestroyBuffer(context->device, surfaceContext->bufferVertexBatchDevicePositions, context->allocator);
    vkFreeMemory(context->device, surfaceContext->deviceMemoryVertexBatchStaging, context->allocator);
    vkFreeMemory(context->device, surfaceContext->deviceMemoryVertexBatchDevice, context->allocator);
    memset(surfaceContext, 0, sizeof(*surfaceContext));
    /* destroy the device-specific resources */
    vkDestroyPipelineLayout(context->device, context->pipelineLayout, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderImmediateColorVert, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderImmediateFrag, context->allocator);
    vkDestroyCommandPool(context->device, context->commandPoolTransfer, context->allocator);
    vkDestroyCommandPool(context->device, context->commandPoolGraphics, context->allocator);
    vkDestroyDevice(context->device, context->allocator);
}
korl_internal void _korl_vulkan_createPipeline(void)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    /* set fixed functions & other pipeline parameters */
    KORL_ZERO_STACK_ARRAY(VkVertexInputBindingDescription, vertexInputBindings, 2);
    vertexInputBindings[0].binding   = 0;
    vertexInputBindings[0].stride    = sizeof(Korl_Math_V3f32);
    vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputBindings[1].binding   = 1;
    vertexInputBindings[1].stride    = sizeof(Korl_Math_V3u8);
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
    createInfoInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
    createInfoRasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
    createInfoRasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;//right-handed triangles!
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
    vkResult = 
        vkCreateGraphicsPipelines(
            context->device, VK_NULL_HANDLE/*pipeline cache*/, 
            1, &createInfoPipeline, context->allocator, 
            &context->pipelineImmediateColor);
}
korl_internal u32 korl_vulkan_frameBegin(const f32 clearRgb[3])
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    KORL_ZERO_STACK(u32, nextImageIndex);
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
        {
            nextImageIndex = _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE;
            goto done;
        }
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
        /** @todo: make this rebuild a generic collection of pipelines based on 
         * some meta data about them */
        _korl_vulkan_createPipeline();
        /** @todo: re-record command buffers?... (waste of time probably) */
        /* clear the deferred resize flag for the next frame */
        surfaceContext->deferredResize = false;
    }
    /* wait on the fence for the current WIP frame */
    vkResult = 
        vkWaitForFences(
            context->device, 1, 
            &surfaceContext->wipFramesFence[surfaceContext->wipFrameCurrent], 
            VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/);
    korl_assert(vkResult == VK_SUCCESS);
    /* acquire the next image from the swap chain */
    vkResult = 
        vkAcquireNextImageKHR(
            context->device, surfaceContext->swapChain, 
            UINT64_MAX/*timeout; UINT64_MAX -> disable*/, 
            surfaceContext->wipFramesSemaphoreImageAvailable[surfaceContext->wipFrameCurrent], 
            VK_NULL_HANDLE/*fence*/, &nextImageIndex);
    korl_assert(vkResult == VK_SUCCESS);
    if(surfaceContext->swapChainFences[nextImageIndex] != VK_NULL_HANDLE)
    {
        vkResult = 
            vkWaitForFences(
                context->device, 1, 
                &surfaceContext->swapChainFences[nextImageIndex], 
                VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/);
        korl_assert(vkResult == VK_SUCCESS);
    }
    surfaceContext->swapChainFences[nextImageIndex] = 
        surfaceContext->wipFramesFence[surfaceContext->wipFrameCurrent];
    /* ----- begin the swap chain command buffer for this frame ----- */
    vkResult = 
        vkResetCommandBuffer(
            surfaceContext->swapChainCommandBuffers[nextImageIndex], 0/*flags*/);
    korl_assert(vkResult == VK_SUCCESS);
    KORL_ZERO_STACK(VkCommandBufferBeginInfo, beginInfoCommandBuffer);
    beginInfoCommandBuffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfoCommandBuffer.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResult = 
        vkBeginCommandBuffer(
            surfaceContext->swapChainCommandBuffers[nextImageIndex], 
            &beginInfoCommandBuffer);
    korl_assert(vkResult == VK_SUCCESS);
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
    beginInfoRenderPass.framebuffer       = surfaceContext->swapChainFrameBuffers[nextImageIndex];
    beginInfoRenderPass.renderArea.extent = surfaceContext->swapChainImageExtent;
    beginInfoRenderPass.clearValueCount   = 1;
    beginInfoRenderPass.pClearValues      = &clearValue;
    vkCmdBeginRenderPass(
        surfaceContext->swapChainCommandBuffers[nextImageIndex], 
        &beginInfoRenderPass, VK_SUBPASS_CONTENTS_INLINE);
done:
    /* help to ensure the user is calling frameBegin & frameEnd in the correct 
        order & the correct frequency */
    korl_assert(surfaceContext->frameStackCounter == 0);
    surfaceContext->frameStackCounter++;
    /* clear the vertex batch metrics for the upcoming frame */
    surfaceContext->batchVertexCountStaging = 0;
    surfaceContext->batchVertexCountDevice  = 0;
    return nextImageIndex;
}
korl_internal void korl_vulkan_frameEnd(u32 nextImageIndex)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    /* if we got an invalid next swapchain image index, just do nothing */
    if(nextImageIndex >= _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE)
        return;
    /* flush any batched vertices in staging, and if the device has any batched 
        vertices we should then execute the actual commands to draw them */
    _korl_vulkan_batchFlush();
    if(surfaceContext->batchVertexCountDevice)
    {
        /* submit the commands to finally draw batched vertices */
        vkCmdBindPipeline(
            surfaceContext->swapChainCommandBuffers[nextImageIndex], 
            VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipelineImmediateColor);
        VkBuffer batchVertexBuffers[] = 
            { surfaceContext->bufferVertexBatchDevicePositions
            , surfaceContext->bufferVertexBatchDeviceColors };
        VkDeviceSize batchVertexBufferOffsets[] = 
            { 0, 0 };
        vkCmdBindVertexBuffers(
            surfaceContext->swapChainCommandBuffers[nextImageIndex], 
            0/*first binding*/, 
            korl_arraySize(batchVertexBuffers), batchVertexBuffers, 
            batchVertexBufferOffsets);
        vkCmdDraw(
            surfaceContext->swapChainCommandBuffers[nextImageIndex], 
            surfaceContext->batchVertexCountDevice, 1, 0, 0);
    }
    vkCmdEndRenderPass(surfaceContext->swapChainCommandBuffers[nextImageIndex]);
    vkResult = 
        vkEndCommandBuffer(surfaceContext->swapChainCommandBuffers[nextImageIndex]);
    korl_assert(vkResult == VK_SUCCESS);
    /* submit graphics commands to the graphics queue */
    VkSemaphore submitGraphicsWaitSemaphores[] = 
        { surfaceContext->wipFramesSemaphoreImageAvailable[surfaceContext->wipFrameCurrent] };
    VkPipelineStageFlags submitGraphicsWaitStages[] = 
        { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore submitGraphicsSignalSemaphores[] = 
        { surfaceContext->wipFramesSemaphoreRenderDone[surfaceContext->wipFrameCurrent] };
    KORL_ZERO_STACK(VkSubmitInfo, submitInfoGraphics);
    submitInfoGraphics.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfoGraphics.waitSemaphoreCount   = korl_arraySize(submitGraphicsWaitSemaphores);
    submitInfoGraphics.pWaitSemaphores      = submitGraphicsWaitSemaphores;
    submitInfoGraphics.pWaitDstStageMask    = submitGraphicsWaitStages;
    submitInfoGraphics.commandBufferCount   = 1;
    submitInfoGraphics.pCommandBuffers      = &surfaceContext->swapChainCommandBuffers[nextImageIndex];
    submitInfoGraphics.signalSemaphoreCount = korl_arraySize(submitGraphicsSignalSemaphores);
    submitInfoGraphics.pSignalSemaphores    = submitGraphicsSignalSemaphores;
    // close the fence in preparation to submit commands to the graphics queue
    vkResult = 
        vkResetFences(
            context->device, 1, 
            &surfaceContext->wipFramesFence[surfaceContext->wipFrameCurrent]);
    korl_assert(vkResult == VK_SUCCESS);
    vkResult = 
        vkQueueSubmit(
            context->queueGraphics, 1, &submitInfoGraphics, 
            surfaceContext->wipFramesFence[surfaceContext->wipFrameCurrent]);
    korl_assert(vkResult == VK_SUCCESS);
    /* present the swap chain */
    VkSwapchainKHR presentInfoSwapChains[] = { surfaceContext->swapChain };
    KORL_ZERO_STACK(VkPresentInfoKHR, presentInfo);
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = submitGraphicsSignalSemaphores;
    presentInfo.swapchainCount     = korl_arraySize(presentInfoSwapChains);
    presentInfo.pSwapchains        = presentInfoSwapChains;
    presentInfo.pImageIndices      = &nextImageIndex;
    vkResult = vkQueuePresentKHR(context->queuePresent, &presentInfo);
    korl_assert(vkResult == VK_SUCCESS);
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
    u32 vertexCount, const Korl_Math_V3f32* positions, 
    const Korl_Math_V3u8* colors)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    u32 stagedVertices = 0;
    // make sure we aren't going to create a situation which will eventually 
    //  overflow the device-local buffer:
    korl_assert(
        vertexCount 
        + surfaceContext->batchVertexCountStaging 
        + surfaceContext->batchVertexCountDevice 
            < _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE);
    while(stagedVertices < vertexCount)
    {
        const u32 remainingVertices = vertexCount - stagedVertices;
        /* figure out how many vertices we can fit in the staging buffer */
        const unsigned stagingFree = 
            _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING - surfaceContext->batchVertexCountStaging;
        /* figure out how many vertices we can batch */
        const unsigned stagingAddition = KORL_MATH_MIN(stagingFree, remainingVertices);
        /* ----- stage this many vertices ----- */
        // stage the positions in mapped staging memory //
        KORL_ZERO_STACK(void*, mappedDeviceMemory);
        vkResult = 
            vkMapMemory(
                context->device, surfaceContext->deviceMemoryVertexBatchStaging, 
                /*offset*/surfaceContext->batchVertexCountStaging * sizeof(Korl_Math_V3f32), 
                /*bytes*/stagingAddition * sizeof(Korl_Math_V3f32), 
                0/*flags*/, &mappedDeviceMemory);
        korl_assert(vkResult == VK_SUCCESS);
        memcpy(
            mappedDeviceMemory, positions + stagedVertices, 
            stagingAddition * sizeof(Korl_Math_V3f32));
        vkUnmapMemory(context->device, surfaceContext->deviceMemoryVertexBatchStaging);
        // stage the colors in mapped staging memory //
        vkResult = 
            vkMapMemory(
                context->device, surfaceContext->deviceMemoryVertexBatchStaging, 
                /*offset*/_KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING * sizeof(Korl_Math_V3f32) 
                    + surfaceContext->batchVertexCountStaging * sizeof(Korl_Math_V3u8), 
                /*bytes*/stagingAddition * sizeof(Korl_Math_V3u8), 
                0/*flags*/, &mappedDeviceMemory);
        korl_assert(vkResult == VK_SUCCESS);
        memcpy(
            mappedDeviceMemory, colors + stagedVertices, 
            stagingAddition * sizeof(Korl_Math_V3u8));
        vkUnmapMemory(context->device, surfaceContext->deviceMemoryVertexBatchStaging);
        /* update the staging metrics */
        stagedVertices += stagingAddition;
        surfaceContext->batchVertexCountStaging += stagingAddition;
        /* if the batch staging buffer is full, flush it */
        if(surfaceContext->batchVertexCountStaging >= _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING)
            _korl_vulkan_batchFlush();
    }
}
