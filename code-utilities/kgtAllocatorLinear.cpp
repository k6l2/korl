#include "kAllocatorLinear.h"
/* each allocation is placed in front of this header in memory */
struct KLinearAllocationHeader
{
	size_t allocationBytes;
};
internal KAllocatorLinear* kalInit(
	void* allocatorMemoryStart, size_t allocatorByteCount)
{
	KAllocatorLinear*const kal = 
		static_cast<KAllocatorLinear*>(allocatorMemoryStart);
	kassert(allocatorByteCount > sizeof(*kal));
	*kal = 
		{ .type            = KAllocatorType::LINEAR
		, .memoryStart     = kal + 1
		, .memoryByteCount = allocatorByteCount - sizeof(*kal)
		, .bytesAllocated  = 0
		, .lastAllocResult = nullptr
	};
	return kal;
}
internal void* kalAlloc(KAllocatorLinear* kal, size_t allocationByteCount)
{
	const size_t totalRequiredBytes = 
		allocationByteCount + sizeof(KLinearAllocationHeader);
	if(totalRequiredBytes > kalMaxTotalUsableBytes(kal) ||
	   allocationByteCount <= 0)
	{
		return nullptr;
	}
	KLinearAllocationHeader*const resultHeader = 
		reinterpret_cast<KLinearAllocationHeader*>(
			reinterpret_cast<u8*>(kal->memoryStart) + kal->bytesAllocated);
	resultHeader->allocationBytes = allocationByteCount;
	void*const result = resultHeader + 1;
	kal->lastAllocResult = result;
	kal->bytesAllocated += totalRequiredBytes;
	/* @TODO: clear the newly allocated memory for safety? */
	return result;
}
internal void kalFree(KAllocatorLinear* kal, void* allocatedAddress)
{
	if(allocatedAddress && allocatedAddress == kal->lastAllocResult)
	{
		const KLinearAllocationHeader*const lastAllocHeader = 
			reinterpret_cast<KLinearAllocationHeader*>(allocatedAddress) - 1;
		kal->bytesAllocated -= 
			lastAllocHeader->allocationBytes + sizeof(*lastAllocHeader);
		kal->lastAllocResult = nullptr;
		/* @TODO: wipe the free'd memory for safety? */
	}
	/* Otherwise, we cannot really reclaim any memory from the allocator if this 
	 * address wasn't the most recently allocated one.  The only way to reclaim 
	 * all other memory is by resetting the allocator! */
}
internal void* kalRealloc(
	KAllocatorLinear* kal, void* allocatedAddress, size_t newAllocationSize)
{
	void* result = allocatedAddress;
	if(allocatedAddress)
	{
		const void*const kalMemoryEnd = 
			reinterpret_cast<u8*>(kal->memoryStart) + kal->memoryByteCount;
		kassert(   allocatedAddress >= kal->memoryStart 
		        && allocatedAddress <  kalMemoryEnd);
	}
	if(allocatedAddress)
	{
		KLinearAllocationHeader*const lastAllocHeader = 
			reinterpret_cast<KLinearAllocationHeader*>(allocatedAddress) - 1;
		if(allocatedAddress == kal->lastAllocResult)
		/* we can simply expand the last allocation in-place without moving it 
		 * around */
		{
			if(newAllocationSize > lastAllocHeader->allocationBytes 
				&& newAllocationSize - lastAllocHeader->allocationBytes 
					> kalMaxTotalUsableBytes(kal))
			/* if the new allocation size will not fit, return nothing 
			 * indicating we are out of memory */
			{
				return nullptr;
			}
			kal->bytesAllocated -= lastAllocHeader->allocationBytes;
			kal->bytesAllocated += newAllocationSize;
			lastAllocHeader->allocationBytes = newAllocationSize;
			if(newAllocationSize <= 0)
			/* because of the lack of book-keeping, we can't determine the 
			 * locations of any previous allocations, so we have to set it to 
			 * null */
			{
				kal->lastAllocResult = nullptr;
				/* also free up the now useless chunk of memory occupied by the 
				 * allocation header */
				kal->bytesAllocated -= sizeof(*lastAllocHeader);
				return nullptr;
			}
		}
		else if(newAllocationSize > lastAllocHeader->allocationBytes)
		/* We can't grow allocations if they aren't the latest allocation, so if 
		 * more memory is being requested we need to make a new allocation and 
		 * move all the old memory to this new location.  Fortunately, if the 
		 * user wants to shrink the allocation we can just return & do no 
		 * work */
		{
			result = kalAlloc(kal, newAllocationSize);
			if(!result)
				return nullptr;
			memcpy(result, allocatedAddress, lastAllocHeader->allocationBytes);
		}
		/* @TODO: clear allocated memory & wipe free'd memory for safety? */
	}
	else
	/* we're effectively just calling `alloc` if we're passing a nullptr as the 
	 * allocated address, so just do that */
	{
		return kalAlloc(kal, newAllocationSize);
	}
	return result;
}
internal void kalReset(KAllocatorLinear* kal)
{
	kal->bytesAllocated = 0;
	kal->lastAllocResult = nullptr;
}
internal size_t kalUsedBytes(KAllocatorLinear* kal)
{
	return kal->bytesAllocated;
}
internal size_t kalMaxTotalUsableBytes(KAllocatorLinear* kal)
{
	return kal->memoryByteCount - kal->bytesAllocated;
}
