#pragma once
#include "korl-globalDefines.h"
// #define KORL_PLATFORM_MEMORY_FILL(name)    void name(void* memory, u$ bytes, u8 pattern)
#define KORL_FUNCTION_korl_memory_zero(name)    void name(void* memory, u$ bytes)
#define KORL_FUNCTION_korl_memory_copy(name)    void name(void* destination, const void* source, u$ bytes)
#define KORL_FUNCTION_korl_memory_move(name)    void name(void* destination, const void* source, u$ bytes)
/**  Should function in the same way as memcmp from the C standard library.
 * \return \c -1 if the first byte that differs has a lower value in \c a than 
 *    in \c b, \c 1 if the first byte that differs has a higher value in \c a 
 *    than in \c b, and \c 0 if the two memory blocks are equal */
#define KORL_FUNCTION_korl_memory_compare(name) int  name(const void* a, const void* b, size_t bytes)
/**
 * \return \c 0 if the length & contents of the arrays are equal
 */
#define KORL_FUNCTION_korl_memory_arrayU8Compare(name) int name(const u8* dataA, u$ sizeA, const u8* dataB, u$ sizeB)
/**
 * \return \c 0 if the length & contents of the arrays are equal
 */
#define KORL_FUNCTION_korl_memory_arrayU16Compare(name) int name(const u16* dataA, u$ sizeA, const u16* dataB, u$ sizeB)
#define KORL_FUNCTION_korl_memory_acu16_hash(name) u$ name(acu16 arrayCU16)
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
#define KORL_FUNCTION_korl_memory_allocator_create(name)     Korl_Memory_AllocatorHandle name(Korl_Memory_AllocatorType type, u$ maxBytes, const wchar_t* allocatorName, Korl_Memory_AllocatorFlags flags, void* address)
/** \param address [OPTIONAL] the allocation will be placed at this exact 
 * address in the allocator.  Safety checks are performed to ensure that the 
 * specified address is valid (within allocator address range, not overlapping 
 * other allocations).  If this value is \c NULL , the allocator will choose the 
 * address of the allocation automatically (as you would typically expect). */
#define KORL_FUNCTION_korl_memory_allocator_allocate(name)   void*                       name(Korl_Memory_AllocatorHandle handle, u$ bytes, const wchar_t* file, int line, void* address)
#define KORL_FUNCTION_korl_memory_allocator_reallocate(name) void*                       name(Korl_Memory_AllocatorHandle handle, void* allocation, u$ bytes, const wchar_t* file, int line)
#define KORL_FUNCTION_korl_memory_allocator_free(name)       void                        name(Korl_Memory_AllocatorHandle handle, void* allocation, const wchar_t* file, int line)
#define KORL_FUNCTION_korl_memory_allocator_empty(name)      void                        name(Korl_Memory_AllocatorHandle handle)
