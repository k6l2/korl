#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-memory.h"
#include "korl-heap.h"
typedef struct Korl_Memory_FileMapAllocation_CreateInfo
{
    u$         physicalMemoryChunkBytes;// this is merely a request; the final physical memory chunk bytes value will be >= to this in order to satisfy memory alignment constraints
    u16        physicalRegionCount;
    u16        virtualRegionCount;
    const u16* virtualRegionMap;
} Korl_Memory_FileMapAllocation_CreateInfo;
/** \return \c true to continue enumeration, \c false to stop enumerating allocators */
#define KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(name) bool name(void* userData, void* opaqueAllocator, Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* allocatorName, Korl_Memory_AllocatorFlags allocatorFlags)
typedef KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(fnSig_korl_memory_allocator_enumerateAllocatorsCallback);
korl_internal void korl_memory_initialize(void);
korl_internal u$   korl_memory_pageBytes(void);
korl_internal u$   korl_memory_virtualAlignBytes(void);
korl_internal bool korl_memory_isLittleEndian(void);
korl_internal void* korl_memory_set(void* target, u8 value, u$ bytes);
korl_internal KORL_FUNCTION_korl_memory_zero(korl_memory_zero);
korl_internal KORL_FUNCTION_korl_memory_copy(korl_memory_copy);
korl_internal KORL_FUNCTION_korl_memory_move(korl_memory_move);
korl_internal KORL_FUNCTION_korl_memory_compare(korl_memory_compare);
korl_internal KORL_FUNCTION_korl_memory_arrayU8Compare(korl_memory_arrayU8Compare);
korl_internal KORL_FUNCTION_korl_memory_arrayU16Compare(korl_memory_arrayU16Compare);
korl_internal KORL_FUNCTION_korl_memory_acu16_hash(korl_memory_acu16_hash);
korl_internal bool korl_memory_isNull(const void* p, size_t bytes);
korl_internal KORL_FUNCTION_korl_memory_allocator_create    (korl_memory_allocator_create);
korl_internal void korl_memory_allocator_destroy(Korl_Memory_AllocatorHandle handle);
korl_internal void korl_memory_allocator_recreate(Korl_Memory_AllocatorHandle handle, u$ heapDescriptorCount, void* heapDescriptors, u$ heapDescriptorStride, u32 heapDescriptorOffset_addressStart, u32 heapDescriptorOffset_addressEnd);
korl_internal KORL_FUNCTION_korl_memory_allocator_allocate  (korl_memory_allocator_allocate);
korl_internal KORL_FUNCTION_korl_memory_allocator_reallocate(korl_memory_allocator_reallocate);
korl_internal KORL_FUNCTION_korl_memory_allocator_free      (korl_memory_allocator_free);
korl_internal KORL_FUNCTION_korl_memory_allocator_empty     (korl_memory_allocator_empty);
korl_internal void korl_memory_allocator_defragment(Korl_Memory_AllocatorHandle handle, Korl_Heap_DefragmentPointer* defragmentPointers, u$ defragmentPointersSize, Korl_Memory_AllocatorHandle handleStack);
korl_internal bool korl_memory_allocator_isEmpty(Korl_Memory_AllocatorHandle handle);
korl_internal void korl_memory_allocator_emptyStackAllocators(void);
/** \return the address of the allocated memory report */
korl_internal void* korl_memory_reportGenerate(void);
/** \param reportAddress the address of the return value of a previous call to \c korl_memory_reportGenerate */
korl_internal void korl_memory_reportLog(void* reportAddress);
korl_internal KORL_FUNCTION_korl_memory_allocator_enumerateAllocators(korl_memory_allocator_enumerateAllocators);
korl_internal KORL_FUNCTION_korl_memory_allocator_enumerateAllocations(korl_memory_allocator_enumerateAllocations);
korl_internal KORL_FUNCTION_korl_memory_allocator_enumerateHeaps(korl_memory_allocator_enumerateHeaps);
/** \param out_allocatorIndex if the return value is \c true , then the memory 
 * at this address is populated with the internal index of the found allocator.
 * \return \c true if the allocator with the given \c name exists */
korl_internal bool korl_memory_allocator_findByName(const wchar_t* name, Korl_Memory_AllocatorHandle* out_allocatorHandle);
korl_internal bool korl_memory_allocator_containsAllocation(void* opaqueAllocator, const void* allocation);
korl_internal void* korl_memory_fileMapAllocation_create(const Korl_Memory_FileMapAllocation_CreateInfo* createInfo, u$* out_physicalMemoryChunkBytes);
korl_internal void korl_memory_fileMapAllocation_destroy(void* allocation, u$ bytesPerRegion, u16 regionCount);
/** These memory (un)pack API can be used to quickly store & retrieve data 
 * to/from a memory buffer in a platform-agnostic way.  For example, if machine 
 * A packs a 64-bit integer into a memory buffer, and sends this data to machine 
 * B over the network or via a file saved to disk, machine B should be able to 
 * successfully unpack and read the data correctly using the same API. */
/** \param dataSize _not_ including the null-terminator */
korl_internal u$ korl_memory_packStringI8(const i8* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd);
/** \param dataSize _not_ including the null-terminator */
korl_internal u$ korl_memory_packStringU16(const u16* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU$ (u$  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU64(u64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU32(u32 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU16(u16 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU8 (u8  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packI64(i64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packI32(i32 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packI16(i16 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packI8 (i8  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packF64(f64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packF32(f32 data, u8** bufferCursor, const u8*const bufferEnd);
/** The caller is responsible for null-terminating the string. 
 * \param dataSize _not_ including the null-terminator required for the string */
korl_internal u$ korl_memory_unpackStringI8(i8* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd);
/** The caller is responsible for null-terminating the string. 
 * \param dataSize _not_ including the null-terminator required for the string */
korl_internal u$ korl_memory_unpackStringU16(u16* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU$ (u$  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU64(u64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU32(u32 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU16(u16 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU8 (u8  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackI64(i64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackI32(i32 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackI16(i16 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackI8 (i8  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackF64(f64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackF32(f32 data, u8** bufferCursor, const u8*const bufferEnd);
