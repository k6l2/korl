#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-windows-globalDefines.h"
#include "utility/korl-checkCast.h"
#include "korl-log.h"
#include "korl-stb-ds.h"
#include "korl-heap.h"
#include "utility/korl-utility-string.h"
#if KORL_DEBUG
    // #define _KORL_MEMORY_DEBUG_HEAP_UNIT_TESTS
#endif
#define _KORL_MEMORY_MAX_ALLOCATORS 64
typedef struct _Korl_Memory_Allocator
{
    void* userData;// points to the start address of the actual specialized allocator virtual memory arena (linear, general, etc...)
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
    u8* stbDaReportData;// the last generated memory report
    KORL_MEMORY_POOL_DECLARE(_Korl_Memory_Allocator, allocators, _KORL_MEMORY_MAX_ALLOCATORS);
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
typedef struct _Korl_Memory_ReportMeta_Heap
{
    const void* virtualAddressStart;
    const void* virtualAddressEnd;
} _Korl_Memory_ReportMeta_Heap;
typedef struct _Korl_Memory_ReportMeta_Allocation
{
    const void* allocationAddress;
    Korl_Memory_AllocationMeta meta;
} _Korl_Memory_ReportMeta_Allocation;
typedef struct _Korl_Memory_ReportMeta_Allocator
{
    Korl_Memory_AllocatorType  allocatorType;
    Korl_Memory_AllocatorFlags flags;
    wchar_t                    name[32];
    struct
    {
        u$ byteOffset;// relative to the first byte of the entire report
        u$ size;
    } heaps;// raw report data containing an array of `_Korl_Memory_ReportMeta_Heap`
    struct
    {
        u$ byteOffset;// relative to the first byte of the entire report
        u$ size;
    } allocations;// raw report data containing an array of `_Korl_Memory_ReportMeta_Allocation`
} _Korl_Memory_ReportMeta_Allocator;
typedef struct _Korl_Memory_ReportMeta
{
    KORL_MEMORY_POOL_DECLARE(_Korl_Memory_ReportMeta_Allocator, allocatorMeta, _KORL_MEMORY_MAX_ALLOCATORS);
} _Korl_Memory_ReportMeta;
korl_global_variable _Korl_Memory_Context _korl_memory_context;
korl_internal void korl_memory_initialize(void)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_memory_zero(context, sizeof(*context));
    GetSystemInfo(&context->systemInfo);// _VERY_ important; must be run before almost everything in the KORL platform layer
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(1);
    context->mainThreadId                     = GetCurrentThreadId();
    context->allocatorHandle                  = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, L"korl-memory"            , KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    context->allocatorHandlePersistentStrings = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR , L"korl-memory-fileStrings", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    context->stringHashKorlMemory             = korl_string_hashRawWide(__FILEW__, 10'000);// _must_ be run before making any dynamic allocations in the korl-memory module
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandlePersistentStrings), context->stbDaFileNameCharacterPool, 1024);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle)                 , context->stbDaFileNameStrings      , 128);
    korl_assert(sizeof(wchar_t) == sizeof(*context->stbDaFileNameCharacterPool));// we are storing __FILEW__ characters in the Windows platform
    /* add the file name string of this file to the beginning of the file name character pool */
    {
        const u$ rawWideStringSize = korl_string_sizeUtf16(__FILEW__, KORL_DEFAULT_C_STRING_SIZE_LIMIT) + 1/*null-terminator*/;
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
    #ifdef _KORL_MEMORY_DEBUG_HEAP_UNIT_TESTS
    {
        heapCreateInfo.initialHeapBytes = context->systemInfo.dwPageSize*4;
        u8* testAllocs[3];
        Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, L"_korl-memory-test-general", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
        // expected allocator page occupied flags: 0b000
        testAllocs[0] = korl_allocate(allocator, 32);
        // expected allocator page occupied flags: 0b010
        testAllocs[1] = korl_allocate(allocator, context->systemInfo.dwPageSize);
        // expected allocator page occupied flags: 0b010 0b011000
        testAllocs[2] = korl_allocate(allocator, context->systemInfo.dwPageSize + 1);
        // expected allocator page occupied flags: 0b010 0b011011
        korl_free(allocator, testAllocs[0]); testAllocs[0] = NULL;
        // expected allocator page occupied flags: 0b000 0b011011
        testAllocs[1] = korl_reallocate(allocator, testAllocs[1], context->systemInfo.dwPageSize*3);// test realloc (move)
        // expected allocator page occupied flags: 0b000 0b000011 0b011110000000
        testAllocs[1] = korl_reallocate(allocator, testAllocs[1], 32);// test realloc (shrink)
        // expected allocator page occupied flags: 0b000 0b000011 0b010000000000
        korl_memory_allocator_empty(allocator); korl_memory_zero(testAllocs, sizeof(testAllocs));
        // expected allocator page occupied flags: 0b000 0b000000 0b000000000000
        korl_memory_allocator_destroy(allocator);
        allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"_korl-memory-test-linear", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
        // expected allocator page occupied flags: 0b000
        testAllocs[0] = korl_allocate(allocator, 32);
        // expected allocator page occupied flags: 0b010
        testAllocs[1] = korl_allocate(allocator, 32);
        // expected allocator page occupied flags: 0b010 0b0100000
        testAllocs[0] = korl_reallocate(allocator, testAllocs[0], context->systemInfo.dwPageSize*2);
        // expected allocator page occupied flags: 0b000 0b0101110
        korl_free(allocator, testAllocs[0]); testAllocs[0] = NULL;
        // expected allocator page occupied flags: 0b000 0b0100000
        testAllocs[0] = korl_allocate(allocator, context->systemInfo.dwPageSize*3);
        // expected allocator page occupied flags: 0b000 0b0101111
        korl_memory_allocator_empty(allocator); korl_memory_zero(testAllocs, sizeof(testAllocs));
        korl_memory_allocator_destroy(allocator);
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
korl_internal KORL_FUNCTION_korl_memory_compare(korl_memory_compare)
{
    const SIZE_T bytesMatched = RtlCompareMemory(a, b, bytes);
    return bytesMatched >= bytes ? 0 
           : KORL_C_CAST(const u8*, a)[bytesMatched] < KORL_C_CAST(const u8*, b)[bytesMatched] 
             ? -1 : 1;
}
#if 0// still not quite sure if this is needed for anything really...
korl_internal KORL_FUNCTION_korl_memory_fill(korl_memory_fill)
{
    FillMemory(memory, bytes, pattern);
}
#endif
korl_internal void* korl_memory_set(void* target, u8 value, u$ bytes)
{
    FillMemory(target, bytes, value);
    return target;
}
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
        if(0 == korl_string_compareUtf16(context->allocators[a].name, allocatorName, KORL_DEFAULT_C_STRING_SIZE_LIMIT))
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
    switch(type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        korl_assert(!(flags & KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE));// we can't support this feature, as it requires the allocator provides a complete & contiguous virtual byte range
        newAllocator->userData = korl_heap_crt_create(heapCreateInfo);
        break;}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        newAllocator->userData = korl_heap_linear_create(heapCreateInfo);
        break;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        newAllocator->userData = korl_heap_general_create(heapCreateInfo);
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
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        korl_heap_crt_destroy(allocator->userData);
        break;}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        korl_heap_linear_destroy(allocator->userData);
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
korl_internal void korl_memory_allocator_recreate(Korl_Memory_AllocatorHandle handle
                                                 ,u$ heapDescriptorCount, const void* heapDescriptors
                                                 ,u$ heapDescriptorStride
                                                 ,u32 heapDescriptorOffset_virtualAddressStart
                                                 ,u32 heapDescriptorOffset_virtualBytes
                                                 ,u32 heapDescriptorOffset_committedBytes)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(GetCurrentThreadId() == context->mainThreadId);
    Korl_MemoryPool_Size a = 0;
    for(; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
        if(context->allocators[a].handle == handle)
            break;
    korl_assert(a < KORL_MEMORY_POOL_SIZE(context->allocators));
    _Korl_Memory_Allocator*const allocator = &context->allocators[a];
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.heapDescriptorCount                      = korl_checkCast_u$_to_u32(heapDescriptorCount);
    heapCreateInfo.heapDescriptors                          = heapDescriptors;
    heapCreateInfo.heapDescriptorStride                     = korl_checkCast_u$_to_u32(heapDescriptorStride);
    heapCreateInfo.heapDescriptorOffset_virtualAddressStart = heapDescriptorOffset_virtualAddressStart;
    heapCreateInfo.heapDescriptorOffset_virtualBytes        = heapDescriptorOffset_virtualBytes;
    heapCreateInfo.heapDescriptorOffset_committedBytes      = heapDescriptorOffset_committedBytes;
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        korl_heap_crt_destroy(allocator->userData);
        allocator->userData = korl_heap_crt_create(&heapCreateInfo);
        break;}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        korl_heap_linear_destroy(allocator->userData);
        allocator->userData = korl_heap_linear_create(&heapCreateInfo);
        break;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        korl_heap_general_destroy(allocator->userData);
        allocator->userData = korl_heap_general_create(&heapCreateInfo);
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
    const u$ rawWideStringHash = korl_string_hashRawWide(rawWideString, 10'000);
    if(rawWideStringHash == context->stringHashKorlMemory)
        return context->stbDaFileNameCharacterPool;// to prevent stack overflows when making allocations within the korl-memory module itself, we will guarantee that the first string is _always_ this module's file handle
    for(u$ i = 0; i < arrlenu(context->stbDaFileNameStrings); i++)
        if(rawWideStringHash == context->stbDaFileNameStrings[i].hash)
            return context->stbDaFileNameStrings[i].data.data;
    /* otherwise, we need to add the string to the character pool & create a new 
        string entry to use */
    const u$ rawWideStringSize = korl_string_sizeUtf16(rawWideString, KORL_DEFAULT_C_STRING_SIZE_LIMIT) + 1/*null-terminator*/;
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
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        return korl_heap_crt_allocate(allocator->userData, allocator->name, bytes, persistentFileString, line, fastAndDirty, byteAlignment);}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        return korl_heap_linear_allocate(allocator->userData, allocator->name, bytes, persistentFileString, line, fastAndDirty, byteAlignment);}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        return korl_heap_general_allocate(allocator->userData, allocator->name, bytes, persistentFileString, line, fastAndDirty, byteAlignment);}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return NULL;
}
korl_internal KORL_FUNCTION_korl_memory_allocator_reallocate(korl_memory_allocator_reallocate)
{
    korl_shared_const u$ byteAlignment = 1;
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
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        if(allocation == NULL)
            return korl_heap_crt_allocate(allocator->userData, allocator->name, bytes, persistentFileString, line, fastAndDirty, byteAlignment);
        return korl_heap_crt_reallocate(allocator->userData, allocator->name, allocation, bytes, persistentFileString, line, fastAndDirty);}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        if(allocation == NULL)
            return korl_heap_linear_allocate(allocator->userData, allocator->name, bytes, persistentFileString, line, fastAndDirty, byteAlignment);
        return korl_heap_linear_reallocate(allocator->userData, allocator->name, allocation, bytes, persistentFileString, line, fastAndDirty);}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        if(allocation == NULL)
            return korl_heap_general_allocate(allocator->userData, allocator->name, bytes, persistentFileString, line, fastAndDirty, byteAlignment);
        return korl_heap_general_reallocate(allocator->userData, allocator->name, allocation, bytes, persistentFileString, line, fastAndDirty);}
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
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        korl_heap_crt_free(allocator->userData, allocation, persistentFileString, line, fastAndDirty);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        korl_heap_linear_free(allocator->userData, allocation, persistentFileString, line, fastAndDirty);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        korl_heap_general_free(allocator->userData, allocation, persistentFileString, line, fastAndDirty);
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
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        korl_heap_crt_empty(allocator->userData);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        korl_heap_linear_empty(allocator->userData);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        korl_heap_general_empty(allocator->userData);
        return;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
}
korl_internal KORL_FUNCTION_korl_memory_allocator_isFragmented(korl_memory_allocator_isFragmented)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    _Korl_Memory_Allocator*const allocator = _korl_memory_allocator_matchHandle(handle);
    korl_assert(allocator);
    if(!(allocator->flags & KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS) && GetCurrentThreadId() != context->mainThreadId)
    {
        korl_log(ERROR, "threadId(%u) != mainThreadId(%u)", GetCurrentThreadId(), context->mainThreadId);
        return false;
    }
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        return korl_heap_linear_isFragmented(KORL_C_CAST(_Korl_Heap_Linear*, allocator->userData));}
    default:{ break; }
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return false;
}
korl_internal KORL_FUNCTION_korl_memory_allocator_defragment(korl_memory_allocator_defragment)
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
        _Korl_Memory_Allocator*const allocatorFrame = _korl_memory_allocator_matchHandle(handleStack);
        korl_assert(allocatorFrame);
        if(!(allocatorFrame->flags & KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS) && GetCurrentThreadId() != context->mainThreadId)
        {
            korl_log(ERROR, "threadId(%u) != mainThreadId(%u)", GetCurrentThreadId(), context->mainThreadId);
            return;
        }
        korl_heap_linear_defragment(KORL_C_CAST(_Korl_Heap_Linear*, allocator->userData), allocator->name, defragmentPointers, defragmentPointersSize, KORL_C_CAST(_Korl_Heap_Linear*, allocatorFrame->userData), allocatorFrame->name, handleStack);
        return;}
    default:{ break; }
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
}
korl_internal KORL_HEAP_ENUMERATE_ALLOCATIONS_CALLBACK(_korl_memory_allocator_isEmpty_enumAllocationsCallback)
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
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        korl_heap_crt_enumerateAllocations(allocator->userData, _korl_memory_allocator_isEmpty_enumAllocationsCallback, &resultIsEmpty);
        return resultIsEmpty;}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        korl_heap_linear_enumerateAllocations(allocator->userData, _korl_memory_allocator_isEmpty_enumAllocationsCallback, &resultIsEmpty);
        return resultIsEmpty;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        korl_heap_general_enumerateAllocations(allocator->userData, _korl_memory_allocator_isEmpty_enumAllocationsCallback, &resultIsEmpty);
        return resultIsEmpty;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return false;
}
korl_internal void korl_memory_allocator_emptyFrameAllocators(void)
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
        case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
            korl_heap_crt_empty(allocator->userData);
            continue;}
        case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
            korl_heap_linear_empty(allocator->userData);
            continue;}
        case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
            korl_heap_general_empty(allocator->userData);
            continue;}
        }
        korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    }
}
typedef struct _Korl_Memory_ReportGenerate_EnumContext
{
    u8**                               stbDaReportData;
    _Korl_Memory_ReportMeta_Allocator* allocatorMeta;
} _Korl_Memory_ReportGenerate_EnumContext;
korl_internal KORL_HEAP_ENUMERATE_CALLBACK(_korl_memory_reportGenerate_enumerateHeapsCallback)
{
    _Korl_Memory_ReportGenerate_EnumContext*const enumContext = KORL_C_CAST(_Korl_Memory_ReportGenerate_EnumContext*, userData);
    const _Korl_Memory_ReportMeta_Heap heapMeta = {.virtualAddressStart = virtualAddressStart
                                                  ,.virtualAddressEnd   = virtualAddressEnd};
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(_korl_memory_context.allocatorHandle), enumContext->stbDaReportData, &heapMeta, sizeof(heapMeta));
    enumContext->allocatorMeta->heaps.size++;
}
korl_internal KORL_HEAP_ENUMERATE_ALLOCATIONS_CALLBACK(_korl_memory_reportGenerate_enumerateAllocationsCallback)
{
    _Korl_Memory_ReportGenerate_EnumContext*const enumContext = KORL_C_CAST(_Korl_Memory_ReportGenerate_EnumContext*, userData);
    const _Korl_Memory_ReportMeta_Allocation allocationMeta = {.allocationAddress = allocation
                                                              ,.meta              = *meta};
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(_korl_memory_context.allocatorHandle), enumContext->stbDaReportData, &allocationMeta, sizeof(allocationMeta));
    enumContext->allocatorMeta->allocations.size++;
    return true;// true => continue enumerating
}
korl_internal void* korl_memory_reportGenerate(void)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    u8* stbDaReportData = NULL;
    /* free the previous report if it exists */
    if(context->stbDaReportData)
        mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaReportData);
    /* because _Korl_Heap data structures can now contain a linked list of 
        heaps, we don't know ahead of time how many heaps each allocator will 
        have; ergo, we will have to enumerate over each allocator & accumulate 
        the following data on the fly in a raw u8 buffer:
        - metadata of each Korl_Heap (virtual memory range)
        - metadata of each allocation (address, file, line, bytes)
        then, at the end of the report, we can place the following data for all 
        allocators in a fixed-size data structure:
        - byte offset of Korl_Heap metadata, & the heap count
        - byte offset of the allocation metadata, & the allocation count
        - remaining allocator data (type, name, etc.) */
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), stbDaReportData, 1024);
    KORL_ZERO_STACK(_Korl_Memory_ReportMeta, reportMeta);
    KORL_MEMORY_POOL_RESIZE(reportMeta.allocatorMeta, KORL_MEMORY_POOL_SIZE(context->allocators));
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
    {
        _Korl_Memory_Allocator*const allocator = &context->allocators[a];
        reportMeta.allocatorMeta[a].allocatorType = allocator->type;
        reportMeta.allocatorMeta[a].flags         = allocator->flags;
        korl_string_copyUtf16(allocator->name, (au16){korl_arraySize(reportMeta.allocatorMeta[a].name), reportMeta.allocatorMeta[a].name});
        KORL_ZERO_STACK(_Korl_Memory_ReportGenerate_EnumContext, enumContext);
        enumContext.stbDaReportData = &stbDaReportData;
        enumContext.allocatorMeta   = &reportMeta.allocatorMeta[a];
        switch(allocator->type)
        {
        case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
            enumContext.allocatorMeta->heaps.byteOffset = arrlenu(stbDaReportData);
            korl_heap_crt_enumerate(allocator->userData, _korl_memory_reportGenerate_enumerateHeapsCallback, &enumContext);
            enumContext.allocatorMeta->allocations.byteOffset = arrlenu(stbDaReportData);
            korl_heap_crt_enumerateAllocations(allocator->userData, _korl_memory_reportGenerate_enumerateAllocationsCallback, &enumContext);
            break;}
        case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
            enumContext.allocatorMeta->heaps.byteOffset = arrlenu(stbDaReportData);
            korl_heap_linear_enumerate(allocator->userData, _korl_memory_reportGenerate_enumerateHeapsCallback, &enumContext);
            enumContext.allocatorMeta->allocations.byteOffset = arrlenu(stbDaReportData);
            korl_heap_linear_enumerateAllocations(allocator->userData, _korl_memory_reportGenerate_enumerateAllocationsCallback, &enumContext);
            break;}
        case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
            enumContext.allocatorMeta->heaps.byteOffset = arrlenu(stbDaReportData);
            korl_heap_general_enumerate(allocator->userData, _korl_memory_reportGenerate_enumerateHeapsCallback, &enumContext);
            enumContext.allocatorMeta->allocations.byteOffset = arrlenu(stbDaReportData);
            korl_heap_general_enumerateAllocations(allocator->userData, _korl_memory_reportGenerate_enumerateAllocationsCallback, &enumContext);
            break;}
        default:{
            korl_log(ERROR, "unknown allocator type '%i' not implemented", allocator->type);
            break;}
        }
        //KORL-ISSUE-000-000-066: memory: sort report allocations by ascending address
    }
    korl_stb_ds_arrayAppendU8(KORL_STB_DS_MC_CAST(context->allocatorHandle), &stbDaReportData, &reportMeta, sizeof(reportMeta));
    context->stbDaReportData = stbDaReportData;// for now, we just set the singleton report in the context to be this report
    return stbDaReportData;
}
korl_internal void korl_memory_reportLog(void* reportAddress)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    const u8* stbDaReportData = KORL_C_CAST(u8*, reportAddress);
    const _Korl_Memory_ReportMeta*const reportMeta = KORL_C_CAST(_Korl_Memory_ReportMeta*, stbDaReportData + arrlen(stbDaReportData)) - 1;
    korl_log_noMeta(INFO, "╔════ 🧠 Memory Report ════════════════════════════════╗");
    for(u$ a = 0; a < KORL_MEMORY_POOL_SIZE(reportMeta->allocatorMeta); a++)
    {
        const _Korl_Memory_ReportMeta_Allocator*const  allocatorMeta     = &reportMeta->allocatorMeta[a];
        const _Korl_Memory_ReportMeta_Heap*const       heapMeta          = KORL_C_CAST(_Korl_Memory_ReportMeta_Heap*      , stbDaReportData + allocatorMeta->heaps.byteOffset);
        const _Korl_Memory_ReportMeta_Allocation*const allocationMeta    = KORL_C_CAST(_Korl_Memory_ReportMeta_Allocation*, stbDaReportData + allocatorMeta->allocations.byteOffset);
        const _Korl_Memory_ReportMeta_Heap*const       heapMetaEnd       = heapMeta + allocatorMeta->heaps.size;
        const _Korl_Memory_ReportMeta_Allocation*const allocationMetaEnd = allocationMeta + allocatorMeta->allocations.size;
        const _Korl_Memory_ReportMeta_Allocation*      ha                = allocationMeta;
        const char* allocatorType = NULL;
        switch(allocatorMeta->allocatorType)
        {
        case KORL_MEMORY_ALLOCATOR_TYPE_CRT    :{ allocatorType = "KORL_MEMORY_ALLOCATOR_TYPE_CRT"; break; }
        case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR :{ allocatorType = "KORL_MEMORY_ALLOCATOR_TYPE_LINEAR"; break; }
        case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{ allocatorType = "KORL_MEMORY_ALLOCATOR_TYPE_GENERAL"; break; }
        }
        korl_log_noMeta(INFO, "║ %hs {\"%ws\", heaps: %llu, allocations: %llu, flags: 0x%X}"
                       ,allocatorType, allocatorMeta->name, heapMetaEnd - heapMeta, allocationMetaEnd - allocationMeta, allocatorMeta->flags);
        for(const _Korl_Memory_ReportMeta_Heap* h = heapMeta; h < heapMetaEnd; h++)
        {
            if(allocatorMeta->allocatorType == KORL_MEMORY_ALLOCATOR_TYPE_CRT)
            {
                korl_log_noMeta(INFO, "║ heap[%llu]: [? ~ ?]", h - heapMeta);
            }
            else
            {
                korl_log_noMeta(INFO, "║ heap[%llu]: [0x%p ~ 0x%p]", h - heapMeta, h->virtualAddressStart, h->virtualAddressEnd);
                for(;   ha < allocationMetaEnd 
                     && ha->allocationAddress >= h->virtualAddressStart 
                     && ha->allocationAddress <  h->virtualAddressEnd
                    ;ha++)
                {
                    const void*const allocAddressEnd = KORL_C_CAST(u8*, ha->allocationAddress) + ha->meta.bytes;
                    const bool lastHeapAllocation =   ha >= allocationMetaEnd - 1
                                                   || (ha+1)->allocationAddress >= h->virtualAddressEnd;
                    korl_log_noMeta(INFO, "║ %ws [0x%p ~ 0x%p] %llu bytes, %ws:%i, %ws"
                                   ,lastHeapAllocation ? L"└" : L"├"
                                   ,ha->allocationAddress, allocAddressEnd
                                   ,ha->meta.bytes
                                   ,ha->meta.file, ha->meta.line
                                   ,   allocatorMeta->allocatorType == KORL_MEMORY_ALLOCATOR_TYPE_LINEAR 
                                    && 0 == (allocatorMeta->flags & KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME)
                                    ? ha->meta.defragmentState == KORL_MEMORY_ALLOCATION_META_DEFRAGMENT_STATE_MANAGED 
                                      ? L"managed" 
                                      : ha->meta.defragmentState == KORL_MEMORY_ALLOCATION_META_DEFRAGMENT_STATE_UNMANAGED 
                                        ? L"UNMANAGED" 
                                        : L"new"
                                    : L"");
                }
            }
        }
        if(allocatorMeta->allocatorType != KORL_MEMORY_ALLOCATOR_TYPE_CRT)
            korl_assert(ha == allocationMetaEnd);// we should have enumerated over all allocations for this allocator
    }
    korl_log_noMeta(INFO, "╚═════ END of Memory Report ════════════════════════════╝");
}
korl_internal KORL_FUNCTION_korl_memory_allocator_enumerateAllocators(korl_memory_allocator_enumerateAllocators)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
    {
        _Korl_Memory_Allocator*const allocator = &context->allocators[a];
        if(!callback(callbackUserData, allocator, allocator->handle, allocator->name, allocator->flags))
            break;
    }
}
korl_internal KORL_FUNCTION_korl_memory_allocator_enumerateAllocations(korl_memory_allocator_enumerateAllocations)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    _Korl_Memory_Allocator*const allocator = opaqueAllocator;
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        korl_heap_crt_enumerateAllocations(allocator->userData, callback, callbackUserData);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        korl_heap_linear_enumerateAllocations(allocator->userData, callback, callbackUserData);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        korl_heap_general_enumerateAllocations(allocator->userData, callback, callbackUserData);
        return;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
}
korl_internal KORL_FUNCTION_korl_memory_allocator_enumerateHeaps(korl_memory_allocator_enumerateHeaps)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    _Korl_Memory_Allocator*const allocator = opaqueAllocator;
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        korl_heap_crt_enumerate(allocator->userData, callback, callbackUserData);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        korl_heap_linear_enumerate(allocator->userData, callback, callbackUserData);
        return;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        korl_heap_general_enumerate(allocator->userData, callback, callbackUserData);
        return;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
}
korl_internal bool korl_memory_allocator_findByName(const wchar_t* name, Korl_Memory_AllocatorHandle* out_allocatorHandle)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    for(Korl_MemoryPool_Size a = 0; a < KORL_MEMORY_POOL_SIZE(context->allocators); a++)
        if(korl_string_compareUtf16(context->allocators[a].name, name, KORL_DEFAULT_C_STRING_SIZE_LIMIT) == 0)
        {
            *out_allocatorHandle = context->allocators[a].handle;
            return true;
        }
    return false;
}
typedef struct _Korl_Memory_ContainsAllocation_EnumContext
{
    bool        containsAllocation;
    const void* allocation;
} _Korl_Memory_ContainsAllocation_EnumContext;
korl_internal KORL_HEAP_ENUMERATE_CALLBACK(_korl_memory_allocator_containsAllocation_enumHeapCallback)
{
    _Korl_Memory_ContainsAllocation_EnumContext*const enumContext = KORL_C_CAST(_Korl_Memory_ContainsAllocation_EnumContext*, userData);
    if(enumContext->allocation >= virtualAddressStart && enumContext->allocation < virtualAddressEnd)
        enumContext->containsAllocation = true;
}
korl_internal bool korl_memory_allocator_containsAllocation(void* opaqueAllocator, const void* allocation)
{
    _Korl_Memory_Context*const context = &_korl_memory_context;
    korl_assert(context->mainThreadId == GetCurrentThreadId());
    _Korl_Memory_Allocator*const allocator = opaqueAllocator;
    KORL_ZERO_STACK(_Korl_Memory_ContainsAllocation_EnumContext, enumContext);
    switch(allocator->type)
    {
    case KORL_MEMORY_ALLOCATOR_TYPE_CRT:{
        korl_heap_crt_enumerate(allocator->userData, _korl_memory_allocator_containsAllocation_enumHeapCallback, &enumContext);
        return enumContext.containsAllocation;}
    case KORL_MEMORY_ALLOCATOR_TYPE_LINEAR:{
        korl_heap_linear_enumerate(allocator->userData, _korl_memory_allocator_containsAllocation_enumHeapCallback, &enumContext);
        return enumContext.containsAllocation;}
    case KORL_MEMORY_ALLOCATOR_TYPE_GENERAL:{
        korl_heap_general_enumerate(allocator->userData, _korl_memory_allocator_containsAllocation_enumHeapCallback, &enumContext);
        return enumContext.containsAllocation;}
    }
    korl_log(ERROR, "Korl_Memory_AllocatorType '%i' not implemented", allocator->type);
    return false;
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
