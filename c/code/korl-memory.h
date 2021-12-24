#pragma once
#include "korl-globalDefines.h"
/**
 * \note This does NOT initialize the memory to a valid known state.
 */
#define KORL_MEMORY_POOL_DECLARE(type, name, size) type name[size]; u32 name##_korlMemoryPoolSize
#define KORL_MEMORY_POOL_SIZE(name) (name##_korlMemoryPoolSize)
#define KORL_MEMORY_POOL_ISFULL(name) (name##_korlMemoryPoolSize >= korl_arraySize(name))
/**
 * \return a pointer to the added element
 */
#define KORL_MEMORY_POOL_ADD(name) \
    ( korl_assert(name##_korlMemoryPoolSize < korl_arraySize(name))\
    , &(name)[name##_korlMemoryPoolSize++] )
#define KORL_MEMORY_POOL_REMOVE(name, index) \
    ( korl_assert(name##_korlMemoryPoolSize > 0) \
    , korl_assert((u32)(index) < name##_korlMemoryPoolSize) \
    , (name)[index] = (name)[name##_korlMemoryPoolSize - 1] \
    , name##_korlMemoryPoolSize-- )
#define KORL_MEMORY_POOL_ISEMPTY(name) (name##_korlMemoryPoolSize == 0)
#define KORL_MEMORY_POOL_EMPTY(name) (name##_korlMemoryPoolSize = 0)
/** @simplify: we don't actually need to expose ANY of this allocator API right 
 * now for any reason - all the user of this module needs is the enumeration of 
 * what kind of allocator they are getting, then the dispatch can just all be 
 * done internally and the user doesn't have to deal with function pointer crap */
/** @simplify: make some macros which automatically inject file + line for calls 
    to these allocator functions so we don't have to think about it */
#define KORL_MEMORY_ALLOCATOR_CALLBACK_ALLOCATE(name)   void* name(void* userData, u$ bytes, wchar_t* file, int line)
#define KORL_MEMORY_ALLOCATOR_CALLBACK_REALLOCATE(name) void* name(void* userData, void* allocation, u$ bytes, wchar_t* file, int line)
#define KORL_MEMORY_ALLOCATOR_CALLBACK_FREE(name)       void  name(void* userData, void* allocation, wchar_t* file, int line)
typedef KORL_MEMORY_ALLOCATOR_CALLBACK_ALLOCATE  (korl_memory_allocator_callback_allocate);
typedef KORL_MEMORY_ALLOCATOR_CALLBACK_REALLOCATE(korl_memory_allocator_callback_reallocate);
typedef KORL_MEMORY_ALLOCATOR_CALLBACK_FREE      (korl_memory_allocator_callback_free);
typedef struct Korl_Memory_Allocator
{
    /** data to be passed to the allocator's callback API */
    void* userData;
    korl_memory_allocator_callback_allocate*   callbackAllocate;
    korl_memory_allocator_callback_reallocate* callbackReallocate;
    korl_memory_allocator_callback_free*       callbackFree;
} Korl_Memory_Allocator;
korl_internal void korl_memory_initialize(void);
korl_internal u$   korl_memory_pageBytes(void);
#if 0 //@unused
korl_internal void* korl_memory_addressMin(void);
korl_internal void* korl_memory_addressMax(void);
#endif
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
 * multiplied by -1 is returned.
 */
korl_internal i$ korl_memory_stringCopy(const wchar_t* source, wchar_t* destination, u$ destinationSize);
korl_internal void korl_memory_nullify(void*const p, size_t bytes);
korl_internal bool korl_memory_isNull(const void* p, size_t bytes);
korl_internal Korl_Memory_Allocator korl_memory_createAllocatorLinear(u$ maxBytes);
