/**
 * The use-case for Korl_Pool is when the user wants to maintain a collection of 
 * either uniform structs or polymorphic-tagged-unions, where they can be 
 * referred to at any time using a small "handle" value, such that operations on 
 * said objects are essentially constant time.  This abstraction allows objects 
 * to refer to each other via these aforementioned "handle" values in an 
 * extremely safe way, such that when an object in the pool is removed and a 
 * different object that has a handle to it tries to get it: the handle used to 
 * attempt to get the object will be invalidated, and the caller will receive a 
 * NULL object.
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-memory.h"
typedef u32 Korl_Pool_Index;
typedef u16 Korl_Pool_ItemType;
typedef u16 Korl_Pool_Salt;
typedef u64 Korl_Pool_Handle;// composed of {Korl_Pool_Index, Korl_Pool_ItemType + 1, Korl_Pool_Salt}; ItemType is incremented internally to prevent the existence of a {0,0,0} handle, since 0 => invalid handle
#define KORL_POOL_HANDLE_ITEM_TYPE(poolHandle) KORL_C_CAST(u16, (((poolHandle) >> 48) & KORL_U16_MAX) - 1)
typedef struct Korl_Pool_ItemMeta
{
    Korl_Pool_ItemType type;// due to the requirement that a Korl_Pool_Handle == 0 is invalid, this value will never be stored as 0 for a valid pool item; see comments for Korl_Pool_Handle
    Korl_Pool_Salt     salt;
} Korl_Pool_ItemMeta;
typedef struct Korl_Pool
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_Pool_Index             capacity;
    u32                         byteStride;
    struct
    {
        // parallel arrays:
        Korl_Pool_ItemMeta* metas;// .type == 0 => this item slot is unoccupied; see comments for Korl_Pool_Handle & Korl_Pool_ItemMeta::type
        void*               datas;
    } items;
    Korl_Pool_Index itemCount;// (de/in)cremented each time an item is added/removed from the Korl_Pool
} Korl_Pool;
#define KORL_POOL_CALLBACK_FOR_EACH(name) void name(void* userData, void* item)
typedef KORL_POOL_CALLBACK_FOR_EACH(korl_pool_callback_forEach);
korl_internal void             korl_pool_initialize(Korl_Pool* context, Korl_Memory_AllocatorHandle allocator, u32 byteStride, Korl_Pool_Index capacity);
korl_internal void             korl_pool_destroy(Korl_Pool* context);
korl_internal void             korl_pool_clear(Korl_Pool* context);
korl_internal Korl_Pool_Handle korl_pool_add(Korl_Pool* context, Korl_Pool_ItemType itemType, void** o_newItem);// note that this API does not assume whether or not the Korl_Pool's items store their own copy of their handles
korl_internal void*            korl_pool_get(Korl_Pool* context, Korl_Pool_Handle* handle);
korl_internal void             korl_pool_remove(Korl_Pool* context, Korl_Pool_Handle* handle);
korl_internal bool             korl_pool_contains(Korl_Pool* context, Korl_Pool_Handle handle);
korl_internal void             korl_pool_forEach(Korl_Pool* context, korl_pool_callback_forEach* callback, void* callbackUserData);
korl_internal Korl_Pool_Handle korl_pool_nextHandle(Korl_Pool* context, Korl_Pool_ItemType itemType, Korl_Pool_Handle previousHandle);// use to predict what the next result from `pool_add` will be, assuming `previousHandle` is the last result from a call to this function
