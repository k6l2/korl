#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-windows-globalDefines.h"
#include "korl-checkCast.h"
#include "korl-log.h"
#include "korl-stb-ds.h"
#include "korl-heap.h"
#if KORL_DEBUG
    //@TODO: comment this out
    #define _KORL_MEMORY_DEBUG_HEAP_UNIT_TESTS
#endif
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
    #ifdef _KORL_MEMORY_DEBUG_HEAP_UNIT_TESTS
    {
        u8* testAllocs[3];
        Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, context->systemInfo.dwPageSize*4, L"_korl-memory-test-general", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL);
        // expected allocator page occupied flags: 0b00
        testAllocs[0] = korl_allocate(allocator, 32);
        // expected allocator page occupied flags: 0b01
        testAllocs[1] = korl_allocate(allocator, context->systemInfo.dwPageSize);
        // expected allocator page occupied flags: 0b01 0b01
        testAllocs[2] = korl_allocate(allocator, context->systemInfo.dwPageSize + 1);
        // expected allocator page occupied flags: 0b01 0b01 0b01
        korl_free(allocator, testAllocs[0]); testAllocs[0] = NULL;
        // expected allocator page occupied flags: 0b00 0b01 0b01
        korl_reallocate(allocator, testAllocs[1], context->systemInfo.dwPageSize*3);// test realloc (move)
        // expected allocator page occupied flags: 
        korl_reallocate(allocator, testAllocs[1], 32);// test realloc (shrink)
        // expected allocator page occupied flags: 
        korl_memory_allocator_empty(allocator); korl_memory_zero(testAllocs, sizeof(testAllocs));
        korl_memory_allocator_destroy(allocator);
        allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, context->systemInfo.dwPageSize*4, L"_korl-memory-test-linear", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL);
        // expected allocator page occupied flags: 0b0000
        testAllocs[0] = korl_allocate(allocator, 32);
        // expected allocator page occupied flags: 
        testAllocs[1] = korl_allocate(allocator, 32);
        // expected allocator page occupied flags: 
        testAllocs[0] = korl_reallocate(allocator, testAllocs[0], context->systemInfo.dwPageSize*2);
        // expected allocator page occupied flags: 
        korl_free(allocator, testAllocs[0]); testAllocs[0] = NULL;
        // expected allocator page occupied flags: 
        // test heap expansion
        testAllocs[0] = korl_allocate(allocator, context->systemInfo.dwPageSize*3);
        // expected allocator page occupied flags: 
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
        newAllocator->userData = korl_heap_linear_create(maxBytes, address);
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
        korl_heap_linear_destroy(allocator->userData);
        allocator->userData = korl_heap_linear_create(allocator->maxBytes, newAddress);
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
        return korl_heap_linear_allocate(KORL_C_CAST(_Korl_Heap_Linear*, allocator->userData), bytes, persistentFileString, line, address);}
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
            return korl_heap_linear_allocate(KORL_C_CAST(_Korl_Heap_Linear*, allocator->userData), bytes, persistentFileString, line, NULL/*automatically find address*/);
        return korl_heap_linear_reallocate(KORL_C_CAST(_Korl_Heap_Linear*, allocator->userData), allocation, bytes, persistentFileString, line);}
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
        korl_heap_linear_free(KORL_C_CAST(_Korl_Heap_Linear*, allocator->userData), allocation, persistentFileString, line);
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
        korl_heap_linear_empty(KORL_C_CAST(_Korl_Heap_Linear*, allocator->userData));
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
        //KORL-ISSUE-000-000-080: memory: korl_heap_linear_enumerateAllocations can't properly early-out if the enumeration callback returns false
        korl_heap_linear_enumerateAllocations(allocator, allocator->userData, _korl_memory_allocator_isEmpty_enumAllocationsCallback, &resultIsEmpty, NULL/*we don't care about the virtual address range end*/);
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
            korl_heap_linear_empty(KORL_C_CAST(_Korl_Heap_Linear*, allocator->userData));
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
            korl_heap_linear_enumerateAllocations(allocator, allocator->userData, _korl_memory_logReport_enumerateCallback, enumContext, &enumContext->virtualAddressEnd);
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
        korl_heap_linear_enumerateAllocations(opaqueAllocator, allocator->userData, callback, callbackUserData, out_allocatorVirtualAddressEnd);
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
