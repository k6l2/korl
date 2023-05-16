#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-memory.h"
#include "korl-heap.h"
/** \return \c true to continue enumeration, \c false to stop enumerating allocators */
#define KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(name) bool name(void* userData, void* opaqueAllocator, Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* allocatorName, Korl_Memory_AllocatorFlags allocatorFlags)
typedef KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(fnSig_korl_memory_allocator_enumerateAllocatorsCallback);
korl_internal void korl_memory_initialize(void);
korl_internal u$   korl_memory_pageBytes(void);
korl_internal u$   korl_memory_virtualAlignBytes(void);
korl_internal void* korl_memory_set(void* target, u8 value, u$ bytes);
korl_internal void korl_memory_allocator_destroy(Korl_Memory_AllocatorHandle handle);
korl_internal void korl_memory_allocator_recreate(Korl_Memory_AllocatorHandle handle
                                                 ,u$ heapDescriptorCount, const void* heapDescriptors
                                                 ,u$ heapDescriptorStride
                                                 ,u32 heapDescriptorOffset_virtualAddressStart
                                                 ,u32 heapDescriptorOffset_virtualBytes
                                                 ,u32 heapDescriptorOffset_committedBytes);
korl_internal bool korl_memory_allocator_isEmpty(Korl_Memory_AllocatorHandle handle);
korl_internal void korl_memory_allocator_emptyStackAllocators(void);
/** \return the address of the allocated memory report */
korl_internal void* korl_memory_reportGenerate(void);
/** \param reportAddress the address of the return value of a previous call to \c korl_memory_reportGenerate */
korl_internal void korl_memory_reportLog(void* reportAddress);
/** \param out_allocatorIndex if the return value is \c true , then the memory 
 * at this address is populated with the internal index of the found allocator.
 * \return \c true if the allocator with the given \c name exists */
korl_internal bool korl_memory_allocator_findByName(const wchar_t* name, Korl_Memory_AllocatorHandle* out_allocatorHandle);
korl_internal bool korl_memory_allocator_containsAllocation(void* opaqueAllocator, const void* allocation);
typedef struct Korl_Memory_FileMapAllocation_CreateInfo
{
    u$         physicalMemoryChunkBytes;// this is merely a request; the final physical memory chunk bytes value will be >= to this in order to satisfy memory alignment constraints
    u16        physicalRegionCount;
    u16        virtualRegionCount;
    const u16* virtualRegionMap;
} Korl_Memory_FileMapAllocation_CreateInfo;
korl_internal void* korl_memory_fileMapAllocation_create(const Korl_Memory_FileMapAllocation_CreateInfo* createInfo, u$* out_physicalMemoryChunkBytes);
korl_internal void korl_memory_fileMapAllocation_destroy(void* allocation, u$ bytesPerRegion, u16 regionCount);
