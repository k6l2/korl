#include "korl-vulkan-memory.h"
#include "korl-vulkan-common.h"
#include "korl-memoryPool.h"
#include "korl-stb-ds.h"
#if KORL_DEBUG
    // #define _KORL_VULKAN_DEVICEMEMORY_DEBUG_VALIDATE
#endif
typedef struct _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion
{
    VkDeviceSize byteOffset;
    VkDeviceSize bytes;
} _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion;
typedef struct _Korl_Vulkan_DeviceMemory_Arena
{
    VkDeviceMemory deviceMemory;
    void* hostVisibleMemory;// only used if the Arena uses VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    VkDeviceSize byteSize;
    _Korl_Vulkan_DeviceMemory_Alloctation* stbDaAllocationSlots;// a slot whose type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED indicates that it is unused
    u16* stbDaUnusedAllocationSlotIndices;// acceleration structure to speed up add/remove operations to stbDaAllocationSlots
    _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion* stbDaUnusedRegions;// sorted acceleration structure to speed up calls to allocate
} _Korl_Vulkan_DeviceMemory_Arena;
korl_internal void _korl_vulkan_deviceMemory_arena_initialize(Korl_Memory_AllocatorHandle allocatorHandle, _Korl_Vulkan_DeviceMemory_Arena* arena)
{
    /* initialize an unallocated allocation which occupies the entire memory arena */
    mcarrsetlen(KORL_C_CAST(void*, allocatorHandle), arena->stbDaUnusedRegions, 1);
    arrlast(arena->stbDaUnusedRegions).byteOffset = 0;
    arrlast(arena->stbDaUnusedRegions).bytes      = arena->byteSize;
    /* make sure all allocation slots are unoccupied */
    korl_memory_zero(arena->stbDaAllocationSlots, arrlenu(arena->stbDaAllocationSlots)*sizeof(*arena->stbDaAllocationSlots));
    /* initialize the list of unused allocation slot indices */
    mcarrsetlen(KORL_C_CAST(void*, allocatorHandle), arena->stbDaUnusedAllocationSlotIndices, arrlen(arena->stbDaAllocationSlots));
    for(u$ i = 0; i < arrlenu(arena->stbDaUnusedAllocationSlotIndices); i++)
        ///@TODO: reverse this order, so that when we pop we take the lower #s first!
        arena->stbDaUnusedAllocationSlotIndices[i] = korl_checkCast_u$_to_u16(i);
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
    mcarrsetlen(KORL_C_CAST(void*, allocatorHandle), result.stbDaAllocationSlots            , 128);
    mcarrsetlen(KORL_C_CAST(void*, allocatorHandle), result.stbDaUnusedAllocationSlotIndices, 128);
    mcarrsetcap(KORL_C_CAST(void*, allocatorHandle), result.stbDaUnusedRegions              , 128);
    _korl_vulkan_deviceMemory_arena_initialize(allocatorHandle, &result);
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
    mcarrfree(KORL_C_CAST(void*, allocatorHandle), arena->stbDaAllocationSlots);
    mcarrfree(KORL_C_CAST(void*, allocatorHandle), arena->stbDaUnusedAllocationSlotIndices);
    mcarrfree(KORL_C_CAST(void*, allocatorHandle), arena->stbDaUnusedRegions);
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
        vkDestroyBuffer(context->device, allocation->subType.buffer.vulkanBuffer, context->allocator);
        } break;
    case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE:{
        vkDestroyImage    (context->device, allocation->subType.texture.image    , context->allocator);
        vkDestroyImageView(context->device, allocation->subType.texture.imageView, context->allocator);
        vkDestroySampler  (context->device, allocation->subType.texture.sampler  , context->allocator);
        } break;
    case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_IMAGE_BUFFER:{
        vkDestroyImage    (context->device, allocation->subType.imageBuffer.image    , context->allocator);
        vkDestroyImageView(context->device, allocation->subType.imageBuffer.imageView, context->allocator);
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
    u16 arenaIndex;
    u16 allocationId;
    u8  type : 3;
} _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked;
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocationHandle_pack(_Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked unpackedHandle)
{
    korl_assert(unpackedHandle.arenaIndex < KORL_U16_MAX);
    korl_assert(unpackedHandle.type != _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
    return unpackedHandle.arenaIndex 
         | (KORL_C_CAST(Korl_Vulkan_DeviceMemory_AllocationHandle, unpackedHandle.allocationId) << 16) 
         | (KORL_C_CAST(Korl_Vulkan_DeviceMemory_AllocationHandle, unpackedHandle.type        ) << 32);
}
korl_internal _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked _korl_vulkan_deviceMemory_allocationHandle_unpack(Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked result;
    result.arenaIndex   = allocationHandle         & KORL_U16_MAX;
    result.allocationId = (allocationHandle >> 16) & KORL_U16_MAX;
    result.type         = KORL_C_CAST(_Korl_Vulkan_DeviceMemory_Allocation_Type, (allocationHandle >> 32) & 0x3);
    return result;
}
#ifdef _KORL_VULKAN_DEVICEMEMORY_DEBUG_VALIDATE
korl_internal void _korl_vulkan_deviceMemory_allocator_validate(_Korl_Vulkan_DeviceMemory_Allocator* allocator)
{
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        const _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[a]);
        for(u$ s = 0; s < arrlenu(arena->stbDaAllocationSlots); s++)
        {
            const _Korl_Vulkan_DeviceMemory_Alloctation*const allocationSlot = &(arena->stbDaAllocationSlots[s]);
            if(allocationSlot->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
            {
                korl_assert(allocationSlot->bytesOccupied == 0);
                korl_assert(allocationSlot->bytesUsed     == 0);
            }
            else
            {
                korl_assert(allocationSlot->bytesOccupied);
                korl_assert(allocationSlot->bytesUsed);
                korl_assert(allocationSlot->bytesUsed <= allocationSlot->bytesOccupied);
                /* ensure that allocated slots do not intersect memory regions of any other allocated slots */
                for(u$ s2 = s + 1; s2 < arrlenu(arena->stbDaAllocationSlots); s2++)
                {
                    const _Korl_Vulkan_DeviceMemory_Alloctation*const allocationSlot2 = &(arena->stbDaAllocationSlots[s2]);
                    if(allocationSlot2->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
                        continue;
                    korl_assert(!(   allocationSlot->byteOffset                                 < allocationSlot2->byteOffset + allocationSlot2->bytesOccupied
                                  && allocationSlot->byteOffset + allocationSlot->bytesOccupied > allocationSlot2->byteOffset));
                }
                /* ensure that there are no unused regions that intersect with an occupied allocation slot */
                for(u$ r = 0; r < arrlenu(arena->stbDaUnusedRegions); r++)
                {
                    const _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion*const region = &(arena->stbDaUnusedRegions[r]);
                    korl_assert(!(   region->byteOffset                 < allocationSlot->byteOffset + allocationSlot->bytesOccupied
                                  && region->byteOffset + region->bytes > allocationSlot->byteOffset));
                }
            }
        }
        /* ensure that all unused slot indices refer to slots that are actually UNALLOCATED */
        for(u$ i = 0; i < arrlenu(arena->stbDaUnusedAllocationSlotIndices); i++)
        {
            const u$ unusedIndex = arena->stbDaUnusedAllocationSlotIndices[i];
            korl_assert(unusedIndex < arrlenu(arena->stbDaAllocationSlots));
            korl_assert(arena->stbDaAllocationSlots[unusedIndex].type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
        }
        VkDeviceSize currentUnusedRegionByte = 0;// helps ensure that unused regions are sorted
        for(u$ r = 0; r < arrlenu(arena->stbDaUnusedRegions); r++)
        {
            const _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion*const region = &(arena->stbDaUnusedRegions[r]);
            korl_assert(region->bytes);
            korl_assert(region->byteOffset >= currentUnusedRegionByte);// ensure that the unused regions are sorted
            currentUnusedRegionByte = region->byteOffset;
            /* ensure that there are no unused regions that overlap this region */
            for(u$ r2 = r + 1; r2 < arrlenu(arena->stbDaUnusedRegions); r2++)
            {
                const _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion*const region2 = &(arena->stbDaUnusedRegions[r2]);
                korl_assert(!(   region->byteOffset                 < region2->byteOffset + region2->bytes
                              && region->byteOffset + region->bytes > region2->byteOffset));
            }
        }
    }
}
#endif
/**
 * Internal function to obtain an Allocation reference given arbitrary memory 
 * requirements.  
 */
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemory_allocator_allocate(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                 ,VkDeviceSize bytes
                                                                                                 ,VkMemoryRequirements memoryRequirements
                                                                                                 ,_Korl_Vulkan_DeviceMemory_Allocation_Type allocationType
                                                                                                 ,Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle
                                                                                                 ,_Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked* out_allocationHandleUnpacked)
{
    u$                                            newRegionId   = KORL_U64_MAX;
    VkDeviceSize                                  alignedOffset = 0;
    _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion* region        = NULL;
    _Korl_Vulkan_DeviceMemory_Arena*              arena         = NULL;
    _Korl_Vulkan_DeviceMemory_Alloctation*        allocation    = NULL;
    if(requiredHandle)
    {
        _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked requiredHandleUnpacked = _korl_vulkan_deviceMemory_allocationHandle_unpack(requiredHandle);
        /* create arenas until we have enough to satisfy the required handle */
        while(requiredHandleUnpacked.arenaIndex >= arrlenu(allocator->stbDaArenas))
        {
            _Korl_Vulkan_DeviceMemory_Arena newArena = 
                _korl_vulkan_deviceMemory_arena_create(allocator->allocatorHandle, allocator->bytesPerArena
                                                      ,allocator->memoryTypeBits , allocator->memoryPropertyFlags);
            mcarrpush(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), allocator->stbDaArenas, newArena);
        }
        arena = &(allocator->stbDaArenas[requiredHandleUnpacked.arenaIndex]);
        /* ensure that the arena has enough allocation slots generated */
        if(requiredHandleUnpacked.allocationId >= arrlenu(arena->stbDaAllocationSlots))
        {
            const u$ slotCountPrevious = arrlenu(arena->stbDaAllocationSlots);
            mcarrsetlen(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), arena->stbDaAllocationSlots, KORL_MATH_MAX(2*slotCountPrevious, requiredHandleUnpacked.allocationId + 1u));
            for(u$ i = slotCountPrevious; i < arrlenu(arena->stbDaAllocationSlots); i++)
                mcarrpush(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), arena->stbDaUnusedAllocationSlotIndices, korl_checkCast_u$_to_u16(i));
        }
        /* ensure that the required allocation index slot in the chosen arena is 
            actually available (unused); we can do this by finding the index of 
            the allocation slot index in the arena's stbDaUnusedAllocationSlotIndices */
        u$ unusedSlotIndexIndex = 0;
        for(; unusedSlotIndexIndex < arrlenu(arena->stbDaUnusedAllocationSlotIndices); unusedSlotIndexIndex++)
        {
            if(arena->stbDaUnusedAllocationSlotIndices[unusedSlotIndexIndex] == requiredHandleUnpacked.allocationId)
                break;
        }
        korl_assert(unusedSlotIndexIndex < arrlenu(arena->stbDaUnusedAllocationSlotIndices));
        const u16 allocationIndex = arena->stbDaUnusedAllocationSlotIndices[unusedSlotIndexIndex];
        arrdel(arena->stbDaUnusedAllocationSlotIndices, unusedSlotIndexIndex);
        /* sanity check; ensure that the allocation slot is, indeed, unoccupied */
        allocation = &(arena->stbDaAllocationSlots[allocationIndex]);
        korl_assert(allocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
        /* choose an unused memory region to place the allocation */
        for(u$ r = 0; r < arrlenu(arena->stbDaUnusedRegions); r++)
        {
            region = &(arena->stbDaUnusedRegions[r]);
            /* merge adjacent empty regions */
            while(   r + 1 < arrlenu(arena->stbDaUnusedRegions) 
                  && arena->stbDaUnusedRegions[r + 1].byteOffset == region->byteOffset + region->bytes)
            {
                region->bytes += arena->stbDaUnusedRegions[r + 1].bytes;
                arrdel(arena->stbDaUnusedRegions, r + 1);
            }
            /* constrain to memory alignment requirements */
            //KORL-ISSUE-000-000-020: ensure that device objects respect `bufferImageGranularity`
            alignedOffset = korl_math_roundUpPowerOf2(region->byteOffset, memoryRequirements.alignment);
            if(alignedOffset >= region->byteOffset + region->bytes)
                /* if the aligned offset pushes the memory requirements outside 
                    of the bounds of this region for some reason, then we can't 
                    use it */
                continue;
            const VkDeviceSize alignedSpace = region->bytes - (alignedOffset - region->byteOffset);// how much space is left in the region after considering alignment
            if(alignedSpace < memoryRequirements.size)
                continue;
            /* this region can satisfy our memory requirements */
            newRegionId = r;
            break;
        }
        korl_assert(newRegionId < KORL_U64_MAX);
        /* we can now safely assume that the allocation can be occupied */
        out_allocationHandleUnpacked->allocationId = allocationIndex;
        out_allocationHandleUnpacked->arenaIndex   = requiredHandleUnpacked.arenaIndex;
        out_allocationHandleUnpacked->type         = allocationType;
        allocation->byteOffset    = alignedOffset;
        allocation->bytesOccupied = memoryRequirements.size;
        allocation->bytesUsed     = bytes;
        allocation->type          = allocationType;
        goto occupyUnusedMemoryRegion;
    }
    /* find an arena within the allocator that has an unoccupied region that can 
        satisfy the memory requirements */
    u$ newAllocationArenaId = KORL_U64_MAX;
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        arena = &(allocator->stbDaArenas[a]);
        for(u$ r = 0; r < arrlenu(arena->stbDaUnusedRegions); r++)
        {
            region = &(arena->stbDaUnusedRegions[r]);
            /* merge adjacent empty regions */
            while(   r + 1 < arrlenu(arena->stbDaUnusedRegions) 
                  && arena->stbDaUnusedRegions[r + 1].byteOffset == region->byteOffset + region->bytes)
            {
                region->bytes += arena->stbDaUnusedRegions[r + 1].bytes;
                arrdel(arena->stbDaUnusedRegions, r + 1);
            }
            /* constrain to memory alignment requirements */
            //KORL-ISSUE-000-000-020: ensure that device objects respect `bufferImageGranularity`
            alignedOffset = korl_math_roundUpPowerOf2(region->byteOffset, memoryRequirements.alignment);
            if(alignedOffset >= region->byteOffset + region->bytes)
                /* if the aligned offset pushes the memory requirements outside 
                    of the bounds of this region for some reason, then we can't 
                    use it */
                continue;
            const VkDeviceSize alignedSpace = region->bytes - (alignedOffset - region->byteOffset);// how much space is left in the region after considering alignment
            if(alignedSpace < memoryRequirements.size)
                continue;
            /* this region can satisfy our memory requirements */
            newAllocationArenaId = a;
            newRegionId          = r;
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
        mcarrpush(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), allocator->stbDaArenas, newArena);
        arena  = &arrlast(allocator->stbDaArenas);
        region = &arrlast(arena->stbDaUnusedRegions);
        //KORL-ISSUE-000-000-020: ensure that device objects respect `bufferImageGranularity`
        alignedOffset = korl_math_roundUpPowerOf2(region->byteOffset, memoryRequirements.alignment);
        const VkDeviceSize alignedSpace = region->bytes - (alignedOffset - region->byteOffset);
        korl_assert(alignedSpace >= memoryRequirements.size);
        newAllocationArenaId = arrlenu(allocator->stbDaArenas)  - 1;
        newRegionId          = 0;
    }
    arena  = &(allocator->stbDaArenas[newAllocationArenaId]);
    region = &(arena->stbDaUnusedRegions[newRegionId]);
    /* occupy a persistent allocation slot in the chosen Arena */
    if(arrlenu(arena->stbDaUnusedAllocationSlotIndices) <= 0)// if we ran out of unused allocation slots, we need to allocate more slots
    {
        const u$ slotCountPrevious = arrlenu(arena->stbDaAllocationSlots);
        mcarrsetlen(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), arena->stbDaAllocationSlots, 2*slotCountPrevious);
        for(u$ i = slotCountPrevious; i < arrlenu(arena->stbDaAllocationSlots); i++)
            mcarrpush(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), arena->stbDaUnusedAllocationSlotIndices, korl_checkCast_u$_to_u16(i));
    }
    korl_assert(arrlenu(arena->stbDaUnusedAllocationSlotIndices) > 0);
    out_allocationHandleUnpacked->arenaIndex   = korl_checkCast_u$_to_u16(newAllocationArenaId);
    out_allocationHandleUnpacked->allocationId = arrpop(arena->stbDaUnusedAllocationSlotIndices);
    out_allocationHandleUnpacked->type         = allocationType;
    allocation = &(arena->stbDaAllocationSlots[out_allocationHandleUnpacked->allocationId]);
    korl_assert(allocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
    allocation->byteOffset    = alignedOffset;
    allocation->bytesOccupied = memoryRequirements.size;
    allocation->bytesUsed     = bytes;
    allocation->type          = allocationType;
    occupyUnusedMemoryRegion:
    /* if there is leftover unused memory after region, insert a new region */
    if(alignedOffset + memoryRequirements.size < region->byteOffset + region->bytes)
    {
        _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion newRegion;
        newRegion.byteOffset = alignedOffset + memoryRequirements.size;
        newRegion.bytes      = (region->byteOffset + region->bytes) - (alignedOffset + memoryRequirements.size);
        mcarrins(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), arena->stbDaUnusedRegions, newRegionId + 1, newRegion);
        region = &(arena->stbDaUnusedRegions[newRegionId]);// re-obtain valid unused memory region, since we potentially just re-allocated the unused regions!
    }
    /* if there is leftover space before the region, modify `region` to occupy 
        this range; otherwise, we can just delete `region` */
    if(alignedOffset > region->byteOffset)
        region->bytes = alignedOffset - region->byteOffset;
    else
        arrdel(arena->stbDaUnusedRegions, newRegionId);
#ifdef _KORL_VULKAN_DEVICEMEMORY_DEBUG_VALIDATE
    _korl_vulkan_deviceMemory_allocator_validate(allocator);
#endif
    return allocation;
}
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemory_allocator_freeShallow(_Korl_Vulkan_DeviceMemory_Allocator* allocator, Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    /* get & validate the allocation */
    const _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked allocationHandleUnpacked = _korl_vulkan_deviceMemory_allocationHandle_unpack(allocationHandle);
    korl_assert(allocationHandleUnpacked.type != _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
    korl_assert(allocationHandleUnpacked.arenaIndex < arrlenu(allocator->stbDaArenas));
    _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[allocationHandleUnpacked.arenaIndex]);
    korl_assert(allocationHandleUnpacked.allocationId < arrlenu(arena->stbDaAllocationSlots));
    _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocationSlots[allocationHandleUnpacked.allocationId]);
    korl_assert(allocation->type == allocationHandleUnpacked.type);
    /* find the sorted index where we should insert the new unused memory range */
    u$ r = 0;
    for(; r < arrlenu(arena->stbDaUnusedRegions); r++)
    {
        _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion*const region = &(arena->stbDaUnusedRegions[r]);
        /* merge adjacent empty regions */
        while(   r + 1 < arrlenu(arena->stbDaUnusedRegions) 
              && arena->stbDaUnusedRegions[r + 1].byteOffset == region->byteOffset + region->bytes)
        {
            region->bytes += arena->stbDaUnusedRegions[r + 1].bytes;
            arrdel(arena->stbDaUnusedRegions, r + 1);
        }
        /* if there is a region before this index && its _end_ byte is _above_ allocation's _start_ byte, this index is invalid */
        if(r > 0 && arena->stbDaUnusedRegions[r - 1].byteOffset + arena->stbDaUnusedRegions[r - 1].bytes > allocation->byteOffset)
            continue;
        /* if there is a region after this index && its _start_ byte is _below_ allocation's _end_ byte, this index is invalid */
        if(r + 1 < arrlenu(arena->stbDaUnusedRegions) && arena->stbDaUnusedRegions[r + 1].byteOffset < allocation->byteOffset + allocation->bytesOccupied)
            continue;
        /* otherwise, this index is where we should insert an unused memory range */
        break;
    }
    /* add a new unused memory range to the arena */
    _Korl_Vulkan_DeviceMemory_Arena_UnusedRegion newRegion;
    newRegion.byteOffset = allocation->byteOffset;
    newRegion.bytes      = allocation->bytesOccupied;
    mcarrins(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), arena->stbDaUnusedRegions, r, newRegion);
    /* add allocation's slot index back into the list of unused allocation slots managed by the arena */
    mcarrpush(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), arena->stbDaUnusedAllocationSlotIndices, allocationHandleUnpacked.allocationId);
    return allocation;
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
    mcarrsetcap(KORL_STB_DS_MC_CAST(allocatorHandle), result.stbDaArenas, 8);
    mcarrsetcap(KORL_STB_DS_MC_CAST(allocatorHandle), result.stbDaShallowFreedAllocations, 128);
    return result;
}
korl_internal void _korl_vulkan_deviceMemory_allocator_destroy(_Korl_Vulkan_DeviceMemory_Allocator* allocator)
{
    _korl_vulkan_deviceMemory_allocator_clear(allocator);
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
        _korl_vulkan_deviceMemory_arena_destroy(allocator->allocatorHandle, &(allocator->stbDaArenas[a]));
    mcarrfree(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), allocator->stbDaArenas);
    mcarrfree(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), allocator->stbDaShallowFreedAllocations);
    korl_memory_zero(allocator, sizeof(*allocator));
}
korl_internal void _korl_vulkan_deviceMemory_allocator_clear(_Korl_Vulkan_DeviceMemory_Allocator* allocator)
{
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[a]);
        for(u$ s = 0; s < arrlenu(arena->stbDaAllocationSlots); s++)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocationSlots[s]);
            if(allocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
                continue;
            _korl_vulkan_deviceMemory_allocation_destroy(allocation);
        }
        _korl_vulkan_deviceMemory_arena_initialize(allocator->allocatorHandle, arena);
    }
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateBuffer(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                          ,VkDeviceSize bytes, VkBufferUsageFlags bufferUsageFlags, VkSharingMode sharingMode
                                                                                                          ,Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle
                                                                                                          ,_Korl_Vulkan_DeviceMemory_Alloctation** out_allocation
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
    _Korl_Vulkan_DeviceMemory_Alloctation*const newAllocation = _korl_vulkan_deviceMemory_allocator_allocate(allocator, bytes, memoryRequirements, _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER, requiredHandle, &unpackedResult);
    newAllocation->file = file;
    newAllocation->line = line;
    if(out_allocation)
        *out_allocation = newAllocation;
    /* bind the buffer to the device memory, making sure to obey the device 
        memory requirements 
        For some more details, see: https://stackoverflow.com/a/45459196 */
    _KORL_VULKAN_CHECK(vkBindBufferMemory(context->device, buffer, allocator->stbDaArenas[unpackedResult.arenaIndex].deviceMemory, newAllocation->byteOffset));
    /* record the vulkan device objects in our new allocation */
    newAllocation->subType.buffer.vulkanBuffer     = buffer;
    newAllocation->subType.buffer.bufferUsageFlags = bufferUsageFlags;
    /* if the caller requires a specific handle value, we need to ensure that 
        requirement is satisfied */
    const Korl_Vulkan_DeviceMemory_AllocationHandle result = _korl_vulkan_deviceMemory_allocationHandle_pack(unpackedResult);
    if(requiredHandle)
        korl_assert(result == requiredHandle);
    return result;
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateTexture(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                           ,u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags
                                                                                                           ,Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle
                                                                                                           ,_Korl_Vulkan_DeviceMemory_Alloctation** out_allocation
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
    _Korl_Vulkan_DeviceMemory_Alloctation*const newAllocation = _korl_vulkan_deviceMemory_allocator_allocate(allocator, /*not sure how else to obtain the VkImage size...*/memoryRequirements.size, memoryRequirements, _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE, requiredHandle, &unpackedResult);
    newAllocation->file = file;
    newAllocation->line = line;
    if(out_allocation)
        *out_allocation = newAllocation;
    /* bind the image to the device memory, making sure to obey the device 
        memory requirements 
        For some more details, see: https://stackoverflow.com/a/45459196 */
    _KORL_VULKAN_CHECK(vkBindImageMemory(context->device, image, allocator->stbDaArenas[unpackedResult.arenaIndex].deviceMemory, newAllocation->byteOffset));
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
    newAllocation->subType.texture.image     = image;
    newAllocation->subType.texture.imageView = imageView;
    newAllocation->subType.texture.sampler   = sampler;
    newAllocation->subType.texture.sizeX     = imageSizeX;
    newAllocation->subType.texture.sizeY     = imageSizeY;
    /* if the caller requires a specific handle value, we need to ensure that 
        requirement is satisfied */
    const Korl_Vulkan_DeviceMemory_AllocationHandle result = _korl_vulkan_deviceMemory_allocationHandle_pack(unpackedResult);
    if(requiredHandle)
        korl_assert(result == requiredHandle);
    return result;
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateImageBuffer(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                               ,u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags
                                                                                                               ,VkImageAspectFlags imageAspectFlags, VkFormat imageFormat
                                                                                                               ,VkImageTiling imageTiling
                                                                                                               ,Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle
                                                                                                               ,_Korl_Vulkan_DeviceMemory_Alloctation** out_allocation
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
    _Korl_Vulkan_DeviceMemory_Alloctation*const newAllocation = _korl_vulkan_deviceMemory_allocator_allocate(allocator, /*not sure how else to obtain the VkImage size...*/memoryRequirements.size, memoryRequirements, _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_IMAGE_BUFFER, requiredHandle, &unpackedResult);
    newAllocation->file = file;
    newAllocation->line = line;
    if(out_allocation)
        *out_allocation = newAllocation;
    /* bind the image to the device memory, making sure to obey the device 
        memory requirements 
        For some more details, see: https://stackoverflow.com/a/45459196 */
    _KORL_VULKAN_CHECK(vkBindImageMemory(context->device, image, allocator->stbDaArenas[unpackedResult.arenaIndex].deviceMemory, newAllocation->byteOffset));
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
    newAllocation->subType.imageBuffer.image     = image;
    newAllocation->subType.imageBuffer.imageView = imageView;
    /* if the caller requires a specific handle value, we need to ensure that 
        requirement is satisfied */
    const Korl_Vulkan_DeviceMemory_AllocationHandle result = _korl_vulkan_deviceMemory_allocationHandle_pack(unpackedResult);
    if(requiredHandle)
        korl_assert(result == requiredHandle);
    return result;
}
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemory_allocator_getAllocation(_Korl_Vulkan_DeviceMemory_Allocator* allocator, Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    const _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked allocationHandleUnpacked = _korl_vulkan_deviceMemory_allocationHandle_unpack(allocationHandle);
    korl_assert(allocationHandleUnpacked.type != _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED);
    korl_assert(allocationHandleUnpacked.arenaIndex < arrlenu(allocator->stbDaArenas));
    _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[allocationHandleUnpacked.arenaIndex]);
    korl_assert(allocationHandleUnpacked.allocationId < arrlenu(arena->stbDaAllocationSlots));
    _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocationSlots[allocationHandleUnpacked.allocationId]);
    korl_assert(allocation->type == allocationHandleUnpacked.type);
    return allocation;
}
korl_internal void* _korl_vulkan_deviceMemory_allocator_getBufferHostVisibleAddress(_Korl_Vulkan_DeviceMemory_Allocator* allocator, Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = _korl_vulkan_deviceMemory_allocator_getAllocation(allocator, allocationHandle);
    korl_assert(allocator->memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    const _Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked allocationHandleUnpacked = _korl_vulkan_deviceMemory_allocationHandle_unpack(allocationHandle);
    _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[allocationHandleUnpacked.arenaIndex]);
    korl_assert(arena->hostVisibleMemory);
    return KORL_C_CAST(u8*, arena->hostVisibleMemory) + allocation->byteOffset;
}
korl_internal void _korl_vulkan_deviceMemory_allocator_free(_Korl_Vulkan_DeviceMemory_Allocator* allocator, Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    if(allocationHandle == 0)
        return;// null handle should just silently do nothing here
    _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = _korl_vulkan_deviceMemory_allocator_freeShallow(allocator, allocationHandle);
    /* fully destroy the allocation */
    _korl_vulkan_deviceMemory_allocation_destroy(allocation);
#ifdef _KORL_VULKAN_DEVICEMEMORY_DEBUG_VALIDATE
    _korl_vulkan_deviceMemory_allocator_validate(allocator);
#endif
}
korl_internal void _korl_vulkan_deviceMemory_allocator_freeShallowQueueDestroyDeviceObjects(_Korl_Vulkan_DeviceMemory_Allocator* allocator, Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
{
    if(allocationHandle == 0)
        return;// null handle should just silently do nothing here
    _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = _korl_vulkan_deviceMemory_allocator_freeShallow(allocator, allocationHandle);
    /* instead of fully destroying the allocation, we simply add the allocation 
        to a queue which we can fully destroy at a later time when the caller 
        knows that the vulkan device objects are no longer in use by a graphics queue */
    mcarrpush(KORL_STB_DS_MC_CAST(allocator->allocatorHandle), allocator->stbDaShallowFreedAllocations, *allocation);
    korl_memory_zero(allocation, sizeof(*allocation));
#ifdef _KORL_VULKAN_DEVICEMEMORY_DEBUG_VALIDATE
    _korl_vulkan_deviceMemory_allocator_validate(allocator);
#endif
}
korl_internal void _korl_vulkan_deviceMemory_allocator_freeShallowDequeue(_Korl_Vulkan_DeviceMemory_Allocator* allocator, u$ dequeueCount)
{
    korl_assert(arrlenu(allocator->stbDaShallowFreedAllocations) >= dequeueCount);
    for(u$ i = 0; i < dequeueCount; i++)
        _korl_vulkan_deviceMemory_allocation_destroy(&(allocator->stbDaShallowFreedAllocations[i]));
    arrdeln(allocator->stbDaShallowFreedAllocations, 0, dequeueCount);
}
korl_internal void _korl_vulkan_deviceMemory_allocator_logReport(_Korl_Vulkan_DeviceMemory_Allocator* allocator)
{
    VkDeviceSize occupiedBytes = 0;
    u$ occupiedAllocationSlots = 0;
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[a]);
        for(u$ s = 0; s < arrlenu(arena->stbDaAllocationSlots); s++)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocationSlots[s]);
            if(allocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
                continue;
            occupiedBytes += allocation->bytesOccupied;
            occupiedAllocationSlots++;
        }
    }
    korl_log_noMeta(INFO, "╔════ 🖥 GPU Memory Report ════════════════════════════════╗");
    korl_log_noMeta(INFO, "║ arenas: %llu", arrlenu(allocator->stbDaArenas));
    korl_log_noMeta(INFO, "║ bytes used=%llu / %llu", occupiedBytes, arrlenu(allocator->stbDaArenas)*allocator->bytesPerArena);
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[a]);
        korl_log_noMeta(INFO, "║ --- Arena[%llu] --- occupied allocations: %llu", a, occupiedAllocationSlots);
        for(u$ s = 0; s < arrlenu(arena->stbDaAllocationSlots); s++)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocationSlots[s]);
            const wchar_t* typeRawString;
            switch(allocation->type)
            {
            case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED:   typeRawString = L"UNALLOCATED";   break;
            case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER: typeRawString = L"VERTEX_BUFFER"; break;
            case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE:       typeRawString = L"TEXTURE";       break;
            case _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_IMAGE_BUFFER:  typeRawString = L"IMAGE_BUFFER";  break;
            default:                                                      typeRawString = NULL;             break;
            }
            if(allocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
                korl_log_noMeta(INFO, "║ %ws [%llu] \"%ws\""
                               ,s == arrlenu(arena->stbDaAllocationSlots) - 1 ? L"└" : L"├"
                               ,s
                               ,typeRawString);
            else
                korl_log_noMeta(INFO, "║ %ws [%llu][0x%016X ~ 0x%016X](%llu bytes) \"%ws\" %ws:%i"
                               ,s == arrlenu(arena->stbDaAllocationSlots) - 1 ? L"└" : L"├"
                               ,s
                               ,allocation->byteOffset, allocation->byteOffset + allocation->bytesOccupied
                               ,allocation->bytesOccupied
                               ,typeRawString
                               ,allocation->file, allocation->line);
            /// KORL-ISSUE-000-000-089: vulkan-memory: log the program address space occupied by host-visible buffer memory maps
        }
    }
    korl_log_noMeta(INFO, "╚═════ END of Memory Report ═════════════════════════════╝");
}
korl_internal void _korl_vulkan_deviceMemory_allocator_forEach(_Korl_Vulkan_DeviceMemory_Allocator* allocator, fnSig_korl_vulkan_deviceMemory_allocator_forEachCallback* callback, void* callbackUserData)
{
    for(u$ a = 0; a < arrlenu(allocator->stbDaArenas); a++)
    {
        _Korl_Vulkan_DeviceMemory_Arena*const arena = &(allocator->stbDaArenas[a]);
        for(u$ s = 0; s < arrlenu(arena->stbDaAllocationSlots); s++)
        {
            _Korl_Vulkan_DeviceMemory_Alloctation*const allocation = &(arena->stbDaAllocationSlots[s]);
            if(allocation->type == _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED)
                continue;
            KORL_ZERO_STACK(_Korl_Vulkan_DeviceMemory_AllocationHandleUnpacked, allocationHandleUnpacked);
            allocationHandleUnpacked.arenaIndex   = korl_checkCast_u$_to_u16(a);
            allocationHandleUnpacked.type         = allocation->type;
            allocationHandleUnpacked.allocationId = korl_checkCast_u$_to_u16(s);
            const Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle = _korl_vulkan_deviceMemory_allocationHandle_pack(allocationHandleUnpacked);
            callback(callbackUserData, allocator, allocation, allocationHandle);
        }
    }
}
