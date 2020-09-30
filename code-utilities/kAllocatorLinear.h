#pragma once
#include "kutil.h"
#include "kAllocator.h"
struct KAllocatorLinear
{
	KAllocatorType type;
	void* memoryStart;
	size_t memoryByteCount;
	size_t bytesAllocated;
	void* lastAllocResult;
};
internal KAllocatorLinear* kalInit(
	void* allocatorMemoryStart, size_t allocatorByteCount);
internal void* kalAlloc(KAllocatorLinear* kal, size_t allocationByteCount);
internal void kalFree(KAllocatorLinear* kal, void* allocatedAddress);
internal void* kalRealloc(
	KAllocatorLinear* kal, void* allocatedAddress, size_t newAllocationSize);
internal void kalReset(KAllocatorLinear* kal);
internal size_t kalUsedBytes(KAllocatorLinear* kal);
internal size_t kalMaxTotalUsableBytes(KAllocatorLinear* kal);
