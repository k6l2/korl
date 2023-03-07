#include "korl-stb-ds.h"
#include "korl-checkCast.h"
#ifdef KORL_PLATFORM
#include "korl-memory.h"
/* STBDS_UNIT_TESTS causes a bunch of really slow code, 
    so it should _never_ run in "production" builds */
// #define STBDS_UNIT_TESTS
/* Performance analysis tool for comparing the efficiency of KORL allocators to 
    whatever the hell is happening in the CRT */
#define _KORL_STB_DS_USE_CRT_MEMORY_MANAGEMENT 0
korl_global_variable Korl_Memory_AllocatorHandle _korl_stb_ds_allocatorHandle;
typedef struct _Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator
{
    const void* in_allocation;
    Korl_Memory_AllocatorHandle out_allocatorHandle;
} _Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator;
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_stb_ds_enumAllocatorCallback_findContainingAllocator)
{
    _Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator*const callbackData = KORL_C_CAST(_Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator*, userData);
    if(korl_memory_allocator_containsAllocation(opaqueAllocator, callbackData->in_allocation))
    {
        callbackData->out_allocatorHandle = allocatorHandle;
        return false;
    }
    return true;
}
korl_internal KORL_FUNCTION__korl_stb_ds_reallocate(_korl_stb_ds_reallocate)
{
    Korl_Memory_AllocatorHandle allocatorHandle = korl_checkCast_u$_to_u16(KORL_C_CAST(u$, context));
    if(!allocatorHandle)
#if _KORL_STB_DS_USE_CRT_MEMORY_MANAGEMENT
        return realloc(allocation, bytes);
#elif defined(STBDS_UNIT_TESTS)
        allocatorHandle = _korl_stb_ds_allocatorHandle;
#else
    {
        KORL_ZERO_STACK(_Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator, enumAllocatorsUserData);
        enumAllocatorsUserData.in_allocation = allocation;
        korl_memory_allocator_enumerateAllocators(_korl_stb_ds_enumAllocatorCallback_findContainingAllocator, &enumAllocatorsUserData);
        allocatorHandle = enumAllocatorsUserData.out_allocatorHandle;
    }
#endif
    return korl_memory_allocator_reallocate(allocatorHandle, allocation, bytes, file, line, false);
}
korl_internal KORL_FUNCTION__korl_stb_ds_free(_korl_stb_ds_free)
{
    Korl_Memory_AllocatorHandle allocatorHandle = korl_checkCast_u$_to_u16(KORL_C_CAST(u$, context));
    if(!allocatorHandle)
#if _KORL_STB_DS_USE_CRT_MEMORY_MANAGEMENT
    {
        free(allocation);
        return;
    }
#elif defined(STBDS_UNIT_TESTS)
        allocatorHandle = _korl_stb_ds_allocatorHandle;
#else
    {
        KORL_ZERO_STACK(_Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator, enumAllocatorsUserData);
        enumAllocatorsUserData.in_allocation = allocation;
        korl_memory_allocator_enumerateAllocators(_korl_stb_ds_enumAllocatorCallback_findContainingAllocator, &enumAllocatorsUserData);
        allocatorHandle = enumAllocatorsUserData.out_allocatorHandle;
    }
#endif
    korl_free(allocatorHandle, allocation);
}
korl_internal void korl_stb_ds_initialize(void)
{
    /* although this allocator is only utilized in KORL_DEBUG mode, we should 
        create it in all builds to maintain allocator handle order */
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(1);
#if defined(STBDS_UNIT_TESTS)
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(448);
    _korl_stb_ds_allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, L"korl-stb-ds", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    stbds_unit_tests();
    korl_assert(korl_memory_allocator_isEmpty(_korl_stb_ds_allocatorHandle));
#else
    _korl_stb_ds_allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, L"korl-stb-ds", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
#endif
}
#endif
korl_internal void korl_stb_ds_arrayAppendU8(void* memoryContext, u8** pStbDsArray, const void* data, u$ bytes)
{
    const u$ arrayLengthInitial = arrlenu(*pStbDsArray);
    mcarrsetlen(memoryContext, *pStbDsArray, arrayLengthInitial + bytes);
    korl_memory_copy(*pStbDsArray + arrayLengthInitial, data, bytes);
}
#define STB_DS_IMPLEMENTATION
#define STBDS_ASSERT(x) korl_assert(x)
#include "stb/stb_ds.h"
typedef struct _Korl_Stb_Ds_HashMap_DefragmentPointer
{
    u$ defragmentPointersIndex;
    u$ elementBytes;
} _Korl_Stb_Ds_HashMap_DefragmentPointer;

/** the following functions _must_ be placed _below_ the stb_ds implementation, 
 * as we need stb_ds implementation struct definitions such as `stbds_hash_index` 
 * and `stbds_array_header` */
korl_internal KORL_HEAP_ON_ALLOCATION_MOVED_CALLBACK(_korl_stb_ds_onAllocationMovedCallback_hashMap_hashTable)
{
    stbds_hash_index*const stbDsHashIndex = KORL_C_CAST(stbds_hash_index*, allocationAddress);
    stbDsHashIndex->storage = KORL_C_CAST(stbds_hash_bucket*, KORL_C_CAST(i$, stbDsHashIndex->storage) + byteOffsetFromOldAddress);
    /*@TODO: `stbDsHashIndex->storage` is expected to be 64-bit-aligned, and it is unlikely to be aligned anymore; performance hazard? */
    switch(stbDsHashIndex->string.mode)
    {
    case STBDS_SH_STRDUP:{// stbDsHashIndex->temp_key potentially points to a single dynamic allocation (see `stbds_hmput_key`)
        korl_assert(!"not implemented");
        break;}
    case STBDS_SH_ARENA:{// stbDsHashIndex->string.storage contains a linked-list of dynamic allocations (see `stbds_hmput_key`, then `stbds_mcstralloc`)
        // stbDsHashIndex->string.storage is being passed as another DefragmentPointer
        // stbDsHashIndex->temp_key is being updated within `_korl_stb_ds_onAllocationMovedCallback_hashMap_stringStorageBlockArena`
        break;}
    case STBDS_SH_DEFAULT:{/*no idea if we have to do anything here...*/ break;}
    default:              {/*no idea if we have to do anything here...*/ break;}
    }
    /*@TODO: do we need to modify stbDsHashIndex->temp_key here in certain cases? ... */
}
korl_internal KORL_HEAP_ON_ALLOCATION_MOVED_CALLBACK(_korl_stb_ds_onAllocationMovedCallback_hashMap_stringStorageBlockArena)
{
    const _Korl_Stb_Ds_HashMap_DefragmentPointer*const hashMapDefragPointerMeta = KORL_C_CAST(_Korl_Stb_Ds_HashMap_DefragmentPointer*, userData);
    /* we have to go through this absolutely bananas dance with `defragmentPointersIndex` 
        and all that jazz because we need a _stable_ way of referencing the 
        stb_ds hash map which owns this string storage block arena; ergo, the 
        userData _must_ be a stable memory address, and this callback _must_ 
        have an immutable array of Korl_Heap_DefragmentPointer used in this 
        defragmentation routine, which can be combined with an immutable 
        DefragmentPointer index to finally achieve this... *phew*! */
    const Korl_Heap_DefragmentPointer*const hashMapDefragmentPointer = defragmentPointers + hashMapDefragPointerMeta->defragmentPointersIndex;
    /* if the hash index temp key is in our old byte range, offset it */
    stbds_array_header*const stbDsArrayHeader = stbds_header(KORL_C_CAST(u8*, *hashMapDefragmentPointer->userAddressPointer) - hashMapDefragPointerMeta->elementBytes);
    stbds_hash_index*const   stbDsHashIndex   = KORL_C_CAST(stbds_hash_index*, stbDsArrayHeader->hash_table);
    const u8*const           oldAllocation    = KORL_C_CAST(u8*, allocationAddress) - byteOffsetFromOldAddress;
    if(   KORL_C_CAST(const u8*, stbDsHashIndex->temp_key) >= oldAllocation 
       && KORL_C_CAST(const u8*, stbDsHashIndex->temp_key) <  oldAllocation + allocationBytes)
        stbDsHashIndex->temp_key += byteOffsetFromOldAddress;
    /* iterate over all hash map array elements; if their `key` is in our old byte range, offset it; THIS IS SO MUCH FUN :) */
    char** hashMapArrayKeyStart = KORL_C_CAST(char**, stbDsArrayHeader + 1);
    for(u$ i = 0; i < stbDsArrayHeader->length; i++)
    {
        /* note: there is a heavy assumption here that the `char* key` element of each hash map entry is the first member of the struct */
        const char**const hashMapArrayKey = KORL_C_CAST(const char**, KORL_C_CAST(u$, hashMapArrayKeyStart) + (i * hashMapDefragPointerMeta->elementBytes));
        if(   KORL_C_CAST(u8*, *hashMapArrayKey) >= oldAllocation 
           && KORL_C_CAST(u8*, *hashMapArrayKey) <  oldAllocation + allocationBytes)
            *hashMapArrayKey += byteOffsetFromOldAddress;
    }
}
korl_internal void _korl_stb_ds_stbDaDefragment_hashMap(void* stbDaDefragmentPointersMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void** pStbDsHashMap, const u$ hashMapElementBytes, void* allocationParent)
{
    if(!(*pStbDsHashMap))
        return;
    const Korl_Memory_AllocatorHandle stackAllocator = korl_checkCast_u$_to_u16(KORL_C_CAST(u$, stbDaDefragmentPointersMemoryContext));// this is stupid, but I don't really care.  stb_ds hash maps are even more stupid
    _Korl_Stb_Ds_HashMap_DefragmentPointer*const hashMapDefragmentPointer = KORL_C_CAST(_Korl_Stb_Ds_HashMap_DefragmentPointer*, korl_allocate(stackAllocator, sizeof(*hashMapDefragmentPointer)));
    hashMapDefragmentPointer->defragmentPointersIndex = arrlenu(*pStbDaDefragmentPointers);
    hashMapDefragmentPointer->elementBytes            = hashMapElementBytes;
    mcarrpush(stbDaDefragmentPointersMemoryContext, *pStbDaDefragmentPointers
             ,(KORL_STRUCT_INITIALIZE(Korl_Heap_DefragmentPointer){pStbDsHashMap, -KORL_C_CAST(i32, sizeof(stbds_array_header) + hashMapElementBytes), allocationParent, NULL}));
    stbds_array_header*const stbDsArrayHeader = stbds_header(KORL_C_CAST(u8*, *pStbDsHashMap) - hashMapElementBytes);
    if(stbDsArrayHeader->hash_table)
    {
        mcarrpush(stbDaDefragmentPointersMemoryContext, *pStbDaDefragmentPointers
                 ,(KORL_STRUCT_INITIALIZE(Korl_Heap_DefragmentPointer){KORL_C_CAST(void**, &stbDsArrayHeader->hash_table), 0, *pStbDsHashMap
                                                                      ,_korl_stb_ds_onAllocationMovedCallback_hashMap_hashTable}));
        stbds_hash_index*const stbDsHashIndex = KORL_C_CAST(stbds_hash_index*, stbDsArrayHeader->hash_table);
        switch(stbDsHashIndex->string.mode)
        {
        case STBDS_SH_STRDUP:{// stbDsHashIndex->temp_key potentially points to a single dynamic allocation (see `stbds_hmput_key`)
            korl_assert(!"not implemented");
            break;}
        case STBDS_SH_ARENA:{// stbDsHashIndex->string.storage contains a linked-list of dynamic allocations (see `stbds_hmput_key`, then `stbds_mcstralloc`)
            void* currentParent = stbDsArrayHeader->hash_table;
            for(stbds_string_block** pStbDsStringBlock = &stbDsHashIndex->string.storage; *pStbDsStringBlock; pStbDsStringBlock = &((*pStbDsStringBlock)->next))
            {
                /* each time a string block is moved in memory, we need to make sure that _all_ members of the hash table's array whose 
                    `char*` `key` member points to a memory address within that string block has their `char*` offset as well!!! 
                    solution: add a `onAllocationMovedUserData` member to `Korl_Heap_DefragmentPointer`, and set it to `pStbDsHashMap`, 
                        since the address at this pointer will be updated upon defragmentation; this will allow us to fully update everything 
                        we need to update in the entire hash map when a string block is moved, including the hash index temp_key (if necessary)! */
                mcarrpush(stbDaDefragmentPointersMemoryContext, *pStbDaDefragmentPointers
                         ,(KORL_STRUCT_INITIALIZE(Korl_Heap_DefragmentPointer){KORL_C_CAST(void**, pStbDsStringBlock), 0, currentParent
                                                                              ,_korl_stb_ds_onAllocationMovedCallback_hashMap_stringStorageBlockArena
                                                                              ,hashMapDefragmentPointer}));
                currentParent = *pStbDsStringBlock;
            }
            break;}
        case STBDS_SH_DEFAULT:{/*no idea if we have to do anything here...*/ break;}
        default:              {/*no idea if we have to do anything here...*/ break;}
        }
    }
}
