#include "kAllocatorLinear.h"
internal KAllocatorLinear* kalInit(
	void* allocatorMemoryStart, size_t allocatorByteCount)
{
	KAllocatorLinear*const kal = 
		static_cast<KAllocatorLinear*>(allocatorMemoryStart);
	kassert(allocatorByteCount > sizeof(*kal));
	*kal = 
		{ .type               = KAllocatorType::LINEAR
		, .memoryStart        = kal + 1
		, .memoryByteCount    = allocatorByteCount - sizeof(*kal)
		, .bytesAllocated     = 0
		, .lastAllocResult    = nullptr
		, .lastAllocByteCount = 0
	};
	return kal;
}
internal void* kalAlloc(KAllocatorLinear* kal, size_t allocationByteCount)
{
	if(allocationByteCount > kalMaxTotalUsableBytes(kal) ||
	   allocationByteCount <= 0)
	{
		return nullptr;
	}
	void*const result = 
		reinterpret_cast<u8*>(kal->memoryStart) + kal->bytesAllocated;
	kal->lastAllocResult = result;
	kal->lastAllocByteCount = allocationByteCount;
	kal->bytesAllocated += allocationByteCount;
	return result;
}
internal void kalFree(KAllocatorLinear* kal, void* allocatedAddress)
{
	if(allocatedAddress && allocatedAddress == kal->lastAllocResult)
	{
		kal->bytesAllocated     -= kal->lastAllocByteCount;
		kal->lastAllocResult     = nullptr;
		kal->lastAllocByteCount  = 0;
	}
	/* Otherwise, we cannot really reclaim any memory from the allocator if this 
	 * address wasn't the most recently allocated one.  The only way to reclaim 
	 * all other memory is by resetting the allocator! */
}
internal void* kalRealloc(
	KAllocatorLinear* kal, void* allocatedAddress, size_t newAllocationSize)
{
	if(allocatedAddress && allocatedAddress == kal->lastAllocResult)
	/* we can simply expand the last allocation in-place without moving it 
	 * around */
	{
		if(kal->bytesAllocated - kal->lastAllocByteCount + newAllocationSize > 
		   kalMaxTotalUsableBytes(kal))
		/* if the new allocation size will not fit, return nothing indicating we 
		 * are out of memory */
		{
			return nullptr;
		}
		kal->bytesAllocated -= kal->lastAllocByteCount;
		kal->bytesAllocated     += newAllocationSize;
		kal->lastAllocByteCount  = newAllocationSize;
		if(newAllocationSize <= 0)
		/* because of the lack of book-keeping, we can't determine the locations 
		 * of any previous allocations, so we have to set it to null */
		{
			kal->lastAllocResult    = nullptr;
			kal->lastAllocByteCount = 0;
			return nullptr;
		}
	}
	else
	/* we need to create an entirely new allocation and move the old memory 
	 * into its place (if there was an old memory location) */
	{
		const size_t lastAllocByteCount = kal->lastAllocByteCount;
		void*const newAddress = kalAlloc(kal, newAllocationSize);
		if(newAddress && allocatedAddress)
		{
			memcpy(newAddress, allocatedAddress, lastAllocByteCount);
		}
	}
	return kal->lastAllocResult;
}
internal void kalReset(KAllocatorLinear* kal)
{
	kal->bytesAllocated = 0;
	kal->lastAllocResult = nullptr;
	kal->lastAllocByteCount = 0;
}
internal size_t kalUsedBytes(KAllocatorLinear* kal)
{
	return kal->bytesAllocated;
}
internal size_t kalMaxTotalUsableBytes(KAllocatorLinear* kal)
{
	return kal->memoryByteCount - kal->bytesAllocated;
}
