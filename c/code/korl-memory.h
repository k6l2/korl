#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
typedef struct Korl_Memory_AllocationMeta
{
    const wchar_t* file;
    int line;
    /** 
     * The amount of actual memory used by the caller.  The grand total amount 
     * of memory used by an allocation will likely be the sum of the following:  
     * - the allocation meta data
     * - the actual memory used by the caller
     * - any additional padding required by the allocator (likely to round 
     *   everything up to the nearest page size)
     */
    u$ bytes;
} Korl_Memory_AllocationMeta;
typedef struct Korl_Memory_FileMapAllocation_CreateInfo
{
    u$         physicalMemoryChunkBytes;// this is merely a request; the final physical memory chunk bytes value will be >= to this in order to satisfy memory alignment constraints
    u16        physicalRegionCount;
    u16        virtualRegionCount;
    const u16* virtualRegionMap;
} Korl_Memory_FileMapAllocation_CreateInfo;
#define KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS_CALLBACK(name) bool name(void* userData, const void* allocation, const Korl_Memory_AllocationMeta* meta)
typedef KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS_CALLBACK(fnSig_korl_memory_allocator_enumerateAllocationsCallback);
#define KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(name) void name(void* opaqueAllocator, void* allocatorUserData, fnSig_korl_memory_allocator_enumerateAllocationsCallback* callback, void* callbackUserData, const void** out_allocatorVirtualAddressEnd)
/** \return \c true to continue enumeration, \c false to stop enumerating allocators */
#define KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(name) bool name(void* userData, void* opaqueAllocator, void* allocatorUserData, Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* allocatorName, Korl_Memory_AllocatorFlags allocatorFlags)
typedef KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(fnSig_korl_memory_allocator_enumerateAllocatorsCallback);
korl_internal void korl_memory_initialize(void);
korl_internal u$   korl_memory_pageBytes(void);
korl_internal bool korl_memory_isLittleEndian(void);
korl_internal KORL_PLATFORM_STRING_COMPARE(korl_memory_stringCompare);
korl_internal KORL_PLATFORM_STRING_COMPARE_UTF8(korl_memory_stringCompareUtf8);
korl_internal KORL_PLATFORM_STRING_SIZE(korl_memory_stringSize);
korl_internal KORL_PLATFORM_STRING_SIZE_UTF8(korl_memory_stringSizeUtf8);
korl_internal KORL_PLATFORM_STRING_COPY(korl_memory_stringCopy);
// korl_internal KORL_PLATFORM_MEMORY_FILL(korl_memory_fill);
korl_internal KORL_PLATFORM_MEMORY_ZERO(korl_memory_zero);
korl_internal KORL_PLATFORM_MEMORY_COPY(korl_memory_copy);
korl_internal KORL_PLATFORM_MEMORY_MOVE(korl_memory_move);
korl_internal KORL_PLATFORM_MEMORY_COMPARE(korl_memory_compare);
korl_internal KORL_PLATFORM_ARRAY_U8_COMPARE(korl_memory_arrayU8Compare);
korl_internal KORL_PLATFORM_ARRAY_U16_COMPARE(korl_memory_arrayU16Compare);
korl_internal KORL_PLATFORM_ARRAY_CONST_U16_HASH(korl_memory_acu16_hash);
korl_internal bool korl_memory_isNull(const void* p, size_t bytes);
korl_internal wchar_t* korl_memory_stringFormat(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, ...);
korl_internal KORL_PLATFORM_STRING_FORMAT_VALIST       (korl_memory_stringFormatVaList);
korl_internal KORL_PLATFORM_STRING_FORMAT_VALIST_UTF8  (korl_memory_stringFormatVaListUtf8);
korl_internal KORL_PLATFORM_STRING_FORMAT_BUFFER       (korl_memory_stringFormatBuffer);
korl_internal i$ korl_memory_stringFormatBufferVaList(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, va_list vaList);
korl_internal KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR    (korl_memory_allocator_create);
korl_internal void korl_memory_allocator_destroy(Korl_Memory_AllocatorHandle handle);
korl_internal void korl_memory_allocator_recreate(Korl_Memory_AllocatorHandle handle, void* newAddress);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE  (korl_memory_allocator_allocate);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(korl_memory_allocator_reallocate);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_FREE      (korl_memory_allocator_free);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY     (korl_memory_allocator_empty);
korl_internal bool korl_memory_allocator_isEmpty(Korl_Memory_AllocatorHandle handle);
korl_internal void korl_memory_allocator_emptyStackAllocators(void);
/** \param allocatorHandle the allocator to use to allocate memory to store the report 
 * \return the address of the allocated memory report */
korl_internal void* korl_memory_reportGenerate(void);
/** \param reportAddress the address of the return value of a previous call to \c korl_memory_reportGenerate */
korl_internal void korl_memory_reportLog(void* reportAddress);
korl_internal void korl_memory_allocator_enumerateAllocators(fnSig_korl_memory_allocator_enumerateAllocatorsCallback *callback, void *callbackUserData);
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(korl_memory_allocator_enumerateAllocations);
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
