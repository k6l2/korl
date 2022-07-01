#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-windows-globalDefines.h"
#include "korl-checkCast.h"
#include "korl-log.h"
#define _KORL_MEMORY_INVALID_BYTE_PATTERN 0xFE
typedef struct _Korl_Memory_Allocator
{
    void* userData;
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
typedef struct _Korl_Memory_AllocatorLinear
{
    /** total amount of reserved virtual address space, including this struct */
    u$ bytes;//KORL-ISSUE-000-000-030: redundant: use QueryVirtualMemoryInformation instead
    void* nextAllocationAddress;
    /** support for realloc/free for arbitrary allocations 
     * - offsets are relative to the start address of this struct 
     * - offsets point to the allocation content itself, NOT the allocation 
     *   meta data page! */
    KORL_MEMORY_POOL_DECLARE(u32, allocationOffsets, 1024);
} _Korl_Memory_AllocatorLinear;
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
korl_internal void* _korl_memory_allocator_linear_allocate(void* allocatorUserData, u$ bytes, const wchar_t* file, int line, void* requestedAddress)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    /* for now, only support operations on the main thread */
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocatorUserData);
    void* allocationAddress = NULL;
    /* release the protection on the allocator pages */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    /* sanity check that the allocator's address range is page-aligned */
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(KORL_C_CAST(u$, allocator) % pageBytes == 0);
    korl_assert(allocator->bytes           % pageBytes == 0);
    /* If the user has requested a custom address for the allocation, we can 
        choose this address instead of the allocator's next available address.  
        In addition, we can conditionally check to make sure the requested 
        address is valid based on any number of metrics (within the address 
        range of the allocator, not going to overlap another allocation, etc...) */
    Korl_Memory_AllocationMeta* metaPageAddress = KORL_C_CAST(Korl_Memory_AllocationMeta*, allocator->nextAllocationAddress);
    const u$ allocationPages = ((bytes + (pageBytes - 1)) / pageBytes) + 1;// +1 for an additional preceding NOACCESS page
    if(requestedAddress)
    {
        metaPageAddress = KORL_C_CAST(Korl_Memory_AllocationMeta*, KORL_C_CAST(u8*, requestedAddress) - pageBytes/*the meta page goes _before_ the requested address*/);
        void* nextAllocationAddressCandidate = KORL_C_CAST(u8*, metaPageAddress) + allocationPages*pageBytes;
        /* ensure that we are not about to clobber an existing allocation, and 
            simultaneously calculate the new nextAllocationAddress by 
            considering all existing allocations */
        for(Korl_MemoryPool_Size o = 0; o < KORL_MEMORY_POOL_SIZE(allocator->allocationOffsets); o++)
        {
            void*const otherAllocation = KORL_C_CAST(u8*, allocator) + allocator->allocationOffsets[o];
            Korl_Memory_AllocationMeta*const otherMetaPage = KORL_C_CAST(Korl_Memory_AllocationMeta*, KORL_C_CAST(u8*, otherAllocation) - pageBytes);
            /* temporarily release protection of the other allocation's meta so we can examine metrics */
            {
                DWORD oldProtect;
                korl_assert(VirtualProtect(otherMetaPage, sizeof(*otherMetaPage), PAGE_READWRITE, &oldProtect));
                korl_assert(oldProtect == PAGE_NOACCESS);
            }
            const u$ otherAllocationPages = ((otherMetaPage->bytes + (pageBytes - 1)) / pageBytes) + 1/*for the meta page*/;
            void*const otherAllocationEnd = KORL_C_CAST(u8*, otherMetaPage) + otherAllocationPages*pageBytes;
            if(otherAllocationEnd > nextAllocationAddressCandidate)
                /* in an attempt to support arbitrary allocation emplacement, let's 
                    calculate nextAllocationAddress based on the new allocation list by 
                    taking the allocation with the largest offset */
                nextAllocationAddressCandidate = otherAllocationEnd;
            /* restore protection of the other allocation's meta page */
            {
                DWORD oldProtect;
                korl_assert(VirtualProtect(otherMetaPage, sizeof(*otherMetaPage), PAGE_NOACCESS, &oldProtect));
                korl_assert(oldProtect == PAGE_READWRITE);
            }
            /* now that we have all the info about the other allocation, we can 
                perform the safety check: */
            if(    KORL_C_CAST(void*, KORL_C_CAST(u8*, metaPageAddress) + allocationPages*pageBytes) > otherAllocation 
                && KORL_C_CAST(void*, metaPageAddress)                                               < otherAllocationEnd)
            {
                korl_log(ERROR, "requested allocation at [0x%p~0x%p) overlaps with existing allocation at [0x%p~0x%p)", 
                         metaPageAddress, KORL_C_CAST(u8*, metaPageAddress) + allocationPages*pageBytes, 
                         otherAllocation, otherAllocationEnd);
                goto protectAllocator_return_allocationAddress;
            }
        }
        allocator->nextAllocationAddress = nextAllocationAddressCandidate;
    }
    /* check to see that this allocation can fit in the range 
        [nextAllocationAddress, lastReservedAddress] */
    const u8*const allocatorEnd = KORL_C_CAST(u8*, allocator) + allocator->bytes;
    if(KORL_C_CAST(u8*, metaPageAddress) + allocationPages*pageBytes > allocatorEnd)
    {
        korl_log(WARNING, "linear allocator out of memory");//KORL-ISSUE-000-000-049: memory: logging an error when allocation fails in log module results in poor error logging
        goto protectAllocator_return_allocationAddress;
    }
    /* commit the pages of this allocation */
    const u$ allocatorPages = (sizeof(*allocator) + (pageBytes - 1)) / pageBytes;
    korl_assert(sizeof(Korl_Memory_AllocationMeta) < pageBytes);
    allocationAddress = KORL_C_CAST(u8*, metaPageAddress) + pageBytes;
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
    if(!requestedAddress)
        allocator->nextAllocationAddress = KORL_C_CAST(u8*, metaPageAddress) + allocationPages*pageBytes;
    u32*const newAllocationOffset = KORL_MEMORY_POOL_ADD(allocator->allocationOffsets);
    *newAllocationOffset = korl_checkCast_i$_to_u32(KORL_C_CAST(u8*, allocationAddress) - KORL_C_CAST(u8*, allocator));
protectAllocator_return_allocationAddress:
    /* protect the allocator pages until the time comes to actually use it */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    return allocationAddress;
}
korl_internal void _korl_memory_allocator_linear_free(void* allocatorUserData, void* allocation, const wchar_t* file, int line)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocatorUserData);
    const u$ pageBytes = korl_memory_pageBytes();
    /* release the protection on the allocator pages */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    korl_assert(allocator->bytes % pageBytes == 0);
    const u$ allocatorPages = (sizeof(*allocator) + (pageBytes - 1)) / pageBytes;
    /* ensure that this address is actually within the allocator's range */
    korl_assert(KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
             && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation address is divisible by the page size, because 
        this is currently a constraint of this allocator */
    korl_assert(KORL_C_CAST(u$, allocation) % pageBytes == 0);
    /* ensure that the offset of this address is recorded in the allocator 
        - remember: the allocation offset should point to the allocation's 
                    content, NOT the allocation's meta data page! */
    const u32 allocationOffset = korl_checkCast_i$_to_u32(KORL_C_CAST(u8*, allocation) - KORL_C_CAST(u8*, allocator));
    u32 allocationOffsetIndex = 0;
    for(; allocationOffsetIndex < KORL_MEMORY_POOL_SIZE(allocator->allocationOffsets); ++allocationOffsetIndex)
        if(allocator->allocationOffsets[allocationOffsetIndex] == allocationOffset)
            break;
    korl_assert(allocationOffsetIndex < KORL_MEMORY_POOL_SIZE(allocator->allocationOffsets));
    /* determine the range of pages occupied by the allocation */
    // get the address of the allocation's metadata page //
    Korl_Memory_AllocationMeta*const allocationMeta = 
        KORL_C_CAST(Korl_Memory_AllocationMeta*, KORL_C_CAST(u8*, allocation) - pageBytes);
    // remove protection on the allocation's metadata page so that we can obtain 
    //   the allocation's `size` information //
    {
        DWORD oldProtect;
        const BOOL resultVirtualProtect = VirtualProtect(allocationMeta, sizeof(*allocationMeta), PAGE_READWRITE, &oldProtect);
        if(!resultVirtualProtect)
            korl_logLastError("VirtualProtect failed!");
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    // the range of pages occupied by the allocation //
    // note that we get the TOTAL pages occupied by the allocation, including 
    //  the meta data
    const u$ allocationPages = ((allocationMeta->bytes + (pageBytes - 1)) / pageBytes) + 1;
    /* set the allocation's pages to NOACCESS, including the meta data page! */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocationMeta, allocationPages * pageBytes, PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    /* update allocator's metrics */
    KORL_MEMORY_POOL_REMOVE(allocator->allocationOffsets, allocationOffsetIndex);
    if(KORL_MEMORY_POOL_ISEMPTY(allocator->allocationOffsets))
        /* if the allocator no longer has any allocations, we can safely start 
            re-using memory from the beginning of the allocator again */
        allocator->nextAllocationAddress = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
    else
    {
        /* set the next allocation address to be immediately after the current 
            highest offset allocation */
        u32 highestOffset = 0;
        for(Korl_MemoryPool_Size i = 0; i < KORL_MEMORY_POOL_SIZE(allocator->allocationOffsets); ++i)
            highestOffset = KORL_MATH_MAX(highestOffset, allocator->allocationOffsets[i]);
        void*const highestAllocation = KORL_C_CAST(u8*, allocator) + highestOffset;
        Korl_Memory_AllocationMeta*const highestAllocationMeta = 
            KORL_C_CAST(Korl_Memory_AllocationMeta*, KORL_C_CAST(u8*, highestAllocation) - pageBytes);
        {
            DWORD oldProtect;
            const BOOL resultVirtualProtect = VirtualProtect(highestAllocationMeta, sizeof(*highestAllocationMeta), PAGE_READWRITE, &oldProtect);
            if(!resultVirtualProtect)
                korl_logLastError("VirtualProtect failed!");
            korl_assert(oldProtect == PAGE_NOACCESS);
        }
        const u$ highestAllocationPages = ((highestAllocationMeta->bytes + (pageBytes - 1)) / pageBytes);// _excluding_ the meta data page
        allocator->nextAllocationAddress = KORL_C_CAST(u8*, highestAllocation) + highestAllocationPages*pageBytes;
        {
            DWORD oldProtect;
            korl_assert(VirtualProtect(highestAllocationMeta, sizeof(*highestAllocationMeta), PAGE_NOACCESS, &oldProtect));
            korl_assert(oldProtect == PAGE_READWRITE);
        }
    }
    /* protect the allocator pages until the time comes to actually use it */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
}
korl_internal void* _korl_memory_allocator_linear_reallocate(void* allocatorUserData, void* allocation, u$ bytes, const wchar_t* file, int line)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocatorUserData);
    /* if allocation is NULL, just call `allocate` */
    if(allocation == NULL)
        return _korl_memory_allocator_linear_allocate(allocator, bytes, file, line, NULL/*automatically find address*/);
    /* release the protection on the allocator pages */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    const u$ pageBytes = korl_memory_pageBytes();
    korl_assert(allocator->bytes % pageBytes == 0);
    const u$ allocatorPages = (sizeof(*allocator) + (pageBytes - 1)) / pageBytes;
    /* ensure that this address is actually within the allocator's range */
    korl_assert(KORL_C_CAST(u8*, allocation) >= KORL_C_CAST(u8*, allocator) + (allocatorPages * pageBytes)
             && KORL_C_CAST(u8*, allocation) <  KORL_C_CAST(u8*, allocator) + allocator->bytes);
    /* ensure that the allocation address is divisible by the page size, because 
        this is currently a constraint of this allocator */
    korl_assert(KORL_C_CAST(u$, allocation) % pageBytes == 0);
    /* ensure that the offset of this address is recorded in the allocator */
    /* additionally, determine if this allocation is the LAST allocation 
        (in other words, determine if we can freely expand into higher addresses) */
    const u32 allocationOffset = korl_checkCast_i$_to_u32(KORL_C_CAST(u8*, allocation) - KORL_C_CAST(u8*, allocator));
    u32 allocationOffsetIndex = KORL_MEMORY_POOL_SIZE(allocator->allocationOffsets);
    bool isLastAllocation = true;
    for(u32 i = 0; i < KORL_MEMORY_POOL_SIZE(allocator->allocationOffsets); ++i)
    {
        if(allocator->allocationOffsets[i] == allocationOffset)
            allocationOffsetIndex = i;
        if(allocator->allocationOffsets[i] > allocationOffset)
            isLastAllocation = false;
    }
    korl_assert(allocationOffsetIndex < KORL_MEMORY_POOL_SIZE(allocator->allocationOffsets));
    /* if bytes is 0, just call `free` & return NULL */
    if(bytes == 0)
    {
        /* conform to our API requirement that the allocator's memory must be 
            protected before each call */
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
        /* now we can just free & return */
        _korl_memory_allocator_linear_free(allocatorUserData, allocation, file, line);
        return NULL;
    }
    // get the address of the allocation's metadata page //
    Korl_Memory_AllocationMeta*const allocationMeta = 
        KORL_C_CAST(Korl_Memory_AllocationMeta*, KORL_C_CAST(u8*, allocation) - pageBytes);
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
    if(!isLastAllocation)
    {
        // JUST the number of pages used by the allocation itself, excluding the 
        //  meta data, etc... //
        const u$ allocationPages = (allocationMeta->bytes + (pageBytes - 1)) / pageBytes;
        /* if we have enough room within the allocation's address range */
        if(allocationPages*pageBytes >= bytes)
            /* in-place allocation size modification code runs in the `done` section */
            goto done;
        /* we don't have enough room in the current allocation's pages - we need 
            to make a new allocation! */
        else
        {
            const u$ oldAllocationBytes = allocationMeta->bytes;
            //KORL-ISSUE-000-000-031: copypasta
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
            void*const newAllocation = _korl_memory_allocator_linear_allocate(allocator, bytes, file, line, NULL/*automatically find address*/);
            if(!newAllocation)
                korl_log(WARNING, "failed to create a new allocation");
            else
            {
                /* copy the old allocation's data to the new allocation 
                    - we can use CopyMemory here because we already know that 
                      the memory regions do NOT overlap */
                CopyMemory(newAllocation, allocation, oldAllocationBytes);
                /* free the old allocation */
                _korl_memory_allocator_linear_free(allocator, allocation, file, line);
            }
            return newAllocation;
        }
    }
    /* if we ARE the last allocation, we can just grow it as much as we want, as 
        long as the final size will fit in the allocator's virtual address range */
    else
    {
        /* determine if the new allocation size will fit in the allocator */
        const u$ allocationPages = ((bytes + (pageBytes - 1)) / pageBytes) + 1;// +1 for an additional preceding NOACCESS page
        const u8*const allocatorEnd = KORL_C_CAST(u8*, allocator) + allocator->bytes;
        /* if we don't have enough room to grow, we can effectively just abort 
            here without changing anything internally */
        if(KORL_C_CAST(u8*, allocationMeta) + allocationPages*pageBytes > allocatorEnd)
        {
            allocation = NULL;
            korl_log(WARNING, "failed to grow the allocation - not enough space");
            goto done;
        }
        /* ensure that if the allocation shrinks, the unused pages are PROTECTED */
        const u$ allocationPagesOld = ((allocationMeta->bytes + (pageBytes - 1)) / pageBytes) + 1;// +1 for an additional preceding NOACCESS page
        {
            DWORD oldProtect;
            if(!VirtualProtect(allocationMeta, allocationPagesOld * pageBytes, PAGE_NOACCESS, &oldProtect))
                korl_logLastError("VirtualProtect failed!");
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
        const u$ allocationPages = ((bytes + (pageBytes - 1)) / pageBytes);
        /* fill allocated/freed space with known bit patterns */
        if(allocationMeta->bytes < bytes)// we're gaining space
            /* fill the newly used space with zeroes */
            ZeroMemory(KORL_C_CAST(u8*, allocation) + allocationMeta->bytes, bytes - allocationMeta->bytes);
        else if(allocationMeta->bytes > bytes)// we're losing space
            /* fill the now unused space with a known bit pattern */
            FillMemory(KORL_C_CAST(u8*, allocation) + bytes, allocationPages*pageBytes - bytes, _KORL_MEMORY_INVALID_BYTE_PATTERN);
        /* update the allocation's metadata */
        allocationMeta->bytes = bytes;
        allocationMeta->file  = file;
        allocationMeta->line  = line;
        /* we need to update the allocator's next allocation address if we're 
            the last allocation */ 
        if(isLastAllocation)
            allocator->nextAllocationAddress = KORL_C_CAST(u8*, allocation) + allocationPages*pageBytes;
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
korl_internal void _korl_memory_allocator_linear_empty(void* allocatorUserData)
{
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocatorUserData);
    const u$ pageBytes = korl_memory_pageBytes();
    /* release the protection on the allocator pages */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    /* eliminate all allocations */
    for(u$ a = 0; a < KORL_MEMORY_POOL_SIZE(allocator->allocationOffsets); a++)
    {
        /* VirtualProtect the region of memory occupied by each allocation to 
            prevent anything from being able to access it normally ever again! */
        /* get the allocation's meta data address */
        Korl_Memory_AllocationMeta*const allocationMeta = 
            KORL_C_CAST(Korl_Memory_AllocationMeta*, 
                        KORL_C_CAST(u8*, allocator) + allocator->allocationOffsets[a] - pageBytes);
        /* release the protection of the meta data page */
        {
            DWORD oldProtect;
            korl_assert(VirtualProtect(allocationMeta, sizeof(*allocationMeta), PAGE_READWRITE, &oldProtect));
            korl_assert(oldProtect == PAGE_NOACCESS);
        }
        /* calculate how many bytes the allocation occupies in TOTAL 
            (content + meta data) */
        const u$ allocationTotalBytes = pageBytes + allocationMeta->bytes;
        /* VirtualProtect this entire region of memory, since these pages must 
            be comitted to memory! */
        {
            DWORD oldProtect;
            korl_assert(VirtualProtect(allocationMeta, allocationTotalBytes, PAGE_NOACCESS, &oldProtect));
            korl_assert(oldProtect == PAGE_READWRITE);
        }
    }
    KORL_MEMORY_POOL_EMPTY(allocator->allocationOffsets);
    /* update the allocator metrics */
    const u$ allocatorPages = (sizeof(*allocator) + (pageBytes - 1)) / pageBytes;
    allocator->nextAllocationAddress = KORL_C_CAST(u8*, allocator) + allocatorPages*pageBytes;
    /* protect the allocator pages once again */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(_korl_memory_allocator_linear_enumerateAllocations)
{
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocatorUserData);
    const u$ pageBytes = korl_memory_pageBytes();
    /* release the protection on the allocator pages */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect));
        korl_assert(oldProtect == PAGE_NOACCESS);
    }
    /**/
    if(out_allocatorVirtualAddressEnd)
        *out_allocatorVirtualAddressEnd = KORL_C_CAST(u8*, allocator) + allocator->bytes;
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(allocator->allocationOffsets); a++)
    {
        const void*const allocation = KORL_C_CAST(u8*, allocator) + allocator->allocationOffsets[a];
        /* get the allocation's meta data address */
        Korl_Memory_AllocationMeta*const allocationMeta = KORL_C_CAST(Korl_Memory_AllocationMeta*, KORL_C_CAST(u8*, allocation) - pageBytes);
        /* release the protection of the meta data page */
        {
            DWORD oldProtect;
            korl_assert(VirtualProtect(allocationMeta, sizeof(*allocationMeta), PAGE_READWRITE, &oldProtect));
            korl_assert(oldProtect == PAGE_NOACCESS);
        }
        /**/
        callback(callbackUserData, allocation, allocationMeta);
        /* protect the allocation's metadata page */
        {
            DWORD oldProtect;
            korl_assert(VirtualProtect(allocationMeta, sizeof(*allocationMeta), PAGE_NOACCESS, &oldProtect));
            korl_assert(oldProtect == PAGE_READWRITE);
        }
    }
    /* protect the allocator pages once again */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
}
korl_internal void* _korl_memory_allocator_createLinear(u$ maxBytes, void* address)
{
    void* result = NULL;
    korl_assert(maxBytes > 0);
    korl_assert(maxBytes >= sizeof(_Korl_Memory_AllocatorLinear));
    /* calculate how many pages we need in virtual address space to satisfy maxBytes */
    const u$ pageCount = (maxBytes + (korl_memory_pageBytes() - 1)) / korl_memory_pageBytes();
    const u$ pageBytes = pageCount * korl_memory_pageBytes();
    /* reserve all these pages */
    LPVOID resultVirtualAlloc = 
        VirtualAlloc(address, pageBytes, MEM_RESERVE, PAGE_NOACCESS);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    result = resultVirtualAlloc;
    /* commit the first pages for the allocator itself */
    resultVirtualAlloc = 
        VirtualAlloc(result, sizeof(_Korl_Memory_AllocatorLinear), MEM_COMMIT, PAGE_READWRITE);
    if(resultVirtualAlloc == NULL)
        korl_logLastError("VirtualAlloc failed!");
    /* initialize the memory of the allocator userData */
    _Korl_Memory_AllocatorLinear*const allocator = KORL_C_CAST(_Korl_Memory_AllocatorLinear*, resultVirtualAlloc);
    korl_memory_zero(allocator, sizeof(*allocator));
    const u$ allocatorPages = (sizeof(*allocator) + (korl_memory_pageBytes() - 1)) / korl_memory_pageBytes();
    allocator->bytes                 = pageBytes;
    allocator->nextAllocationAddress = KORL_C_CAST(u8*, allocator) + allocatorPages*korl_memory_pageBytes();
    /* protect the allocator pages until the time comes to actually use it */
    {
        DWORD oldProtect;
        korl_assert(VirtualProtect(allocator, sizeof(*allocator), PAGE_NOACCESS, &oldProtect));
        korl_assert(oldProtect == PAGE_READWRITE);
    }
    /**/
    return result;
}
korl_internal void _korl_memory_allocator_linear_destroy(void* allocatorUserData)
{
    if(!VirtualFree(allocatorUserData, 0/*release everything*/, MEM_RELEASE))
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
        newAllocator->userData = _korl_memory_allocator_createLinear(maxBytes, address);
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
        allocator->userData = _korl_memory_allocator_createLinear(allocator->maxBytes, newAddress);
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
        return _korl_memory_allocator_linear_allocate(allocator->userData, bytes, file, line, address);}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return NULL;
}
korl_internal KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(korl_memory_allocator_reallocate)
{
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
        return _korl_memory_allocator_linear_reallocate(allocator->userData, allocation, bytes, file, line);}
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
        _korl_memory_allocator_linear_free(allocator->userData, allocation, file, line);
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
        _korl_memory_allocator_linear_empty(allocator->userData);
        return;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
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
            _korl_memory_allocator_linear_empty(allocator->userData);
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
        callback(callbackUserData, allocator, allocator->userData, allocator->name, allocator->flags);
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
