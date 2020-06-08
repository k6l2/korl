#include "generalAllocator.h"
#include <cstring>
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
// This is just a relatively inexpensive sanity check to see if the address 
//	spaces of the chunk and its immediate neighbors lie within the total address
//	space of the allocator.  Also doing some other simple checks now... //
internal bool kgaVerifyChunk(const KGeneralAllocator* kga, 
                             const KGeneralAllocatorChunk* chunk)
{
	const size_t kgaAddressSpaceStart = reinterpret_cast<size_t>(kga);
	const size_t kgaAddressSpaceEnd   = kgaAddressSpaceStart + kga->totalBytes +
		sizeof(KGeneralAllocator) + sizeof(KGeneralAllocatorChunk);
	const size_t chunkAddress = reinterpret_cast<size_t>(chunk);
	const size_t chunkAddressEnd = 
		chunkAddress + sizeof(KGeneralAllocatorChunk) + chunk->bytes;
	if (chunkAddress < kgaAddressSpaceStart ||
		chunkAddress > 
			kgaAddressSpaceEnd - sizeof(KGeneralAllocatorChunk) - 1 ||
		chunkAddressEnd < kgaAddressSpaceStart ||
		chunkAddressEnd > kgaAddressSpaceEnd)
	{
		return false;
	}
	const size_t chunkAddressPrev = reinterpret_cast<size_t>(chunk->chunkPrev);
	const size_t chunkAddressPrevEnd = chunkAddressPrev + 
		sizeof(KGeneralAllocatorChunk) + chunk->chunkPrev->bytes;
	if (chunkAddressPrev < kgaAddressSpaceStart ||
		chunkAddressPrev > 
			kgaAddressSpaceEnd - sizeof(KGeneralAllocatorChunk) - 1 ||
		chunkAddressPrevEnd < kgaAddressSpaceStart ||
		chunkAddressPrevEnd > kgaAddressSpaceEnd)
	{
		return false;
	}
	const size_t chunkAddressNext = reinterpret_cast<size_t>(chunk->chunkNext);
	const size_t chunkAddressNextEnd = chunkAddressNext + 
		sizeof(KGeneralAllocatorChunk) + chunk->chunkNext->bytes;
	if (chunkAddressNext < kgaAddressSpaceStart ||
		chunkAddressNext > 
			kgaAddressSpaceEnd - sizeof(KGeneralAllocatorChunk) - 1 ||
		chunkAddressNextEnd < kgaAddressSpaceStart ||
		chunkAddressNextEnd > kgaAddressSpaceEnd)
	{
		return false;
	}
	// verify the integrity of the double-linked list using the `totalChunks` //
	const KGeneralAllocatorChunk* chunkNextLeft  = chunk;
	const KGeneralAllocatorChunk* chunkNextRight = chunk;
	for(size_t c = 0; c < kga->totalChunks; c++)
	{
		chunkNextLeft  = chunkNextLeft ->chunkPrev;
		chunkNextRight = chunkNextRight->chunkNext;
		if(c < kga->totalChunks - 1)
		{
			if(chunkNextLeft == chunk || chunkNextRight == chunk)
			{
				return false;
			}
		}
	}
	if(chunkNextLeft != chunk || chunkNextRight != chunk)
	{
		return false;
	}
	return true;
}
internal KgaHandle kgaInit(void* allocatorMemoryLocation, 
                           size_t allocatorByteCount)
{
	local_persist const size_t MIN_ALLOC_OVERHEAD =
		sizeof(KGeneralAllocator) + sizeof(KGeneralAllocatorChunk);
	kassert(allocatorByteCount > MIN_ALLOC_OVERHEAD);
	KGeneralAllocator*const kga = 
		reinterpret_cast<KGeneralAllocator*>(allocatorMemoryLocation);
	*kga = {};
	kga->freeBytes  = allocatorByteCount - MIN_ALLOC_OVERHEAD;
	kga->totalBytes = allocatorByteCount - MIN_ALLOC_OVERHEAD;
	kga->totalChunks = 1;
	kga->firstChunk = reinterpret_cast<KGeneralAllocatorChunk*>(
	                  reinterpret_cast<u8*>(kga) + sizeof(KGeneralAllocator));
	kga->firstChunk->chunkPrev = kga->firstChunk;
	kga->firstChunk->chunkNext = kga->firstChunk;
	kga->firstChunk->bytes     = allocatorByteCount - MIN_ALLOC_OVERHEAD;
	kga->firstChunk->allocated = false;
	kassert(kgaVerifyChunk(kga, kga->firstChunk));
	return kga;
}
internal void* kgaAlloc(KgaHandle kgaHandle, size_t allocationByteCount)
{
	if(allocationByteCount == 0)
	{
		return nullptr;
	}
	local_persist const size_t MIN_ALLOC_OVERHEAD = 
		sizeof(KGeneralAllocatorChunk);
	KGeneralAllocator*const kga = 
		reinterpret_cast<KGeneralAllocator*>(kgaHandle);
	if(kga->freeBytes < allocationByteCount)
	{
		return nullptr;
	}
	// First, we need to find the first unallocated chunk that satisfies our
	//	memory requirements //
	KGeneralAllocatorChunk* firstAvailableChunk = nullptr;
	KGeneralAllocatorChunk* nextChunk = kga->firstChunk;
	do
	{
		if(!nextChunk->allocated && 
			nextChunk->bytes >= allocationByteCount)
		{
			firstAvailableChunk = nextChunk;
			break;
		}
		nextChunk = nextChunk->chunkNext;
	} while (nextChunk != kga->firstChunk);
	if(!firstAvailableChunk)
	{
		return nullptr;
	}
	kassert(kgaVerifyChunk(kga, firstAvailableChunk));
	// Mark the available chunk as allocated & create a new empty chunk adjacent
	//	in the double-linked list of memory //
	if(firstAvailableChunk->bytes > 
		allocationByteCount + MIN_ALLOC_OVERHEAD + 1)
	{
		// we only need to add another link in the chunk chain if there is 
		//	enough memory leftover to support another chunk header PLUS some 
		//	non-zero amount of allocatable memory! //
		KGeneralAllocatorChunk*const newChunk = 
			reinterpret_cast<KGeneralAllocatorChunk*>(
				reinterpret_cast<u8*>(firstAvailableChunk) + 
				MIN_ALLOC_OVERHEAD + allocationByteCount);
		newChunk->chunkNext = firstAvailableChunk->chunkNext;
		newChunk->chunkPrev = firstAvailableChunk;
		newChunk->bytes     = firstAvailableChunk->bytes - MIN_ALLOC_OVERHEAD - 
		                      allocationByteCount;
		newChunk->allocated = false;
		firstAvailableChunk->chunkNext->chunkPrev = newChunk;
		firstAvailableChunk->chunkNext = newChunk;
		if(newChunk->chunkNext == firstAvailableChunk)
		{
			firstAvailableChunk->chunkPrev = newChunk;
		}
		firstAvailableChunk->bytes     = allocationByteCount;
		kga->freeBytes -= MIN_ALLOC_OVERHEAD;
		kga->totalChunks++;
		kassert(kgaVerifyChunk(kga, firstAvailableChunk));
	}
	firstAvailableChunk->allocated = true;
	kga->freeBytes -= allocationByteCount;
	// Clear the newly allocated space to zero for safety //
	memset(reinterpret_cast<u8*>(firstAvailableChunk) + MIN_ALLOC_OVERHEAD, 
	       0x0, firstAvailableChunk->bytes);
	return reinterpret_cast<u8*>(firstAvailableChunk) + MIN_ALLOC_OVERHEAD;
}
internal void* kgaRealloc(KgaHandle kgaHandle, void* allocatedAddress, 
                          size_t newAllocationSize)
{
	void* newAllocation = kgaAlloc(kgaHandle, newAllocationSize);
	if(!newAllocation)
	{
		if(allocatedAddress)
		{
			kgaFree(kgaHandle, allocatedAddress);
		}
		return nullptr;
	}
	if(allocatedAddress)
	{
#if INTERNAL_BUILD || SLOW_BUILD
		KGeneralAllocator*const kga = 
			reinterpret_cast<KGeneralAllocator*>(kgaHandle);
#endif// INTERNAL_BUILD || SLOW_BUILD
		KGeneralAllocatorChunk* allocatedChunk = 
			reinterpret_cast<KGeneralAllocatorChunk*>(
				reinterpret_cast<u8*>(allocatedAddress) - 
				sizeof(KGeneralAllocatorChunk));
		kassert(kgaVerifyChunk(kga, allocatedChunk));
		const size_t bytesToCopy = allocatedChunk->bytes < newAllocationSize
			? allocatedChunk->bytes : newAllocationSize;
		memcpy(newAllocation, allocatedAddress, bytesToCopy);
		kgaFree(kgaHandle, allocatedAddress);
	}
	return newAllocation;
	///TODO:
	// If new size is smaller than chunk size
	//	If leftover size meets minimum requirements for a new chunk,
	//	Then reduce chunk size, spawn a new next chunk, & merge new chunk if it
	//		is neighboring to another unallocated chunk
	//	Else just alloc, then free
	// Else If new size > chunk size
	//	If previous chunk is not allocated, set potential merge size to 
	//		include previous chunk size
	//	If new size > potential merge size, set potential merge size to include
	//		next chunk's size
	//	If new size > potential merge size,
	//		then we have no choice but to alloc, then free
}
internal void kgaFree(KgaHandle kgaHandle, void* allocatedAddress)
{
	if(!allocatedAddress)
	{
		return;
	}
	KGeneralAllocator*const kga = 
		reinterpret_cast<KGeneralAllocator*>(kgaHandle);
	KGeneralAllocatorChunk* allocatedChunk = 
		reinterpret_cast<KGeneralAllocatorChunk*>(
			reinterpret_cast<u8*>(allocatedAddress) - 
			sizeof(KGeneralAllocatorChunk));
	kassert(kgaVerifyChunk(kga, allocatedChunk));
	allocatedChunk->allocated = false;
	kga->freeBytes += allocatedChunk->bytes;
	// destroy this chunk & merge with the previous chunk if it is also free //
	if (allocatedChunk->chunkPrev != allocatedChunk &&
		!allocatedChunk->chunkPrev->allocated &&
		// DO NOT EVER DESTROY THE FIRST CHUNK!!!
		allocatedChunk != kga->firstChunk)
	{
		allocatedChunk->chunkPrev->chunkNext = allocatedChunk->chunkNext;
		allocatedChunk->chunkNext->chunkPrev = allocatedChunk->chunkPrev;
		allocatedChunk->chunkPrev->bytes += 
			allocatedChunk->bytes + sizeof(KGeneralAllocatorChunk);
		kga->freeBytes += sizeof(KGeneralAllocatorChunk);
		kga->totalChunks--;
		KGeneralAllocatorChunk*const chunkToClear = allocatedChunk;
		allocatedChunk = allocatedChunk->chunkPrev;
		kassert(kgaVerifyChunk(kga, allocatedChunk));
		// fill cleared memory space with some value for safety //
		memset(reinterpret_cast<u8*>(chunkToClear), 0xFE, 
		       sizeof(KGeneralAllocatorChunk) + chunkToClear->bytes);
	}
	// destroy the next chunk & merge with it if it is also free //
	if (allocatedChunk->chunkNext != allocatedChunk &&
		!allocatedChunk->chunkNext->allocated &&
		// DO NOT EVER DESTROY THE FIRST CHUNK!!!
		allocatedChunk->chunkNext != kga->firstChunk)
	{
		allocatedChunk->chunkNext->chunkNext->chunkPrev = allocatedChunk;
		allocatedChunk->bytes +=
			allocatedChunk->chunkNext->bytes + sizeof(KGeneralAllocatorChunk);
		kga->freeBytes += sizeof(KGeneralAllocatorChunk);
		kga->totalChunks--;
		KGeneralAllocatorChunk*const chunkToClear = allocatedChunk->chunkNext;
		allocatedChunk->chunkNext = allocatedChunk->chunkNext->chunkNext;
		kassert(kgaVerifyChunk(kga, allocatedChunk));
		// fill cleared memory space with some value for safety //
		//	Here, we only have to clear the chunk header because free'd chunk
		//	contents are assumed to already be cleared.
		memset(reinterpret_cast<u8*>(chunkToClear), 0xFE, 
		       sizeof(KGeneralAllocatorChunk));
	}
	///TODO: verify the integrity of `kga`'s double-linked list of chunks
}
internal size_t kgaUsedBytes(KgaHandle kgaHandle)
{
	const KGeneralAllocator*const kga = 
		reinterpret_cast<KGeneralAllocator*>(kgaHandle);
	return kga->totalBytes - kga->freeBytes;
}
internal size_t kgaMaxTotalUsableBytes(KgaHandle kgaHandle)
{
	const KGeneralAllocator*const kga = 
		reinterpret_cast<KGeneralAllocator*>(kgaHandle);
	return kga->totalBytes;
}