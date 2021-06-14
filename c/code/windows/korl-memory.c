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
korl_internal struct Korl_Memory_Allocation korl_memory_allocate(size_t bytes)
{
    /* round bytes up to the nearest page size */
    const size_t pageCount = 
        (bytes + (korl_memory_pageSize() - 1)) / korl_memory_pageSize();
    const size_t pageBytes = pageCount * korl_memory_pageSize();
    /* attempt to allocate pages of memory to satisfy byte requirement */
    LPVOID resultVirtualAlloc = 
        VirtualAlloc(
            NULL/*start address*/, pageBytes, 
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    /* generate the result; only populate the # of bytes if the allocation 
        succeeds */
    struct Korl_Memory_Allocation result;
    ZeroMemory(&result, sizeof(result));
    result.address = resultVirtualAlloc;
    if(resultVirtualAlloc)
    {
        result.bytes = pageBytes;
    }
    return result;
}
