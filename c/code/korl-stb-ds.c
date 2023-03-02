#include "korl-stb-ds.h"
#include "korl-memory.h"
#include "korl-checkCast.h"
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
    return korl_memory_allocator_reallocate(allocatorHandle, allocation, bytes, file, line);
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
korl_internal void korl_stb_ds_arrayAppendU8(void* memoryContext, u8** pStbDsArray, const void* data, u$ bytes)
{
    const u$ arrayLengthInitial = arrlenu(*pStbDsArray);
    mcarrsetlen(memoryContext, *pStbDsArray, arrayLengthInitial + bytes);
    korl_memory_copy(*pStbDsArray + arrayLengthInitial, data, bytes);
}
#define STB_DS_IMPLEMENTATION
#define STBDS_ASSERT(x) korl_assert(x)
#include "stb/stb_ds.h"
/** this _must_ be placed _below_ the stb_ds implementation, as we need the 
 * `stbds_hash_index` struct definition */
korl_internal KORL_HEAP_ON_ALLOCATION_MOVED_CALLBACK(korl_stb_ds_onAllocationMovedCallback_hashMap_hashTable)
{
    stbds_hash_index*const stbDsHashIndex = KORL_C_CAST(stbds_hash_index*, allocationAddress);
    stbDsHashIndex->storage = KORL_C_CAST(stbds_hash_bucket*, KORL_C_CAST(i$, stbDsHashIndex->storage) + byteOffsetFromOldAddress);
}
