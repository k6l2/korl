#include "korl-vulkan-memory.h"
#include "korl-vulkan-common.h"
#include "korl-memoryPool.h"
#include "korl-stb-ds.h"
typedef struct _Korl_Vulkan_DeviceMemory_Arena
{
    VkDeviceMemory deviceMemory;
    void* hostVisibleMemory;// only used if the Arena uses VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    VkDeviceSize byteSize;
    _Korl_Vulkan_DeviceMemory_Alloctation* stbDaAllocations;
    u16* stbDaUnusedIds;
} _Korl_Vulkan_DeviceMemory_Arena;
korl_internal void _korl_vulkan_deviceMemory_arena_initialize(Korl_Memory_AllocatorHandle allocatorHandle, _Korl_Vulkan_DeviceMemory_Arena* arena)
{
    /* initialize an unallocated allocation which occupies the entire memory arena */
    mcarrsetlen(KORL_C_CAST(void*, allocatorHandle), arena->stbDaAllocations, 1);
    korl_memory_zero(&(arena->stbDaAllocations[0]), sizeof(arena->stbDaAllocations[0]));
    arena->stbDaAllocations[0].bytesOccupied = arena->byteSize;
}
korl_internal _Korl_Vulkan_DeviceMemory_Arena _korl_vulkan_deviceMemory_arena_create(Korl_Memory_AllocatorHandle allocatorHandle, VkDeviceSize bytes, u32 memoryTypeBits, VkMemoryPropertyFlags memoryPropertyFlags)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /* create the device memory used in the arena */
    VkDeviceMemory deviceMemory;
    KORL_ZERO_STACK(VkMemoryAllocateInfo, allocateInfo);
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize  = bytes;
    allocateInfo.memoryTypeIndex = _korl_vulkan_findMemoryType(memoryTypeBits, memoryPropertyFlags);
    _KORL_VULKAN_CHECK(vkAllocateMemory(context->device, &allocateInfo, context->allocator, &deviceMemory));
    /* create & initialize the arena */
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceMemory_Arena, result);
    result.deviceMemory = deviceMemory;
    result.byteSize     = bytes;
    mcarrsetcap(KORL_C_CAST(void*, allocatorHandle), result.stbDaAllocations, 128);
    _korl_vulkan_deviceMemory_arena_initialize(allocatorHandle, &result);
    mcarrsetlen(KORL_C_CAST(void*, allocatorHandle), result.stbDaUnusedIds, KORL_U16_MAX - 1/*exclude 0, since 0 => invalid id*/);
    for(u32 i = 0; i < arrlenu(result.stbDaUnusedIds); i++)
        result.stbDaUnusedIds[i] = korl_checkCast_u$_to_u16(KORL_U16_MAX - i);// start distributing lowest ids first
    /* if this Arena is host-visible, map the buffer to memory so we can 
        access it at any time */
    if(memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        _KORL_VULKAN_CHECK(vkMapMemory(context->device, deviceMemory
                                      ,/*device object offset*/0, /*size*/VK_WHOLE_SIZE, /*flags; reserved*/0
                                      ,&(result.hostVisibleMemory)));
    return result;
}
korl_internal void _korl_vulkan_deviceMemory_arena_destroy(Korl_Memory_AllocatorHandle allocatorHandle, _Korl_Vulkan_DeviceMemory_Arena* arena)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    if(arena->hostVisibleMemory)
        vkUnmapMemory(context->device, arena->deviceMemory);
    vkFreeMemory(context->device, arena->deviceMemory, context->allocator);
    mcarrfree(KORL_C_CAST(void*, allocatorHandle), arena->stbDaAllocations);
    mcarrfree(KORL_C_CAST(void*, allocatorHandle), arena->stbDaUnusedIds);
    korl_memory_zero(arena, sizeof(*arena));
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
        vkDestroyBuffer(context->device, allocation->deviceObject.buffer.vulkanBuffer, context->allocator);
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
    korl_memory_zero(allocation, sizeof(*allocation));
}
typedef struct _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked
{
    u$ arenaIndex;
    u16 allocationId;
    _Korl_Vulkan_DeviceMemory_Allocation_Type type;
} _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked;
korl_internal _Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocationHandle_pack(_Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked unpackedHandle)
{
    korl_assert(unpackedHandle.arenaIndex < KORL_U16_MAX);
    korl_assert(unpackedHandle.allocationId > 0);
    korl_assert(unpackedHandle.type != _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
    return unpackedHandle.arenaIndex 
         | (KORL_C_CAST(_Korl_Vulkan_DeviceMemory_AllocationHandle, unpackedHandle.allocationId) << 16) 
         | (KORL_C_CAST(_Korl_Vulkan_DeviceMemory_AllocationHandle, unpackedHandle.type        ) << 32);
}
korl_internal _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked _korl_vulkan_deviceMemory_allocationHandle_unpack(_Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked result;
    result.arenaIndex   = allocationHandle         & KORL_U16_MAX;
    result.allocationId = (allocationHandle >> 16) & KORL_U16_MAX;
    result.type         = KORL_C_CAST(_Korl_Vulkan_DeviceMemory_Allocation_Type, (allocationHandle >> 32) & 0x3);
    return result;
}
/**
 * Internal function to obtain an Allocation reference given arbitrary memory 
 * requirements.  
 */
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemory_allocator_allocate(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                 ,VkDeviceSize bytes
                                                                                                 ,VkMemoryRequirements memoryRequirements
                                                                                                 ,_Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked* out_allocationHandleUnpacked)
{
    VkDeviceSize alignedOffset = 0;
    u$ newAllocationArenaId = KORL_U64_MAX;
    u$ newAllocationId      = KORL_U64_MAX;
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[a]);
        for(u$ aa = 0; aa < arrlenu(arena->stbDaAllocations); aa++)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocations[aa]);
            if(allocation->type != _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
                continue;
            //KORL-ISSUE-000-000-020: ensure that device objects respect `bufferImageGranularity`
            alignedOffset = korl_math_roundUpPowerOf2(allocation->byteOffset, memoryRequirements.alignment);
            const VkDeviceSize alignedSpace = allocation->bytesOccupied - (alignedOffset - allocation->byteOffset);
            if(alignedSpace < memoryRequirements.size)
                continue;
            /* this UNALLOCATED allocation can be used to satisfy the memory requirements; 
                create & return a new allocation by subdividing allocation (if necessary) */
            newAllocationArenaId = a;
            newAllocationId      = aa;
            break;
        }
        if(newAllocationArenaId < KORL_U64_MAX)
            break;
    }
    /* if we failed to find an arena, we must attempt to create a new one */
    if(newAllocationArenaId == KORL_U64_MAX)
    {
        _Korl_Vulkan_DeviceMemory_Arena newArena = 
            _korl_vulkan_deviceMemory_arena_create(allocator->allocatorHandle, allocator->bytesPerArena
                                                  ,allocator->memoryTypeBits , allocator->memoryPropertyFlags);
        mcarrpush(KORL_C_CAST(void*, allocator->allocatorHandle), allocator->stbDaArenas, newArena);
        _Korl_Vulkan_DeviceMemory_Arena*const arena            = &arrlast(allocator->stbDaArenas);
        _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &arrlast(arena->stbDaAllocations);
        korl_assert(allocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
        //KORL-ISSUE-000-000-020: ensure that device objects respect `bufferImageGranularity`
        alignedOffset = korl_math_roundUpPowerOf2(allocation->byteOffset, memoryRequirements.alignment);
        const VkDeviceSize alignedSpace = allocation->bytesOccupied - (alignedOffset - allocation->byteOffset);
        korl_assert(alignedSpace >= memoryRequirements.size);
        newAllocationArenaId = arrlenu(allocator->stbDaArenas)  - 1;
        newAllocationId      = arrlenu(arena->stbDaAllocations) - 1;
    }
    _Korl_Vulkan_DeviceMemory_Arena*const  arena      = &(allocator->stbDaArenas[newAllocationArenaId]);
    _Korl_Vulkan_DeviceMemory_Alloctation* allocation = &(arena->stbDaAllocations[newAllocationId]);
    /* now that we have a valid arena index & allocation index; we can actually 
        claim or create the occupied allocation to utilize */
    korl_assert(arrlenu(arena->stbDaUnusedIds) > 0);// if this fails, we ran out of possible allocation ids
    out_allocationHandleUnpacked->arenaIndex   = newAllocationArenaId;
    out_allocationHandleUnpacked->allocationId = arrpop(arena->stbDaUnusedIds);// here we obtain the new handle id, but we still don't yet know the Allocation struct that will match it...
    const VkDeviceSize bytesToOccupy = alignedOffset + memoryRequirements.size - allocation->byteOffset;
    if(bytesToOccupy < allocation->bytesOccupied)
    {
        /* if we're going to occupy less bytes than what is contained 
            within `allocation`, we need to create a new allocation so 
            that we can keep track of the unused portion */
        KORL_ZERO_STACK(_Korl_Vulkan_DeviceMemory_Alloctation, tempAllocation);
        tempAllocation.byteOffset    = allocation->byteOffset;
        tempAllocation.bytesOccupied = bytesToOccupy;
        allocation->byteOffset     = tempAllocation.byteOffset + tempAllocation.bytesOccupied;
        allocation->bytesOccupied -= tempAllocation.bytesOccupied;
        mcarrins(KORL_C_CAST(void*, allocator->allocatorHandle), arena->stbDaAllocations, newAllocationId, tempAllocation);
        /* allocation is now potentially invalid if the allocations array was re-sized! */
        allocation = &(arena->stbDaAllocations[newAllocationId]);
    }
    else
        /* otherwise, we can just use the entire allocation */
        korl_assert(bytesToOccupy == allocation->bytesOccupied);
    allocation->bytesUsed = bytes;
    _Korl_Vulkan_DeviceMemory_Alloctation* resultAllocation = &(arena->stbDaAllocations[newAllocationId]);// we need to do this again because allocation is potentially now invalidated
    resultAllocation->id                = out_allocationHandleUnpacked->allocationId;// assign the new Allocation an id that matches the new handle id from earlier
    resultAllocation->byteOffsetAligned = alignedOffset;
    return resultAllocation;
}
korl_internal _Korl_Vulkan_DeviceMemory_Allocator _korl_vulkan_deviceMemory_allocator_create(Korl_Memory_AllocatorHandle allocatorHandle
                                                                                            ,_Korl_Vulkan_DeviceMemory_AllocatorType type
                                                                                            ,VkMemoryPropertyFlagBits memoryPropertyFlags
                                                                                            ,VkBufferUsageFlags bufferUsageFlags
                                                                                            ,VkImageUsageFlags imageUsageFlags
                                                                                            ,VkDeviceSize bytesPerArena)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;// lazy hack to allow me to call this without having to pass things like VkDevice, VkAllocationCallbacks, etc...
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
            //KORL-ISSUE-000-000-019: HACK!  poor allocator configuration for images
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
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceMemory_Allocator, result);
    result.type                = type;
    result.allocatorHandle     = allocatorHandle;
    result.bytesPerArena       = bytesPerArena;
    result.memoryPropertyFlags = memoryPropertyFlags;
    result.memoryTypeBits      = memoryTypeBits;
    mcarrsetcap(KORL_C_CAST(void*, allocatorHandle), result.stbDaArenas, 8);
    return result;
}
korl_internal void _korl_vulkan_deviceMemory_allocator_destroy(_Korl_Vulkan_DeviceMemory_Allocator* allocator)
{
    _korl_vulkan_deviceMemory_allocator_clear(allocator);
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
        _korl_vulkan_deviceMemory_arena_destroy(allocator->allocatorHandle, &(allocator->stbDaArenas[a]));
    mcarrfree(KORL_C_CAST(void*, allocator->allocatorHandle), allocator->stbDaArenas);
    korl_memory_zero(allocator, sizeof(*allocator));
}
korl_internal void _korl_vulkan_deviceMemory_allocator_clear(_Korl_Vulkan_DeviceMemory_Allocator* allocator)
{
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[a]);
        for(u$ aa = 0; aa < arrlenu(arena->stbDaAllocations); aa++)
            _korl_vulkan_deviceMemory_allocation_destroy(&(arena->stbDaAllocations[aa]));
        _korl_vulkan_deviceMemory_arena_initialize(allocator->allocatorHandle, arena);
    }
}
korl_internal _Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateBuffer(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                           ,VkDeviceSize bytes, VkBufferUsageFlags bufferUsageFlags, VkSharingMode sharingMode
                                                                                                           ,const wchar_t* file, int line)
{
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked, unpackedResult);
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /* create the buffer with the given parameters */
    KORL_ZERO_STACK(VkBuffer, buffer);
    KORL_ZERO_STACK(VkBufferCreateInfo, createInfoBuffer);
    createInfoBuffer.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfoBuffer.size        = bytes;
    createInfoBuffer.usage       = bufferUsageFlags;
    createInfoBuffer.sharingMode = sharingMode;
    _KORL_VULKAN_CHECK(vkCreateBuffer(context->device, &createInfoBuffer, context->allocator, &buffer));
    /* obtain the device memory requirements for the buffer */
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirements);
    vkGetBufferMemoryRequirements(context->device, buffer, &memoryRequirements);
    /* attempt to find a device memory arena capable of binding the device 
        object to using the device object's memory requirements */
    _Korl_Vulkan_DeviceMemory_Alloctation*const newAllocation = _korl_vulkan_deviceMemory_allocator_allocate(allocator, bytes, memoryRequirements, &unpackedResult);
    newAllocation->file = file;
    newAllocation->line = line;
    /* bind the buffer to the device memory, making sure to obey the device 
        memory requirements 
        For some more details, see: https://stackoverflow.com/a/45459196 */
    _KORL_VULKAN_CHECK(vkBindBufferMemory(context->device, buffer, allocator->stbDaArenas[unpackedResult.arenaIndex].deviceMemory, newAllocation->byteOffsetAligned));
    /* record the vulkan device objects in our new allocation */
    newAllocation->type                                 = _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER;
    newAllocation->deviceObject.buffer.vulkanBuffer     = buffer;
    newAllocation->deviceObject.buffer.bufferUsageFlags = bufferUsageFlags;
    unpackedResult.type = newAllocation->type;
    return _korl_vulkan_deviceMemory_allocationHandle_pack(unpackedResult);
}
korl_internal _Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateTexture(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                            ,u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags
                                                                                                            ,const wchar_t* file, int line)
{
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked, unpackedResult);
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /* create the image with the given parameters */
    KORL_ZERO_STACK(VkImage, image);
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
#if 0// all these options are already defaulted to 0 //
    imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
#endif
    _KORL_VULKAN_CHECK(vkCreateImage(context->device, &imageCreateInfo, context->allocator, &image));
    /* obtain the device memory requirements for the image */
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirements);
    vkGetImageMemoryRequirements(context->device, image, &memoryRequirements);
    /* attempt to find a device memory arena capable of binding the device 
        object to using the device object's memory requirements */
    _Korl_Vulkan_DeviceMemory_Alloctation*const newAllocation = _korl_vulkan_deviceMemory_allocator_allocate(allocator, /*not sure how else to obtain the VkImage size...*/memoryRequirements.size, memoryRequirements, &unpackedResult);
    newAllocation->file = file;
    newAllocation->line = line;
    /* bind the image to the device memory, making sure to obey the device 
        memory requirements 
        For some more details, see: https://stackoverflow.com/a/45459196 */
    _KORL_VULKAN_CHECK(vkBindImageMemory(context->device, image, allocator->stbDaArenas[unpackedResult.arenaIndex].deviceMemory, newAllocation->byteOffsetAligned));
    /* create the image view for the image */
    KORL_ZERO_STACK(VkImageView, imageView);
    KORL_ZERO_STACK(VkImageViewCreateInfo, createInfoImageView);
    createInfoImageView.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfoImageView.image                           = image;
    createInfoImageView.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    createInfoImageView.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    createInfoImageView.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfoImageView.subresourceRange.baseMipLevel   = 0;
    createInfoImageView.subresourceRange.levelCount     = 1;
    createInfoImageView.subresourceRange.baseArrayLayer = 0;
    createInfoImageView.subresourceRange.layerCount     = 1;
    _KORL_VULKAN_CHECK(vkCreateImageView(context->device, &createInfoImageView, context->allocator, &imageView));
    /* create the sampler for the image */
    // first obtain physical device properties to get the max anisotropy value
    KORL_ZERO_STACK(VkPhysicalDeviceProperties, physicalDeviceProperties);
    vkGetPhysicalDeviceProperties(context->physicalDevice, &physicalDeviceProperties);
    KORL_ZERO_STACK(VkSampler, sampler);
    KORL_ZERO_STACK(VkSamplerCreateInfo, createInfoSampler);
    createInfoSampler.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfoSampler.magFilter               = VK_FILTER_LINEAR;
    createInfoSampler.minFilter               = VK_FILTER_LINEAR;
    createInfoSampler.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfoSampler.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfoSampler.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfoSampler.anisotropyEnable        = VK_TRUE;
    createInfoSampler.maxAnisotropy           = physicalDeviceProperties.limits.maxSamplerAnisotropy;//KORL-PERFORMANCE-000-000-011: quality trade-off
    createInfoSampler.borderColor             = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    createInfoSampler.unnormalizedCoordinates = VK_FALSE;
    createInfoSampler.compareEnable           = VK_FALSE;
    createInfoSampler.compareOp               = VK_COMPARE_OP_ALWAYS;
    createInfoSampler.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfoSampler.mipLodBias              = 0.0f;
    createInfoSampler.minLod                  = 0.0f;
    createInfoSampler.maxLod                  = 0.0f;
    _KORL_VULKAN_CHECK(vkCreateSampler(context->device, &createInfoSampler, context->allocator, &sampler));
    /* record the vulkan device objects in our new allocation */
    newAllocation->type                           = _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE;
    newAllocation->deviceObject.texture.image     = image;
    newAllocation->deviceObject.texture.imageView = imageView;
    newAllocation->deviceObject.texture.sampler   = sampler;
    newAllocation->deviceObject.texture.sizeX     = imageSizeX;
    newAllocation->deviceObject.texture.sizeY     = imageSizeY;
    unpackedResult.type = newAllocation->type;
    return _korl_vulkan_deviceMemory_allocationHandle_pack(unpackedResult);
}
korl_internal _Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateImageBuffer(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                                ,u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags
                                                                                                                ,VkImageAspectFlags imageAspectFlags, VkFormat imageFormat
                                                                                                                ,VkImageTiling imageTiling
                                                                                                                ,const wchar_t* file, int line)
{
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked, unpackedResult);
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    /* create the image with the given parameters */
    KORL_ZERO_STACK(VkImage, image);
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
#if 0// all these options are already defaulted to 0 //
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
#endif
    _KORL_VULKAN_CHECK(vkCreateImage(context->device, &imageCreateInfo, context->allocator, &image));
    /* obtain the device memory requirements for the image */
    KORL_ZERO_STACK(VkMemoryRequirements, memoryRequirements);
    vkGetImageMemoryRequirements(context->device, image, &memoryRequirements);
    korl_assert(memoryRequirements.size <= allocator->bytesPerArena);
    /* attempt to find a device memory arena capable of binding the device 
        object to using the device object's memory requirements */
    _Korl_Vulkan_DeviceMemory_Alloctation*const newAllocation = _korl_vulkan_deviceMemory_allocator_allocate(allocator, /*not sure how else to obtain the VkImage size...*/memoryRequirements.size, memoryRequirements, &unpackedResult);
    newAllocation->file = file;
    newAllocation->line = line;
    /* bind the image to the device memory, making sure to obey the device 
        memory requirements 
        For some more details, see: https://stackoverflow.com/a/45459196 */
    _KORL_VULKAN_CHECK(vkBindImageMemory(context->device, image, allocator->stbDaArenas[unpackedResult.arenaIndex].deviceMemory, newAllocation->byteOffsetAligned));
    /* create the image view for the image; this _must_ be done after binding 
        the image to memory, otherwise it is a validation error */
    KORL_ZERO_STACK(VkImageView, imageView);
    KORL_ZERO_STACK(VkImageViewCreateInfo, createInfoImageView);
    createInfoImageView.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfoImageView.image                           = image;
    createInfoImageView.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    createInfoImageView.format                          = imageFormat;
    createInfoImageView.subresourceRange.aspectMask     = imageAspectFlags;
    createInfoImageView.subresourceRange.baseMipLevel   = 0;
    createInfoImageView.subresourceRange.levelCount     = 1;
    createInfoImageView.subresourceRange.baseArrayLayer = 0;
    createInfoImageView.subresourceRange.layerCount     = 1;
    _KORL_VULKAN_CHECK(vkCreateImageView(context->device, &createInfoImageView, context->allocator, &imageView));
    /* record the vulkan device objects in our new allocation */
    newAllocation->type                               = _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_IMAGE_BUFFER;
    newAllocation->deviceObject.imageBuffer.image     = image;
    newAllocation->deviceObject.imageBuffer.imageView = imageView;
    unpackedResult.type = newAllocation->type;
    return _korl_vulkan_deviceMemory_allocationHandle_pack(unpackedResult);
}
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemory_allocator_getAllocation(_Korl_Vulkan_DeviceMemory_Allocator* allocator, _Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    const _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked allocationHandleUnpacked = _korl_vulkan_deviceMemory_allocationHandle_unpack(allocationHandle);
    korl_assert(allocationHandleUnpacked.type != _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
    korl_assert(allocationHandleUnpacked.arenaIndex < arrlenu(allocator->stbDaArenas));
    _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[allocationHandleUnpacked.arenaIndex]);
    _Korl_Vulkan_DeviceMemory_Alloctation* allocation = NULL;
    for(u$ a = 0; a < arrlenu(arena->stbDaAllocations); a++)
    {
        if(arena->stbDaAllocations[a].id != allocationHandleUnpacked.allocationId)
            continue;
        if(arena->stbDaAllocations[a].type != allocationHandleUnpacked.type)
            continue;
        allocation = &(arena->stbDaAllocations[a]);
        break;
    }
    return allocation;
}
korl_internal void* _korl_vulkan_deviceMemory_allocator_getBufferHostVisibleAddress(_Korl_Vulkan_DeviceMemory_Allocator* allocator, _Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle, const _Korl_Vulkan_DeviceMemory_Alloctation* allocation)
{
    const _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked allocationHandleUnpacked = _korl_vulkan_deviceMemory_allocationHandle_unpack(allocationHandle);
    korl_assert(allocationHandleUnpacked.type != _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
    korl_assert(allocationHandleUnpacked.arenaIndex < arrlenu(allocator->stbDaArenas));
    korl_assert(allocationHandleUnpacked.allocationId == allocation->id);
    _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[allocationHandleUnpacked.arenaIndex]);
    korl_assert(allocator->memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    korl_assert(arena->hostVisibleMemory);
    return KORL_C_CAST(u8*, arena->hostVisibleMemory) + allocation->byteOffsetAligned;
}
korl_internal void _korl_vulkan_deviceMemory_allocator_free(_Korl_Vulkan_DeviceMemory_Allocator* allocator, _Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    const _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked allocationHandleUnpacked = _korl_vulkan_deviceMemory_allocationHandle_unpack(allocationHandle);
    korl_assert(allocationHandleUnpacked.type != _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
    korl_assert(allocationHandleUnpacked.arenaIndex < arrlenu(allocator->stbDaArenas));
    _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[allocationHandleUnpacked.arenaIndex]);
    for(u$ a = 0; a < arrlenu(arena->stbDaAllocations); a++)
    {
        _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocations[a]);
        if(allocation->id != allocationHandleUnpacked.allocationId)
            continue;
        korl_assert(allocation->type == allocationHandleUnpacked.type);
        mcarrpush(KORL_C_CAST(void*, allocator->allocatorHandle), arena->stbDaUnusedIds, korl_checkCast_u$_to_u16(allocation->id));
        _korl_vulkan_deviceMemory_allocation_destroy(allocation);
        korl_assert(allocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
        /* Attempt to combine this UNALLOCATED allocation with neighboring 
            allocations which are also UNALLOCATED */
        if(a < arrlenu(arena->stbDaAllocations) - 1)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const allocationNext = &(arena->stbDaAllocations[a + 1]);
            if(allocationNext->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
            {
                allocation->bytesOccupied += allocationNext->bytesOccupied;
                arrdel(arena->stbDaAllocations, a + 1);
            }
        }
        if(a > 0)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const allocationPrevious = &(arena->stbDaAllocations[a - 1]);
            if(allocationPrevious->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
            {
                allocationPrevious->bytesOccupied += allocation->bytesOccupied;
                arrdel(arena->stbDaAllocations, a);
            }
        }
        return;
    }
    korl_log(ERROR, "allocation not found in device memory allocator: {%llu, %hu, %i}", 
                    allocationHandleUnpacked.arenaIndex, 
                    allocationHandleUnpacked.allocationId, 
                    allocationHandleUnpacked.type);
}
korl_internal void _korl_vulkan_deviceMemory_allocator_logReport(_Korl_Vulkan_DeviceMemory_Allocator* allocator)
{
    VkDeviceSize occupiedBytes = 0;
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[a]);
        for(u$ aa = 0; aa < arrlenu(arena->stbDaAllocations); aa++)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocations[aa]);
            if(allocation->type != _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
                occupiedBytes += allocation->bytesOccupied;
        }
    }
    korl_log_noMeta(INFO, "╔════ 🖥 GPU Memory Report ════════════════════════════════╗");
    korl_log_noMeta(INFO, "║ arenas: %llu", arrlenu(allocator->stbDaArenas));
    korl_log_noMeta(INFO, "║ bytes used=%llu / %llu", occupiedBytes, arrlenu(allocator->stbDaArenas)*allocator->bytesPerArena);
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[a]);
        korl_log_noMeta(INFO, "║ --- Arena[%llu] --- allocations: %llu", a, arrlenu(arena->stbDaAllocations));
        for(u$ aa = 0; aa < arrlenu(arena->stbDaAllocations); aa++)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocations[aa]);
            const wchar_t* typeRawString;
            switch(allocation->type)
            {
            case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED:   typeRawString = L"UNALLOCATED";   break;
            case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER: typeRawString = L"VERTEX_BUFFER"; break;
            case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE:       typeRawString = L"TEXTURE";       break;
            case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_IMAGE_BUFFER:  typeRawString = L"IMAGE_BUFFER";  break;
            default:                                                      typeRawString = NULL;             break;
            }
            korl_log_noMeta(INFO, "║ %ws [0x%016X ~ 0x%016X](%llu bytes) \"%ws\" %ws:%i", 
                            aa == arrlenu(arena->stbDaAllocations) - 1 ? L"└" : L"├", 
                            allocation->byteOffset, allocation->byteOffset + allocation->bytesOccupied, 
                            allocation->bytesOccupied, 
                            typeRawString, 
                            allocation->file, allocation->line);
            ///@TODO: log the program address space occupied by host-visible buffer memory maps
        }
    }
    korl_log_noMeta(INFO, "╚═════ END of Memory Report ═════════════════════════════╝");
}
