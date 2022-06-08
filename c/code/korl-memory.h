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
#define KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS_CALLBACK(name) void name(void* userData, const void* allocation, const Korl_Memory_AllocationMeta* meta)
typedef KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS_CALLBACK(fnSig_korl_memory_allocator_enumerateAllocationsCallback);
#define KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(name) void name(void* opaqueAllocator, void* allocatorUserData, fnSig_korl_memory_allocator_enumerateAllocationsCallback* callback, void* callbackUserData, const void** out_allocatorVirtualAddressEnd)
#define KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(name) void name(void* userData, void* opaqueAllocator, void* allocatorUserData, const wchar_t* allocatorName, Korl_Memory_AllocatorFlags allocatorFlags)
typedef KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(fnSig_korl_memory_allocator_enumerateAllocatorsCallback);
korl_internal void korl_memory_initialize(void);
korl_internal u$   korl_memory_pageBytes(void);
/**  Should function in the same way as memcmp from the C standard library.
 * \return \c -1 if the first byte that differs has a lower value in \c a than 
 *    in \c b, \c 1 if the first byte that differs has a higher value in \c a 
 *    than in \c b, and \c 0 if the two memory blocks are equal
 */
korl_internal int korl_memory_compare(const void* a, const void* b, size_t bytes);
/**
 * \return \c 0 if the two strings are equal
 */
korl_internal int korl_memory_stringCompare(const wchar_t* a, const wchar_t* b);
/**
 * \return the size of string \c s EXCLUDING the null terminator
 */
korl_internal u$ korl_memory_stringSize(const wchar_t* s);
/**
 * \return the number of characters copied from \c source into \c destination , 
 * INCLUDING the null terminator.  If the \c source cannot be copied into the 
 * \c destination then the size of \c source INCLUDING the null terminator and 
 * multiplied by -1 is returned, and \c destination is filled with the maximum 
 * number of characters that can be copied, including a null-terminator.
 */
korl_internal i$ korl_memory_stringCopy(const wchar_t* source, wchar_t* destination, u$ destinationSize);
korl_internal KORL_PLATFORM_MEMORY_ZERO(korl_memory_zero);
korl_internal KORL_PLATFORM_MEMORY_COPY(korl_memory_copy);
korl_internal KORL_PLATFORM_MEMORY_MOVE(korl_memory_move);
korl_internal bool korl_memory_isNull(const void* p, size_t bytes);
korl_internal wchar_t* korl_memory_stringFormat(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, ...);
korl_internal wchar_t* korl_memory_stringFormatVaList(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, va_list vaList);
/**
 * \return the number of characters copied from \c format into \c buffer , 
 * INCLUDING the null terminator.  If the \c format cannot be copied into the 
 * \c buffer then the size of \c format , INCLUDING the null terminator, 
 * multiplied by -1, is returned.
 */
korl_internal i$ korl_memory_stringFormatBuffer(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, ...);
korl_internal i$ korl_memory_stringFormatBufferVaList(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, va_list vaList);
korl_internal KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR    (korl_memory_allocator_create);
korl_internal void korl_memory_allocator_destroy(Korl_Memory_AllocatorHandle handle);
korl_internal void korl_memory_allocator_recreate(Korl_Memory_AllocatorHandle handle, void* newAddress);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE  (korl_memory_allocator_allocate);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(korl_memory_allocator_reallocate);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_FREE      (korl_memory_allocator_free);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY     (korl_memory_allocator_empty);
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
