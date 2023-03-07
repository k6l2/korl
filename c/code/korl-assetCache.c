#include "korl-assetCache.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-stringPool.h"
#include "korl-file.h"
#include "korl-time.h"
#include "korl-stb-ds.h"
#include "korl-checkCast.h"
#ifdef _LOCAL_STRING_POOL_POINTER
    #undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER _korl_assetCache_context->stringPool
typedef enum _Korl_AssetCache_AssetState
    { _KORL_ASSET_CACHE_ASSET_STATE_INITIALIZED// the file hasn't been opened yet
    , _KORL_ASSET_CACHE_ASSET_STATE_PENDING    // the file is open, and we are async loading the asset
    , _KORL_ASSET_CACHE_ASSET_STATE_LOADED     // the file is closed, and the asset data is loaded
    , _KORL_ASSET_CACHE_ASSET_STATE_RELOADING  // assetCache has detected that the file has a newer version on disk and must be reloaded; the asset will remain in this state until (1) the asset is loaded, & (2) the assetCache has been queried for all newly reloaded assets
} _Korl_AssetCache_AssetState;
typedef enum _Korl_AssetCache_AssetFlags
    { _KORL_ASSET_CACHE_ASSET_FLAGS_NONE                 = 0
    , _KORL_ASSET_CACHE_ASSET_FLAG_RELOAD_WARNING_ISSUED = 1 << 0 // used to prevent log spam for asset hot-reload warning conditions
} _Korl_AssetCache_AssetFlags;
typedef struct _Korl_AssetCache_Asset
{
    _Korl_AssetCache_AssetState state;
    Korl_AssetCache_AssetData   assetData;
    Korl_StringPool_String      name;
    Korl_File_Descriptor        fileDescriptor;
    Korl_File_AsyncIoHandle     asyncIoHandle;
    KorlPlatformDateStamp       dateStampLastWrite;
    _Korl_AssetCache_AssetFlags flags;
} _Korl_AssetCache_Asset;
typedef struct _Korl_AssetCache_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;// all data that _isn't_ stbDaAssets[i].assetData.data is stored here, including this struct itself
    Korl_Memory_AllocatorHandle allocatorHandleTransient;// _only_ stbDaAssets[i].assetData.data is stored here; this entire allocator should be wiped when a korl-memoryState is loaded
    _Korl_AssetCache_Asset*     stbDaAssets;
    Korl_StringPool*            stringPool;// @korl-string-pool-no-data-segment-storage
} _Korl_AssetCache_Context;
korl_global_variable _Korl_AssetCache_Context* _korl_assetCache_context;
korl_internal void korl_assetCache_initialize(void)
{
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(8);
    const Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-assetCache", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_gigabytes(1);
    _korl_assetCache_context = korl_allocate(allocator, sizeof(*_korl_assetCache_context));
    _Korl_AssetCache_Context*const context = _korl_assetCache_context;
    context->allocatorHandle          = allocator;
    context->allocatorHandleTransient = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-assetCache-transient", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);;
    context->stringPool               = korl_allocate(context->allocatorHandle, sizeof(*context->stringPool));
    *context->stringPool              = korl_stringPool_create(context->allocatorHandle);
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaAssets, 1024);// reduce reallocations by setting the asset database to some arbitrary large size
}
korl_internal KORL_FUNCTION_korl_assetCache_get(korl_assetCache_get)
{
    _Korl_AssetCache_Context*const context = _korl_assetCache_context;
    const u$ assetNameSize = korl_string_sizeUtf16(assetName);
    const bool asyncLoad = flags & KORL_ASSETCACHE_GET_FLAG_LAZY;
    _Korl_AssetCache_Asset* asset = NULL;
    /* see if the asset is already loaded, and if so select it */
    const _Korl_AssetCache_Asset* assetsEnd = context->stbDaAssets + arrlen(context->stbDaAssets);
    for(asset = context->stbDaAssets; asset < assetsEnd; asset++)
        if(string_equalsUtf16(&asset->name, assetName))
            break;
    if(asset >= assetsEnd)
    {
        /* otherwise, add a new asset */
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaAssets, (_Korl_AssetCache_Asset){0});
        asset = &arrlast(context->stbDaAssets);
        korl_memory_zero(asset, sizeof(*asset));// not _entirely_ sure this is necessary, but I've read that struct initialization using {0} might have portability issues?...
        asset->name = string_newUtf16(assetName);
    }
    assetsEnd = context->stbDaAssets + arrlen(context->stbDaAssets);
    korl_assert(asset >= context->stbDaAssets && asset < assetsEnd);
    switch(asset->state)
    {
    case _KORL_ASSET_CACHE_ASSET_STATE_INITIALIZED:{
        korl_assert(asset->fileDescriptor.flags == 0);
        const bool resultFileOpen = korl_file_open(KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY, 
                                                   string_getRawUtf16(&asset->name), 
                                                   &(asset->fileDescriptor), 
                                                   asyncLoad);
        if(resultFileOpen)
        {
            asset->dateStampLastWrite  = korl_file_getDateStampLastWrite(asset->fileDescriptor);
            asset->assetData.dataBytes = korl_file_getTotalBytes(asset->fileDescriptor);
            asset->assetData.data      = korl_allocate(context->allocatorHandleTransient, asset->assetData.dataBytes);
            if(asyncLoad)
            {
                asset->asyncIoHandle = korl_file_readAsync(asset->fileDescriptor, asset->assetData.data, asset->assetData.dataBytes);
                asset->state         = _KORL_ASSET_CACHE_ASSET_STATE_PENDING;
            }
            else
            {
                const bool resultFileRead = korl_file_read(asset->fileDescriptor, asset->assetData.data, asset->assetData.dataBytes);
                if(resultFileRead)
                {
                    asset->state = _KORL_ASSET_CACHE_ASSET_STATE_LOADED;
                    asset->flags = KORL_ASSETCACHE_GET_FLAGS_NONE;
                    korl_file_close(&(asset->fileDescriptor));
                    goto returnLoadedData;
                }
                else
                {
                    /* I guess we should just close the file & attempt to reload 
                        the asset next time? */
                    korl_file_close(&(asset->fileDescriptor));
                }
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
            asset->flags = KORL_ASSETCACHE_GET_FLAGS_NONE;
            goto returnLoadedData;}
        case KORL_FILE_GET_ASYNC_IO_RESULT_PENDING:{
            return KORL_ASSETCACHE_GET_RESULT_PENDING;}
        case KORL_FILE_GET_ASYNC_IO_RESULT_INVALID_HANDLE:{
            /* For whatever reason, the async handle was invalidated; just make 
                another attempt to async load.  NOTE: we are assuming that the 
                file descriptor is still valid.  If this breaks, we should do 
                something about that. */
            asset->asyncIoHandle = korl_file_readAsync(asset->fileDescriptor, asset->assetData.data, asset->assetData.dataBytes);
            break;}
        }
        break;}
    case _KORL_ASSET_CACHE_ASSET_STATE_LOADED:{
        returnLoadedData:
        *o_assetData = asset->assetData;
        return KORL_ASSETCACHE_GET_RESULT_LOADED;}
    case _KORL_ASSET_CACHE_ASSET_STATE_RELOADING:{
        return KORL_ASSETCACHE_GET_RESULT_PENDING;}
    default:{
        korl_log(ERROR, "invalid asset state: %i", asset->state);
        break;}
    }
    return KORL_ASSETCACHE_GET_RESULT_PENDING;
}
korl_internal void korl_assetCache_checkAssetObsolescence(fnSig_korl_assetCache_onAssetHotReloadedCallback* callbackOnAssetHotReloaded)
{
    _Korl_AssetCache_Context*const context = _korl_assetCache_context;
    for(u$ a = 0; a < arrlenu(context->stbDaAssets); a++)
    {
        _Korl_AssetCache_Asset*const asset = &(context->stbDaAssets[a]);
        if(asset->state == _KORL_ASSET_CACHE_ASSET_STATE_RELOADING)
        {
            const Korl_File_GetAsyncIoResult asyncIoResult = 
                korl_file_getAsyncIoResult(&asset->asyncIoHandle, false/*don't block*/);
            switch(asyncIoResult)
            {
            case KORL_FILE_GET_ASYNC_IO_RESULT_DONE:{
                korl_file_close(&asset->fileDescriptor);
                asset->state = _KORL_ASSET_CACHE_ASSET_STATE_LOADED;
                asset->flags = KORL_ASSETCACHE_GET_FLAGS_NONE;
                const acu16 rawUtf16AssetName = string_getRawAcu16(&asset->name);
                korl_log(INFO, "Asset \"%ws\" has been hot-reloaded!  Running callbacks...", rawUtf16AssetName.data);
                callbackOnAssetHotReloaded(rawUtf16AssetName, asset->assetData);
                break;}
            case KORL_FILE_GET_ASYNC_IO_RESULT_PENDING:{
                break;}
            case KORL_FILE_GET_ASYNC_IO_RESULT_INVALID_HANDLE:{
                /* For whatever reason, the async handle was invalidated; just make 
                    another attempt to async load.  NOTE: we are assuming that the 
                    file descriptor is still valid.  If this breaks, we should do 
                    something about that. */
                asset->asyncIoHandle = korl_file_readAsync(asset->fileDescriptor, asset->assetData.data, asset->assetData.dataBytes);
                break;}
            }
            continue;
        }
        else if(asset->state != _KORL_ASSET_CACHE_ASSET_STATE_LOADED)
            continue;
        const wchar_t*const rawUtf16AssetName = string_getRawUtf16(&asset->name);
        KorlPlatformDateStamp dateStampLatestFileWrite;
        /* @TODO: approx. 93% of the time spent in this entire function is inside 
            korl_file_getDateStampLastWriteFileName; this wastes > 850µs of 
            optimized build frame time; is it possible to perform this same 
            functionality via async events with the OS?  
            https://learn.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications */
        if(   korl_file_getDateStampLastWriteFileName(KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY, 
                                                      rawUtf16AssetName, &dateStampLatestFileWrite)
           && KORL_TIME_DATESTAMP_COMPARE_RESULT_FIRST_TIME_EARLIER == korl_time_dateStampCompare(asset->dateStampLastWrite, dateStampLatestFileWrite))
        {
            korl_assert(asset->fileDescriptor.flags == 0);
            const bool resultFileOpen = korl_file_open(KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY, 
                                                       string_getRawUtf16(&asset->name), 
                                                       &(asset->fileDescriptor), 
                                                       true/*async*/);
            if(resultFileOpen)
            {
                korl_assert(!asset->asyncIoHandle);
                asset->assetData.dataBytes = korl_file_getTotalBytes(asset->fileDescriptor);
                if(0 == asset->assetData.dataBytes)
                {
                    /* It is possible for us to open a file for exclusive write 
                        access that has a file size set to 0 by the program 
                        we're using to edit it.  One such example is VS Code, 
                        which after using Microsoft Process Monitor I have 
                        observed that VS Code will open a file and then 
                        immediately set the end-of-file pointer to be the 
                        current file pointer position (effectively setting the 
                        size of the file to 0 bytes).  If this ever happens, we 
                        should just assume that we didn't open the file in a 
                        valid state, and just attempt to open the file again at 
                        a future time.  */
                    if(!(asset->flags & _KORL_ASSET_CACHE_ASSET_FLAG_RELOAD_WARNING_ISSUED))
                    {
                        korl_log(WARNING, "asset \"%ws\" opened with 0 bytes; closing, then re-trying later...", string_getRawUtf16(&asset->name));
                        asset->flags |= _KORL_ASSET_CACHE_ASSET_FLAG_RELOAD_WARNING_ISSUED;
                    }
                    korl_file_close(&(asset->fileDescriptor));
                    continue;
                }
                /* otherwise, it should be safe to assume that we opened the 
                    file in a valid state, so we can async load the contents */
                asset->dateStampLastWrite = dateStampLatestFileWrite;
                asset->state              = _KORL_ASSET_CACHE_ASSET_STATE_RELOADING;
                asset->assetData.data     = korl_reallocate(context->allocatorHandleTransient, asset->assetData.data, asset->assetData.dataBytes);
                asset->asyncIoHandle      = korl_file_readAsync(asset->fileDescriptor, asset->assetData.data, asset->assetData.dataBytes);
            }
            else
                /* It should be okay for us to continue trying to open the file; 
                    this condition is likely to occur often when the editor 
                    program being used to modify the asset hasn't finished 
                    writing operations on it yet. */
                if(!(asset->flags & _KORL_ASSET_CACHE_ASSET_FLAG_RELOAD_WARNING_ISSUED))
                {
                    korl_log(INFO, "asset \"%ws\" failed to open; re-trying later...", string_getRawUtf16(&asset->name));
                    asset->flags |= _KORL_ASSET_CACHE_ASSET_FLAG_RELOAD_WARNING_ISSUED;
                }
        }
    }
}
korl_internal void korl_assetCache_clearAllFileHandles(void)
{
    _Korl_AssetCache_Context*const context = _korl_assetCache_context;
    for(u$ a = 0; a < arrlenu(context->stbDaAssets); a++)
    {
        _Korl_AssetCache_Asset*const asset = &(context->stbDaAssets[a]);
        if(asset->fileDescriptor.flags)
            korl_file_close(&(asset->fileDescriptor));
    }
}
korl_internal void korl_assetCache_defragment(Korl_Memory_AllocatorHandle stackAllocator)
{
    if(!korl_memory_allocator_isFragmented(_korl_assetCache_context->allocatorHandle))
        return;
    Korl_Heap_DefragmentPointer* stbDaDefragmentPointers = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(stackAllocator), stbDaDefragmentPointers, 64);
    KORL_MEMORY_STB_DA_DEFRAGMENT(stackAllocator, stbDaDefragmentPointers, _korl_assetCache_context);
    KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocator, stbDaDefragmentPointers, _korl_assetCache_context->stbDaAssets, _korl_assetCache_context);
    korl_stringPool_collectDefragmentPointers(_korl_assetCache_context->stringPool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_assetCache_context);
    korl_memory_allocator_defragment(_korl_assetCache_context->allocatorHandle, stbDaDefragmentPointers, arrlenu(stbDaDefragmentPointers), stackAllocator);
}
korl_internal u32 korl_assetCache_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer)
{
    const u32 byteOffset = korl_checkCast_u$_to_u32((*pByteBuffer)->size);
    korl_memory_byteBuffer_append(pByteBuffer, (acu8){.data = KORL_C_CAST(u8*, &_korl_assetCache_context), .size = sizeof(_korl_assetCache_context)});
    return byteOffset;
}
korl_internal void korl_assetCache_memoryStateRead(const u8* memoryState)
{
    _korl_assetCache_context = *KORL_C_CAST(_Korl_AssetCache_Context**, memoryState);
    _Korl_AssetCache_Context*const context = _korl_assetCache_context;
    /* we do _not_ store transient data in a memoryState!; all pointers to 
        memory within the transient allocator are now considered invalid, and we 
        need to wipe all transient memory from the current application session */
    korl_memory_allocator_empty(context->allocatorHandleTransient);
    /* the asset cache has been wiped, so we must mark all our assets as being 
        in a fresh unloaded "initialized" state; we must re-cache this file data 
        in order to use them again */
    for(u$ a = 0; a < arrlenu(context->stbDaAssets); a++)
    {
        _Korl_AssetCache_Asset*const asset = &(context->stbDaAssets[a]);
        asset->assetData.data = NULL;
        asset->state          = _KORL_ASSET_CACHE_ASSET_STATE_INITIALIZED;
    }
}
#undef _LOCAL_STRING_POOL_POINTER
