#include "korl-global-defines.h"
#include "korl-windows-global-defines.h"
#include "korl-io.h"
#include "korl-assert.h"
#include "korl-windows-utilities.h"
#include "korl-memory.h"
#include "korl-math.h"
struct Korl_AllocatorStack_AllocationMetaData
{
    void* address;
    const char* file;
    int line;
    char _padding[4];
};
struct KorlAllocatorStack
{
    void* nextAddress;
    void* endAddress;
    u32 allocationMax;
    u32 allocationCount;
    struct Korl_AllocatorStack_AllocationMetaData allocationMetaDatas[1];
};
#if 0
/** 
 * \param address where to store the \c KorlAllocatorStack object
 * \param bytes how many bytes that the allocator will occupy adjacent to 
 * \c address
 * \return a pointer to the \c KorlAllocatorStack which occupies all the memory 
 * starting at \c address , and occupying \c bytes bytes afterwards
*/
korl_internal struct KorlAllocatorStack* korl_allocator_stack_initialize(
    void* address, size_t bytes)
{
    korl_assert(bytes >= korl_memory_pageSize());
    void*const endAddress = (u8*)address + bytes;
}
#endif//0
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    korl_log(INFO, "start");
    korl_memory_initialize();
    /* how do you align stuff in C? */
    korl_log(INFO, "sizeof(struct KorlAllocatorStack)=%llu alignof=%llu", 
        sizeof(struct KorlAllocatorStack), alignof(struct KorlAllocatorStack));
    korl_log(INFO, "sizeof(struct Korl_AllocatorStack_AllocationMetaData)=%llu", 
        sizeof(struct Korl_AllocatorStack_AllocationMetaData));
    //offsetof (struct DummyStruct{ char c; struct KorlAllocatorStack member; }, member);
    //ALIGNOF(KorlAllocatorStack);
    //korl_log(INFO, "alignof(KorlAllocatorStack)=%i", );
    /* let's play with dynamic memory allocation~ */
    struct Korl_Memory_Allocation allocationTest = 
        korl_memory_allocate(5000, korl_memory_addressMin());
    korl_assert(allocationTest.address);
    korl_log(INFO, "allocationTest.address=%p bytes=%llu", 
        allocationTest.address, allocationTest.bytes);
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
    /* end the application */
    korl_log(INFO, "end");
    ExitProcess(KORL_EXIT_SUCCESS);
}
#include "korl-assert.c"
#include "korl-io.c"
#include "korl-windows-utilities.c"
#include "korl-math.c"
#include "korl-memory.c"
