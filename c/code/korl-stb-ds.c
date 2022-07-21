#include "korl-stb-ds.h"
#include "korl-memory.h"
#include "korl-checkCast.h"
/* STBDS_UNIT_TESTS causes a bunch of really slow code, 
    so it should _never_ run in "release" builds */
#if 1//KORL_DEBUG @TODO
#   define STBDS_UNIT_TESTS
#endif
#define _KORL_STB_DS_USE_CRT_MEMORY_MANAGEMENT 0
korl_global_variable Korl_Memory_AllocatorHandle _korl_stb_ds_allocatorHandle;
typedef struct _Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator
{
    const void* in_allocation;
    Korl_Memory_AllocatorHandle out_allocatorHandle;
} _Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator;
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_stb_ds_enumAllocatorCallback_findContainingAllocator)
{
    _Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator*const callbackData = 
        KORL_C_CAST(_Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator*, userData);
    if(korl_memory_allocator_containsAllocation(opaqueAllocator, callbackData->in_allocation))
    {
        callbackData->out_allocatorHandle = allocatorHandle;
        return false;
    }
    return true;
}
korl_internal void* _korl_stb_ds_reallocate(void* context, void* allocation, u$ bytes, const wchar_t*const file, int line)
{
    Korl_Memory_AllocatorHandle allocatorHandle = korl_checkCast_u$_to_u16(KORL_C_CAST(u$, context));
    if(!allocatorHandle)
#if _KORL_STB_DS_USE_CRT_MEMORY_MANAGEMENT
        return realloc(allocation, bytes);
#else
        allocatorHandle = _korl_stb_ds_allocatorHandle;
#endif
    else
    {
        KORL_ZERO_STACK(_Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator, enumAllocatorsUserData);
        enumAllocatorsUserData.in_allocation = allocation;
        korl_memory_allocator_enumerateAllocators(_korl_stb_ds_enumAllocatorCallback_findContainingAllocator, &enumAllocatorsUserData);
        allocatorHandle = enumAllocatorsUserData.out_allocatorHandle;
    }
    return korl_memory_allocator_reallocate(allocatorHandle, allocation, bytes, file, line);
}
korl_internal void _korl_stb_ds_free(void* context, void* allocation)
{
    Korl_Memory_AllocatorHandle allocatorHandle = korl_checkCast_u$_to_u16(KORL_C_CAST(u$, context));
    if(!allocatorHandle)
#if _KORL_STB_DS_USE_CRT_MEMORY_MANAGEMENT
    {
        free(allocation);
        return;
    }
#else
        allocatorHandle = _korl_stb_ds_allocatorHandle;
#endif
    else
    {
        KORL_ZERO_STACK(_Korl_Stb_Ds_EnumAllocatorsUserData_FindContainingAllocator, enumAllocatorsUserData);
        enumAllocatorsUserData.in_allocation = allocation;
        korl_memory_allocator_enumerateAllocators(_korl_stb_ds_enumAllocatorCallback_findContainingAllocator, &enumAllocatorsUserData);
        allocatorHandle = enumAllocatorsUserData.out_allocatorHandle;
    }
    korl_free(allocatorHandle, allocation);
}
korl_internal void korl_stb_ds_initialize(void)
{
    /* although this allocator is only utilized in KORL_DEBUG mode, we should 
        create it in all builds to maintain allocator handle order */
    _korl_stb_ds_allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_megabytes(448), L"korl-stb-ds", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL);
#if defined(STBDS_UNIT_TESTS)
    stbds_unit_tests();
    korl_assert(korl_memory_allocator_isEmpty(_korl_stb_ds_allocatorHandle));
#endif
}
#define STB_DS_IMPLEMENTATION
#define STBDS_ASSERT(x) korl_assert(x)
#include "stb/stb_ds.h"
