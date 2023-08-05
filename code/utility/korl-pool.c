#include "korl-pool.h"
#include "korl-interface-platform.h"
#include "utility/korl-checkCast.h"
#define _KORL_POOL_ITEM_TYPE_MAX (KORL_C_CAST(Korl_Pool_ItemType, ~0) - 1)
typedef struct _Korl_Pool_HandleUnpacked
{
    Korl_Pool_Index    index;
    Korl_Pool_ItemType type;
    Korl_Pool_Salt     salt;
} _Korl_Pool_HandleUnpacked;
korl_internal Korl_Pool_Handle _korl_pool_handle_pack(_Korl_Pool_HandleUnpacked handleUnpacked)
{
    return  KORL_C_CAST(Korl_Pool_Handle, handleUnpacked.index)
         | (KORL_C_CAST(Korl_Pool_Handle, handleUnpacked.salt) << 32)
         | (KORL_C_CAST(Korl_Pool_Handle, handleUnpacked.type) << 48);
}
korl_internal _Korl_Pool_HandleUnpacked _korl_pool_handle_unpack(Korl_Pool_Handle handle)
{
    const _Korl_Pool_HandleUnpacked handleUnpacked = KORL_STRUCT_INITIALIZE(_Korl_Pool_HandleUnpacked){.index = KORL_C_CAST(u32,  handle        & KORL_U32_MAX)
                                                                                                      ,.type  = KORL_C_CAST(u16, (handle >> 48) & KORL_U16_MAX)
                                                                                                      ,.salt  = KORL_C_CAST(u16, (handle >> 32) & KORL_U16_MAX)};
    return handleUnpacked;
}
korl_internal void korl_pool_initialize(Korl_Pool* context, Korl_Memory_AllocatorHandle allocator, u32 byteStride, Korl_Pool_Index capacity)
{
    korl_assert(capacity);
    context->allocator   = allocator;
    context->byteStride  = byteStride; 
    context->capacity    = capacity;
    context->items.metas = KORL_C_CAST(Korl_Pool_ItemMeta*, korl_allocate(context->allocator, context->capacity * sizeof(*context->items.metas)));
    context->items.datas =                                  korl_allocate(context->allocator, context->capacity * context->byteStride);
}
korl_internal void korl_pool_collectDefragmentPointers(Korl_Pool* context, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent)
{
    KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD(stbDaMemoryContext, *pStbDaDefragmentPointers, context->items.datas, parent);
    KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD(stbDaMemoryContext, *pStbDaDefragmentPointers, context->items.metas, parent);
}
korl_internal void korl_pool_destroy(Korl_Pool* context)
{
    korl_free(context->allocator, context->items.metas);
    korl_free(context->allocator, context->items.datas);
    korl_memory_zero(context, sizeof(*context));
}
korl_internal void korl_pool_clear(Korl_Pool* context)
{
    /* instead of just zeroing out all the ItemMeta memory, it's better to just 
        set all their ItemType values to 0, preserving the salt; then, when new 
        items are added to the pool, there _should_ be a higher chance that 
        invalid handles will be caught since we don't recycle salt values right 
        away */
    for(Korl_Pool_Index i = 0; i < context->capacity; i++)
    {
        context->items.metas[i].type = 0;
        context->items.metas[i].salt++;
    }
    /* clearing the meta types to 0 should suffice, as a Korl_Pool_ItemType value of 
        0 is reserved to represent an "unused" item slot in the pool, but we 
        might as well clear all the actual item data as well */
    korl_memory_zero(context->items.datas, context->capacity * context->byteStride);
    /**/
    context->itemCount = 0;
}
korl_internal Korl_Pool_Handle korl_pool_add(Korl_Pool* context, Korl_Pool_ItemType itemType, void** o_newItem)
{
    korl_assert(itemType <= _KORL_POOL_ITEM_TYPE_MAX);
    Korl_Pool_Index index = 0;
    for(; index < context->capacity; index++)
        if(context->items.metas[index].type == 0)
            break;
    if(index >= context->capacity)
    {
        context->capacity *= 2;
        context->items.metas = KORL_C_CAST(Korl_Pool_ItemMeta*, korl_reallocate(context->allocator, context->items.metas, context->capacity * sizeof(*context->items.metas)));
        context->items.datas =                                  korl_reallocate(context->allocator, context->items.datas, context->capacity * context->byteStride);
    }
    if(o_newItem)
        *o_newItem = KORL_C_CAST(u8*, context->items.datas) + index * context->byteStride;
    Korl_Pool_ItemMeta*const newItemMeta = context->items.metas + index;
    newItemMeta->type = itemType + 1u;// itemType needs to be stored in the ItemMeta in this way so that we know this item slot is now occupied, as opposed to only transforming the type only for the handle
    context->itemCount++;
    return _korl_pool_handle_pack(KORL_STRUCT_INITIALIZE(_Korl_Pool_HandleUnpacked){.index = index, .type = newItemMeta->type, .salt = newItemMeta->salt});
}
korl_internal void* korl_pool_get(Korl_Pool* context, Korl_Pool_Handle* handle)
{
    const Korl_Pool_ItemMeta* itemMeta = NULL;
    if(!*handle)
        return NULL;
    const _Korl_Pool_HandleUnpacked handleUnpacked = _korl_pool_handle_unpack(*handle);
    if(handleUnpacked.index >= context->capacity)
        goto invalidateHandle_returnNull;
    itemMeta = context->items.metas + handleUnpacked.index;
    if(   handleUnpacked.salt != itemMeta->salt
       || handleUnpacked.type != itemMeta->type)
        goto invalidateHandle_returnNull;
    return KORL_C_CAST(u8*, context->items.datas) + handleUnpacked.index * context->byteStride;
    invalidateHandle_returnNull:
        korl_log(WARNING, "invalid handle %llu", *handle);
        *handle = 0;
        return NULL;
}
korl_internal Korl_Pool_Handle korl_pool_itemHandle(Korl_Pool* context, const void* item)
{
    const i$ itemByteOffset = KORL_C_CAST(const u8*, item) - KORL_C_CAST(const u8*, context->items.datas);
    korl_assert(item >= context->items.datas);
    korl_assert(itemByteOffset % context->byteStride == 0);
    korl_assert(itemByteOffset < context->capacity * context->byteStride);
    const Korl_Pool_Index poolIndex = korl_checkCast_i$_to_u32(itemByteOffset / context->byteStride);
    const Korl_Pool_ItemMeta*const itemMeta = context->items.metas + poolIndex;
    if(itemMeta->type == 0)
        return 0;
    return _korl_pool_handle_pack(KORL_STRUCT_INITIALIZE(_Korl_Pool_HandleUnpacked){.index = poolIndex, .type = itemMeta->type, .salt = itemMeta->salt});
}
korl_internal void korl_pool_remove(Korl_Pool* context, Korl_Pool_Handle* handle)
{
    void*               item     = NULL;
    Korl_Pool_ItemMeta* itemMeta = NULL;
    if(!*handle)
        return;
    const _Korl_Pool_HandleUnpacked handleUnpacked = _korl_pool_handle_unpack(*handle);
    if(handleUnpacked.index >= context->capacity)
        goto invalidHandleDetected;
    itemMeta = context->items.metas + handleUnpacked.index;
    if(   handleUnpacked.salt != itemMeta->salt
       || handleUnpacked.type != itemMeta->type)
        goto invalidHandleDetected;
    item = KORL_C_CAST(u8*, context->items.datas) + handleUnpacked.index * context->byteStride;
    korl_memory_zero(item, context->byteStride);
    itemMeta->salt++;
    itemMeta->type = 0;
    *handle        = 0;
    context->itemCount--;
    return;
    invalidHandleDetected:
        korl_log(WARNING, "invalid handle %llu", *handle);
        *handle = 0;
        return;
}
korl_internal bool korl_pool_contains(Korl_Pool* context, Korl_Pool_Handle handle)
{
    if(!handle)
        return false;
    const _Korl_Pool_HandleUnpacked handleUnpacked = _korl_pool_handle_unpack(handle);
    if(handleUnpacked.index >= context->capacity)
        return false;
    const Korl_Pool_ItemMeta*const itemMeta = context->items.metas + handleUnpacked.index;
    return handleUnpacked.salt == itemMeta->salt
        && handleUnpacked.type == itemMeta->type;
}
korl_internal void korl_pool_forEach(Korl_Pool* context, korl_pool_callback_forEach* callback, void* callbackUserData)
{
    for(Korl_Pool_Index i = 0; i < context->capacity; i++)
    {
        void*const               item     = KORL_C_CAST(void*, KORL_C_CAST(u8*, context->items.datas) + i * context->byteStride);
        Korl_Pool_ItemMeta*const itemMeta = context->items.metas + i;
        if(itemMeta->type == 0)
            continue;
        callback(callbackUserData, item);
    }
}
korl_internal Korl_Pool_Handle korl_pool_nextHandle(Korl_Pool* context, Korl_Pool_ItemType itemType, Korl_Pool_Handle previousHandle)
{
    korl_assert(itemType <= _KORL_POOL_ITEM_TYPE_MAX);
    _Korl_Pool_HandleUnpacked previousHandleUnpacked = _korl_pool_handle_unpack(previousHandle);
    Korl_Pool_Index i    = previousHandle ? previousHandleUnpacked.index + 1 : 0;
    Korl_Pool_Salt  salt = 0;
    for(; i < context->capacity; i++)
        if(context->items.metas[i].type == 0)
        {
            salt = context->items.metas[i].salt;
            break;
        }
    return _korl_pool_handle_pack(KORL_STRUCT_INITIALIZE(_Korl_Pool_HandleUnpacked){.index = i, .type = KORL_C_CAST(Korl_Pool_ItemType, itemType + 1u), .salt = salt});
}
