#include "korl-memory.h"
#include "korl-windows-global-defines.h"
korl_global_variable SYSTEM_INFO g_korl_memory_systemInfo;
korl_internal void korl_memory_initialize(void)
{
    ZeroMemory(&g_korl_memory_systemInfo, sizeof(g_korl_memory_systemInfo));
    GetSystemInfo(&g_korl_memory_systemInfo);
}
korl_internal u32 korl_memory_pageSize(void)
{
    return g_korl_memory_systemInfo.dwPageSize;
}
korl_internal void* korl_memory_addressMin(void)
{
    return g_korl_memory_systemInfo.lpMinimumApplicationAddress;
}
korl_internal void* korl_memory_addressMax(void)
{
    return g_korl_memory_systemInfo.lpMaximumApplicationAddress;
}
korl_internal Korl_Memory_Allocation korl_memory_allocate(
    size_t bytes, void* desiredAddress)
{
    /* round bytes up to the nearest page size */
    const size_t pageCount = 
        (bytes + (korl_memory_pageSize() - 1)) / korl_memory_pageSize();
    const size_t pageBytes = pageCount * korl_memory_pageSize();
    /* attempt to allocate pages of memory to satisfy byte requirement */
    LPVOID resultVirtualAlloc = 
        VirtualAlloc(
            desiredAddress, pageBytes, 
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if(resultVirtualAlloc == NULL)
        korl_log(ERROR, "VirtualAlloc failed!  GetLastError=%lu", 
            GetLastError());
    /* generate the result; only populate the # of bytes if the allocation 
        succeeds */
    Korl_Memory_Allocation result;
    ZeroMemory(&result, sizeof(result));
    result.address = resultVirtualAlloc;
    if(resultVirtualAlloc)
        result.addressEnd = KORL_C_CAST(u8*, resultVirtualAlloc) + pageBytes;
    return result;
}
