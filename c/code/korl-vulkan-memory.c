#include "korl-vulkan-memory.h"
#include "korl-vulkan-common.h"
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
    case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE:{
        vkDestroyImage    (context->device, allocation->deviceObject.texture.image    , context->allocator);
        vkDestroyImageView(context->device, allocation->deviceObject.texture.imageView, context->allocator);
        vkDestroySampler  (context->device, allocation->deviceObject.texture.sampler  , context->allocator);
        } break;
    case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_IMAGE_BUFFER:{
        vkDestroyImage    (context->device, allocation->deviceObject.imageBuffer.image    , context->allocator);
        vkDestroyImageView(context->device, allocation->deviceObject.imageBuffer.imageView, context->allocator);
        } break;
    /* add code to handle new device object types here!
        @vulkan-device-allocation-type */
    default:
        korl_assert(!"device object type not implemented!");
    }
    korl_memory_nullify(allocation, sizeof(*allocation));
}
korl_internal void _korl_vulkan_deviceMemoryLinear_create(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, 
    VkMemoryPropertyFlagBits memoryPropertyFlags, 
    VkBufferUsageFlags bufferUsageFlags, 
    VkImageUsageFlags imageUsageFlags, 
    VkDeviceSize bytes)
{
    /* sanity check - ensure the deviceMemoryLinear is already nullified */
    korl_assert(korl_memory_isNull(deviceMemoryLinear, sizeof(*deviceMemoryLinear)));
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /** this will store the accumulation of all necessary memory types for this 
     * allocator, which is based on the provided *UsageFlags parameters */
    u32 memoryTypeBits = 0;
    /* Create a dummy buffer using the buffer usage flags.  According to sources 
        I've read on this subject, this should allow us to allocate device 
        memory that can accomodate buffers made with any subset of these usage 
        flags in the future.  This Vulkan API idiom is, in my opinion, very 
        obtuse/unintuitive, but whatever!  
        Source: https://stackoverflow.com/a/55456540 */
    if(bufferUsageFlags)
    {
        VkBuffer dummyBuffer;
        KORL_ZERO_STACK(VkBufferCreateInfo, bufferCreateInfo);
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size  = 1;
        bufferCreateInfo.usage = bufferUsageFlags;
        _KORL_VULKAN_CHECK(vkCreateBuffer(context->device, &bufferCreateInfo, context->allocator, &dummyBuffer));
        KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirementsDummyBuffer);
        vkGetBufferMemoryRequirements(context->device, dummyBuffer, &memoryRequirementsDummyBuffer);
        vkDestroyBuffer(context->device, dummyBuffer, context->allocator);
        memoryTypeBits |= memoryRequirementsDummyBuffer.memoryTypeBits;
    }
    /* Create dummy image, query mem reqs, extract memory type bits.  See notes 
        above regarding the same procedures for VkBuffer for details.  */
    if(imageUsageFlags)
    {
        VkImage dummyImage;
        VkFormat dummyFormat = VK_FORMAT_R8G8B8A8_SRGB;
        if(imageUsageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            /** @HACK: this is a really stupid way of making sure the allocator 
             * can handle depth image buffers.  Instead of doing it this way, we 
             * should have some way of iterating over all possible format 
             * combinations which could potentially be used with this allocator 
             * and create a dummy image for each combination.  If we don't 
             * remove this hack, we will always be limited to just one type of 
             * image buffer per allocator! */
            dummyFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
        KORL_ZERO_STACK(VkImageCreateInfo, imageCreateInfo);
        imageCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width  = 1;
        imageCreateInfo.extent.height = 1;
        imageCreateInfo.extent.depth  = 1;
        imageCreateInfo.mipLevels     = 1;
        imageCreateInfo.arrayLayers   = 1;
        imageCreateInfo.format        = dummyFormat;
        imageCreateInfo.usage         = imageUsageFlags;
        imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        // all these options are already defaulted to 0 //
        //imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        //imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        //imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        _KORL_VULKAN_CHECK(vkCreateImage(context->device, &imageCreateInfo, context->allocator, &dummyImage));
        KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirements);
        vkGetImageMemoryRequirements(context->device, dummyImage, &memoryRequirements);
        vkDestroyImage(context->device, dummyImage, context->allocator);
        memoryTypeBits |= memoryRequirements.memoryTypeBits;
    }
    /* put additional device object memory type bit queries here 
        (don't forget to add the usage flags as a parameter)
        @vulkan-device-allocation-type */
    /* now we can create the device memory used in the allocator */
    KORL_ZERO_STACK(VkMemoryAllocateInfo, allocateInfo);
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize  = bytes;
    allocateInfo.memoryTypeIndex = _korl_vulkan_findMemoryType(memoryTypeBits, memoryPropertyFlags);
    _KORL_VULKAN_CHECK(
        vkAllocateMemory(
            context->device, &allocateInfo, context->allocator, &deviceMemoryLinear->deviceMemory));
    /* initialize the rest of the allocator */
    deviceMemoryLinear->byteSize            = bytes;
    deviceMemoryLinear->memoryPropertyFlags = memoryPropertyFlags;
    deviceMemoryLinear->memoryTypeBits      = memoryTypeBits;
}
korl_internal void _korl_vulkan_deviceMemoryLinear_destroy(_Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    for(unsigned a = 0; a < KORL_MEMORY_POOL_SIZE(deviceMemoryLinear->allocations); a++)
        _korl_vulkan_deviceMemory_allocation_destroy(&deviceMemoryLinear->allocations[a]);
    vkFreeMemory(context->device, deviceMemoryLinear->deviceMemory, context->allocator);
    korl_memory_nullify(deviceMemoryLinear, sizeof(*deviceMemoryLinear));
}
/**
 * Iterate over each allocation and destroy it, effectively emptying the allocator.
 */
korl_internal void _korl_vulkan_deviceMemoryLinear_clear(_Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear)
{
    for(unsigned a = 0; a < KORL_MEMORY_POOL_SIZE(deviceMemoryLinear->allocations); a++)
        _korl_vulkan_deviceMemory_allocation_destroy(&deviceMemoryLinear->allocations[a]);
    KORL_MEMORY_POOL_EMPTY(deviceMemoryLinear->allocations);
    deviceMemoryLinear->bytesAllocated = 0;
}
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemoryLinear_allocateBuffer(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, VkDeviceSize bytes, 
    VkBufferUsageFlags bufferUsageFlags, VkSharingMode sharingMode)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    korl_assert(!KORL_MEMORY_POOL_ISFULL(deviceMemoryLinear->allocations));
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
    const VkDeviceSize alignedOffset = korl_math_roundUpPowerOf2(deviceMemoryLinear->bytesAllocated, memoryRequirements.alignment);
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
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemoryLinear_allocateTexture(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, 
    u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    korl_assert(!KORL_MEMORY_POOL_ISFULL(deviceMemoryLinear->allocations));
    _Korl_Vulkan_DeviceMemory_Alloctation* result = KORL_MEMORY_POOL_ADD(deviceMemoryLinear->allocations);
    result->type = _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE;
    /* create the image with the given parameters */
    KORL_ZERO_STACK(VkImageCreateInfo, imageCreateInfo);
    imageCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width  = imageSizeX;
    imageCreateInfo.extent.height = imageSizeY;
    imageCreateInfo.extent.depth  = 1;
    imageCreateInfo.mipLevels     = 1;
    imageCreateInfo.arrayLayers   = 1;
    imageCreateInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageCreateInfo.usage         = imageUsageFlags;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    // all these options are already defaulted to 0 //
    //imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    //imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    _KORL_VULKAN_CHECK(vkCreateImage(context->device, &imageCreateInfo, context->allocator, &result->deviceObject.texture.image));
    /* obtain the device memory requirements for the image */
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirements);
    vkGetImageMemoryRequirements(context->device, result->deviceObject.texture.image, &memoryRequirements);
    /* bind the image to the device memory, making sure to obey the device 
        memory requirements 
        For some more details, see: https://stackoverflow.com/a/45459196 */
    /** @robustness: ensure that device objects respect `bufferImageGranularity`, 
                    obtained from `VkPhysicalDeviceLimits`, if we ever have NON-
                    LINEAR device objects in the same allocation (in addition to 
                    respecting memory alignment requirements, of course).  */
    const VkDeviceSize alignedOffset = korl_math_roundUpPowerOf2(deviceMemoryLinear->bytesAllocated, memoryRequirements.alignment);
    // ensure the allocation can fit in the allocator!  Maybe handle this gracefully in the future?
    korl_assert(alignedOffset + memoryRequirements.size <= deviceMemoryLinear->byteSize);
    _KORL_VULKAN_CHECK(vkBindImageMemory(context->device, result->deviceObject.texture.image, deviceMemoryLinear->deviceMemory, alignedOffset));
    /* update the rest of the allocations meta data */
    result->byteOffset = alignedOffset;
    result->byteSize   = memoryRequirements.size;
    /* update allocator book keeping */
    deviceMemoryLinear->bytesAllocated = alignedOffset + memoryRequirements.size;
    /* create the image view for the image */
    KORL_ZERO_STACK(VkImageViewCreateInfo, createInfoImageView);
    createInfoImageView.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfoImageView.image                           = result->deviceObject.texture.image;
    createInfoImageView.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    createInfoImageView.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    createInfoImageView.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfoImageView.subresourceRange.baseMipLevel   = 0;
    createInfoImageView.subresourceRange.levelCount     = 1;
    createInfoImageView.subresourceRange.baseArrayLayer = 0;
    createInfoImageView.subresourceRange.layerCount     = 1;
    _KORL_VULKAN_CHECK(vkCreateImageView(context->device, &createInfoImageView, context->allocator, &result->deviceObject.texture.imageView));
    /* create the sampler for the image */
    // first obtain physical device properties to get the max anisotropy value
    KORL_ZERO_STACK(VkPhysicalDeviceProperties, physicalDeviceProperties);
    vkGetPhysicalDeviceProperties(context->physicalDevice, &physicalDeviceProperties);
    KORL_ZERO_STACK(VkSamplerCreateInfo, createInfoSampler);
    createInfoSampler.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfoSampler.magFilter               = VK_FILTER_LINEAR;
    createInfoSampler.minFilter               = VK_FILTER_LINEAR;
    createInfoSampler.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfoSampler.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfoSampler.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfoSampler.anisotropyEnable        = VK_TRUE;
    createInfoSampler.maxAnisotropy           = physicalDeviceProperties.limits.maxSamplerAnisotropy;/// @performance: setting this to a lower value will trade off performance for quality
    createInfoSampler.borderColor             = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    createInfoSampler.unnormalizedCoordinates = VK_FALSE;
    createInfoSampler.compareEnable           = VK_FALSE;
    createInfoSampler.compareOp               = VK_COMPARE_OP_ALWAYS;
    createInfoSampler.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfoSampler.mipLodBias              = 0.0f;
    createInfoSampler.minLod                  = 0.0f;
    createInfoSampler.maxLod                  = 0.0f;
    _KORL_VULKAN_CHECK(vkCreateSampler(context->device, &createInfoSampler, context->allocator, &result->deviceObject.texture.sampler));
    return result;
}
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemoryLinear_allocateImageBuffer(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, 
    u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags, 
    VkImageAspectFlags imageAspectFlags, VkFormat imageFormat, 
    VkImageTiling imageTiling)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    korl_assert(!KORL_MEMORY_POOL_ISFULL(deviceMemoryLinear->allocations));
    _Korl_Vulkan_DeviceMemory_Alloctation* result = KORL_MEMORY_POOL_ADD(deviceMemoryLinear->allocations);
    result->type = _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_IMAGE_BUFFER;
    /* create the image with the given parameters */
    KORL_ZERO_STACK(VkImageCreateInfo, imageCreateInfo);
    imageCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width  = imageSizeX;
    imageCreateInfo.extent.height = imageSizeY;
    imageCreateInfo.extent.depth  = 1;
    imageCreateInfo.mipLevels     = 1;
    imageCreateInfo.arrayLayers   = 1;
    imageCreateInfo.format        = imageFormat;
    imageCreateInfo.usage         = imageUsageFlags;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling        = imageTiling;
    // all these options are already defaulted to 0 //
    //imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    _KORL_VULKAN_CHECK(vkCreateImage(context->device, &imageCreateInfo, context->allocator, &result->deviceObject.texture.image));
    /* obtain the device memory requirements for the image */
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirements);
    vkGetImageMemoryRequirements(context->device, result->deviceObject.texture.image, &memoryRequirements);
    /* bind the image to the device memory, making sure to obey the device 
        memory requirements 
        For some more details, see: https://stackoverflow.com/a/45459196 */
    /** @robustness: ensure that device objects respect `bufferImageGranularity`, 
                    obtained from `VkPhysicalDeviceLimits`, if we ever have NON-
                    LINEAR device objects in the same allocation (in addition to 
                    respecting memory alignment requirements, of course).  */
    const VkDeviceSize alignedOffset = korl_math_roundUpPowerOf2(deviceMemoryLinear->bytesAllocated, memoryRequirements.alignment);
    // ensure the allocation can fit in the allocator!  Maybe handle this gracefully in the future?
    korl_assert(alignedOffset + memoryRequirements.size <= deviceMemoryLinear->byteSize);
    _KORL_VULKAN_CHECK(vkBindImageMemory(context->device, result->deviceObject.texture.image, deviceMemoryLinear->deviceMemory, alignedOffset));
    /* update the rest of the allocations meta data */
    result->byteOffset = alignedOffset;
    result->byteSize   = memoryRequirements.size;
    /* update allocator book keeping */
    deviceMemoryLinear->bytesAllocated = alignedOffset + memoryRequirements.size;
    /* create the image view for the image */
    KORL_ZERO_STACK(VkImageViewCreateInfo, createInfoImageView);
    createInfoImageView.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfoImageView.image                           = result->deviceObject.texture.image;
    createInfoImageView.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    createInfoImageView.format                          = imageFormat;
    createInfoImageView.subresourceRange.aspectMask     = imageAspectFlags;
    createInfoImageView.subresourceRange.baseMipLevel   = 0;
    createInfoImageView.subresourceRange.levelCount     = 1;
    createInfoImageView.subresourceRange.baseArrayLayer = 0;
    createInfoImageView.subresourceRange.layerCount     = 1;
    _KORL_VULKAN_CHECK(vkCreateImageView(context->device, &createInfoImageView, context->allocator, &result->deviceObject.texture.imageView));
    return result;
}