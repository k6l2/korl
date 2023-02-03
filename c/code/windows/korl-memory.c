#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-windows-globalDefines.h"
#include "korl-checkCast.h"
#include "korl-log.h"
#include "korl-stb-ds.h"
#include "korl-heap.h"
#define _KORL_MEMORY_INVALID_BYTE_PATTERN 0xFE
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
typedef struct _Korl_Memory_RawString
{
    size_t hash;
    acu16  data;
} _Korl_Memory_RawString;
typedef struct _Korl_Memory_Context
{
    SYSTEM_INFO systemInfo;
    DWORD mainThreadId;
    Korl_Memory_AllocatorHandle allocatorHandle;                 // used for storing memory reports
    Korl_Memory_AllocatorHandle allocatorHandlePersistentStrings;// used for cold storage of __FILEW__ strings; we use a separate allocator here to guarantee that the addresses of strings added to the pool remain _constant_ in the event that the character pool allocation changes page size during reallocation
    struct _Korl_Memory_Report* report;// the last generated memory report
    KORL_MEMORY_POOL_DECLARE(_Korl_Memory_Allocator, allocators, 64);
    /* Although it would be more convenient to do so, it is not practical to 
        just store __FILEW__ pointers directly in allocation meta data for the 
        following reasons:
        - if the code module is unloaded at run-time (like if we're 
          hot-reloading the game module), the data segment storing the file name 
          strings can potentially get unloaded, causing random crashes!
        - if we load a savestate, the file name string data is transient, so we 
          need a place to store them!  
        Ergo, we will accumulate all file name strings */
    u16* stbDaFileNameCharacterPool;             // Although we _could_ use the StringPool module here, I want to try and minimize the performance impact since korl-memory code will be hitting this data a _lot_
    _Korl_Memory_RawString* stbDaFileNameStrings;// Although we _could_ use the StringPool module here, I want to try and minimize the performance impact since korl-memory code will be hitting this data a _lot_
    u$ stringHashKorlMemory;
} _Korl_Memory_Context;
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
    _Korl_Memory_ReportAllocationMetaData* stbDaAllocationMeta;
    u$ totalUsedBytes;
    const void* virtualAddressStart;
    const void* virtualAddressEnd;
    Korl_Memory_AllocatorType allocatorType;
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
#define _KORL_MEMORY_U$_BITS               ((sizeof (u$)) * 8)
#define _KORL_MEMORY_ROTATE_LEFT(val, n)   (((val) << (n)) | ((val) >> (_KORL_MEMORY_U$_BITS - (n))))
#define _KORL_MEMORY_ROTATE_RIGHT(val, n)  (((val) >> (n)) | ((val) << (_KORL_MEMORY_U$_BITS - (n))))
/** This is a modified version of `stbds_hash_string` from `stb_ds.h` */
korl_internal u$ _korl_memory_hashString(const wchar_t* rawWideString)
{
    korl_shared_const u$ _KORL_MEMORY_STRING_HASH_SEED = 0x3141592631415926;
    u$ hash = _KORL_MEMORY_STRING_HASH_SEED;
    while (*rawWideString)
        hash = _KORL_MEMORY_ROTATE_LEFT(hash, 9) + *rawWideString++;
    // Thomas Wang 64-to-32 bit mix function, hopefully also works in 32 bits
    hash ^= _KORL_MEMORY_STRING_HASH_SEED;
    hash = (~hash) + (hash << 18);
    hash ^= hash ^ _KORL_MEMORY_ROTATE_RIGHT(hash,31);
    hash = hash * 21;
    hash ^= hash ^ _KORL_MEMORY_ROTATE_RIGHT(hash,11);
    hash += (hash << 6);
    hash ^= _KORL_MEMORY_ROTATE_RIGHT(hash,22);
    return hash + _KORL_MEMORY_STRING_HASH_SEED;
}
korl_internal void korl_memory_initialize(void)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_memory_zero(context, sizeof(*context));
    GetSystemInfo(&context->systemInfo);// _VERY_ important; must be run before almost everything in the KORL platform layer
    context->mainThreadId                     = GetCurrentThreadId();
    context->allocatorHandle                  = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_megabytes(1), L"korl-memory"            , KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL/*let platform choose address*/);
    context->allocatorHandlePersistentStrings = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR , korl_math_megabytes(1), L"korl-memory-fileStrings", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL/*let platform choose address*/);
    context->stringHashKorlMemory             = _korl_memory_hashString(__FILEW__);// _must_ be run before making any dynamic allocations in the korl-memory module
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandlePersistentStrings), context->stbDaFileNameCharacterPool, 1024);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle)                 , context->stbDaFileNameStrings      , 128);
    korl_assert(sizeof(wchar_t) == sizeof(*context->stbDaFileNameCharacterPool));// we are storing __FILEW__ characters in the Windows platform
    /* add the file name string of this file to the beginning of the file name character pool */
    {
        const u$ rawWideStringSize = korl_string_sizeUtf16(__FILEW__) + 1/*null-terminator*/;
        const u$ persistDataStartOffset = arrlenu(context->stbDaFileNameCharacterPool);
        mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandlePersistentStrings), context->stbDaFileNameCharacterPool, arrlenu(context->stbDaFileNameCharacterPool) + rawWideStringSize);
        wchar_t*const persistDataStart = context->stbDaFileNameCharacterPool + persistDataStartOffset;
        korl_assert(korl_checkCast_u$_to_i$(rawWideStringSize) == korl_string_copyUtf16(__FILEW__, (au16){rawWideStringSize, persistDataStart}));
    }
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
    return _korl_memory_context.systemInfo.dwPageSize;
}
korl_internal u$ korl_memory_virtualAlignBytes(void)
{
    return _korl_memory_context.systemInfo.dwPageSize;
}
korl_internal bool korl_memory_isLittleEndian(void)
{
    return !_korl_memory_isBigEndian();
}
//KORL-ISSUE-000-000-029: pull out platform-agnostic code
korl_internal KORL_FUNCTION_korl_memory_compare(korl_memory_compare)
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
korl_internal KORL_FUNCTION_korl_memory_arrayU8Compare(korl_memory_arrayU8Compare)
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
korl_internal KORL_FUNCTION_korl_memory_arrayU16Compare(korl_memory_arrayU16Compare)
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
/** this is mostly copy-pasta from \c _korl_memory_hashString */
korl_internal KORL_FUNCTION_korl_memory_acu16_hash(korl_memory_acu16_hash)
{
    korl_shared_const u$ _KORL_MEMORY_STRING_HASH_SEED = 0x3141592631415926;
    u$ hash = _KORL_MEMORY_STRING_HASH_SEED;
    for(u$ i = 0; i < arrayCU16.size; i++)
        hash = _KORL_MEMORY_ROTATE_LEFT(hash, 9) + arrayCU16.data[i];
    // Thomas Wang 64-to-32 bit mix function, hopefully also works in 32 bits
    hash ^= _KORL_MEMORY_STRING_HASH_SEED;
    hash = (~hash) + (hash << 18);
    hash ^= hash ^ _KORL_MEMORY_ROTATE_RIGHT(hash,31);
    hash = hash * 21;
    hash ^= hash ^ _KORL_MEMORY_ROTATE_RIGHT(hash,11);
    hash += (hash << 6);
    hash ^= _KORL_MEMORY_ROTATE_RIGHT(hash,22);
    return hash + _KORL_MEMORY_STRING_HASH_SEED;
}
#if 0// still not quite sure if this is needed for anything really...
korl_internal KORL_FUNCTION_korl_memory_fill(korl_memory_fill)
{
    FillMemory(memory, bytes, pattern);
}
#endif
korl_internal KORL_FUNCTION_korl_memory_zero(korl_memory_zero)
{
    SecureZeroMemory(memory, bytes);
}
korl_internal KORL_FUNCTION_korl_memory_copy(korl_memory_copy)
{
    CopyMemory(destination, source, bytes);
}
korl_internal KORL_FUNCTION_korl_memory_move(korl_memory_move)
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
korl_internal void _korl_memory_allocator_linear_allocatorPageGuard(_Korl_Memory_AllocatorLinear* allocator)
{
    DWORD oldProtect;
    if(!VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE | PAGE_GUARD, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == PAGE_READWRITE);
}
korl_internal void _korl_memory_allocator_linear_allocatorPageUnguard(_Korl_Memory_AllocatorLinear* allocator)
{
    DWORD oldProtect;
    if(!VirtualProtect(allocator, sizeof(*allocator), PAGE_READWRITE, &oldProtect))
        korl_logLastError("VirtualProtect failed!");
    korl_assert(oldProtect == (PAGE_READWRITE | PAGE_GUARD));
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
    const bool resultRecurse = _korl_memory_allocator_linear_enumerateAllocationsRecurse(allocator, allocationMeta->previousAllocation, callback, callbackUserData);
    /* perform the enumeration callback after the stack is drained */
    //KORL-ISSUE-000-000-080: memory: _korl_memory_allocator_linear_enumerateAllocations can't properly early-out if the enumeration callback returns false
    return callback(callbackUserData, allocation, &(allocationMeta->allocationMeta)) | resultRecurse;
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
    mcarrpush(KORL_STB_DS_MC_CAST(_korl_memory_context.allocatorHandle), context->stbDaAllocationMeta, (_Korl_Memory_ReportAllocationMetaData){0});
    _Korl_Memory_ReportAllocationMetaData*const newAllocMeta = &arrlast(context->stbDaAllocationMeta);
    newAllocMeta->allocationAddress = allocation;
    newAllocMeta->meta              = *meta;
    context->totalUsedBytes += meta->bytes;
    return true;// true => continue enumerating
}
korl_internal KORL_FUNCTION_korl_memory_allocator_create(korl_memory_allocator_create)
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
        if(0 == korl_string_compareUtf16(context->allocators[a].name, allocatorName))
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
        newAllocator->userData = korl_heap_general_create(maxBytes, address);
        break;}
    default:{
        korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", type);
        break;}
    }
    korl_string_copyUtf16(allocatorName, (au16){korl_arraySize(newAllocator->name), newAllocator->name});
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
        korl_heap_general_destroy(allocator->userData);
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
        korl_heap_general_destroy(allocator->userData);
        allocator->userData = korl_heap_general_create(allocator->maxBytes, newAddress);
        break;}
    default:{
        korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
        break;}
    }
}
korl_internal const wchar_t* _korl_memory_getPersistentString(const wchar_t* rawWideString)
{
    if(!rawWideString)
        return NULL;
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(GetCurrentThreadId() == context->mainThreadId);//KORL-ISSUE-000-000-082: memory: _korl_memory_getPersistentString is not thread-safe
    /* if the raw string already exists in our persistent storage, let's use it */
    const u$ rawWideStringHash = _korl_memory_hashString(rawWideString);
    if(rawWideStringHash == context->stringHashKorlMemory)
        return context->stbDaFileNameCharacterPool;// to prevent stack overflows when making allocations within the korl-memory module itself, we will guarantee that the first string is _always_ this module's file handle
    for(u$ i = 0; i < arrlenu(context->stbDaFileNameStrings); i++)
        if(rawWideStringHash == context->stbDaFileNameStrings[i].hash)
            return context->stbDaFileNameStrings[i].data.data;
    /* otherwise, we need to add the string to the character pool & create a new 
        string entry to use */
    const u$ rawWideStringSize = korl_string_sizeUtf16(rawWideString) + 1/*null-terminator*/;
    const u$ fileNameCharacterPoolSizePrevious = arrlenu(context->stbDaFileNameCharacterPool);
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandlePersistentStrings), context->stbDaFileNameCharacterPool, arrlenu(context->stbDaFileNameCharacterPool) + rawWideStringSize);
    wchar_t*const persistDataStart = context->stbDaFileNameCharacterPool + fileNameCharacterPoolSizePrevious;
    korl_assert(korl_checkCast_u$_to_i$(rawWideStringSize) == korl_string_copyUtf16(rawWideString, (au16){rawWideStringSize, persistDataStart}));
    const _Korl_Memory_RawString newRawString = { .data = { .data = persistDataStart
                                                          , .size = rawWideStringSize - 1/*ignore the null-terminator*/}
                                                , .hash = rawWideStringHash};
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaFileNameStrings, newRawString);
    return newRawString.data.data;
}
korl_internal KORL_FUNCTION_korl_memory_allocator_allocate(korl_memory_allocator_allocate)
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
    const wchar_t* persistentFileString = _korl_memory_getPersistentString(file);
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        return _korl_memory_allocator_linear_allocate(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData), bytes, persistentFileString, line, address);}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        return korl_heap_general_allocate(KORL_C_CAST(_Korl_Heap_General*, allocator->userData), bytes, persistentFileString, line, address);}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return NULL;
}
korl_internal KORL_FUNCTION_korl_memory_allocator_reallocate(korl_memory_allocator_reallocate)
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
    const wchar_t* persistentFileString = _korl_memory_getPersistentString(file);
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        if(allocation == NULL)
            return _korl_memory_allocator_linear_allocate(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData), bytes, persistentFileString, line, NULL/*automatically find address*/);
        return _korl_memory_allocator_linear_reallocate(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData), allocation, bytes, persistentFileString, line);}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        if(allocation == NULL)
            return korl_heap_general_allocate(KORL_C_CAST(_Korl_Heap_General*, allocator->userData), bytes, persistentFileString, line, NULL/*automatically find address*/);
        return korl_heap_general_reallocate(KORL_C_CAST(_Korl_Heap_General*, allocator->userData), allocation, bytes, persistentFileString, line);}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return NULL;
}
korl_internal KORL_FUNCTION_korl_memory_allocator_free(korl_memory_allocator_free)
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
    const wchar_t* persistentFileString = _korl_memory_getPersistentString(file);
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        _korl_memory_allocator_linear_free(KORL_C_CAST(_Korl_Memory_AllocatorLinear*, allocator->userData), allocation, persistentFileString, line);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        korl_heap_general_free(KORL_C_CAST(_Korl_Heap_General*, allocator->userData), allocation, persistentFileString, line);
        return;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
}
korl_internal KORL_FUNCTION_korl_memory_allocator_empty(korl_memory_allocator_empty)
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
        korl_heap_general_empty(KORL_C_CAST(_Korl_Heap_General*, allocator->userData));
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
        //KORL-ISSUE-000-000-080: memory: _korl_memory_allocator_linear_enumerateAllocations can't properly early-out if the enumeration callback returns false
        _korl_memory_allocator_linear_enumerateAllocations(allocator, allocator->userData, _korl_memory_allocator_isEmpty_enumAllocationsCallback, &resultIsEmpty, NULL/*we don't care about the virtual address range end*/);
        return resultIsEmpty;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        korl_heap_general_enumerateAllocations(allocator, allocator->userData, _korl_memory_allocator_isEmpty_enumAllocationsCallback, &resultIsEmpty, NULL/*we don't care about the virtual address range end*/);
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
            korl_heap_general_empty(KORL_C_CAST(_Korl_Heap_General*, allocator->userData));
            continue;}
        }
        korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    }
}
korl_internal void* korl_memory_reportGenerate(void)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    /* free the previous report if it exists */
    if(context->report)
    {
        for(u$ i = 0; i < context->report->allocatorCount; i++)
            mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->report->allocatorData[i].stbDaAllocationMeta);
        korl_free(context->allocatorHandle, context->report);
    }
    _Korl_Memory_Report*const report = korl_allocate(context->allocatorHandle, 
                                                     sizeof(*report) - sizeof(report->allocatorData) + KORL_MEMORY_POOL_SIZE(context->allocators)*sizeof(report->allocatorData));
    context->report = report;
    korl_memory_zero(report, sizeof(*report));
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
    {
        _Korl_Memory_Allocator*const allocator = &context->allocators[a];
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
        korl_string_copyUtf16(allocator->name, (au16){korl_arraySize(enumContext->name), enumContext->name});
        enumContext->allocatorType       = allocator->type;
        enumContext->virtualAddressStart = allocator->userData;
        mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), enumContext->stbDaAllocationMeta, 16);
        switch(allocator->type)
        {
        case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
            _korl_memory_allocator_linear_enumerateAllocations(allocator, allocator->userData, _korl_memory_logReport_enumerateCallback, enumContext, &enumContext->virtualAddressEnd);
            break;}
        case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
            korl_heap_general_enumerateAllocations(allocator, allocator->userData, _korl_memory_logReport_enumerateCallback, enumContext, &enumContext->virtualAddressEnd);
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
        const char* allocatorType = NULL;
        switch(enumContext->allocatorType)
        {
        case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR :{ allocatorType = "KORL_MEMORY_ALLOCATOR_TYPE_LINEAR"; break; }
        case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{ allocatorType = "KORL_MEMORY_ALLOCATOR_TYPE_GENERAL"; break; }
        }
        korl_log_noMeta(INFO, "║ %hs {\"%ws\", [0x%p ~ 0x%p], total used bytes: %llu}", 
                        allocatorType, enumContext->name, enumContext->virtualAddressStart, enumContext->virtualAddressEnd, enumContext->totalUsedBytes);
        for(u$ a = 0; a < arrlenu(enumContext->stbDaAllocationMeta); a++)
        {
            _Korl_Memory_ReportAllocationMetaData*const allocMeta = &enumContext->stbDaAllocationMeta[a];
            const void*const allocAddressEnd = KORL_C_CAST(u8*, allocMeta->allocationAddress) + allocMeta->meta.bytes;
            korl_log_noMeta(INFO, "║ %ws [0x%p ~ 0x%p] %llu bytes, %ws:%i", 
                            a == arrlenu(enumContext->stbDaAllocationMeta) - 1 ? L"└" : L"├", 
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
        korl_heap_general_enumerateAllocations(opaqueAllocator, allocator->userData, callback, callbackUserData, out_allocatorVirtualAddressEnd);
        return;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
}
korl_internal bool korl_memory_allocator_findByName(const wchar_t* name, Korl_Memory_AllocatorHandle* out_allocatorHandle)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
        if(korl_string_compareUtf16(context->allocators[a].name, name) == 0)
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
korl_internal void* korl_memory_fileMapAllocation_create(const Korl_Memory_FileMapAllocation_CreateInfo* createInfo, u$* out_physicalMemoryChunkBytes)
{
    /** system compatibility notice:
     * VirtualAlloc2 & MapViewOfFile3 require Windows 10+ 😟 */
    /* satisfy memory alignment requirements of the requested physical memory chunk bytes; 
        we need to use dwAllocationGranularity here because each resulting 
        virtual alloc region must have a base address that is aligned to this 
        value, including the ones split from a placeholder region */
    korl_assert(0 != _korl_memory_context.systemInfo.dwAllocationGranularity);
    KORL_ZERO_STACK(ULARGE_INTEGER, physicalMemoryChunkBytes);
    physicalMemoryChunkBytes.QuadPart = korl_math_roundUpPowerOf2(createInfo->physicalMemoryChunkBytes, _korl_memory_context.systemInfo.dwAllocationGranularity);
    /* reserve a placeholder region in virtual memory */
    void*const virtualAddressPlaceholder = VirtualAlloc2(/*process*/NULL, /*baseAddress*/NULL
                                                        ,createInfo->virtualRegionCount*physicalMemoryChunkBytes.QuadPart
                                                        ,MEM_RESERVE | MEM_RESERVE_PLACEHOLDER
                                                        ,PAGE_NOACCESS
                                                        ,/*extendedParameters*/NULL, /*parameterCount*/0);
    if(!virtualAddressPlaceholder)
        korl_logLastError("VirtualAlloc2 failed");
    /* split the placeholder region into the user-requested # of regions */
    for(u16 v = createInfo->virtualRegionCount - 1; v > 0; v--)
        if(!VirtualFree(KORL_C_CAST(u8*, virtualAddressPlaceholder) + v*physicalMemoryChunkBytes.QuadPart
                       ,physicalMemoryChunkBytes.QuadPart
                       ,MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
            korl_logLastError("VirtualFree failed");
    /* create pagefile-backed chunks of physical memory */
    KORL_ZERO_STACK_ARRAY(HANDLE, physicalChunkHandles, 8);
    korl_assert(createInfo->physicalRegionCount <= korl_arraySize(physicalChunkHandles));
    for(u16 p = 0; p < createInfo->physicalRegionCount; p++)
    {
        physicalChunkHandles[p] = CreateFileMapping(/*hFile; invalid=>pageFile*/INVALID_HANDLE_VALUE
                                                   ,/*fileMappingAttributes*/NULL
                                                   ,PAGE_READWRITE
                                                   ,physicalMemoryChunkBytes.HighPart
                                                   ,physicalMemoryChunkBytes.LowPart
                                                   ,/*name*/NULL);
        if(NULL == physicalChunkHandles[p])
            korl_logLastError("CreateFileMapping failed");
    }
    /* map the virtual memory regions into the physical memory regions */
    for(u16 v = 0; v < createInfo->virtualRegionCount; v++)
    {
        void*const view = MapViewOfFile3(physicalChunkHandles[createInfo->virtualRegionMap[v]]
                                        ,/*process*/NULL
                                        ,KORL_C_CAST(u8*, virtualAddressPlaceholder) + v*physicalMemoryChunkBytes.QuadPart
                                        ,/*offset*/0
                                        ,physicalMemoryChunkBytes.QuadPart
                                        ,MEM_REPLACE_PLACEHOLDER
                                        ,PAGE_READWRITE
                                        ,/*extendedParams*/NULL, /*paramCount*/0);
        if(!view)
            korl_logLastError("MapViewOfFile3 failed");
    }
    *out_physicalMemoryChunkBytes = physicalMemoryChunkBytes.QuadPart;
    return virtualAddressPlaceholder;
}
korl_internal void korl_memory_fileMapAllocation_destroy(void* allocation, u$ bytesPerRegion, u16 regionCount)
{
    for(u16 r = 0; r < regionCount; r++)
        if(!UnmapViewOfFile(KORL_C_CAST(u8*, allocation) + r*bytesPerRegion))
            korl_logLastError("UnmapViewOfFile failed");
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
