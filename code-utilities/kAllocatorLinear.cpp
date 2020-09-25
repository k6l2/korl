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
internal void* kalRealloc(
	KAllocatorLinear* kal, void* allocatedAddress, size_t newAllocationSize)
{
	if(allocatedAddress != kal->lastAllocResult)
	// if the allocated address isn't the last allocated address, just don't do 
	//	anything and return nullptr //
	{
		return nullptr;
	}
	if(kal->bytesAllocated - kal->lastAllocByteCount + newAllocationSize > 
	   kalMaxTotalUsableBytes(kal))
	// if the new allocation size will not fit, just do nothing //
	{
		return nullptr;
	}
	kal->bytesAllocated -= kal->lastAllocByteCount;
	kal->bytesAllocated += newAllocationSize;
	kal->lastAllocByteCount = newAllocationSize;
	if(newAllocationSize <= 0)
	{
		kal->lastAllocResult = nullptr;
		return nullptr;
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
