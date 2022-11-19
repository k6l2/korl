#pragma once
#include "korl-globalDefines.h"
// #define KORL_PLATFORM_MEMORY_FILL(name)    void name(void* memory, u$ bytes, u8 pattern)
#define KORL_PLATFORM_MEMORY_ZERO(name)    void name(void* memory, u$ bytes)
#define KORL_PLATFORM_MEMORY_COPY(name)    void name(void* destination, const void* source, u$ bytes)
#define KORL_PLATFORM_MEMORY_MOVE(name)    void name(void* destination, const void* source, u$ bytes)
/**  Should function in the same way as memcmp from the C standard library.
 * \return \c -1 if the first byte that differs has a lower value in \c a than 
 *    in \c b, \c 1 if the first byte that differs has a higher value in \c a 
 *    than in \c b, and \c 0 if the two memory blocks are equal */
#define KORL_PLATFORM_MEMORY_COMPARE(name) int  name(const void* a, const void* b, size_t bytes)
/**
 * \return \c 0 if the length & contents of the arrays are equal
 */
#define KORL_PLATFORM_ARRAY_U8_COMPARE(name) int name(const u8* dataA, u$ sizeA, const u8* dataB, u$ sizeB)
/**
 * \return \c 0 if the length & contents of the arrays are equal
 */
#define KORL_PLATFORM_ARRAY_U16_COMPARE(name) int name(const u16* dataA, u$ sizeA, const u16* dataB, u$ sizeB)
#define KORL_PLATFORM_ARRAY_CONST_U16_HASH(name) u$ name(acu16 arrayCU16)
/**
 * \return \c 0 if the two strings are equal
 */
#define KORL_PLATFORM_STRING_COMPARE(name)              int name(const wchar_t* a, const wchar_t* b)
/**
 * \return \c 0 if the two strings are equal
 */
#define KORL_PLATFORM_STRING_COMPARE_UTF8(name)         int name(const char* a, const char* b)
/**
 * \return the size of string \c s _excluding_ the null terminator
 */
#define KORL_PLATFORM_STRING_SIZE(name)                 u$ name(const wchar_t* s)
/**
 * \return The size of string \c s _excluding_ the null terminator.  "Size" is 
 * defined as the total number of characters; _not_ the total number of 
 * codepoints!
 */
#define KORL_PLATFORM_STRING_SIZE_UTF8(name)            u$ name(const char* s)
/**
 * \return the number of characters copied from \c source into \c destination , 
 * INCLUDING the null terminator.  If the \c source cannot be copied into the 
 * \c destination then the size of \c source INCLUDING the null terminator and 
 * multiplied by -1 is returned, and \c destination is filled with the maximum 
 * number of characters that can be copied, including a null-terminator.
 */
#define KORL_PLATFORM_STRING_COPY(name)                 i$ name(const wchar_t* source, wchar_t* destination, u$ destinationSize)
#define KORL_PLATFORM_STRING_FORMAT_VALIST(name)        wchar_t* name(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, va_list vaList)
#define KORL_PLATFORM_STRING_FORMAT_VALIST_UTF8(name)   char* name(Korl_Memory_AllocatorHandle allocatorHandle, const char* format, va_list vaList)
/**
 * \return the number of characters copied from \c format into \c buffer , 
 * INCLUDING the null terminator.  If the \c format cannot be copied into the 
 * \c buffer then the size of \c format , INCLUDING the null terminator, 
 * multiplied by -1, is returned.
 */
#define KORL_PLATFORM_STRING_FORMAT_BUFFER(name)        i$ name(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, ...)
typedef u16 Korl_Memory_AllocatorHandle;
typedef enum Korl_Memory_AllocatorType
    { KORL_MEMORY_ALLOCATOR_TYPE_LINEAR
    , KORL_MEMORY_ALLOCATOR_TYPE_GENERAL
} Korl_Memory_AllocatorType;
typedef enum Korl_Memory_AllocatorFlags
    { KORL_MEMORY_ALLOCATOR_FLAGS_NONE                        = 0
    , KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS = 1 << 0
    , KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME            = 1 << 1
    , KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE         = 1 << 2
} Korl_Memory_AllocatorFlags;
/** As of right now, the smallest possible value for \c maxBytes for a linear 
 * allocator is 16 kilobytes (4 pages), since two pages are required for the 
 * allocator struct, and a minimum of two pages are required for each allocation 
 * (one for the meta data page, and one for the actual allocation itself).  
 * This behavior is subject to change in the future. 
 * \param address [OPTIONAL] the allocator itself will attempt to be placed at 
 * the specified virtual address.  If this value is \c NULL , the operating 
 * system will choose this address for us. */
#define KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR(name)     Korl_Memory_AllocatorHandle name(Korl_Memory_AllocatorType type, u$ maxBytes, const wchar_t* allocatorName, Korl_Memory_AllocatorFlags flags, void* address)
/** \param address [OPTIONAL] the allocation will be placed at this exact 
 * address in the allocator.  Safety checks are performed to ensure that the 
 * specified address is valid (within allocator address range, not overlapping 
 * other allocations).  If this value is \c NULL , the allocator will choose the 
 * address of the allocation automatically (as you would typically expect). */
#define KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE(name)   void*                       name(Korl_Memory_AllocatorHandle handle, u$ bytes, const wchar_t* file, int line, void* address)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(name) void*                       name(Korl_Memory_AllocatorHandle handle, void* allocation, u$ bytes, const wchar_t* file, int line)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_FREE(name)       void                        name(Korl_Memory_AllocatorHandle handle, void* allocation, const wchar_t* file, int line)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY(name)      void                        name(Korl_Memory_AllocatorHandle handle)
