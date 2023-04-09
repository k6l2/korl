#pragma once
#include "korl-globalDefines.h"
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
#define KORL_MEMORY_POOL_LAST(name) \
    ( korl_assert(name##_korlMemoryPoolSize > 0)\
    , (name)[name##_korlMemoryPoolSize - 1])
#define KORL_MEMORY_POOL_LAST_POINTER(name) \
    ( korl_assert(name##_korlMemoryPoolSize > 0)\
    , (name) + (name##_korlMemoryPoolSize) - 1)
#define KORL_MEMORY_POOL_POP(name) \
    ( korl_assert(name##_korlMemoryPoolSize > 0)\
    , name##_korlMemoryPoolSize--\
    , (name)[name##_korlMemoryPoolSize])
