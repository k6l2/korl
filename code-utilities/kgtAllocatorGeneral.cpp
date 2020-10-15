#include "kgtAllocatorGeneral.h"
#include <cstring>
// This is just a relatively inexpensive sanity check to see if the address 
//	spaces of the chunk and its immediate neighbors lie within the total address
//	space of the allocator.  Also doing some other simple checks now... //
internal bool 
	kgtAllocGeneralVerifyChunk(
		const KgtAllocatorGeneral* kga, const KgtAllocatorGeneralChunk* chunk)
{
	const size_t kgaAddressSpaceStart = reinterpret_cast<size_t>(kga);
	const size_t kgaAddressSpaceEnd   = kgaAddressSpaceStart + kga->totalBytes +
		sizeof(KgtAllocatorGeneral) + sizeof(KgtAllocatorGeneralChunk);
	const size_t chunkAddress = reinterpret_cast<size_t>(chunk);
	const size_t chunkAddressEnd = 
		chunkAddress + sizeof(KgtAllocatorGeneralChunk) + chunk->bytes;
	if (chunkAddress < kgaAddressSpaceStart ||
		chunkAddress > 
			kgaAddressSpaceEnd - sizeof(KgtAllocatorGeneralChunk) - 1 ||
		chunkAddressEnd < kgaAddressSpaceStart ||
		chunkAddressEnd > kgaAddressSpaceEnd)
	{
		return false;
	}
	const size_t chunkAddressPrev = reinterpret_cast<size_t>(chunk->chunkPrev);
	const size_t chunkAddressPrevEnd = chunkAddressPrev + 
		sizeof(KgtAllocatorGeneralChunk) + chunk->chunkPrev->bytes;
	if (chunkAddressPrev < kgaAddressSpaceStart ||
		chunkAddressPrev > 
			kgaAddressSpaceEnd - sizeof(KgtAllocatorGeneralChunk) - 1 ||
		chunkAddressPrevEnd < kgaAddressSpaceStart ||
		chunkAddressPrevEnd > kgaAddressSpaceEnd)
	{
		return false;
	}
	const size_t chunkAddressNext = reinterpret_cast<size_t>(chunk->chunkNext);
	const size_t chunkAddressNextEnd = chunkAddressNext + 
		sizeof(KgtAllocatorGeneralChunk) + chunk->chunkNext->bytes;
	if (chunkAddressNext < kgaAddressSpaceStart ||
		chunkAddressNext > 
			kgaAddressSpaceEnd - sizeof(KgtAllocatorGeneralChunk) - 1 ||
		chunkAddressNextEnd < kgaAddressSpaceStart ||
		chunkAddressNextEnd > kgaAddressSpaceEnd)
	{
		return false;
	}
	// verify the integrity of the double-linked list using the `totalChunks` //
	const KgtAllocatorGeneralChunk* chunkNextLeft  = chunk;
	const KgtAllocatorGeneralChunk* chunkNextRight = chunk;
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
internal bool kgtAllocGeneralVerifyFreeBytes(const KgtAllocatorGeneral* kga)
{
	KgtAllocatorGeneralChunk* chunk = kga->firstChunk;
	size_t freeBytes = 0;
	for(size_t c = 0; c < kga->totalChunks; c++)
	{
		kassert(chunk);
		if(!chunk->allocated)
			freeBytes += chunk->bytes;
		chunk = chunk->chunkNext;
	}
	return freeBytes == kga->freeBytes;
}
internal KgtAllocatorGeneral* kgtAllocGeneralInit(
	void* allocatorMemoryLocation, size_t allocatorByteCount)
{
	local_persist const size_t MIN_ALLOC_OVERHEAD =
		sizeof(KgtAllocatorGeneral) + sizeof(KgtAllocatorGeneralChunk);
	kassert(allocatorByteCount > MIN_ALLOC_OVERHEAD);
	KgtAllocatorGeneral*const kga = 
		reinterpret_cast<KgtAllocatorGeneral*>(allocatorMemoryLocation);
	*kga = {};
	kga->type        = KgtAllocatorType::GENERAL;
	kga->freeBytes   = allocatorByteCount - MIN_ALLOC_OVERHEAD;
	kga->totalBytes  = allocatorByteCount - MIN_ALLOC_OVERHEAD;
	kga->totalChunks = 1;
	kga->firstChunk  = reinterpret_cast<KgtAllocatorGeneralChunk*>(
	                   reinterpret_cast<u8*>(kga) + sizeof(KgtAllocatorGeneral));
	kga->firstChunk->chunkPrev = kga->firstChunk;
	kga->firstChunk->chunkNext = kga->firstChunk;
	kga->firstChunk->bytes     = allocatorByteCount - MIN_ALLOC_OVERHEAD;
	kga->firstChunk->allocated = false;
	kassert(kgtAllocGeneralVerifyChunk(kga, kga->firstChunk));
	kassert(kga->freeBytes <= kga->totalBytes);
	return kga;
}
internal void* kgtAllocGeneralAlloc(
	KgtAllocatorGeneral* kga, size_t allocationByteCount)
{
	if(allocationByteCount == 0)
	{
		kassert(kga->freeBytes <= kga->totalBytes);
		return nullptr;
	}
	local_persist const size_t MIN_ALLOC_OVERHEAD = 
		sizeof(KgtAllocatorGeneralChunk);
	if(kga->freeBytes < allocationByteCount)
	{
		kassert(kga->freeBytes <= kga->totalBytes);
		return nullptr;
	}
	// First, we need to find the first unallocated chunk that satisfies our
	//	memory requirements //
	KgtAllocatorGeneralChunk* firstAvailableChunk = nullptr;
	KgtAllocatorGeneralChunk* nextChunk = kga->firstChunk;
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
		kassert(kga->freeBytes <= kga->totalBytes);
		return nullptr;
	}
	kassert(kgtAllocGeneralVerifyChunk(kga, firstAvailableChunk));
	// Mark the available chunk as allocated & create a new empty chunk adjacent
	//	in the double-linked list of memory //
	if(firstAvailableChunk->bytes > 
		allocationByteCount + MIN_ALLOC_OVERHEAD + 1)
	{
		// we only need to add another link in the chunk chain if there is 
		//	enough memory leftover to support another chunk header PLUS some 
		//	non-zero amount of allocatable memory! //
		KgtAllocatorGeneralChunk*const newChunk = 
			reinterpret_cast<KgtAllocatorGeneralChunk*>(
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
		kga->freeBytes -= (MIN_ALLOC_OVERHEAD + allocationByteCount);
		kga->totalChunks++;
		kassert(kgtAllocGeneralVerifyChunk(kga, firstAvailableChunk));
	}
	else
	{
		/* there's not enough memory to create another chunk header + memory, so 
		 * we lose the entire region of memory! */
		kga->freeBytes -= firstAvailableChunk->bytes;
	}
	firstAvailableChunk->allocated = true;
	// Clear the newly allocated space to zero for safety //
	memset(reinterpret_cast<u8*>(firstAvailableChunk) + MIN_ALLOC_OVERHEAD, 
	       0x0, firstAvailableChunk->bytes);
	kassert(kga->freeBytes <= kga->totalBytes);
	kassert(kgtAllocGeneralVerifyFreeBytes(kga));
	return reinterpret_cast<u8*>(firstAvailableChunk) + MIN_ALLOC_OVERHEAD;
}
internal void* kgtAllocGeneralRealloc(
	KgtAllocatorGeneral* kga, void* allocatedAddress, size_t newAllocationSize)
{
	void* newAllocation = kgtAllocGeneralAlloc(kga, newAllocationSize);
	if(!newAllocation)
	{
		if(allocatedAddress)
		{
			kgtAllocGeneralFree(kga, allocatedAddress);
		}
		kassert(kga->freeBytes <= kga->totalBytes);
		kassert(kgtAllocGeneralVerifyFreeBytes(kga));
		return nullptr;
	}
	if(allocatedAddress)
	{
		KgtAllocatorGeneralChunk* allocatedChunk = 
			reinterpret_cast<KgtAllocatorGeneralChunk*>(
				reinterpret_cast<u8*>(allocatedAddress) - 
				sizeof(KgtAllocatorGeneralChunk));
		kassert(kgtAllocGeneralVerifyChunk(kga, allocatedChunk));
		const size_t bytesToCopy = allocatedChunk->bytes < newAllocationSize
			? allocatedChunk->bytes : newAllocationSize;
		memcpy(newAllocation, allocatedAddress, bytesToCopy);
		kgtAllocGeneralFree(kga, allocatedAddress);
	}
	kassert(kga->freeBytes <= kga->totalBytes);
	kassert(kgtAllocGeneralVerifyFreeBytes(kga));
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
internal void kgtAllocGeneralFree(
	KgtAllocatorGeneral* kga, void* allocatedAddress)
{
	if(!allocatedAddress)
	{
		kassert(kga->freeBytes <= kga->totalBytes);
		return;
	}
	kassert(kga->freeBytes <= kga->totalBytes);
	KgtAllocatorGeneralChunk* allocatedChunk = 
		reinterpret_cast<KgtAllocatorGeneralChunk*>(
			reinterpret_cast<u8*>(allocatedAddress) - 
			sizeof(KgtAllocatorGeneralChunk));
	kassert(kgtAllocGeneralVerifyChunk(kga, allocatedChunk));
	allocatedChunk->allocated = false;
	kga->freeBytes += allocatedChunk->bytes;
	// destroy this chunk & merge with the previous chunk if it is also free //
	if (allocatedChunk->chunkPrev != allocatedChunk &&
		!allocatedChunk->chunkPrev->allocated &&
		// DO NOT EVER DESTROY THE FIRST CHUNK!!!
		allocatedChunk != kga->firstChunk)
	{
		kga->freeBytes += sizeof(KgtAllocatorGeneralChunk);
		kassert(kga->freeBytes <= kga->totalBytes);
		allocatedChunk->chunkPrev->chunkNext = allocatedChunk->chunkNext;
		allocatedChunk->chunkNext->chunkPrev = allocatedChunk->chunkPrev;
		allocatedChunk->chunkPrev->bytes += 
			allocatedChunk->bytes + sizeof(KgtAllocatorGeneralChunk);
		kga->totalChunks--;
		KgtAllocatorGeneralChunk*const chunkToClear = allocatedChunk;
		allocatedChunk = allocatedChunk->chunkPrev;
		kassert(kgtAllocGeneralVerifyChunk(kga, allocatedChunk));
		// fill cleared memory space with some value for safety //
		memset(reinterpret_cast<u8*>(chunkToClear), 0xFE, 
		       sizeof(KgtAllocatorGeneralChunk) + chunkToClear->bytes);
	}
	// destroy the next chunk & merge with it if it is also free //
	if (allocatedChunk->chunkNext != allocatedChunk &&
		!allocatedChunk->chunkNext->allocated &&
		// DO NOT EVER DESTROY THE FIRST CHUNK!!!
		allocatedChunk->chunkNext != kga->firstChunk)
	{
		kga->freeBytes += sizeof(KgtAllocatorGeneralChunk);
		kassert(kga->freeBytes <= kga->totalBytes);
		allocatedChunk->chunkNext->chunkNext->chunkPrev = allocatedChunk;
		allocatedChunk->bytes +=
			allocatedChunk->chunkNext->bytes + sizeof(KgtAllocatorGeneralChunk);
		kga->totalChunks--;
		KgtAllocatorGeneralChunk*const chunkToClear = allocatedChunk->chunkNext;
		allocatedChunk->chunkNext = allocatedChunk->chunkNext->chunkNext;
		kassert(kgtAllocGeneralVerifyChunk(kga, allocatedChunk));
		// fill cleared memory space with some value for safety //
		//	Here, we only have to clear the chunk header because free'd chunk
		//	contents are assumed to already be cleared.
		memset(reinterpret_cast<u8*>(chunkToClear), 0xFE, 
		       sizeof(KgtAllocatorGeneralChunk));
	}
	kassert(kga->freeBytes <= kga->totalBytes);
	kassert(kgtAllocGeneralVerifyFreeBytes(kga));
	///TODO: verify the integrity of `kga`'s double-linked list of chunks
}
internal size_t kgtAllocGeneralUsedBytes(KgtAllocatorGeneral* kga)
{
	kassert(kga->freeBytes <= kga->totalBytes);
	return kga->totalBytes - kga->freeBytes;
}
internal size_t kgtAllocGeneralMaxTotalUsableBytes(KgtAllocatorGeneral* kga)
{
	kassert(kga->freeBytes <= kga->totalBytes);
	return kga->totalBytes;
}
