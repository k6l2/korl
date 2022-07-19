#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-windows-globalDefines.h"
#include "korl-checkCast.h"
#include "korl-log.h"
#define _KORL_MEMORY_INVALID_BYTE_PATTERN 0xFE
korl_global_const i8 _KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR[]     = "[KORL-general-allocation]";
korl_global_const i8 _KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR[]      = "[KORL-linear-allocation]";
korl_global_const i8 _KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR_FREE[] = "[KORL-linear-free-alloc]";
typedef struct _Korl_Memory_Allocator
{
    void* userData;// points to the start address of the actual specialized allocator virtual memory arena (linear, general, etc...)
    u$ maxBytes;// redundant; conveniently get this number from the allocator pool without having to call allocator-type-specific API
    Korl_Memory_AllocatorHandle handle;
    Korl_Memory_AllocatorType type;
    Korl_Memory_AllocatorFlags flags;
    wchar_t name[32];
} _Korl_Memory_Allocator;
typedef struct _Korl_Memory_Context
{
    SYSTEM_INFO systemInfo;
    DWORD mainThreadId;
    Korl_Memory_AllocatorHandle allocatorHandleReporting;// temporary memory used only for reporting memory module metrics
    KORL_MEMORY_POOL_DECLARE(_Korl_Memory_Allocator, allocators, 64);
} _Korl_Memory_Context;
/** Allocator properties: 
 * - all allocations are page-aligned
 * - all allocations are separated by an un-committed "guard page"
 * - when new allocations are created, we will start a search from the end of 
 *   the last allocation for available pages, which wraps around to the 
 *   beginning of the allocator's available pages if necessary (prioritize fast 
 *   allocations; assume that by the time we wrap around to the beginning of the 
 *   allocator's available pages, those pages will already be freed/available 
 *   again)
 * - if we end up suffering from memory fragmentation, we can always implement 
 *   a defragmentation routine and run it occasionally
 * Because of this, a good data structure to use to determine where to place new 
 * allocations would be a giant array of flags.  The status of each flag 
 * determines the availability of each page in the allocator.  Because bitfields 
 * are so compact, performing a search for the next available space will 
 * generally have very few cache misses.  Some back of the envelope 
 * calculations:
 * - page size on Windows is 4096 bytes
 * - this stores a maximum of 4096*8 = 32768 bits/flags
 * - if each flag indicates the status of a single page, this means with just a 
 *   single page of flags, we can store the status of 
 *   32768*4096 = 134217728 bytes of virtual memory
 * - assuming we must separate each allocation with a guard page, we still 
 *   maintain a maximum of 4096/2 = 2048 allocations
 * - since the entire allocator index is in a single page, and cache line size 
 *   is usually around 64 bytes, we only have to visit a maximum of 
 *   4096/64 = 64 cache lines to perform the search for an available set of 
 *   virtual memory addresses for the new allocation (and similarly, we retain 
 *   the ability to enumerate over all allocations relatively quickly, which is 
 *   a requirement of KORL allocators, mainly for the savestate feature; 
 *   enumeration of all allocations can be done easily because we know an 
 *   allocation is there if it is preceded by an unused guard page, or bit 
 *   pattern: 01*)
 * So with just a single page dedicated to allocator book-keeping, we can 
 * basically handle the vast majority of allocator cases.
 */
typedef struct _Korl_Memory_AllocatorGeneral
{
    // u$ bytes;//KORL-ISSUE-000-000-030: redundant: use QueryVirtualMemoryInformation instead
    u$ allocationPages;// this is the # which determines how many flags are actually relevant in the availablePageFlags member
    u$ availablePageFlagsSize;// just used to determine the total size of the general allocator data structure
    u$ availablePageFlags[];// see comments about this data structure for details of how this is used; the size of this array is determined by `bytes`, which should be calculated as `nextHighestDivision(nextHighestDivision(bytes, pageBytes), bitCount(u$))`
} _Korl_Memory_AllocatorGeneral;
typedef struct Korl_Memory_AllocatorGeneral_AllocationMeta
{
    Korl_Memory_AllocationMeta allocationMeta;
    u$ pagesCommitted;// Overengineering; all this can do is allow us to support reserving of arbitrary # of pages.  In reality, I probably will never implement this, and if so, the page count can be derived from calculating `nextHighestDivision(sizeof(Korl_Memory_AllocatorGeneral_AllocationMeta) + metaData.bytes, pageBytes)`
} Korl_Memory_AllocatorGeneral_AllocationMeta;
/* The main purpose of the Linear Allocator is to have an extremely light & fast 
    allocation strategy.  We want to sacrifice tons of memory for the sake of 
    speed.  Desired behavior characteristics:
    - _always_ take new memory from the end of the allocator, _unless_ the 
      allocator is emptied (lastAllocation == NULL)
    - retain the ability to iterate over all allocations
    - when an allocation is freed, it will essentially cause the allocation 
      before it to "realloc" into the space it occupies
    - the exception is the very first allocation, which has no allocations 
      "before" it in memory
    Thus effectively creating a double-linked list, where the "previous" 
    node is tracked via a pointer, and the "next" node is implicit since we 
    can assume that the allocator is fully packed, meaning there is no empty 
    space between allocations.  Ergo, we know the next node is at 
    `allocation + allocation.bytes` in memory!
    We must be willing to accept the following consequences for the sake of 
    speed/simplicity!!!:
    - the main downside to this strategy is that we set ourselves up to leaking 
      infinite memory if we are continuously allocating things and never 
      emptying the allocator!  If we continously allocate memory without ever 
      emptying the allocator, we will eventually run out of memory very quickly
    - reallocating memory alternately between multiple allocations will yield 
      extremely poor performance in both time/space, since the allocations will 
      likely need to be copied each time to the end of the allocator, and the 
      unused space will never be recycled (see previous consequence)
    */
typedef struct _Korl_Memory_AllocatorLinear
{
    /** total amount of reserved virtual address space, including this struct 
     * calculated as: korl_math_nextHighestDivision(maxBytes, pageBytes) * 
    */
    u$ bytes;//KORL-ISSUE-000-000-030: redundant: use QueryVirtualMemoryInformation instead
    void* lastAllocation;
} _Korl_Memory_AllocatorLinear;
typedef struct Korl_Memory_AllocatorLinear_AllocationMeta
{
    Korl_Memory_AllocationMeta allocationMeta;
    void* previousAllocation;
    u$ availableBytes;// calculated as: (metaBytesRequired + bytes) rounded up to nearest page size
} Korl_Memory_AllocatorLinear_AllocationMeta;
typedef struct _Korl_Memory_ReportAllocationMetaData
{
    const void* allocationAddress;
    Korl_Memory_AllocationMeta meta;
} _Korl_Memory_ReportAllocationMetaData;
typedef struct _Korl_Memory_ReportEnumerateContext
{
    //KORL-FEATURE-000-000-007: dynamic resizing arrays, similar to stb_ds
    u$ allocationMetaCapacity;
    u$ allocationMetaSize;
    _Korl_Memory_ReportAllocationMetaData* allocationMeta;
    u$ totalUsedBytes;
    const void* virtualAddressStart;
    const void* virtualAddressEnd;
    wchar_t name[32];
} _Korl_Memory_ReportEnumerateContext;
typedef struct _Korl_Memory_Report
{
    u$ allocatorCount;
    _Korl_Memory_ReportEnumerateContext allocatorData[1];
} _Korl_Memory_Report;
korl_global_variable _Korl_Memory_Context _korl_memory_context;
korl_internal bool _korl_memory_isBigEndian(void)
{
    korl_shared_const i32 I = 1;
    return KORL_C_CAST(const u8*const, &I)[0] == 0;
}
korl_internal u$ _korl_memory_packCommon(const u8* data, u$ dataBytes, u8** bufferCursor, const u8*const bufferEnd)
{
    if(*bufferCursor + dataBytes > bufferEnd)
        return 0;
    if(_korl_memory_isBigEndian())
        korl_assert(!"big-endian not supported");
    else
        korl_memory_copy(*bufferCursor, data, dataBytes);
    *bufferCursor += dataBytes;
    return dataBytes;
}
korl_internal u$ _korl_memory_unpackCommon(void* unpackedData, u$ unpackedDataBytes, const u8** bufferCursor, const u8*const bufferEnd)
{
    if(*bufferCursor + unpackedDataBytes > bufferEnd)
        return 0;
    if(_korl_memory_isBigEndian())
        korl_assert(!"big-endian not supported");
    else
        korl_memory_copy(unpackedData, *bufferCursor, unpackedDataBytes);
    *bufferCursor += unpackedDataBytes;
    return unpackedDataBytes;
}
korl_internal void korl_memory_initialize(void)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_memory_zero(context, sizeof(*context));
    GetSystemInfo(&context->systemInfo);
    context->mainThreadId = GetCurrentThreadId();
    context->allocatorHandleReporting = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_megabytes(1), L"korl-memory-reporting", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL/*let platform choose address*/);
#if KORL_DEBUG/* testing out bitwise operations */
    {
        u64 ui = (~(~0ULL << 4)) << (64 - 4);
        ui >>= 1;
        i64 si = 0x8000000000000000;
        si >>= 1;
        si >>= 1;
    }
#endif
#if KORL_DEBUG/* testing out windows memory management functionality */
    {
        u8* data = VirtualAlloc(NULL, context->systemInfo.dwPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        korl_memory_copy(data, "testing", 7);
        // data = VirtualAlloc(data, context->systemInfo.dwPageSize, MEM_RESET, PAGE_READWRITE);
        VirtualFree(data, context->systemInfo.dwPageSize, MEM_DECOMMIT);
        data = VirtualAlloc(data, context->systemInfo.dwPageSize, MEM_COMMIT, PAGE_READWRITE);
        korl_memory_copy(data + 7, "testing", 7);
        VirtualFree(data, context->systemInfo.dwPageSize, MEM_RELEASE);
    }
#endif
}
korl_internal u$ korl_memory_pageBytes(void)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    return context->systemInfo.dwPageSize;
}
//KORL-ISSUE-000-000-029: pull out platform-agnostic code
korl_internal KORL_PLATFORM_MEMORY_COMPARE(korl_memory_compare)
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
korl_internal KORL_PLATFORM_ARRAY_U8_COMPARE(korl_memory_arrayU8Compare)
{
    if(sizeA < sizeB)
        return -1;
    else if(sizeA > sizeB)
        return 1;
    else
        for(u$ i = 0; i < sizeA; i++)
            if(dataA[i] < dataB[i])
                return -1;
            else if(dataA[i] > dataB[i])
                return 1;
    return 0;
}
korl_internal KORL_PLATFORM_ARRAY_U16_COMPARE(korl_memory_arrayU16Compare)
{
    if(sizeA < sizeB)
        return -1;
    else if(sizeA > sizeB)
        return 1;
    else
        for(u$ i = 0; i < sizeA; i++)
            if(dataA[i] < dataB[i])
                return -1;
            else if(dataA[i] > dataB[i])
                return 1;
    return 0;
}
korl_internal KORL_PLATFORM_STRING_COMPARE(korl_memory_stringCompare)
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
korl_internal KORL_PLATFORM_STRING_COMPARE_UTF8(korl_memory_stringCompareUtf8)
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
korl_internal KORL_PLATFORM_STRING_SIZE(korl_memory_stringSize)
{
    if(!s)
        return 0;
    /*  [ t][ e][ s][ t][\0]
        [ 0][ 1][ 2][ 3][ 4]
        [sB]            [s ] */
    const wchar_t* sBegin = s;
    for(; *s; ++s) {}
    return s - sBegin;
}
korl_internal KORL_PLATFORM_STRING_SIZE_UTF8(korl_memory_stringSizeUtf8)
{
    if(!s)
        return 0;
    /*  [ t][ e][ s][ t][\0]
        [ 0][ 1][ 2][ 3][ 4]
        [sB]            [s ] */
    const char* sBegin = s;
    for(; *s; ++s) {}
    return s - sBegin;
}
korl_internal KORL_PLATFORM_STRING_COPY(korl_memory_stringCopy)
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
#if 0// still not quite sure if this is needed for anything really...
korl_internal KORL_PLATFORM_MEMORY_FILL(korl_memory_fill)
{
    FillMemory(memory, bytes, pattern);
}
#endif
korl_internal KORL_PLATFORM_MEMORY_ZERO(korl_memory_zero)
{
    SecureZeroMemory(memory, bytes);
}
korl_internal KORL_PLATFORM_MEMORY_COPY(korl_memory_copy)
{
    CopyMemory(destination, source, bytes);
}
korl_internal KORL_PLATFORM_MEMORY_MOVE(korl_memory_move)
{
    MoveMemory(destination, source, bytes);
}
//KORL-ISSUE-000-000-029: pull out platform-agnostic code
korl_internal bool korl_memory_isNull(const void* p, size_t bytes)
{
    const void*const pEnd = KORL_C_CAST(const u8*, p) + bytes;
    for(; p != pEnd; p = KORL_C_CAST(const u8*, p) + 1)
        if(*KORL_C_CAST(const u8*, p))
            return false;
    return true;
}
korl_internal wchar_t* korl_memory_stringFormat(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    wchar_t*const result = korl_memory_stringFormatVaList(allocatorHandle, format, args);
    va_end(args);
    return result;
}
korl_internal KORL_PLATFORM_STRING_FORMAT_VALIST(korl_memory_stringFormatVaList)
{
    const int bufferSize = _vscwprintf(format, vaList) + 1/*for the null terminator*/;
    korl_assert(bufferSize > 0);
    wchar_t*const result = (wchar_t*)korl_allocate(allocatorHandle, bufferSize * sizeof(*result));
    korl_assert(result);
    const int charactersWritten = vswprintf_s(result, bufferSize, format, vaList);
    korl_assert(charactersWritten == bufferSize - 1);
    return result;
}
korl_internal KORL_PLATFORM_STRING_FORMAT_VALIST_UTF8(korl_memory_stringFormatVaListUtf8)
{
    const int bufferSize = _vscprintf(format, vaList) + 1/*for the null terminator*/;
    korl_assert(bufferSize > 0);
    char*const result = (char*)korl_allocate(allocatorHandle, bufferSize * sizeof(*result));
    korl_assert(result);
    const int charactersWritten = vsprintf_s(result, bufferSize, format, vaList);
    korl_assert(charactersWritten == bufferSize - 1);
    return result;
}
korl_internal KORL_PLATFORM_STRING_FORMAT_BUFFER(korl_memory_stringFormatBuffer)
{
    va_list args;
    va_start(args, format);
    i$ result = korl_memory_stringFormatBufferVaList(buffer, bufferBytes, format, args);
    va_end(args);
    return result;
}
korl_internal i$ korl_memory_stringFormatBufferVaList(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, va_list vaList)
{
    const int bufferSize = _vscwprintf(format, vaList);// excludes null terminator
    korl_assert(bufferSize >= 0);
    if(korl_checkCast_i$_to_u$(bufferSize + 1/*for null terminator*/) > bufferBytes / sizeof(*buffer))
        return -(bufferSize + 1/*for null terminator*/);
    const int charactersWritten = vswprintf_s(buffer, bufferBytes/sizeof(*buffer), format, vaList);// excludes null terminator
    korl_assert(charactersWritten == bufferSize);
    return charactersWritten + 1/*for null terminator*/;
}
korl_internal void _korl_memory_allocator_linear_allocatorPageGuard(_Korl_Memory_AllocatorLinear* allocator)
{
#if KORL_DEBUG
    DWORD oldProtect;
    if(!VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE | PAGE_GUARD, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == PAGE_READWRITE);
#endif
}
korl_internal void _korl_memory_allocator_linear_allocatorPageUnguard(_Korl_Memory_AllocatorLinear* allocator)
{
#if KORL_DEBUG
    DWORD oldProtect;
    if(!VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
#endif
}

/** iterate over allocations recursively on the stack so that we can iterate in 
 * monotonically-increasing memory addresses */
korl_internal bool _korl_memory_allocator_linear_enumerateAllocationsRecurse(_Korl_Memory_AllocatorLinear*const allocator, void* allocation, fnSig_korl_memory_allocator_enumerateAllocationsCallback* callback, void* callbackUserData)
{
    /* halt recursion when we reach a NULL allocation */
    if(!allocation)
        return false;
    /* sanity checks */
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    /* ensure that this address is actually within the allocator's range */
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    korl_assert(KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
             && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation meta address is divisible by the page size, 
        because this is currently a constraint of this allocator */
    const u$ metaBytesRequired = sizeof(Korl_Memory_AllocatorLinear_AllocationMeta) 
                               + sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/;
    korl_assert((KORL_C_CAST(u$, allocation) - metaBytesRequired) % pageBytes == 0);
    /* ensure that the memory just before the allocation is the constant meta 
        separator string */
    korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, allocation) - (sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/), 
                                         _KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR, 
                                         sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
    /* recurse into the previous allocation */
    Korl_Memory_AllocatorLinear_AllocationMeta*const allocationMeta = 
        KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    if(!_korl_memory_allocator_linear_enumerateAllocationsRecurse(allocator, allocationMeta->previousAllocation, callback, callbackUserData))
        return false;
    /* perform the enumeration callback after the stack is drained */
    return callback(callbackUserData, allocation, &(allocationMeta->allocationMeta));
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(_korl_memory_allocator_linear_enumerateAllocations)
{
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocatorUserData);
    /* sanity checks */
    _korl_memory_allocator_linear_allocatorPageUnguard(allocator);
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    korl_assert(0 == KORL_C_CAST(u$, allocator) % _korl_memory_context.systemInfo.dwAllocationGranularity);
    korl_assert(0 == allocator->bytes           % pageBytes);
    /**/
    if(out_allocatorVirtualAddressEnd)
        *out_allocatorVirtualAddressEnd = KORL_C_CAST(u8*, allocator) + allocator->bytes;
    _korl_memory_allocator_linear_enumerateAllocationsRecurse(allocator, allocator->lastAllocation, callback, callbackUserData);
    _korl_memory_allocator_linear_allocatorPageGuard(allocator);
}
korl_internal void* _korl_memory_allocator_linear_allocate(_Korl_Memory_AllocatorLinear*const allocator, u$ bytes, const wchar_t* file, int line, void* requestedAddress)
{
    void* allocationAddress = NULL;
    _korl_memory_allocator_linear_allocatorPageUnguard(allocator);
    const u8*const allocatorEnd = KORL_C_CAST(u8*, allocator) + allocator->bytes;
    /* sanity check that the allocator's address range is page-aligned */
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    korl_assert(0 == KORL_C_CAST(u$, allocator) % _korl_memory_context.systemInfo.dwAllocationGranularity);
    korl_assert(0 == allocator->bytes           % pageBytes);
    /* If the user has requested a custom address for the allocation, we can 
        choose this address instead of the allocator's next available address.  
        In addition, we can conditionally check to make sure the requested 
        address is valid based on any number of metrics (within the address 
        range of the allocator, not going to overlap another allocation, etc...) */
    const u$ metaBytesRequired = sizeof(Korl_Memory_AllocatorLinear_AllocationMeta) 
                               + sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*don't include the null terminator*/;
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    const u$ allocatorBytes = allocatorPages * pageBytes;
    Korl_Memory_AllocatorLinear_AllocationMeta* metaAddress = 
        KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, 
                    KORL_C_CAST(u8*, allocator) + allocatorBytes + pageBytes/*skip guard page*/);
    if(allocator->lastAllocation)
    {
        const Korl_Memory_AllocatorLinear_AllocationMeta*const lastAllocationMeta = 
            KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, KORL_C_CAST(u8*, allocator->lastAllocation) - metaBytesRequired);
        korl_assert(lastAllocationMeta->availableBytes % pageBytes == 0);
        metaAddress = KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, 
                                  KORL_C_CAST(u8*, lastAllocationMeta) + lastAllocationMeta->availableBytes + pageBytes/*skip guard page*/);
    }
    const u$ totalAllocationPages = korl_math_nextHighestDivision(metaBytesRequired + bytes, pageBytes);
    const u$ totalAllocationBytes = totalAllocationPages * pageBytes;
    if(requestedAddress)
    {
        /* _huge_ simplification here: let's try just requiring the caller to 
            inject allocations at specific addresses in a strictly increasing 
            order; guaranteeing that each allocation is contiguous along the way */
        metaAddress = KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, KORL_C_CAST(u8*, requestedAddress) - metaBytesRequired);
        Korl_Memory_AllocatorLinear_AllocationMeta*const lastAllocationMeta = allocator->lastAllocation 
            ? KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, KORL_C_CAST(u8*, allocator->lastAllocation) - metaBytesRequired)
            : NULL;
        /* this address must be _above_ the allocator pages */
        korl_assert(KORL_C_CAST(u8*, allocator) + allocatorBytes + pageBytes/*skip guard page*/ <= KORL_C_CAST(u8*, metaAddress));
        /* this address must be _after_ the last allocation */
        if(lastAllocationMeta)
            korl_assert(KORL_C_CAST(u8*, lastAllocationMeta) + lastAllocationMeta->availableBytes + pageBytes/*guard page*/ <= KORL_C_CAST(u8*, metaAddress));
    }
    /* check to see that this allocation can fit in the range 
        [nextAllocationAddress, lastReservedAddress] */
    if(KORL_C_CAST(u8*, metaAddress) + totalAllocationBytes > allocatorEnd)
    {
        korl_log(WARNING, "linear allocator out of memory");//KORL-ISSUE-000-000-049: memory: logging an error when allocation fails in log module results in poor error logging
        goto guardAllocator_return_allocationAddress;
    }
    /* commit the pages of this allocation */
    {
        /* MSDN page for VirtualAlloc says:
            "It can commit a page that is already committed. This means you can 
            commit a range of pages, regardless of whether they have already 
            been committed, and the function will not fail." */
        LPVOID resultVirtualAlloc = 
            VirtualAlloc(metaAddress, totalAllocationBytes, MEM_COMMIT, PAGE_READWRITE);
        if(resultVirtualAlloc == NULL)
            korl_logLastError("VirtualAlloc failed!");
        korl_assert(resultVirtualAlloc == metaAddress);
    }
    /* ensure that the space within the allocation's pages that lie outside the 
        range of `bytes` is filled with some known invalid byte pattern */
    if(totalAllocationBytes - (metaBytesRequired + bytes))
        FillMemory(KORL_C_CAST(u8*, metaAddress) + (metaBytesRequired + bytes), totalAllocationBytes - (metaBytesRequired + bytes), _KORL_MEMORY_INVALID_BYTE_PATTERN);
    /* ensure that the allocation address is filled with zeros */
    allocationAddress = KORL_C_CAST(u8*, metaAddress) + metaBytesRequired;
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
#if 0/* We don't have to do this, since re-commited pages are guaranteed to be 
        filled with 0.  This is, of course, assuming that our strategy is going 
        to re-commit new pages every time guaranteed. */
    FillMemory(allocationAddress, bytes, 0);
#endif
    /* initialize allocation's metadata */
    metaAddress->allocationMeta.file  = file;
    metaAddress->allocationMeta.line  = line;
    metaAddress->allocationMeta.bytes = bytes;
    metaAddress->previousAllocation   = allocator->lastAllocation;
    metaAddress->availableBytes       = totalAllocationBytes;
    korl_memory_copy(metaAddress + 1, _KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR, sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*don't include the null terminator*/);
    /* update allocator's metrics */
    allocator->lastAllocation = allocationAddress;
    /**/
    guardAllocator_return_allocationAddress:
    _korl_memory_allocator_linear_allocatorPageGuard(allocator);
    return allocationAddress;
}
korl_internal void _korl_memory_allocator_linear_free(_Korl_Memory_AllocatorLinear*const allocator, void* allocation, const wchar_t* file, int line)
{
    /* sanity checks */
    _korl_memory_allocator_linear_allocatorPageUnguard(allocator);
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    korl_assert(0 == KORL_C_CAST(u$, allocator) % _korl_memory_context.systemInfo.dwAllocationGranularity);
    korl_assert(0 == allocator->bytes           % pageBytes);
    /* ensure that this address is actually within the allocator's range */
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    korl_assert(KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
             && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation meta address is divisible by the page size, 
        because this is currently a constraint of this allocator */
    const u$ metaBytesRequired = sizeof(Korl_Memory_AllocatorLinear_AllocationMeta) 
                               + sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/;
    korl_assert((KORL_C_CAST(u$, allocation) - metaBytesRequired) % pageBytes == 0);
    /* ensure that the memory just before the allocation is the constant meta 
        separator string */
    korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, allocation) - (sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/), 
                                         _KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR, 
                                         sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
    /* at this point, it's _fairly_ safe to assume that allocation is a linear 
        allocation */
    Korl_Memory_AllocatorLinear_AllocationMeta*const allocationMeta = 
        KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    /* If we aren't the last allocation, then we're either:
        - in between two allocations
            - set the next allocation's previous pointer to our previous
            - merge our memory region with previous allocation
            - safety memory wipe this allocation
        - the first allocation, with at least one allocation after us
            - set the next allocation's previous pointer to our previous (should be set to NULL)
            - safety memory wipe this allocation */
    if(allocator->lastAllocation != allocation)
    {
        Korl_Memory_AllocatorLinear_AllocationMeta*const nextAllocationMeta = 
            KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, KORL_C_CAST(u8*, allocationMeta) + allocationMeta->availableBytes + pageBytes/*guard page*/);
        void*const nextAllocation = KORL_C_CAST(u8*, nextAllocationMeta) + metaBytesRequired;
        /* sanity checks for next allocation */
        /* ensure that this address is actually within the allocator's range */
        korl_assert(KORL_C_CAST(u8*, nextAllocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
                 && KORL_C_CAST(u8*, allocation)     <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
        /* ensure that the allocation meta address is divisible by the page size, 
            because this is currently a constraint of this allocator */
        korl_assert((KORL_C_CAST(u$, nextAllocation) - metaBytesRequired) % pageBytes == 0);
        /* ensure that the memory just before the allocation is the constant meta 
            separator string */
        korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, nextAllocation) - (sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/), 
                                             _KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR, 
                                             sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
        /* everything looks good; we should now be able to update the next allocation's meta data */
        nextAllocationMeta->previousAllocation = allocationMeta->previousAllocation;
        /* increase the previous allocation's available bytes by the appropriate 
            amount, if there is a previous allocation */
        if(allocationMeta->previousAllocation)
        {
            /* sanity checks for previous allocation */
            /* ensure that this address is actually within the allocator's range */
            korl_assert(KORL_C_CAST(u8*, allocationMeta->previousAllocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
                     && KORL_C_CAST(u8*, allocation)                         <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
            /* ensure that the allocation meta address is divisible by the page size, 
                because this is currently a constraint of this allocator */
            korl_assert((KORL_C_CAST(u$, allocationMeta->previousAllocation) - metaBytesRequired) % pageBytes == 0);
            /* ensure that the memory just before the allocation is the constant 
                meta separator string */
            korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, allocationMeta->previousAllocation) - (sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/), 
                                                 _KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR, 
                                                 sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
            /* we're pretty sure previous allocation is valid */
            Korl_Memory_AllocatorLinear_AllocationMeta*const previousAllocationMeta = 
                KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, KORL_C_CAST(u8*, allocationMeta->previousAllocation) - metaBytesRequired);
            /* ensure previous allocation is indeed adjacent to us, since that is 
                required by this allocation strategy */
            korl_assert(KORL_C_CAST(u8*, previousAllocationMeta) + previousAllocationMeta->availableBytes + pageBytes/*guard page*/ == KORL_C_CAST(u8*, allocationMeta));
            /* commit the guard page, since the previous allocation will absorb it & 
                potentially use this space in the future */
            if(!VirtualAlloc(KORL_C_CAST(u8*, previousAllocationMeta) + previousAllocationMeta->availableBytes, pageBytes, MEM_COMMIT, PAGE_READWRITE))
                korl_logLastError("VirtualAlloc failed!");
            /* increase the previous allocation's metrics to be able to use this 
                entire region of memory in the future (guardPage + allocation) */
            previousAllocationMeta->availableBytes += pageBytes/*guard page*/ + allocationMeta->availableBytes;
            /* according to the current reallocate algorithm, the memory that this 
                allocation is occupying will be zeroed out if it becomes utilized by 
                previousAllocation */
        }//if there is a previous allocation
        /* to make it easier on ourselves in the debugger, let's at least smash 
            the allocation's meta data */
        allocationMeta->allocationMeta.file = file;
        allocationMeta->allocationMeta.line = line;
        korl_memory_copy(allocationMeta + 1, _KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR_FREE, sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR_FREE) - 1/*don't include the null terminator*/);
    }// if we're not the last allocation
    /* this is the last allocation; the entire allocation can just be wiped */
    else
    {
        allocator->lastAllocation = allocationMeta->previousAllocation;
        //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
        if(!VirtualFree(allocationMeta, allocationMeta->availableBytes, MEM_DECOMMIT))
            korl_logLastError("VirtualFree failed!");
    }
    /* if the allocator has no more allocations, we should empty it! */
    if(!allocator->lastAllocation)
    {
        //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
        const u$ allocatorBytes          = allocatorPages * pageBytes;
        const u$ allocationBytes         = allocator->bytes - allocatorBytes;
        void*const allocationRegionStart = KORL_C_CAST(u8*, allocator) + allocatorBytes;
        if(!VirtualFree(allocationRegionStart, allocationBytes, MEM_DECOMMIT))
            korl_logLastError("VirtualFree failed!");
    }
    _korl_memory_allocator_linear_allocatorPageGuard(allocator);
}
korl_internal void* _korl_memory_allocator_linear_reallocate(_Korl_Memory_AllocatorLinear*const allocator, void* allocation, u$ bytes, const wchar_t* file, int line)
{
    /* sanity checks */
    _korl_memory_allocator_linear_allocatorPageUnguard(allocator);
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    korl_assert(0 == KORL_C_CAST(u$, allocator) % _korl_memory_context.systemInfo.dwAllocationGranularity);
    korl_assert(0 == allocator->bytes           % pageBytes);
    /* ensure that this address is actually within the allocator's range */
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    korl_assert(KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
             && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation meta address is divisible by the page size, 
        because this is currently a constraint of this allocator */
    const u$ metaBytesRequired = sizeof(Korl_Memory_AllocatorLinear_AllocationMeta) 
                               + sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/;
    korl_assert((KORL_C_CAST(u$, allocation) - metaBytesRequired) % pageBytes == 0);
    /* ensure that the memory just before the allocation is the constant meta 
        separator string */
    korl_assert(0 == korl_memory_compare(KORL_C_CAST(u8*, allocation) - (sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/), 
                                         _KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR, 
                                         sizeof(_KORL_ALLOCATOR_LINEAR_ALLOCATION_META_SEPARATOR) - 1/*exclude null terminator*/));
    /* at this point, it's _fairly_ safe to assume that allocation is a linear allocation */
    Korl_Memory_AllocatorLinear_AllocationMeta*const allocationMeta = 
        KORL_C_CAST(Korl_Memory_AllocatorLinear_AllocationMeta*, KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    /* if the requested bytes is equal to the current bytes, there is no work to do */
    if(allocationMeta->allocationMeta.bytes == bytes)
        goto guard_allocator_return_allocation;
    /* if the allocation has enough total available space for the caller's 
        requirements, we can just change the size of the allocation in-place & 
        be done with it */
    if(allocationMeta->availableBytes - metaBytesRequired >= bytes)
    {
        /* if we're growing, we need to make sure the new memory is zeroed */
        if(allocationMeta->allocationMeta.bytes < bytes)
            korl_memory_zero(KORL_C_CAST(u8*, allocation) + allocationMeta->allocationMeta.bytes, bytes - allocationMeta->allocationMeta.bytes);
        /* otherwise, we must be shrinking; zero out the memory we're losing 
            just for safety */
        else
        {
            korl_assert(allocationMeta->allocationMeta.bytes > bytes);// sanity check; we should have already handled this case above
            korl_memory_zero(KORL_C_CAST(u8*, allocation) + bytes, allocationMeta->allocationMeta.bytes - bytes);
        }
        allocationMeta->allocationMeta.bytes = bytes;
    }
    /* otherwise, if we are the last allocation we can just commit more pages, 
        assuming we will remain in bounds of the allocator's address pool */
    else if(allocation == allocator->lastAllocation)
    {
        const u$ totalAllocationPages = korl_math_nextHighestDivision(metaBytesRequired + bytes, pageBytes);
        const u$ totalAllocationBytes = totalAllocationPages * pageBytes;
        //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
        Korl_Memory_AllocatorLinear_AllocationMeta*const newMeta = VirtualAlloc(allocationMeta, totalAllocationBytes, MEM_COMMIT, PAGE_READWRITE);
        if(newMeta == NULL)
            korl_logLastError("VirtualAlloc failed!");
        newMeta->availableBytes       = totalAllocationBytes;
        newMeta->allocationMeta.bytes = bytes;
        newMeta->allocationMeta.file  = file;
        newMeta->allocationMeta.line  = line;
        allocation = KORL_C_CAST(u8*, newMeta) + metaBytesRequired;
    }
    /* otherwise, we must allocate->copy->free */
    else
    {
        _korl_memory_allocator_linear_allocatorPageGuard(allocator);
        void*const newAllocation = _korl_memory_allocator_linear_allocate(allocator, bytes, file, line, NULL/*auto-select address*/);
        if(newAllocation)
        {
            korl_memory_copy(newAllocation, allocation, allocationMeta->allocationMeta.bytes);
            _korl_memory_allocator_linear_free(allocator, allocation, file, line);
        }
        else
            korl_log(WARNING, "failed to create a new reallocation");
        return newAllocation;
    }
    guard_allocator_return_allocation:
    _korl_memory_allocator_linear_allocatorPageGuard(allocator);
    return allocation;
}
korl_internal void _korl_memory_allocator_linear_empty(_Korl_Memory_AllocatorLinear*const allocator)
{
    /* sanity checks */
    _korl_memory_allocator_linear_allocatorPageUnguard(allocator);
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    korl_assert(0 == KORL_C_CAST(u$, allocator) % _korl_memory_context.systemInfo.dwAllocationGranularity);
    korl_assert(0 == allocator->bytes           % pageBytes);
    /* eliminate all allocations */
    /* MSDN page for VirtualFree says: 
        "The function does not fail if you attempt to decommit an uncommitted 
        page. This means that you can decommit a range of pages without first 
        determining the current commitment state."  It would be really easy to 
        just do this and re-commit pages each time we allocate, this is 
        apparently expensive, so we might not want to do this. */
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
    const u$ allocatorPages          = korl_math_nextHighestDivision(sizeof(*allocator), pageBytes);
    const u$ allocatorBytes          = allocatorPages * pageBytes;
    const u$ allocationBytes         = allocator->bytes - allocatorBytes;
    void*const allocationRegionStart = KORL_C_CAST(u8*, allocator) + allocatorBytes;
    if(!VirtualFree(allocationRegionStart, allocationBytes, MEM_DECOMMIT))
        korl_logLastError("VirtualFree failed!");
    /* update the allocator metrics */
    allocator->lastAllocation = NULL;
    _korl_memory_allocator_linear_allocatorPageGuard(allocator);
}
korl_internal _Korl_Memory_AllocatorLinear* _korl_memory_allocator_linear_create(u$ maxBytes, void* address)
{
    _Korl_Memory_AllocatorLinear* result = NULL;
    korl_assert(maxBytes > 0);
    korl_assert(maxBytes >= sizeof(*result));
    const u$ pagesRequired      = korl_math_nextHighestDivision(maxBytes, korl_memory_pageBytes());
    const u$ totalBytesRequired = pagesRequired * korl_memory_pageBytes();
    /* reserve all these pages */
    LPVOID resultVirtualAlloc = VirtualAlloc(address, totalBytesRequired, MEM_RESERVE, PAGE_NOACCESS);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    result = resultVirtualAlloc;
    /* commit the first pages for the allocator itself */
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*result), korl_memory_pageBytes());
    const u$ allocatorBytes = allocatorPages * korl_memory_pageBytes();
    resultVirtualAlloc = VirtualAlloc(result, allocatorBytes, MEM_COMMIT, PAGE_READWRITE);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* initialize the memory of the allocator userData */
    korl_shared_const i8 ALLOCATOR_PAGE_MEMORY_PATTERN[] = "KORL-LinearAllocator-Page|";
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, resultVirtualAlloc);
    // fill the allocator page with a readable pattern for easier debugging //
    for(u$ currFillByte = 0; currFillByte < allocatorBytes; )
    {
        const u$ remainingBytes = allocatorBytes - currFillByte;
        const u$ availableFillBytes = KORL_MATH_MIN(remainingBytes, sizeof(ALLOCATOR_PAGE_MEMORY_PATTERN) - 1/*do not copy NULL-terminator*/);
        korl_memory_copy(KORL_C_CAST(u8*, allocator) + currFillByte, ALLOCATOR_PAGE_MEMORY_PATTERN, availableFillBytes);
        currFillByte += availableFillBytes;
    }
    korl_memory_zero(allocator, sizeof(*allocator));
    allocator->bytes = totalBytesRequired;
    /* guard the allocator page */
    _korl_memory_allocator_linear_allocatorPageGuard(allocator);
    return result;
}
korl_internal void _korl_memory_allocator_linear_destroy(_Korl_Memory_AllocatorLinear*const allocator)
{
    if(!VirtualFree(allocator, 0/*release everything*/, MEM_RELEASE))
        korl_logLastError("VirtualFree failed!");
}
korl_internal void _korl_memory_allocator_general_allocatorPagesGuard(_Korl_Memory_AllocatorGeneral* allocator)
{
#if KORL_DEBUG
    const u$ allocatorBytes = sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags));
    DWORD oldProtect;
    if(!VirtualProtect(allocator, allocatorBytes, PAGE_READWRITE | PAGE_GUARD, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == PAGE_READWRITE);
#endif
}
korl_internal void _korl_memory_allocator_general_allocatorPagesUnguard(_Korl_Memory_AllocatorGeneral* allocator)
{
#if KORL_DEBUG
    DWORD oldProtect;
    /* unguard the first page to find the total # of allocator pages */
    if(!VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
    /* now we can unguard the rest, if necessary */
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    korl_assert(sizeof(*allocator) < pageBytes);// probably not necessary at all due to OS constraints of w/e, but just sanity check to make sure the allocator struct fits in the first page!
    const u$ allocatorBytes = sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags));
    if(allocatorBytes > pageBytes)
    {
        if(!VirtualProtect(KORL_C_CAST(u8*, allocator) + pageBytes, allocatorBytes - pageBytes, PAGE_READWRITE, &oldProtect))
            korl_logLastError("VirtualProtect failed!");
        korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
    }
#endif
}
/** \return \c pageCount if there was no occupied pages in the range 
 * \c [pageRangeStartIndex,pageRangeStartIndex+pageCount) */
korl_internal u$ _korl_memory_allocator_general_occupiedPageOffset(_Korl_Memory_AllocatorGeneral* allocator, u$ pageRangeStartIndex, u$ pageCount, bool highest)
{
    korl_assert(pageCount > 0);
    ///@TODO: handle the case where the user is attempting to iterate past the allocator's total allocation pages
    /* iterate over the page count and test each flag, appropriately iterating 
        the flag register & index along the way */
    const u$ bitsPerFlagRegister = 8*sizeof(*(allocator->availablePageFlags));
    u$ occupiedPageOffset = pageCount;
    ///@TODO: iterate over page flag registers and use bitscan instead of individual page flag indices
    for(u$ p = pageRangeStartIndex; p < pageRangeStartIndex + pageCount; p++)
    {
        const u$ pageFlagRegister = p / bitsPerFlagRegister;// the array index into allocator->availablePageFlags
        const u$ flagIndex        = p % bitsPerFlagRegister;// the bit index into this specific element of allocator->availablePageFlags
        if(allocator->availablePageFlags[pageFlagRegister] & (1LLU<<(bitsPerFlagRegister - 1 - flagIndex)))
        {
            occupiedPageOffset = p - pageRangeStartIndex;
            if(!highest)
                break;
        }
    }
    return occupiedPageOffset;
}
///@TODO: make sure all the code calling this function is passing properly clamped page ranges for the allocator?  or perhaps we pass signed start indices & do a range check here (less desireable, since I'm worried about incorrectly interpreted return values)?
korl_internal u$ _korl_memory_allocator_general_highestOccupiedPageOffset(_Korl_Memory_AllocatorGeneral* allocator, u$ pageRangeStartIndex, u$ pageCount)
{
    return _korl_memory_allocator_general_occupiedPageOffset(allocator, pageRangeStartIndex, pageCount, true/*we want the _highest_ occupied page offset*/);
}
// ///@TODO: make sure all the code calling this function is passing properly clamped page ranges for the allocator?  or perhaps we pass signed start indices & do a range check here (less desireable, since I'm worried about incorrectly interpreted return values)?
// korl_internal u$ _korl_memory_allocator_general_lowestOccupiedPageOffset(_Korl_Memory_AllocatorGeneral* allocator, u$ pageRangeStartIndex, u$ pageCount)
// {
//     return _korl_memory_allocator_general_occupiedPageOffset(allocator, pageRangeStartIndex, pageCount, false/*we want the _lowest_ occupied page offset*/);
// }
korl_internal void _korl_memory_allocator_general_setPageFlags(_Korl_Memory_AllocatorGeneral* allocator, u$ pageRangeStartIndex, u$ pageCount, bool pageFlagState)
{
    /* find the page flag register index & the flag index within the register */
    const u$ bitsPerFlagRegister = 8*sizeof(*(allocator->availablePageFlags));
    for(u$ p = pageRangeStartIndex; p < pageRangeStartIndex + pageCount; p++)
    {
        const u$ pageFlagRegister = p / bitsPerFlagRegister;// the array index into allocator->availablePageFlags
        const u$ flagIndex        = p % bitsPerFlagRegister;// the bit index into this specific element of allocator->availablePageFlags
        if(pageFlagState)
            allocator->availablePageFlags[pageFlagRegister] |=   1LLU<<(bitsPerFlagRegister - 1 - flagIndex);
        else
            allocator->availablePageFlags[pageFlagRegister] &= ~(1LLU<<(bitsPerFlagRegister - 1 - flagIndex));
    }
}
korl_internal u$ _korl_memory_allocator_general_occupyAvailablePages(_Korl_Memory_AllocatorGeneral* allocator, u$ pageCount)
{
#if 1
    u$ allocationPageIndex = allocator->allocationPages;
    korl_shared_const u8 SURROUNDING_GUARD_PAGE_COUNT = 1;
    const u8 bitsPerRegister        = /*bitsPerRegister*/(8*sizeof(*(allocator->availablePageFlags)));
    const u$ fullRegistersRequired  = (pageCount + 2*SURROUNDING_GUARD_PAGE_COUNT/*leave room for one guard pages on either side of the allocation*/) / bitsPerRegister;
    const u$ remainderFlagsRequired = (pageCount + 2*SURROUNDING_GUARD_PAGE_COUNT/*leave room for one guard pages on either side of the allocation*/) % bitsPerRegister;
    const u$ remainderFlagsMask     = (~(~0ULL << remainderFlagsRequired)) << (bitsPerRegister - remainderFlagsRequired);// the remainder mask aligned to the most significant bit
    u$ consecutiveEmptyRegisters = 0;
    for(u$ r = 0; r < allocator->allocationPages; r++)
    {
        ///@TODO: this will be (on average) faster if we offset & mod `r` by an expected next available page index, but let's just get this working first...
        if(0 != allocator->availablePageFlags[r])
            consecutiveEmptyRegisters = 0;
        else
            consecutiveEmptyRegisters++;
        if(fullRegistersRequired > 0)
        {
            if(consecutiveEmptyRegisters < fullRegistersRequired)
                continue;
            u$ fullRegistersToBeTaken      = fullRegistersRequired;
            u$ fullRegistersToBeTakenStart = r + 1 - fullRegistersRequired;
            /* we need to low-to-high-bitscan the preceding register (if it exists), 
                and high-to-low-bitscan the next register (if it exits) 
                in order to see if we can occupy this region */
            u32 precedingBits  = 0;
            u32 proceedingBits = 0;
            if(fullRegistersToBeTakenStart > 0)// there _is_ a preceding register to the consecutive empty registers
            {
                DWORD bitScanIndex = bitsPerRegister;
                if(BitScanForward64(&bitScanIndex, allocator->availablePageFlags[r - fullRegistersToBeTaken]))
                    precedingBits = bitScanIndex/*bit indices start at 0, and we want the quantity of zeroes (the bitScanIndex is the index of the first 1!)*/;
                else
                    precedingBits = bitsPerRegister;// we can occupy the entire preceding register
            }
            if(r < allocator->allocationPages - 1)// there _is_ a next register after the consecutive empty registers
            {
                DWORD bitScanIndex = bitsPerRegister;
                if(BitScanReverse64(&bitScanIndex, allocator->availablePageFlags[r + 1]))
                    proceedingBits = bitsPerRegister - 1 - bitScanIndex;// total # of leading 0s, as bitScanIndex is the offset from the least-significant bit of the most-significant 1
                else
                    proceedingBits = bitsPerRegister;// we can occupy the entire proceeding register
            }
            /* how many preceding registers can we occupy? (if any) */
            // korl_assert(!"@TODO");//uhh... seems like we already have this info; delete this?
            /* how many proceeding registers can we occupy? (if any) */
            // korl_assert(!"@TODO");//uhh... seems like we already have this info; delete this?
            /* check if we have enough preceding/proceeding flags; if not, we need to keep searching */
            const u$ extendedEmptyPageFlags = precedingBits + proceedingBits;
            if(extendedEmptyPageFlags < remainderFlagsRequired)
                continue;
            /* at this point, we _know_ that we are going to be able to occupy 
                open space in/around register r! */
            /* remove guard flags from the preceding page; if there are not 
                enough, the first full register becomes the preceding page */
            if(precedingBits >= SURROUNDING_GUARD_PAGE_COUNT)
                precedingBits -= SURROUNDING_GUARD_PAGE_COUNT;
            else
            {
                fullRegistersToBeTaken--;
                fullRegistersToBeTakenStart++;
                precedingBits = bitsPerRegister - SURROUNDING_GUARD_PAGE_COUNT;
            }
            /* remove guard flags from the proceeding page; if there are not 
                enough, the last full register becomes the proceeding page */
            if(proceedingBits >= SURROUNDING_GUARD_PAGE_COUNT)
                proceedingBits -= SURROUNDING_GUARD_PAGE_COUNT;
            else
            {
                fullRegistersToBeTaken--;
                proceedingBits = bitsPerRegister - SURROUNDING_GUARD_PAGE_COUNT;
            }
            /* our new occupy index is now the beginning of the bits that 
                precede the "full register range" */
            const u$ occupyIndex = bitsPerRegister*fullRegistersToBeTakenStart - precedingBits;
            u$ remainingFlagsToOccupy = pageCount;
            if(precedingBits)
            {
                korl_assert(fullRegistersToBeTakenStart > 0);
                korl_assert(0 == (allocator->availablePageFlags[fullRegistersToBeTakenStart - 1] & ~((~0ULL) << precedingBits)));
                allocator->availablePageFlags[fullRegistersToBeTakenStart - 1] |= ~((~0ULL) << precedingBits);
                remainingFlagsToOccupy -= precedingBits;
            }
            /* occupy all the full registers required, then any leftover flags */
            u$ rr = fullRegistersToBeTakenStart;
            while(remainingFlagsToOccupy)
            {
                const u$ flags = KORL_MATH_MIN(remainingFlagsToOccupy, bitsPerRegister);
                const u$ mask  = (~(~0ULL << flags)) << (bitsPerRegister - flags);
                remainingFlagsToOccupy -= flags;
                korl_assert(rr < allocator->allocationPages);
                korl_assert(0 == (allocator->availablePageFlags[rr] & mask));
                allocator->availablePageFlags[rr++] |= mask;
            }
            /* and finally, return the page index of the first occupied page */
            allocationPageIndex = occupyIndex;
            goto returnAllocationPageIndex;
        }
        else///@TODO: only do this if we know the register isn't full (timings required for this)// if(allocator->availablePageFlags[r] < ~((~OLL) << bitsPerRegister))
        {
            /* iterate over each offset in the flag register */
            for(u8 i = 0; i < bitsPerRegister; i++)
            {
                /* if the entire mask fits in the register, we just need to do a bitwise & */
                if(i + remainderFlagsRequired <= bitsPerRegister)
                {
                    if(!(allocator->availablePageFlags[r] & (remainderFlagsMask >> i)))
                    {
                        /* occupy the pages; excluding the first bits since they are reserved as guard pages */
                        const u$ occupyPageCount = remainderFlagsRequired - 2*SURROUNDING_GUARD_PAGE_COUNT;
                        const u$ occupyFlagsMask = (~(~0ULL << occupyPageCount)) << (bitsPerRegister - occupyPageCount);// flags without guard page flags aligned to the most significant bit
                        korl_assert(0 == (allocator->availablePageFlags[r] & occupyFlagsMask >> (i + SURROUNDING_GUARD_PAGE_COUNT)));
                        allocator->availablePageFlags[r] |= occupyFlagsMask >> (i + SURROUNDING_GUARD_PAGE_COUNT);
                        allocationPageIndex = r*bitsPerRegister + i + SURROUNDING_GUARD_PAGE_COUNT;
                        goto returnAllocationPageIndex;
                    }
                    continue;
                }
                /* if the mask doesn't fit in the register & there are no more registers, we're done */
                if(r >= allocator->allocationPages - 1)
                    break;
                /* otherwise, there is a next register, and the mask spills over to the next one, so we need to do two flag register operator& operations */
                const u$ spillFlags = (i + remainderFlagsRequired) % bitsPerRegister;
                const u$ spillMask  = (~(~0ULL << spillFlags)) << (bitsPerRegister - spillFlags);
                if(   !(allocator->availablePageFlags[r    ] & (remainderFlagsMask >> i))
                   && !(allocator->availablePageFlags[r + 1] & spillMask))
                {
                    /* generate masks without the leading/trailing guard pages taken into account */
                    // const u$ occupyFlagsMask  = (~(~0ULL << occupyPageCount)) << (bitsPerRegister - occupyPageCount);// flags without guard page flags aligned to the most significant bit
                    const u$ occupyPageCount  = remainderFlagsRequired - 2*SURROUNDING_GUARD_PAGE_COUNT;
                    const u$ occupySpillFlags = (i + SURROUNDING_GUARD_PAGE_COUNT + occupyPageCount) % bitsPerRegister;// # of flags that spill over to the next flag register
                    const u$ occupySpillMask  = (~(~0ULL << occupySpillFlags)) << (bitsPerRegister - occupySpillFlags);// the spillover flag mask aligned to the most-significant bit (exactly what we need!)
                    const u$ leadingMask      = ~(~0ULL << (occupyPageCount - occupySpillFlags));
                    /* occupy the pages at the adjusted offset */
                    korl_assert(0 == (allocator->availablePageFlags[r    ] & leadingMask));
                    korl_assert(0 == (allocator->availablePageFlags[r + 1] & occupySpillMask));
                    allocator->availablePageFlags[r    ] |= leadingMask;
                    allocator->availablePageFlags[r + 1] |= occupySpillMask;
                    allocationPageIndex = r*bitsPerRegister + i + SURROUNDING_GUARD_PAGE_COUNT;
                    goto returnAllocationPageIndex;
                }
            }
        }
    }
    returnAllocationPageIndex:
    return allocationPageIndex;
#else
    korl_assert(pageCount + 1/*preceding guard page*/ <= allocator->allocationPages);
    /* Example of a search for an available set of allocation pages:
     *  [ 0][ 1][ 2][ 3][ 4][ 5][ 6][ 7][ 8]// flag indices (allocationPages=9)
     *  [ 0][ 1][ 0][ 1][ 0][ 1][ 0][ 0][ 0]// flags
     *                          [ G][ 1][ 1]// allocation: bytes=5000, pages=2 (+1 preceding guard page)
     */
    u$ availablePageIndex = allocator->allocationPages;
    for(u$ p = 0; p <= allocator->allocationPages - (pageCount + 1/*preceding guard page*/); )
    {
        ///@TODO: this will be much faster if we offset & mod `p` by an expected next available page index, but let's just get this working first...
        const u$ highestOccupiedPageOffset = _korl_memory_allocator_general_highestOccupiedPageOffset(allocator, p, pageCount + 1/*preceding guard page*/);
        /* if there were no occupied page flags in the given range, we can just occupy this region */
        if(highestOccupiedPageOffset >= pageCount + 1/*preceding guard page*/)
        {
            _korl_memory_allocator_general_setPageFlags(allocator, p + 1/*skip guard page*/, pageCount, true/*occupied*/);
            availablePageIndex = p + 1/*skip guard page*/;
            break;
        }
        else
            p += highestOccupiedPageOffset + 1;
    }
    return availablePageIndex;
#endif
}
korl_internal void* _korl_memory_allocator_general_allocate(_Korl_Memory_AllocatorGeneral* allocator, u$ bytes, const wchar_t* file, int line, void* requestedAddress)
{
    void* result = NULL;
    korl_assert(bytes > 0);
    /* allocate a range of pages within the allocator's available page flags */
    const u$ pageBytes         = _korl_memory_context.systemInfo.dwPageSize;
    const u$ metaBytesRequired = sizeof(Korl_Memory_AllocatorGeneral_AllocationMeta) + sizeof(_KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
    const u$ allocationPages   = korl_math_nextHighestDivision(metaBytesRequired + bytes, pageBytes);
    _korl_memory_allocator_general_allocatorPagesUnguard(allocator);
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    u$ availablePageIndex = allocator->allocationPages;
    if(requestedAddress)
    {
        Korl_Memory_AllocatorGeneral_AllocationMeta*const requestedMeta = KORL_C_CAST(Korl_Memory_AllocatorGeneral_AllocationMeta*, 
            KORL_C_CAST(u8*, requestedAddress) - metaBytesRequired);
        korl_assert(KORL_C_CAST(u$, requestedMeta) % pageBytes == 0);
        const void*const allocationRegionBegin = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
        const void*const allocationRegionEnd   = KORL_C_CAST(u8*, allocationRegionBegin) + allocator->allocationPages*pageBytes;
        korl_assert(requestedAddress >= allocationRegionBegin);// we must be inside the allocation region
        korl_assert(requestedAddress <  allocationRegionEnd);  // we must be inside the allocation region
        const u$ requestedAllocationPage = (KORL_C_CAST(u$, requestedMeta) - KORL_C_CAST(u$, allocationRegionBegin)) / pageBytes;
        /* ensure that the page flags required are not occupied */
        const u$ highestOccupiedPageOffset = _korl_memory_allocator_general_highestOccupiedPageOffset(allocator, requestedAllocationPage - 1/*account for preceding guard page*/, allocationPages + 1/*include preceding guard page*/);
        if(highestOccupiedPageOffset < allocationPages + 1/*include preceding guard page*/)
        {
            korl_log(WARNING, "Allocation failure; requested address is occupied!");
            goto guardAllocator_returnResult;
        }
        /* set the available page index appropriately & occupy the necessary page flags */
        _korl_memory_allocator_general_setPageFlags(allocator, requestedAllocationPage, allocationPages, true/*occupied*/);
        availablePageIndex = requestedAllocationPage;
    }
    else
        availablePageIndex = _korl_memory_allocator_general_occupyAvailablePages(allocator, allocationPages);
    if(availablePageIndex >= allocator->allocationPages)
    {
        korl_log(WARNING, "occupyAvailablePages failed!  (allocator may require defragmentation, or may have run out of space)");
        goto guardAllocator_returnResult;
    }
    /* now that we have a set of pages which we can safely assume are unused, we 
        need to obtain the address & commit the memory */
    Korl_Memory_AllocatorGeneral_AllocationMeta*const metaAddress = KORL_C_CAST(Korl_Memory_AllocatorGeneral_AllocationMeta*, 
        KORL_C_CAST(u8*, allocator) + (allocatorPages + availablePageIndex)*pageBytes);
    result = KORL_C_CAST(u8*, metaAddress) + metaBytesRequired;
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
    const LPVOID resultVirtualAllocCommit = VirtualAlloc(metaAddress, allocationPages*pageBytes, MEM_COMMIT, PAGE_READWRITE);
    if(resultVirtualAllocCommit == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* now we can initialize the memory */
    metaAddress->allocationMeta.bytes = bytes;
    metaAddress->allocationMeta.file  = file;
    metaAddress->allocationMeta.line  = line;
    metaAddress->pagesCommitted       = allocationPages;
    korl_memory_copy(metaAddress + 1, _KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/);
    guardAllocator_returnResult:
    _korl_memory_allocator_general_allocatorPagesGuard(allocator);
    return result;
}
korl_internal void _korl_memory_allocator_general_free(_Korl_Memory_AllocatorGeneral* allocator, void* allocation, const wchar_t* file, int line)
{
    /* sanity checks */
    _korl_memory_allocator_general_allocatorPagesUnguard(allocator);
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    korl_assert(0 == KORL_C_CAST(u$, allocator)  % _korl_memory_context.systemInfo.dwAllocationGranularity);
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    const void*const allocationRegionBegin = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
    const void*const allocationRegionEnd   = KORL_C_CAST(u8*, allocationRegionBegin) + allocator->allocationPages*pageBytes;
    korl_assert(allocation >= allocationRegionBegin);// we must be inside the allocation region
    korl_assert(allocation <  allocationRegionEnd);  // we must be inside the allocation region
    /* access the allocation meta data */
    const u$ metaBytesRequired = sizeof(Korl_Memory_AllocatorGeneral_AllocationMeta) + sizeof(_KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
    Korl_Memory_AllocatorGeneral_AllocationMeta*const allocationMeta = KORL_C_CAST(Korl_Memory_AllocatorGeneral_AllocationMeta*, 
        KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    /* allocation sanity checks */
    korl_assert(0 == KORL_C_CAST(u$, allocationMeta) % pageBytes);// we must be page-aligned
    korl_assert(0 == korl_memory_compare(allocationMeta + 1, _KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/));
    /* calculate the page index of the allocation */
    const u$ allocationPage = (KORL_C_CAST(u$, allocationMeta) - KORL_C_CAST(u$, allocationRegionBegin)) / pageBytes;
    /* clear the allocation page flags associated with this allocation */
    _korl_memory_allocator_general_setPageFlags(allocator, allocationPage, allocationMeta->pagesCommitted, false/*clear*/);
    /* decommit the pages */
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
    if(!VirtualFree(allocationMeta, allocationMeta->pagesCommitted*pageBytes, MEM_DECOMMIT))
        korl_logLastError("VirtualFree failed!");
    _korl_memory_allocator_general_allocatorPagesGuard(allocator);
}
korl_internal void* _korl_memory_allocator_general_reallocate(_Korl_Memory_AllocatorGeneral* allocator, void* allocation, u$ bytes, const wchar_t* file, int line)
{
    /* sanity checks */
    _korl_memory_allocator_general_allocatorPagesUnguard(allocator);
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    korl_assert(0 == KORL_C_CAST(u$, allocator)  % _korl_memory_context.systemInfo.dwAllocationGranularity);
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    const void*const allocationRegionBegin = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
    const void*const allocationRegionEnd   = KORL_C_CAST(u8*, allocationRegionBegin) + allocator->allocationPages*pageBytes;
    korl_assert(allocation >= allocationRegionBegin);// we must be inside the allocation region
    korl_assert(allocation <  allocationRegionEnd);  // we must be inside the allocation region
    /* access the allocation meta data */
    const u$ metaBytesRequired = sizeof(Korl_Memory_AllocatorGeneral_AllocationMeta) + sizeof(_KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
    Korl_Memory_AllocatorGeneral_AllocationMeta*const allocationMeta = KORL_C_CAST(Korl_Memory_AllocatorGeneral_AllocationMeta*, 
        KORL_C_CAST(u8*, allocation) - metaBytesRequired);
    /* allocation sanity checks */
    korl_assert(0 == KORL_C_CAST(u$, allocationMeta) % pageBytes);// we must be page-aligned
    korl_assert(0 == korl_memory_compare(allocationMeta + 1, _KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/));
    /* calculate the page index of the allocation */
    const u$ allocationPage     = (KORL_C_CAST(u$, allocationMeta) - KORL_C_CAST(u$, allocationRegionBegin)) / pageBytes;
    const u$ newAllocationPages = korl_math_nextHighestDivision(metaBytesRequired + bytes, pageBytes);
    /* if we're shrinking the allocation... */
    if(newAllocationPages <= allocationMeta->pagesCommitted)
    {
        /* only if we are shrinking the # of committed pages, we must decommit pages that become unoccupied */
        if(newAllocationPages < allocationMeta->pagesCommitted)
        {
            //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
            if(!VirtualFree(KORL_C_CAST(u8*, allocationMeta) + newAllocationPages*pageBytes, (allocationMeta->pagesCommitted - newAllocationPages)*pageBytes, MEM_DECOMMIT))
                korl_logLastError("VirtualFree failed!");
            _korl_memory_allocator_general_setPageFlags(allocator, allocationPage + newAllocationPages, (allocationMeta->pagesCommitted - newAllocationPages), false/*unoccupied*/);
        }
        allocationMeta->pagesCommitted       = newAllocationPages;
        allocationMeta->allocationMeta.bytes = bytes;
        goto guardAllocator_returnAllocation;
    }
    /* all the code past here involves cases where we need to expand the allocation */
    korl_assert(newAllocationPages > allocationMeta->pagesCommitted);
    /// @TODO: optimization (minor?); in-place expand to higher contiguous pages; it's likely that only in-place expansion into higher pages is going to be worth the effort
#if 1
    /* we were unable to quickly expand the allocation; we need to allocate=>copy=>free */
    /* clear our page flags */
    _korl_memory_allocator_general_setPageFlags(allocator, allocationPage, allocationMeta->pagesCommitted, false/*unoccupied*/);
    /* occupy new page flags that satisfy newAllocationPages */
    const u$ newAllocationPage = _korl_memory_allocator_general_occupyAvailablePages(allocator, newAllocationPages);
    if(newAllocationPage >= allocator->allocationPages)
    {
        korl_log(WARNING, "occupyAvailablePages failed!  (allocator may require defragmentation, or may have run out of space)");
        _korl_memory_allocator_general_setPageFlags(allocator, allocationPage, allocationMeta->pagesCommitted, true/*occupied*/);
        allocation = NULL;
        goto guardAllocator_returnAllocation;
    }
    /* commit new pages */
    Korl_Memory_AllocatorGeneral_AllocationMeta*const newMeta = KORL_C_CAST(Korl_Memory_AllocatorGeneral_AllocationMeta*, 
        KORL_C_CAST(u8*, allocator) + (allocatorPages + newAllocationPage)*pageBytes);
    void*const newAllocation = KORL_C_CAST(u8*, newMeta) + metaBytesRequired;
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
    const LPVOID resultVirtualAllocCommit = VirtualAlloc(newMeta, newAllocationPages*pageBytes, MEM_COMMIT, PAGE_READWRITE);
    if(resultVirtualAllocCommit == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* move data from old allocation to new allocation */
    const u$ allocationPageEnd    = allocationPage    + allocationMeta->pagesCommitted;// gather this data before allocationMeta gets clobbered
    const u$ newAllocationPageEnd = newAllocationPage + newAllocationPages;            // gather this data before allocationMeta gets clobbered
    korl_memory_move(newAllocation, allocation, allocationMeta->allocationMeta.bytes);
    allocation = newAllocation;// we're done w/ allocation at this point
    /* create new meta data with updated metrics */
    newMeta->pagesCommitted = newAllocationPages;
    newMeta->allocationMeta.bytes = bytes;
    newMeta->allocationMeta.file  = file;
    newMeta->allocationMeta.line  = line;
    korl_memory_copy(newMeta + 1, _KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR, sizeof(_KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/);
    /* Decommit pages that are outside of the new allocation pages, and inside 
        the old allocation pages.  Possible scenarios:
        - [newBegin             newEnd][oldBegin             oldEnd]// decommit required // oldEnd > newBegin && oldBegin > newEnd
        - [newBegin            [oldBegin newEnd]             oldEnd]// decommit required // oldEnd > newBegin && oldBegin < newEnd
        - [newBegin      [oldBegin             oldEnd]       newEnd]// do nothing        // 
        - [oldBegin             [newBegin oldEnd]            newEnd]// decommit required // oldEnd > newBegin && oldBegin < newEnd
        - [oldBegin             oldEnd][newBegin             newEnd]// decommit required // oldEnd < newBegin && oldBegin < newEnd
        We _know_ that since we are _expanding_ the allocation, we can never 
        encounter the following scenarios:
        - [oldBegin      [newBegin             newEnd]       oldEnd]
        - newBegin == oldBegin && newEnd == oldEnd (allocations occupy the same region) */
    if(allocationPage >= newAllocationPage && allocationPageEnd <= newAllocationPageEnd)//old allocation is contained within new allocation
        goto guardAllocator_returnAllocation;
    u$ decommitPage    = allocationPage;
    u$ decommitPageEnd = allocationPageEnd;
    if(   allocationPageEnd > newAllocationPage && allocationPageEnd > newAllocationPageEnd //    old allocation is higher than new allocation
       && allocationPage < newAllocationPageEnd)                                            //AND old allocation is intersecting
        decommitPage = newAllocationPageEnd;
    if(   allocationPage < newAllocationPage && allocationPage < newAllocationPageEnd //    old allocation is lower than new allocation
       && allocationPageEnd > newAllocationPage)                                      //AND old allocation is intersecting
        decommitPageEnd = newAllocationPage;
    korl_assert(decommitPageEnd > decommitPage);
    const u$ decommitPages = decommitPageEnd - decommitPage;
    korl_assert(decommitPages > 0);// this _must_ be the case, since we have eliminated all other possible intersection scenarios... right?
    if(!VirtualFree(KORL_C_CAST(u8*, allocator) + (allocatorPages + decommitPage)*pageBytes, decommitPages*pageBytes, MEM_DECOMMIT))
        korl_logLastError("VirtualFree failed!");
#else
    /* we now know expansion is impossible; we need to allocate=>copy=>free */
    void*const newAllocation = _korl_memory_allocator_general_allocate(allocator, bytes, file, line, NULL/*auto-select address*/);
    if(newAllocation)
    {
        korl_memory_copy(newAllocation, allocation, allocationMeta->allocationMeta.bytes);
        _korl_memory_allocator_general_free(allocator, allocation, file, line);
    }
    else
        korl_log(WARNING, "new allocation for reallocate failed; original allocation will be unmodified");
    allocation = newAllocation;
#endif
    guardAllocator_returnAllocation:
    _korl_memory_allocator_general_allocatorPagesGuard(allocator);
    return allocation;
}
korl_internal void _korl_memory_allocator_general_empty(_Korl_Memory_AllocatorGeneral* allocator)
{
    /* sanity checks */
    _korl_memory_allocator_general_allocatorPagesUnguard(allocator);
    const u$ pageBytes = _korl_memory_context.systemInfo.dwPageSize;
    korl_assert(0 == KORL_C_CAST(u$, allocator) % _korl_memory_context.systemInfo.dwAllocationGranularity);
    /* eliminate all allocations */
    /* MSDN page for VirtualFree says: 
        "The function does not fail if you attempt to decommit an uncommitted 
        page. This means that you can decommit a range of pages without first 
        determining the current commitment state."  It would be really easy to 
        just do this and re-commit pages each time we allocate, this is 
        apparently expensive, so we might not want to do this. */
    //KORL-PERFORMANCE-000-000-027: memory: uncertain performance characteristics associated with re-committed pages
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    void*const allocationRegionStart = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
    if(!VirtualFree(allocationRegionStart, allocator->allocationPages*pageBytes, MEM_DECOMMIT))
        korl_logLastError("VirtualFree failed!");
    /* update the allocator metrics; clear all the allocation page flags */
    const u$ usedPageFlagRegisters = korl_math_nextHighestDivision(allocator->allocationPages, (8*sizeof(*allocator->availablePageFlags))/*bitsPerPageFlagRegister*/);
    korl_memory_zero(KORL_C_CAST(u8*, allocator) + sizeof(*allocator), usedPageFlagRegisters*sizeof(*(allocator->availablePageFlags)));
    /**/
    _korl_memory_allocator_general_allocatorPagesGuard(allocator);
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(_korl_memory_allocator_general_enumerateAllocations)
{
    _Korl_Memory_AllocatorGeneral*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorGeneral*, allocatorUserData);
    /* sanity checks */
    _korl_memory_allocator_general_allocatorPagesUnguard(allocator);
    korl_assert(0 == KORL_C_CAST(u$, allocator) % _korl_memory_context.systemInfo.dwAllocationGranularity);
    /* calculate virtual address range */
    const u$ pageBytes      = _korl_memory_context.systemInfo.dwPageSize;
    const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*allocator) + allocator->availablePageFlagsSize*sizeof(*(allocator->availablePageFlags)), pageBytes);
    if(out_allocatorVirtualAddressEnd)
        *out_allocatorVirtualAddressEnd = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes + allocator->allocationPages*pageBytes;
    const u$ bitsPerFlagRegister   = 8*sizeof(*(allocator->availablePageFlags));
    const u$ usedPageFlagRegisters = korl_math_nextHighestDivision(allocator->allocationPages, bitsPerFlagRegister);
    u$ pageFlagsRemainder = 0;
    for(u$ pfr = 0; pfr < usedPageFlagRegisters; pfr++)
    {
        u$ pageFlagRegister = allocator->availablePageFlags[pfr];
        /* remove page flag bits that are remaining from previous register(s) */
        const u$ pageFlagsRemainderInRegister     = KORL_MATH_MIN(bitsPerFlagRegister, pageFlagsRemainder);
        const u$ pageFlagsRemainderInRegisterMask = ~((~0LLU) << pageFlagsRemainderInRegister);
        pageFlagRegister &= ~(pageFlagsRemainderInRegisterMask << (bitsPerFlagRegister - pageFlagsRemainderInRegister));
        /**/
        unsigned long mostSignificantSetBitIndex = korl_checkCast_u$_to_u32(bitsPerFlagRegister)/*smallest invalid value*/;
        while(_BitScanReverse64(&mostSignificantSetBitIndex, pageFlagRegister))
        {
            const u$ allocationPageIndex = pfr*bitsPerFlagRegister + (bitsPerFlagRegister - 1 - mostSignificantSetBitIndex);
            const Korl_Memory_AllocatorGeneral_AllocationMeta*const metaAddress = KORL_C_CAST(Korl_Memory_AllocatorGeneral_AllocationMeta*, 
                KORL_C_CAST(u8*, allocator) + (allocatorPages + allocationPageIndex)*pageBytes);
            const u$ metaBytesRequired = sizeof(*metaAddress) + sizeof(_KORL_ALLOCATOR_GENERAL_ALLOCATION_META_SEPARATOR) - 1/*don't include null-terminator*/;
            if(!callback(callbackUserData, KORL_C_CAST(u8*, metaAddress) + metaBytesRequired, &(metaAddress->allocationMeta)))
                goto guardAllocator_return;
            const u$ maxFlagsInRegister        = mostSignificantSetBitIndex + 1;
            const u$ allocationFlagsInRegister = KORL_MATH_MIN(maxFlagsInRegister, metaAddress->pagesCommitted);
            /* remove the flags that this allocation occupies */
            const u$ allocationFlagsInRegisterMask = ~((~0LLU) << allocationFlagsInRegister);
            pageFlagRegister &= ~(allocationFlagsInRegisterMask << (mostSignificantSetBitIndex - (allocationFlagsInRegister - 1)));
            /* carry over any remaining flags that will occupy future register(s) */
            pageFlagsRemainder += metaAddress->pagesCommitted - allocationFlagsInRegister;
        }
        ///@TODO: skip `pageFlagsRemainder/bitsPerFlagRegister` registers, since the last allocation is expected to span all of them
    }
    guardAllocator_return:
    _korl_memory_allocator_general_allocatorPagesGuard(allocator);
}
korl_internal _Korl_Memory_AllocatorGeneral* _korl_memory_allocator_general_create(u$ maxBytes, void* address)
{
    korl_assert(maxBytes > 0);
    _Korl_Memory_AllocatorGeneral* result = NULL;
    const u$ pageBytes                     = _korl_memory_context.systemInfo.dwPageSize;
    const u$ pagesRequired                 = korl_math_nextHighestDivision(maxBytes, pageBytes);
    const u$ allocatorBytesRequiredMinimum = sizeof(*result);
    /* we need to figure out the minimum # of allocator pages we need in order 
        to be able to store an array of flags used to keep track of all the 
        allocation pages */
    u$ allocatorPages = korl_math_nextHighestDivision(allocatorBytesRequiredMinimum, pageBytes);
    for(;; allocatorPages++)
    {
        korl_assert(pagesRequired >= allocatorPages + 2);// we need at least 2 extra pages to be able to make a single allocation
        const u$ allocationPages            = pagesRequired - allocatorPages;
        const u$ bytesAvailableForPageFlags = allocatorPages*pageBytes - allocatorBytesRequiredMinimum;
        const u$ availablePageFlagsSizeMax  = bytesAvailableForPageFlags / sizeof(*(result->availablePageFlags));// round down by the size of each flag register just to make things easier (negligible wasted memory anyway)
        const u$ availablePageFlagBitsMax   = (8*sizeof(*(result->availablePageFlags)))/*bitsPerPageFlagRegister*/*availablePageFlagsSizeMax;
        if(availablePageFlagBitsMax >= allocationPages)
            break;
    }
    /* now that we know how many pages are required for the allocator data 
        structure, we can calculate the final size of the available page flags 
        array (same calculations as above); we will later be able to determine 
        how many allocator pages there are based on the size of the available 
        page flags array member */
    const u$ allocationPages            = pagesRequired - allocatorPages;
    const u$ bytesAvailableForPageFlags = allocatorPages*pageBytes - allocatorBytesRequiredMinimum;
    const u$ availablePageFlagsSizeMax  = bytesAvailableForPageFlags / sizeof(*(result->availablePageFlags));// round down by the size of each flag register just to make things easier (negligible wasted memory anyway)
    /* reserve the full range of virtual memory */
    LPVOID resultVirtualAlloc = VirtualAlloc(address, pagesRequired*pageBytes, MEM_RESERVE, PAGE_NOACCESS);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* commit the first pages for the allocator itself */
    // const u$ allocatorPages = korl_math_nextHighestDivision(sizeof(*result), korl_memory_pageBytes());
    const u$ allocatorBytes = allocatorPages * pageBytes;
    resultVirtualAlloc = VirtualAlloc(resultVirtualAlloc, allocatorBytes, MEM_COMMIT, PAGE_READWRITE);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* initialize the memory of the allocator userData */
    korl_shared_const i8 ALLOCATOR_PAGE_MEMORY_PATTERN[] = "KORL-GeneralAllocator-Page|";
    _Korl_Memory_AllocatorGeneral*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorGeneral*, resultVirtualAlloc);
    result = allocator;
    // fill the allocator page with a readable pattern for easier debugging //
    for(u$ currFillByte = 0; currFillByte < allocatorBytes; )
    {
        const u$ remainingBytes = allocatorBytes - currFillByte;
        const u$ availableFillBytes = KORL_MATH_MIN(remainingBytes, sizeof(ALLOCATOR_PAGE_MEMORY_PATTERN) - 1/*do not copy NULL-terminator*/);
        korl_memory_copy(KORL_C_CAST(u8*, allocator) + currFillByte, ALLOCATOR_PAGE_MEMORY_PATTERN, availableFillBytes);
        currFillByte += availableFillBytes;
    }
    const u$ usedPageFlagRegisters = korl_math_nextHighestDivision(allocationPages, (8*sizeof(*result->availablePageFlags))/*bitsPerPageFlagRegister*/);
    korl_memory_zero(allocator, sizeof(*result) + usedPageFlagRegisters*sizeof(*(result->availablePageFlags)));
    allocator->allocationPages        = allocationPages;
    allocator->availablePageFlagsSize = availablePageFlagsSizeMax;
    /* guard the allocator pages */
    _korl_memory_allocator_general_allocatorPagesGuard(allocator);
    return result;
}
korl_internal void _korl_memory_allocator_general_destroy(_Korl_Memory_AllocatorGeneral* allocator)
{
    if(!VirtualFree(allocator, 0/*release everything*/, MEM_RELEASE))
        korl_logLastError("VirtualFree failed!");
}
korl_internal _Korl_Memory_Allocator* _korl_memory_allocator_matchHandle(Korl_Memory_AllocatorHandle handle)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    /* NOTE: this is most likely not an issue, but this is obviously not 
             thread-safe, even though this is very likely going to be called 
             from many different threads in the event we want to do 
             multi-threaded dynamic memory allocations, which is basically 100%.  
             However, it's _likely_ nothing to worry about, since what is likely 
             going to happen is that the allocators are going to be set up at 
             the start of the program, and then the allocator memory pool will 
             become read-only for the vast majority of operation.  It is worth 
             noting that this is only okay if the above usage assumptions 
             continue */
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
        if(context->allocators[a].handle == handle)
            return &context->allocators[a];
    korl_log(WARNING, "no allocator found for handle %u", handle);
    return NULL;
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS_CALLBACK(_korl_memory_logReport_enumerateCallback)
{
    _Korl_Memory_ReportEnumerateContext*const context = KORL_C_CAST(_Korl_Memory_ReportEnumerateContext*, userData);
    if(context->allocationMetaSize == context->allocationMetaCapacity)
    {
        context->allocationMetaCapacity *= 2;
        context->allocationMeta = KORL_C_CAST(_Korl_Memory_ReportAllocationMetaData*, 
            korl_reallocate(_korl_memory_context.allocatorHandleReporting, 
                            context->allocationMeta, 
                            context->allocationMetaCapacity * sizeof(*context->allocationMeta)));
        korl_assert(context->allocationMeta);
    }
    _Korl_Memory_ReportAllocationMetaData*const newAllocMeta = &context->allocationMeta[context->allocationMetaSize++];
    newAllocMeta->allocationAddress = allocation;
    newAllocMeta->meta              = *meta;
    context->totalUsedBytes += meta->bytes;
    return true;// true => continue enumerating
}
korl_internal KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR(korl_memory_allocator_create)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(GetCurrentThreadId() == context->mainThreadId);
    korl_assert(!KORL_MEMORY_POOL_ISFULL(context->allocators));
    /* obtain a new unique handle for this allocator */
    Korl_Memory_AllocatorHandle newHandle = 0;
    for(Korl_Memory_AllocatorHandle h = 1; h <= KORL_MEMORY_POOL_CAPACITY(context->allocators); h++)
    {
        newHandle = h;
        for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
            if(context->allocators[a].handle == h)
            {
                newHandle = 0;
                break;
            }
        if(newHandle)
            break;
    }
    korl_assert(newHandle);
    /* ensure that allocatorName has not been used in any other allocator */
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
        if(0 == korl_memory_stringCompare(context->allocators[a].name, allocatorName))
        {
            korl_log(ERROR, "allocator name %s already in use", allocatorName);
            return 0;// return an invalid handle
        }
    /* create the allocator */
    _Korl_Memory_Allocator* newAllocator = KORL_MEMORY_POOL_ADD(context->allocators);
    korl_memory_zero(newAllocator, sizeof(*newAllocator));
    newAllocator->type     = type;
    newAllocator->handle   = newHandle;
    newAllocator->flags    = flags;
    newAllocator->maxBytes = maxBytes;
    switch(type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        newAllocator->userData = _korl_memory_allocator_linear_create(maxBytes, address);
        break;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        newAllocator->userData = _korl_memory_allocator_general_create(maxBytes, address);
        break;}
    default:{
        korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", type);
        break;}
    }
    korl_memory_stringCopy(allocatorName, newAllocator->name, korl_arraySize(newAllocator->name));
    /**/
    return newAllocator->handle;
}
korl_internal void korl_memory_allocator_destroy(Korl_Memory_AllocatorHandle handle)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(GetCurrentThreadId() == context->mainThreadId);
    Korl_MemoryPool_Size a = 0;
    for(; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
        if(context->allocators[a].handle == handle)
            break;
    korl_assert(a < KORL_MEMORY_POOL_SIZE(context->allocators));
    _Korl_Memory_Allocator*const allocator = &context->allocators[a];
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        _korl_memory_allocator_linear_destroy(allocator->userData);
        break;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        _korl_memory_allocator_general_destroy(allocator->userData);
        break;}
    default:{
        korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
        break;}
    }
    KORL_MEMORY_POOL_REMOVE(context->allocators, a);
}
korl_internal void korl_memory_allocator_recreate(Korl_Memory_AllocatorHandle handle, void* newAddress)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(GetCurrentThreadId() == context->mainThreadId);
    Korl_MemoryPool_Size a = 0;
    for(; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
        if(context->allocators[a].handle == handle)
            break;
    korl_assert(a < KORL_MEMORY_POOL_SIZE(context->allocators));
    _Korl_Memory_Allocator*const allocator = &context->allocators[a];
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        _korl_memory_allocator_linear_destroy(allocator->userData);
        allocator->userData = _korl_memory_allocator_linear_create(allocator->maxBytes, newAddress);
        break;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        _korl_memory_allocator_general_destroy(allocator->userData);
        allocator->userData = _korl_memory_allocator_general_create(allocator->maxBytes, newAddress);
        break;}
    default:{
        korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
        break;}
    }
}
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE(korl_memory_allocator_allocate)
{
    if(bytes == 0)
        return NULL;
    _Korl_Memory_Context*const context = &_korl_memory_context;
    _Korl_Memory_Allocator*const allocator = _korl_memory_allocator_matchHandle(handle);
    korl_assert(allocator);
    if(!(allocator->flags & KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS) && GetCurrentThreadId() != context->mainThreadId)
    {
        korl_log(ERROR, "threadId(%u) != mainThreadId(%u)", GetCurrentThreadId(), context->mainThreadId);
        return NULL;
    }
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        return _korl_memory_allocator_linear_allocate(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData), bytes, file, line, address);}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        return _korl_memory_allocator_general_allocate(KORL_C_CAST(_Korl_Memory_AllocatorGeneral*, allocator->userData), bytes, file, line, address);}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return NULL;
}
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(korl_memory_allocator_reallocate)
{
    korl_assert(bytes > 0);// CRT realloc documentations says calling it with 0 bytes is "undefined", so let's just never let this case happen
    _Korl_Memory_Context*const context = &_korl_memory_context;
    _Korl_Memory_Allocator*const allocator = _korl_memory_allocator_matchHandle(handle);
    korl_assert(allocator);
    if(!(allocator->flags & KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS) && GetCurrentThreadId() != context->mainThreadId)
    {
        korl_log(ERROR, "threadId(%u) != mainThreadId(%u)", GetCurrentThreadId(), context->mainThreadId);
        return NULL;
    }
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        if(allocation == NULL)
            return _korl_memory_allocator_linear_allocate(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData), bytes, file, line, NULL/*automatically find address*/);
        return _korl_memory_allocator_linear_reallocate(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData), allocation, bytes, file, line);}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        if(allocation == NULL)
            return _korl_memory_allocator_general_allocate(KORL_C_CAST(_Korl_Memory_AllocatorGeneral*, allocator->userData), bytes, file, line, NULL/*automatically find address*/);
        return _korl_memory_allocator_general_reallocate(KORL_C_CAST(_Korl_Memory_AllocatorGeneral*, allocator->userData), allocation, bytes, file, line);}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return NULL;
}
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_FREE(korl_memory_allocator_free)
{
    if(!allocation)
        return;
    _Korl_Memory_Context*const context = &_korl_memory_context;
    _Korl_Memory_Allocator*const allocator = _korl_memory_allocator_matchHandle(handle);
    korl_assert(allocator);
    if(!(allocator->flags & KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS) && GetCurrentThreadId() != context->mainThreadId)
    {
        korl_log(ERROR, "threadId(%u) != mainThreadId(%u)", GetCurrentThreadId(), context->mainThreadId);
        return;
    }
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        _korl_memory_allocator_linear_free(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData), allocation, file, line);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        _korl_memory_allocator_general_free(KORL_C_CAST(_Korl_Memory_AllocatorGeneral*, allocator->userData), allocation, file, line);
        return;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
}
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY(korl_memory_allocator_empty)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    _Korl_Memory_Allocator*const allocator = _korl_memory_allocator_matchHandle(handle);
    korl_assert(allocator);
    if(!(allocator->flags & KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS) && GetCurrentThreadId() != context->mainThreadId)
    {
        korl_log(ERROR, "threadId(%u) != mainThreadId(%u)", GetCurrentThreadId(), context->mainThreadId);
        return;
    }
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        _korl_memory_allocator_linear_empty(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData));
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        _korl_memory_allocator_general_empty(KORL_C_CAST(_Korl_Memory_AllocatorGeneral*, allocator->userData));
        return;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS_CALLBACK(_korl_memory_allocator_isEmpty_enumAllocationsCallback)
{
    bool*const resultIsEmpty = KORL_C_CAST(bool*, userData);
    *resultIsEmpty = false;
    return false;// false => stop enumerating; we only need to encounter one allocation
}
korl_internal bool korl_memory_allocator_isEmpty(Korl_Memory_AllocatorHandle handle)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    _Korl_Memory_Allocator*const allocator = _korl_memory_allocator_matchHandle(handle);
    korl_assert(allocator);
    if(!(allocator->flags & KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS) && GetCurrentThreadId() != context->mainThreadId)
    {
        korl_log(ERROR, "threadId(%u) != mainThreadId(%u)", GetCurrentThreadId(), context->mainThreadId);
        return true;
    }
    bool resultIsEmpty = true;
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        _korl_memory_allocator_linear_enumerateAllocations(allocator, allocator->userData, _korl_memory_allocator_isEmpty_enumAllocationsCallback, &resultIsEmpty, NULL/*we don't care about the virtual address range end*/);
        return resultIsEmpty;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        _korl_memory_allocator_general_enumerateAllocations(allocator, allocator->userData, _korl_memory_allocator_isEmpty_enumAllocationsCallback, &resultIsEmpty, NULL/*we don't care about the virtual address range end*/);
        return resultIsEmpty;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return false;
}
korl_internal void korl_memory_allocator_emptyStackAllocators(void)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
    {
        _Korl_Memory_Allocator*const allocator = &context->allocators[a];
        if(!(allocator->flags & KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME))
            continue;
        switch(allocator->type)
        {
        case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
            _korl_memory_allocator_linear_empty(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData));
            continue;}
        case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
            _korl_memory_allocator_general_empty(KORL_C_CAST(_Korl_Memory_AllocatorGeneral*, allocator->userData));
            continue;}
        }
        korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    }
}
korl_internal void* korl_memory_reportGenerate(void)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    korl_memory_allocator_empty(context->allocatorHandleReporting);// it should be fine to just only ever have 1 report in existence at a time
    _Korl_Memory_Report*const report = korl_allocate(context->allocatorHandleReporting, 
                                                     sizeof(*report) - sizeof(report->allocatorData) + KORL_MEMORY_POOL_SIZE(context->allocators)*sizeof(report->allocatorData));
    korl_memory_zero(report, sizeof(*report));
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
    {
        _Korl_Memory_Allocator*const allocator = &context->allocators[a];
        if(allocator->handle == context->allocatorHandleReporting)
            /* let's just skip the internal reporting allocator, since it is 
                transient and shouldn't be used anywhere else in the application */
            continue;
        /* we want the following information out of the allocator:
            - the full range of occupied virtual memory addresses
            - the total amount of committed memory // not currently keeping track of this!
            - the total amount of _used_ comitted memory 
                (in this case, _used_ is defined as the sum of all the address ranges for each allocation; 
                this does _not_ include the regions of memory used for allocator metrics & allocation metadata)
            - for each allocation:
                - metadata: function, file, line
                - address_start
                - address_end */
        _Korl_Memory_ReportEnumerateContext*const enumContext = &report->allocatorData[report->allocatorCount++];
        korl_memory_zero(enumContext, sizeof(*enumContext));
        korl_memory_stringCopy(allocator->name, enumContext->name, korl_arraySize(enumContext->name));
        enumContext->virtualAddressStart    = allocator->userData;
        enumContext->allocationMetaCapacity = 8;
        enumContext->allocationMeta         = korl_allocate(context->allocatorHandleReporting, enumContext->allocationMetaCapacity*sizeof(*(enumContext->allocationMeta)));
        korl_assert(enumContext->allocationMeta);
        switch(allocator->type)
        {
        case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
            _korl_memory_allocator_linear_enumerateAllocations(allocator, allocator->userData, _korl_memory_logReport_enumerateCallback, enumContext, &enumContext->virtualAddressEnd);
            break;}
        case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
            _korl_memory_allocator_general_enumerateAllocations(allocator, allocator->userData, _korl_memory_logReport_enumerateCallback, enumContext, &enumContext->virtualAddressEnd);
            break;}
        default:{
            korl_log(ERROR, "unknown allocator type '%i' not implemented", allocator->type);
            break;}
        }
        //KORL-ISSUE-000-000-066: memory: sort report allocations by ascending address
    }
    return report;
}
korl_internal void korl_memory_reportLog(void* reportAddress)
{
    _Korl_Memory_Report*const report = KORL_C_CAST(_Korl_Memory_Report*, reportAddress);
    korl_log_noMeta(INFO, "╔════ 🧠 Memory Report ════════════════════════════════╗");
    for(u$ ec = 0; ec < report->allocatorCount; ec++)
    {
        _Korl_Memory_ReportEnumerateContext*const enumContext = &report->allocatorData[ec];
        ///@TODO: report the type of allocator (linear, general, etc...)
        korl_log_noMeta(INFO, "║ allocator {\"%ws\", [0x%p ~ 0x%p], total used bytes: %llu}", 
                        enumContext->name, enumContext->virtualAddressStart, enumContext->virtualAddressEnd, enumContext->totalUsedBytes);
        for(u$ a = 0; a < enumContext->allocationMetaSize; a++)
        {
            _Korl_Memory_ReportAllocationMetaData*const allocMeta = &enumContext->allocationMeta[a];
            const void*const allocAddressEnd = KORL_C_CAST(u8*, allocMeta->allocationAddress) + allocMeta->meta.bytes;
            korl_log_noMeta(INFO, "║ %ws [0x%p ~ 0x%p] %llu bytes, %ws:%i", 
                            a == enumContext->allocationMetaSize - 1 ? L"└" : L"├", 
                            allocMeta->allocationAddress, allocAddressEnd, 
                            allocMeta->meta.bytes, 
                            allocMeta->meta.file, allocMeta->meta.line);
        }
    }
    korl_log_noMeta(INFO, "╚═════ END of Memory Report ════════════════════════════╝");
}
korl_internal void korl_memory_allocator_enumerateAllocators(fnSig_korl_memory_allocator_enumerateAllocatorsCallback *callback, void *callbackUserData)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
    {
        _Korl_Memory_Allocator*const allocator = &context->allocators[a];
        if(!callback(callbackUserData, allocator, allocator->userData, allocator->handle, allocator->name, allocator->flags))
            break;
    }
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(korl_memory_allocator_enumerateAllocations)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    _Korl_Memory_Allocator*const allocator = opaqueAllocator;
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        _korl_memory_allocator_linear_enumerateAllocations(opaqueAllocator, allocator->userData, callback, callbackUserData, out_allocatorVirtualAddressEnd);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        _korl_memory_allocator_general_enumerateAllocations(opaqueAllocator, allocator->userData, callback, callbackUserData, out_allocatorVirtualAddressEnd);
        return;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
}
korl_internal bool korl_memory_allocator_findByName(const wchar_t* name, Korl_Memory_AllocatorHandle* out_allocatorHandle)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
        if(korl_memory_stringCompare(context->allocators[a].name, name) == 0)
        {
            *out_allocatorHandle = context->allocators[a].handle;
            return true;
        }
    return false;
}
korl_internal bool korl_memory_allocator_containsAllocation(void* opaqueAllocator, const void* allocation)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    _Korl_Memory_Allocator*const allocator = opaqueAllocator;
    return allocation >= allocator->userData 
        && KORL_C_CAST(const u8*, allocation) < KORL_C_CAST(const u8*, allocator->userData) + allocator->maxBytes;
}
korl_internal u$ korl_memory_packStringI8(const i8* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, data), dataSize*sizeof(*data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packStringU16(const u16* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, data), dataSize*sizeof(*data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU$ (u$  data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU64(u64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU32(u32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU16(u16 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU8(u8 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packI64(i64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packI32(i32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packI16(i16 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packI8(i8 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packF64(f64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packF32(f32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackStringI8(i8* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(data, dataSize*sizeof(*data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackStringU16(u16* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(data, dataSize*sizeof(*data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackU$(u$ data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackU64(u64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackU32(u32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackU16(u16 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackU8 (u8  data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackI64(i64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackI32(i32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackI16(i16 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackI8 (i8  data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackF64(f64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackF32(f32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
