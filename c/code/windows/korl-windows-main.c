#include "korl-global-defines.h"
#include "korl-windows-global-defines.h"
#include "korl-io.h"
#include "korl-assert.h"
#include "korl-windows-utilities.h"
#include "korl-memory.h"
#include "korl-math.h"
typedef struct
{
    void* address;
    const wchar_t* file;
    int line;
} Korl_AllocatorStack_AllocationMetaData;
typedef struct
{
    void* nextAddress;
    void* endAddress;
    u32 allocationMax;
    u32 allocationCount;
    Korl_AllocatorStack_AllocationMetaData allocationMetaDatas[1];
} KorlAllocatorStack;
/** 
 * \param address where to store the \c KorlAllocatorStack object
 * \param addressEnd the address immediately following the range of memory the 
 * new allocator will occupy
 * \return a pointer to the \c KorlAllocatorStack which occupies all the memory 
 * in the range [ \c address , \c addressEnd )
 */
korl_internal KorlAllocatorStack* korl_allocator_stack_initialize(
    void* address, void* addressEnd)
{
    KorlAllocatorStack* result = NULL;
    /* let's make sure that there is at LEAST one page in the target address 
        space so that we can protect the allocator book-keeping memory */
    const uintptr_t bytes = 
        KORL_C_CAST(uintptr_t, addressEnd) - KORL_C_CAST(uintptr_t, address);
    korl_assert(bytes >= korl_memory_pageSize());
    /* make sure the result struct is initialized on an address 
        that is aligned to the start of the first page-aligned address */
    void*const addressAllocatorPage = 
        KORL_C_CAST(void*, korl_math_roundUpPowerOf2(
            KORL_C_CAST(uintptr_t, address), korl_memory_pageSize()));
    /* make sure the result struct is initialized on an address 
        that satisfies the result struct's alignment requirements */
    result = KORL_C_CAST(KorlAllocatorStack*, korl_math_roundUpPowerOf2(
        KORL_C_CAST(uintptr_t, address), alignof(KorlAllocatorStack)));
    /* sanity check our final address */
    korl_assert(KORL_C_CAST(void*, result) >= address);
    korl_assert(KORL_C_CAST(void*, result) <  addressEnd);
    korl_assert(
        KORL_C_CAST(u8*, result) + sizeof(*result) <= 
            KORL_C_CAST(u8*, addressEnd));
    /* initialize the result struct */
    ZeroMemory(result, sizeof(KorlAllocatorStack));
    result->nextAddress = KORL_C_CAST(u8*, addressAllocatorPage) + 
        korl_memory_pageSize();
    result->endAddress = addressEnd;
    // we need to calculate how many allocations we can store in the allocator's 
    //  protected page:
    korl_assert(korl_memory_pageSize() > sizeof(KorlAllocatorStack));
    //korl_memory_pageSize() - sizeof(KorlAllocatorStack);
    //result->allocationMax = ;
    /** @todo */
    /* protect the allocator struct's page(s) */
    /** @todo */
    /* and finally, we can return the allocator's address */
    return result;
}
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    korl_log(INFO, "start");
    korl_memory_initialize();
    /* how do you align stuff in C? */
    korl_log(INFO, "sizeof(KorlAllocatorStack)=%llu alignof=%llu", 
        sizeof(KorlAllocatorStack), alignof(KorlAllocatorStack));
    korl_log(INFO, "sizeof(Korl_AllocatorStack_AllocationMetaData)=%llu", 
        sizeof(Korl_AllocatorStack_AllocationMetaData));
    /* let's play with dynamic memory allocation~ */
    Korl_Memory_Allocation allocationTest = 
        korl_memory_allocate(5000, korl_memory_addressMin());
    korl_assert(allocationTest.address);
    korl_log(INFO, "allocationTest.address=%p bytes=%llu", 
        allocationTest.address, 
        KORL_C_CAST(uintptr_t, allocationTest.addressEnd) - 
            KORL_C_CAST(uintptr_t, allocationTest.address));
#if 0
#if 1
    for(i32 i = 0; i < korl_windows_dword_to_i32(korl_memory_pageSize()) + 1; i++)
        testChars[i] = 69;
#else
    korl_shared_const char TEST_C_STR[] = "lololol...";
    for(size_t i = 0; i < korl_arraySize(TEST_C_STR); i++)
        testChars[i] = TEST_C_STR[i];
#endif//0
#endif//0
    /* okay, let's try initializing a stack allocator */
    korl_allocator_stack_initialize(
        allocationTest.address, allocationTest.addressEnd);
    /* end the application */
    korl_log(INFO, "end");
    ExitProcess(KORL_EXIT_SUCCESS);
}
#include "korl-assert.c"
#include "korl-io.c"
#include "korl-windows-utilities.c"
#include "korl-math.c"
#include "korl-memory.c"
