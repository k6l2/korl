#include "korl-assetCache.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-stringPool.h"
#include "korl-file.h"
#ifdef _LOCAL_STRING_POOL_POINTER
#undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_assetCache_context.stringPool))
#define _KORL_ASSETCACHE_ASSET_COUNT_MAX 1024
typedef enum _Korl_AssetCache_AssetState
    { _KORL_ASSET_CACHE_ASSET_STATE_INITIALIZED// the file hasn't been opened yet
    , _KORL_ASSET_CACHE_ASSET_STATE_PENDING    // the file is open, and we are async loading the asset
    , _KORL_ASSET_CACHE_ASSET_STATE_LOADED     // the file is closed, and the asset data is loaded
} _Korl_AssetCache_AssetState;
typedef struct _Korl_AssetCache_Asset
{
    _Korl_AssetCache_AssetState state;
    Korl_AssetCache_AssetData data;
    Korl_StringPool_StringHandle name;
    Korl_File_Descriptor fileDescriptor;
    Korl_File_AsyncIoHandle asyncIoHandle;
} _Korl_AssetCache_Asset;
typedef struct _Korl_AssetCache_Context
{
    KORL_MEMORY_POOL_DECLARE(_Korl_AssetCache_Asset, assets, _KORL_ASSETCACHE_ASSET_COUNT_MAX);//KORL-FEATURE-000-000-007: dynamic resizing arrays
    /** this allocator will store all the data for the raw assets */
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool stringPool;
} _Korl_AssetCache_Context;
#undef _KORL_ASSETCACHE_ASSET_NAME_SIZE_MAX
#undef _KORL_ASSETCACHE_ASSET_COUNT_MAX
korl_global_variable _Korl_AssetCache_Context _korl_assetCache_context;
korl_internal void korl_assetCache_initialize(void)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    korl_memory_zero(context, sizeof(*context));
    //KORL-PERFORMANCE-000-000-026: savestate/assetCache: there is no need to save/load every asset; we only need assets that have been flagged as "operation critical"
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_gigabytes(1), L"korl-assetCache", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, NULL/*let platform choose address*/);
    context->stringPool      = korl_stringPool_create(context->allocatorHandle);
}
korl_internal KORL_PLATFORM_ASSETCACHE_GET(korl_assetCache_get)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    const u$ assetNameSize = korl_memory_stringSize(assetName);
    const bool asyncLoad = flags & KORL_ASSETCACHE_GET_FLAG_LAZY;
    _Korl_AssetCache_Asset* asset = NULL;
    /* see if the asset is already loaded, and if so select it */
    for(u$ a = 0; a < KORL_MEMORY_POOL_SIZE(context->assets); a++)
    {
        if(string_equalsUtf16(context->assets[a].name, assetName))
        {
            asset = &(context->assets[a]);
            break;
        }
    }
    if(!asset)
    {
        /* otherwise, add a new asset */
        korl_assert(!KORL_MEMORY_POOL_ISFULL(context->assets));
        asset = KORL_MEMORY_POOL_ADD(context->assets);
        korl_memory_zero(asset, sizeof(*asset));
        asset->name = string_newUtf16(assetName);
    }
    switch(asset->state)
    {
    case _KORL_ASSET_CACHE_ASSET_STATE_INITIALIZED:{
        const bool resultFileOpen = korl_file_open(KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY, 
                                                   string_getRawUtf16(asset->name), 
                                                   &(asset->fileDescriptor), 
                                                   asyncLoad);
        if(resultFileOpen)
        {
            asset->data.dataBytes = korl_file_getTotalBytes(asset->fileDescriptor);
            asset->data.data = korl_allocate(context->allocatorHandle, asset->data.dataBytes);
            if(asyncLoad)
            {
                asset->asyncIoHandle = korl_file_readAsync(asset->fileDescriptor, asset->data.data, asset->data.dataBytes);
                asset->state = _KORL_ASSET_CACHE_ASSET_STATE_PENDING;
            }
            else
            {
                const bool resultFileRead = korl_file_read(asset->fileDescriptor, asset->data.data, asset->data.dataBytes);
                if(resultFileRead)
                {
                    asset->state = _KORL_ASSET_CACHE_ASSET_STATE_LOADED;
                    goto returnLoadedData;
                }
                else
                    /* I guess we should just close the file & attempt to reload 
                        the asset next time? */
                    korl_file_close(&(asset->fileDescriptor));
            }
        }
        else
        {
            //we failed to open the file; log a warning?  although, that might result in spam, so I'm not quite sure...
            //in any case, it should be okay for us to continue trying to open the file
        }
        break;}
    case _KORL_ASSET_CACHE_ASSET_STATE_PENDING:{
        const Korl_File_GetAsyncIoResult asyncIoResult = 
            korl_file_getAsyncIoResult(&asset->asyncIoHandle, !asyncLoad);
        switch(asyncIoResult)
        {
        case KORL_FILE_GET_ASYNC_IO_RESULT_DONE:{
            korl_file_close(&asset->fileDescriptor);
            asset->state = _KORL_ASSET_CACHE_ASSET_STATE_LOADED;
            goto returnLoadedData;}
        case KORL_FILE_GET_ASYNC_IO_RESULT_PENDING:{
            return KORL_ASSETCACHE_GET_RESULT_PENDING;}
        case KORL_FILE_GET_ASYNC_IO_RESULT_INVALID_HANDLE:{
            /* For whatever reason, the async handle was invalidated; just make 
                another attempt to async load.  NOTE: we are assuming that the 
                file descriptor is still valid.  If this breaks, we should do 
                something about that. */
            asset->asyncIoHandle = korl_file_readAsync(asset->fileDescriptor, asset->data.data, asset->data.dataBytes);
            break;}
        }
        break;}
    case _KORL_ASSET_CACHE_ASSET_STATE_LOADED:{
        returnLoadedData:
        *o_assetData = asset->data;
        return KORL_ASSETCACHE_GET_RESULT_LOADED;}
    default:{
        korl_log(ERROR, "invalid asset state: %i", asset->state);
        break;}
    }
    return KORL_ASSETCACHE_GET_RESULT_PENDING;
}
korl_internal void korl_assetCache_saveStateWrite(Korl_Memory_AllocatorHandle allocatorHandle, void** saveStateBuffer, u$* saveStateBufferBytes, u$* saveStateBufferBytesUsed)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    const u$ bytesRequired = sizeof(context->assets_korlMemoryPoolSize) 
                           + context->assets_korlMemoryPoolSize*sizeof(*context->assets)
                           //KORL-ISSUE-000-000-079: stringPool/file/savestate: either create a (de)serialization API for stringPool, or just put context state into a single allocation?
                           + sizeof(context->stringPool);
    u8* bufferCursor    = KORL_C_CAST(u8*, *saveStateBuffer) + *saveStateBufferBytesUsed;
    const u8* bufferEnd = KORL_C_CAST(u8*, *saveStateBuffer) + *saveStateBufferBytes;
    if(bufferCursor + bytesRequired > bufferEnd)
    {
        *saveStateBufferBytes = KORL_MATH_MAX(2*(*saveStateBufferBytes), 
                                              // at _least_ make sure that we are about to realloc enough room for the required bytes for the manifest:
                                              (*saveStateBufferBytes) + bytesRequired);
        *saveStateBuffer = korl_reallocate(allocatorHandle, *saveStateBuffer, *saveStateBufferBytes);
        korl_assert(*saveStateBuffer);
        bufferCursor = KORL_C_CAST(u8*, *saveStateBuffer) + *saveStateBufferBytesUsed;
        bufferEnd    = bufferCursor + *saveStateBufferBytes;
    }
    korl_assert(sizeof(context->assets_korlMemoryPoolSize)                  == korl_memory_packU32(context->assets_korlMemoryPoolSize, &bufferCursor, bufferEnd));
    korl_assert(context->assets_korlMemoryPoolSize*sizeof(*context->assets) == korl_memory_packStringI8(KORL_C_CAST(i8*, context->assets), context->assets_korlMemoryPoolSize*sizeof(*context->assets), &bufferCursor, bufferEnd));
    //KORL-ISSUE-000-000-079: stringPool/file/savestate: either create a (de)serialization API for stringPool, or just put context state into a single allocation?
    korl_assert(sizeof(context->stringPool)                                 == korl_memory_packStringI8(KORL_C_CAST(i8*, &context->stringPool), sizeof(context->stringPool), &bufferCursor, bufferEnd));
    *saveStateBufferBytesUsed += bytesRequired;
}
korl_internal bool korl_assetCache_saveStateRead(HANDLE hFile)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    if(!ReadFile(hFile, &context->assets_korlMemoryPoolSize, sizeof(context->assets_korlMemoryPoolSize), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    if(!ReadFile(hFile, context->assets, context->assets_korlMemoryPoolSize*sizeof(*context->assets), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    //KORL-ISSUE-000-000-079: stringPool/file/savestate: either create a (de)serialization API for stringPool, or just put context state into a single allocation?
    if(!ReadFile(hFile, &context->stringPool, sizeof(context->stringPool), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    return true;
}
#undef _LOCAL_STRING_POOL_POINTER
