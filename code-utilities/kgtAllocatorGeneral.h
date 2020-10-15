#pragma once
#include "kutil.h"
#include "kgtAllocator.h"
/** Kyle's General Allocator */
struct KgtAllocatorGeneralChunk
{
	KgtAllocatorGeneralChunk* chunkPrev;
	KgtAllocatorGeneralChunk* chunkNext;
	/** represents total available memory EXCLUDING the chunk header! */
	size_t bytes;
	bool allocated;
	u8 allocated_PADDING[7];
};
struct KgtAllocatorGeneral
{
	KgtAllocatorType type;
	/** 
	 * Represents total available fragmented memory EXCLUDING allocator header 
	 * structs.  This is the theoretical maximum amount of data we have left to
	 * allocate (excluding possible additional chunk headers) assuming no more 
	 * fragmentation occurs.
	*/
	size_t freeBytes;
	/** 
	 * Only represents the total memory assuming there is a SINGLE chunk.  This 
	 * is the theoretical maximum amount of data the allocator can allocate.
	*/
	size_t totalBytes;
	size_t totalChunks;
	KgtAllocatorGeneralChunk* firstChunk;
};
internal KgtAllocatorGeneral* 
	kgtAllocGeneralInit(
		void* allocatorMemoryLocation, size_t allocatorByteCount);
internal void* 
	kgtAllocGeneralAlloc(KgtAllocatorGeneral* kga, size_t allocationByteCount);
internal void* 
	kgtAllocGeneralRealloc(
		KgtAllocatorGeneral* kga, void* allocatedAddress, 
		size_t newAllocationSize);
internal void 
	kgtAllocGeneralFree(KgtAllocatorGeneral* kga, void* allocatedAddress);
internal size_t 
	kgtAllocGeneralUsedBytes(KgtAllocatorGeneral* kga);
internal size_t 
	kgtAllocGeneralMaxTotalUsableBytes(KgtAllocatorGeneral* kga);
