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
#if 0// @unused
korl_internal void* korl_memory_addressMin(void)
{
    return _korl_memory_systemInfo.lpMinimumApplicationAddress;
}
korl_internal void* korl_memory_addressMax(void)
{
    return _korl_memory_systemInfo.lpMaximumApplicationAddress;
}
#endif
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
    /* @redundant: technically we don't need this, since we can just call 
        QueryVirtualMemoryInformation */
    /** total amount of reserved virtual address space, including this struct */
    u$ bytes;
    /** 
     * Total amount of comitted virtual address space AFTER pages used by this 
     * struct, including any NOACCESS pages, allocation metadata, etc.
     * This is effectively the total comitted memory.
     */
    /** @rename: bytesComitted - better descriptor */
    u$ bytesAllocated;
    /** support for realloc/free for the last allocation */
    void* lastAllocation;
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
    allocator->lastAllocation  = allocationAddress;
    /* protect the allocator pages until the time comes to actually use it */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    return allocationAddress;
}
KORL_MEMORY_ALLOCATOR_CALLBACK_FREE(korl_memory_allocatorLinear_free)
{
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, userData);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(allocator->bytes % pageBytes == 0);
    const u$ allocatorPages = (sizeof(*allocator) + (pageBytes - 1)) / pageBytes;
    /* release the protection on the allocator pages */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    /* ensure that this address is actually within the allocator's range */
    korl_assert(KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
             && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation address is divisible by the page size, because 
        this is currently a constraint of this allocator */
    korl_assert(KORL_C_CAST(u$, allocation) % pageBytes == 0);
    /* if we're not the last allocation, we currently don't support freeing it */
    if(allocation != allocator->lastAllocation)
    {
        korl_log(WARNING, "Attempted to free an allocation that was not the last allocation!");
        goto done;
    }
    /* determine the range of pages occupied by the allocation */
    // get the address of the allocation's metadata page //
    _Korl_Memory_AllocationMeta*const allocationMeta = 
        KORL_C_CAST(_Korl_Memory_AllocationMeta*, KORL_C_CAST(u8*, allocation) - pageBytes);
    // remove protection on the allocation's metadata page so that we can obtain 
    //   the allocation's `size` information //
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocationMeta, sizeof(*allocationMeta), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    // the range of pages occupied by the allocation //
    // note that we get the TOTAL pages occupied by the allocation, including 
    //  the meta data
    const u$ allocationPages = ((allocationMeta->bytes + (pageBytes - 1)) / pageBytes) + 1;
    /* set the allocation's pages to NOACCESS */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocation, allocationPages * pageBytes, PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    /* update allocator's metrics 
        - reduce the # of bytesAllocated
        - clear the allocation address */
    allocator->bytesAllocated -= allocationPages * pageBytes;
    allocator->lastAllocation  = NULL;
done:
    /* protect the allocator pages until the time comes to actually use it */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
}
KORL_MEMORY_ALLOCATOR_CALLBACK_REALLOCATE(korl_memory_allocatorLinear_reallocate)
{
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, userData);
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(allocator->bytes % pageBytes == 0);
    const u$ allocatorPages = (sizeof(*allocator) + (pageBytes - 1)) / pageBytes;
    /* if allocation is NULL, just call `allocate` */
    if(allocation == NULL)
        return korl_memory_allocatorLinear_allocate(allocator, bytes, file, line);
    /* release the protection on the allocator pages */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    /* ensure that this address is actually within the allocator's range */
    korl_assert(KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
             && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation address is divisible by the page size, because 
        this is currently a constraint of this allocator */
    korl_assert(KORL_C_CAST(u$, allocation) % pageBytes == 0);
    /* if bytes is 0, just call `free` & return NULL */
    if(bytes == 0)
    {
        /* conform to our API requirement that the allocator's memory must be 
            protected before each call */
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
        /* now we can just free & return */
        korl_memory_allocatorLinear_free(userData, allocation, file, line);
        return NULL;
    }
    /* determine the range of pages occupied by the allocation */
    // get the address of the allocation's metadata page //
    _Korl_Memory_AllocationMeta*const allocationMeta = 
        KORL_C_CAST(_Korl_Memory_AllocationMeta*, KORL_C_CAST(u8*, allocation) - pageBytes);
    // remove protection on the allocation's metadata page so that we can obtain 
    //   the allocation's `size` information //
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocationMeta, sizeof(*allocationMeta), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    /* if we're not the last allocation, we have one of two options:  
        - if the new requested size can fit in padding at the end of the last 
          page of the allocation, we can just return the original address and be 
          done with it
        - otherwise, we have to create a new allocation and copy the data from 
          the old allocation to the new one 
          - call korl_memory_allocatorLinear_allocate
          - copy the data from the old allocation to the new one
       To determine which case we fall in, we must first obtain the allocation's 
       meta data to see how much actual memory the caller is using, which we 
       have already done above.  */
    if(allocation != allocator->lastAllocation)
    {
        // JUST the number of pages used by the allocation itself, excluding the 
        //  meta data, etc... //
        const u$ allocationPages = (allocationMeta->bytes + (pageBytes - 1)) / pageBytes;
        /* if we have enough room within the allocation's comitted pages */
        if(allocationPages * pageBytes >= bytes)
        {
            /* in-place allocation size modification code runs in the `done` section */
            goto done;
        }
        /* we don't have enough room in the current allocation's pages - we need 
            to make a new allocation! */
        else
        {
            const u$ oldAllocationBytes = allocationMeta->bytes;
            /** @copypasta: same code that runs in the `done` label (not sure if 
             * there is a good way to take this out though) */
            /* protect the allocation's metadata page so we can call other 
                allocator API safely */
            {
                DWORD oldProtect;
                korl_assert(VirtualProtect(allocationMeta, sizeof(*allocationMeta), PAGE_NOACCESS, &oldProtect));
                korl_assert(oldProtect == PAGE_READWRITE);
            }
            /* protect the allocator pages once again so we can call other 
                allocator API safely */
            {
                DWORD oldProtect;
                korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
                korl_assert(oldProtect == PAGE_READWRITE);
            }
            /* create a new allocation */
            void*const newAllocation = korl_memory_allocatorLinear_allocate(allocator, bytes, file, line);
            if(!newAllocation)
                korl_log(WARNING, "failed to create a new allocation");
            else
            {
                /* copy the old allocation's data to the new allocation 
                    - we can use CopyMemory here because we already know that 
                      the memory regions do NOT overlap */
                CopyMemory(newAllocation, allocation, oldAllocationBytes);
                /* free the old allocation */
                korl_memory_allocatorLinear_free(allocator, allocation, file, line);
            }
            return newAllocation;
        }
    }
    /* if we ARE the last allocation, we can just grow it as much as we want, as 
        long as the final size will fit in the allocator's virtual address range */
    else
    {
        /* determine if the new allocation size will fit in the allocator */
        const u$ pagesAllocated       = (allocator->bytesAllocated + (pageBytes - 1)) / pageBytes;
        const u$ totalReservedPages   = allocator->bytes / pageBytes;
        const u$ allocationPagesOld = ((allocationMeta->bytes + (pageBytes - 1)) / pageBytes) + 1;// +1 for an additional preceding NOACCESS page
        const u$ allocationPages    = ((bytes + (pageBytes - 1)) / pageBytes) + 1;// +1 for an additional preceding NOACCESS page
        /* if we don't have enough room to grow, we can effectively just abort 
            here without changing anything internally */
        if(totalReservedPages < pagesAllocated - allocationPagesOld + KORL_MATH_MAX(allocationPagesOld, allocationPages))
        {
            allocation = NULL;
            korl_log(WARNING, "failed to grow the allocation - not enough space");
            goto done;
        }
        /* ensure that if the allocation shrinks, the unused pages are PROTECTED */
        {
            DWORD oldProtect;
            korl_assert(VirtualProtect(allocationMeta, KORL_MATH_MAX(allocationPagesOld, allocationPages) * pageBytes, PAGE_NOACCESS, &oldProtect));
            korl_assert(oldProtect == PAGE_READWRITE);
        }
        /* ensure that if the allocation grows, all the pages are comitted 
            - we can do this because re-comitting a page w/ VirtualAlloc is a 
              valid operation, so this will be just fine if the allocation shrinks */
        {
            LPVOID resultVirtualAlloc = 
                VirtualAlloc(allocationMeta, allocationPages * pageBytes, MEM_COMMIT, PAGE_READWRITE);
            if(resultVirtualAlloc == NULL)
                korl_logLastError("VirtualAlloc failed!");
            korl_assert(resultVirtualAlloc == allocationMeta);
        }
    }
done:
    /* in-place allocation size modification code */
    if(allocation)
    {
        /* fill allocated/freed space with known bit patterns */
        if(allocationMeta->bytes < bytes)// we're gaining space
            /* fill the newly used space with zeroes */
            ZeroMemory(KORL_C_CAST(u8*, allocation) + allocationMeta->bytes, bytes - allocationMeta->bytes);
        else if(allocationMeta->bytes > bytes)// we're losing space
            /* fill the now unused space with a known bit pattern */
            /** @todo: this will fail in the case that our allocation's page 
             * count shrinks, as we will accidentally attempt to write in the 
             * old page - fix me pls */
            FillMemory(KORL_C_CAST(u8*, allocation) + bytes, allocationMeta->bytes - bytes, _KORL_MEMORY_INVALID_BYTE_PATTERN);
        /* update the allocation's metadata */
        allocationMeta->bytes = bytes;
        allocationMeta->file  = file;
        allocationMeta->line  = line;
    }
    /* protect the allocation's metadata page */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocationMeta, sizeof(*allocationMeta), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    /* protect the allocator pages until the time comes to actually use it */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    return allocation;
}
korl_internal Korl_Memory_Allocator korl_memory_createAllocatorLinear(u$ maxBytes)
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
    korl_memory_nullify(allocator, sizeof(*allocator));
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
