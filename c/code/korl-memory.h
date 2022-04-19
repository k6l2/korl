#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
/* macro for automatically initializing stack variables to 0 */
#define KORL_ZERO_STACK(variableType, variableIdentifier) \
    variableType variableIdentifier;\
    korl_memory_zero(&(variableIdentifier), sizeof(variableIdentifier));
/* same as KORL_ZERO_STACK, except for arrays */
#define KORL_ZERO_STACK_ARRAY(variableType, variableIdentifier, arraySize) \
    variableType variableIdentifier[arraySize]; \
    korl_memory_zero(variableIdentifier, sizeof(variableIdentifier));
typedef u32 Korl_MemoryPool_Size;
/**
 * \note This does NOT initialize the memory to a valid known state.
 */
#define KORL_MEMORY_POOL_DECLARE(type, name, size) type name[size]; Korl_MemoryPool_Size name##_korlMemoryPoolSize
#define KORL_MEMORY_POOL_SIZE(name) (name##_korlMemoryPoolSize)
#define KORL_MEMORY_POOL_CAPACITY(name) (sizeof(name) / sizeof((name)[0]))
#define KORL_MEMORY_POOL_ISFULL(name) (name##_korlMemoryPoolSize >= korl_arraySize(name))
/**
 * \return a pointer to the added element
 */
#define KORL_MEMORY_POOL_ADD(name) \
    ( korl_assert(name##_korlMemoryPoolSize < korl_arraySize(name))\
    , &(name)[name##_korlMemoryPoolSize++] )
#define KORL_MEMORY_POOL_REMOVE(name, index) \
    ( korl_assert(name##_korlMemoryPoolSize > 0) \
    , korl_assert((Korl_MemoryPool_Size)(index) < name##_korlMemoryPoolSize) \
    , (name)[index] = (name)[name##_korlMemoryPoolSize - 1] \
    , name##_korlMemoryPoolSize-- )
#define KORL_MEMORY_POOL_ISEMPTY(name) (name##_korlMemoryPoolSize == 0)
#define KORL_MEMORY_POOL_EMPTY(name) (name##_korlMemoryPoolSize = 0)
#define KORL_MEMORY_POOL_RESIZE(name, newSize) \
    ( korl_assert(newSize <= korl_arraySize(name)) \
    , name##_korlMemoryPoolSize = newSize )
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
 * multiplied by -1 is returned.
 */
korl_internal i$ korl_memory_stringCopy(const wchar_t* source, wchar_t* destination, u$ destinationSize);
korl_internal KORL_PLATFORM_MEMORY_ZERO(korl_memory_zero);
korl_internal bool korl_memory_isNull(const void* p, size_t bytes);
korl_internal wchar_t* korl_memory_stringFormat(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, va_list vaList);
korl_internal KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR    (korl_memory_allocator_create);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE  (korl_memory_allocator_allocate);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(korl_memory_allocator_reallocate);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_FREE      (korl_memory_allocator_free);
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY     (korl_memory_allocator_empty);
#define korl_memory_allocate(handle, bytes)               korl_memory_allocator_allocate(handle, bytes, __FILEW__, __LINE__)
#define korl_memory_reallocate(handle, allocation, bytes) korl_memory_allocator_reallocate(handle, allocation, bytes, __FILEW__, __LINE__)
#define korl_memory_free(handle, allocation)              korl_memory_allocator_free(handle, allocation, __FILEW__, __LINE__)
