#pragma once
#include "kutil.h"
#include "kgtAllocator.h"
struct KgtAllocatorLinear
{
	KgtAllocatorType type;
	void* memoryStart;
	size_t memoryByteCount;
	size_t bytesAllocated;
	void* lastAllocResult;
};
internal KgtAllocatorLinear* 
	kgtAllocLinearInit(
		void* allocatorMemoryStart, size_t allocatorByteCount);
internal void* 
	kgtAllocLinearAlloc(KgtAllocatorLinear* kal, size_t allocationByteCount);
internal void 
	kgtAllocLinearFree(KgtAllocatorLinear* kal, void* allocatedAddress);
internal void* 
	kgtAllocLinearRealloc(
		KgtAllocatorLinear* kal, void* allocatedAddress, 
		size_t newAllocationSize);
internal void 
	kgtAllocLinearReset(KgtAllocatorLinear* kal);
internal size_t 
	kgtAllocLinearUsedBytes(KgtAllocatorLinear* kal);
internal size_t 
	kgtAllocLinearMaxTotalUsableBytes(KgtAllocatorLinear* kal);
