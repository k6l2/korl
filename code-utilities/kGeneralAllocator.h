#pragma once
#include "kutil.h"
#include "kAllocator.h"
/** Kyle's General Allocator */
struct KGeneralAllocatorChunk
{
	KGeneralAllocatorChunk* chunkPrev;
	KGeneralAllocatorChunk* chunkNext;
	/** represents total available memory EXCLUDING the chunk header! */
	size_t bytes;
	bool allocated;
	u8 allocated_PADDING[7];
};
struct KGeneralAllocator
{
	KAllocatorType type;
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
	KGeneralAllocatorChunk* firstChunk;
};
internal KGeneralAllocator* kgaInit(
	void* allocatorMemoryLocation, size_t allocatorByteCount);
internal void* kgaAlloc(KGeneralAllocator* kga, size_t allocationByteCount);
internal void* kgaRealloc(
	KGeneralAllocator* kga, void* allocatedAddress, size_t newAllocationSize);
internal void kgaFree(KGeneralAllocator* kga, void* allocatedAddress);
internal size_t kgaUsedBytes(KGeneralAllocator* kga);
internal size_t kgaMaxTotalUsableBytes(KGeneralAllocator* kga);
