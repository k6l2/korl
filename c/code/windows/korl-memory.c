#include "korl-memory.h"
#include "korl-windows-globalDefines.h"
#include "korl-checkCast.h"
#include "korl-assert.h"
#include "korl-io.h"
#define _KORL_MEMORY_INVALID_BYTE_PATTERN 0xFE
korl_global_variable SYSTEM_INFO _korl_memory_systemInfo;
korl_internal void korl_memory_initialize(void)
{
    SecureZeroMemory(&_korl_memory_systemInfo, sizeof(_korl_memory_systemInfo));
    GetSystemInfo(&_korl_memory_systemInfo);
}
korl_internal u$ korl_memory_pageBytes(void)
{
    return _korl_memory_systemInfo.dwPageSize;
}
korl_internal void* korl_memory_addressMin(void)
{
    return _korl_memory_systemInfo.lpMinimumApplicationAddress;
}
korl_internal void* korl_memory_addressMax(void)
{
    return _korl_memory_systemInfo.lpMaximumApplicationAddress;
}
/** @todo: move this into an OS-independent file since this isn't using any OS-
 * specific code - @pull-out-os-agnostic */
korl_internal int korl_memory_compare(const void* a, const void* b, size_t bytes)
{
    const u8* aBytes = KORL_C_CAST(const u8*, a);
    const u8* bBytes = KORL_C_CAST(const u8*, b);
    for(size_t i = 0; i < bytes; ++i)
    {
        if(aBytes[i] < bBytes[i])
            return -1;
        else if(aBytes[i] > bBytes[i])
            return 1;
    }
    return 0;
}
korl_internal int korl_memory_stringCompare(const wchar_t* a, const wchar_t* b)
{
    for(; *a && *b; ++a, ++b)
    {
        if(*a < *b)
            return -1;
        else if(*a > *b)
            return 1;
    }
    if(*a)
        return 1;
    else if(*b)
        return -1;
    return 0;
}
korl_internal u$ korl_memory_stringSize(const wchar_t* s)
{
    /*  [ t][ e][ s][ t][\0]
        [ 0][ 1][ 2][ 3][ 4]
        [sB]            [s ] */
    const wchar_t* sBegin = s;
    for(; *s; ++s) {}
    return s - sBegin;
}
korl_internal i$ korl_memory_stringCopy(const wchar_t* source, wchar_t* destination, u$ destinationSize)
{
    const wchar_t*const destinationEnd = destination + destinationSize;
    i$ charsCopied = 0;
    for(; *source; ++source, ++destination, ++charsCopied)
        if(destination < destinationEnd - 1)
            *destination = *source;
    *destination = L'\0';
    ++charsCopied;// +1 for the null terminator
    if(korl_checkCast_i$_to_u$(charsCopied) > destinationSize)
        charsCopied *= -1;
    return charsCopied;
}
korl_internal void korl_memory_nullify(void*const p, size_t bytes)
{
    SecureZeroMemory(p, bytes);
}
/** @todo: @pull-out-os-agnostic */
korl_internal bool korl_memory_isNull(const void* p, size_t bytes)
{
    const void*const pEnd = KORL_C_CAST(const u8*, p) + bytes;
    for(; p != pEnd; p = KORL_C_CAST(const u8*, p) + 1)
        if(*KORL_C_CAST(const u8*, p))
            return false;
    return true;
}
typedef struct _Korl_Memory_AllocatorLinear
{
    /** total amount of reserved virtual address space, including this struct */
    u$ bytes;
    /** 
     * total amount of comitted virtual address space AFTER pages used by this 
     * struct, including any NOACCESS pages, allocation metadata, etc.
     */
    u$ bytesAllocated;
} _Korl_Memory_AllocatorLinear;
typedef struct _Korl_Memory_AllocationMeta
{
    const wchar_t* file;
    int line;
    /** 
     * The amount of actual memory used by the caller.  The grand total amount 
     * of memory used by an allocation will likely be the sum of the following:  
     * - the allocation meta data
     * - the actual memory used by the caller
     * - any additional padding required by the allocator (likely to round 
     *   everything up to the nearest page size)
     */
    u$ bytes;
} _Korl_Memory_AllocationMeta;
KORL_MEMORY_ALLOCATOR_CALLBACK_ALLOCATE(korl_memory_allocatorLinear_allocate)
{
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, userData);
    /* release the protection on the allocator pages */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(allocator->bytes % pageBytes == 0);
    const u$ allocatorPages       = (sizeof(*allocator)        + (pageBytes - 1)) / pageBytes;
    const u$ pagesAllocated       = (allocator->bytesAllocated + (pageBytes - 1)) / pageBytes;
    const u$ totalReservedPages   = allocator->bytes / pageBytes;
    const u$ availableCommitPages = totalReservedPages - allocatorPages - pagesAllocated;
    const u$ allocationPages      = ((bytes + (pageBytes - 1)) / pageBytes) + 1;// +1 for an additional preceding NOACCESS page
    /* ensure that the allocator has enough space for the allocation + any 
        additional overhead required for the allocation */
    if(availableCommitPages < allocationPages)
    {
        korl_log(WARNING, "Not enough space to allocate %lu bytes!", bytes);
        return NULL;
    }
    /* commit the pages of this allocation */
    korl_assert(sizeof(_Korl_Memory_AllocationMeta) < pageBytes);
    _Korl_Memory_AllocationMeta*const metaPageAddress = KORL_C_CAST(_Korl_Memory_AllocationMeta*, 
        KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes) + allocator->bytesAllocated);
    void*const allocationAddress = KORL_C_CAST(u8*, metaPageAddress) + pageBytes;
    {
        LPVOID resultVirtualAlloc = 
            VirtualAlloc(metaPageAddress, allocationPages * pageBytes, MEM_COMMIT, PAGE_READWRITE);
        if(resultVirtualAlloc == NULL)
            korl_logLastError("VirtualAlloc failed!");
        korl_assert(resultVirtualAlloc == metaPageAddress);
    }
    /* ensure that the allocation address is filled with zeros */
    FillMemory(allocationAddress, bytes, 0);
    /* ensure that the space within the allocation's pages that lie outside the 
        range of `bytes` is filled with some known invalid byte pattern */
    FillMemory(
        KORL_C_CAST(u8*, allocationAddress) + bytes, 
        (allocationPages - 1) * pageBytes - bytes, 
        _KORL_MEMORY_INVALID_BYTE_PATTERN);
    /* initialize allocation's metadata */
    metaPageAddress->file  = file;
    metaPageAddress->line  = line;
    metaPageAddress->bytes = bytes;
    /* protect the allocation's metadata page */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(metaPageAddress, sizeof(*metaPageAddress), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    /* update allocator's metrics */
    allocator->bytesAllocated += allocationPages * pageBytes;
    /* protect the allocator pages until the time comes to actually use it */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    return allocationAddress;
}
KORL_MEMORY_ALLOCATOR_CALLBACK_REALLOCATE(korl_memory_allocatorLinear_reallocate)
{
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, userData);
    korl_assert(!"@todo");
    return NULL;
}
KORL_MEMORY_ALLOCATOR_CALLBACK_FREE(korl_memory_allocatorLinear_free)
{
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, userData);
    korl_assert(!"@todo");
}
korl_internal Korl_Memory_Allocator korl_memory_createAllocatorLinear(u64 maxBytes)
{
    /* @safety: ensure that the thread calling this function is the main thread 
        to ensure that the function pointers of the Korl_Memory_Allocator will 
        always be valid, since the main program module (should) never be 
        reloaded */
    korl_assert(maxBytes > 0);
    korl_assert(maxBytes >= sizeof(_Korl_Memory_AllocatorLinear));
    KORL_ZERO_STACK(Korl_Memory_Allocator, result);
    /* calculate how many pages we need in virtual address space to satisfy maxBytes */
    const u$ pageCount = (maxBytes + (korl_memory_pageBytes() - 1)) / korl_memory_pageBytes();
    const u$ pageBytes = pageCount * korl_memory_pageBytes();
    /* reserve all these pages */
    LPVOID resultVirtualAlloc = 
        VirtualAlloc(NULL/*specify start address; optional*/, pageBytes, MEM_RESERVE, PAGE_NOACCESS);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    result.userData = resultVirtualAlloc;
    /* commit the first page for the allocator itself */
    resultVirtualAlloc = 
        VirtualAlloc(result.userData, korl_memory_pageBytes(), MEM_COMMIT, PAGE_READWRITE);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* initialize the memory of the allocator userData */
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, resultVirtualAlloc);
    allocator->bytes = pageBytes;
    /* protect the allocator pages until the time comes to actually use it */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    /* populate the allocator callback function pointers */
    result.callbackAllocate   = korl_memory_allocatorLinear_allocate;
    result.callbackReallocate = korl_memory_allocatorLinear_reallocate;
    result.callbackFree       = korl_memory_allocatorLinear_free;
    return result;
}
