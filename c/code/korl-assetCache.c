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
#define _LOCAL_STRING_POOL_POINTER (&(_korl_assetCache_context.stringPool))
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
    Korl_AssetCache_AssetData data;
    Korl_StringPool_StringHandle name;
    Korl_File_Descriptor fileDescriptor;
    Korl_File_AsyncIoHandle asyncIoHandle;
    KorlPlatformDateStamp dateStampLastWrite;
    _Korl_AssetCache_AssetFlags flags;
} _Korl_AssetCache_Asset;
typedef struct _Korl_AssetCache_Context
{
    _Korl_AssetCache_Asset* stbDaAssets;
    /** this allocator will store all the data for the raw assets */
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool stringPool;
} _Korl_AssetCache_Context;
korl_global_variable _Korl_AssetCache_Context _korl_assetCache_context;
korl_internal void korl_assetCache_initialize(void)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    korl_memory_zero(context, sizeof(*context));
    //KORL-PERFORMANCE-000-000-026: savestate/assetCache: there is no need to save/load every asset; we only need assets that have been flagged as "operation critical"
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_gigabytes(1), L"korl-assetCache", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, NULL/*let platform choose address*/);
    context->stringPool      = korl_stringPool_create(context->allocatorHandle);
    mcarrsetcap(KORL_C_CAST(void*, context->allocatorHandle), context->stbDaAssets, 1024);// reduce reallocations by setting the asset database to some arbitrary large size
}
korl_internal KORL_PLATFORM_ASSETCACHE_GET(korl_assetCache_get)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    const u$ assetNameSize = korl_memory_stringSize(assetName);
    const bool asyncLoad = flags & KORL_ASSETCACHE_GET_FLAG_LAZY;
    _Korl_AssetCache_Asset* asset = NULL;
    /* see if the asset is already loaded, and if so select it */
    for(u$ a = 0; a < arrlenu(context->stbDaAssets); a++)
    {
        if(string_equalsUtf16(context->stbDaAssets[a].name, assetName))
        {
            asset = &(context->stbDaAssets[a]);
            break;
        }
    }
    if(!asset)
    {
        /* otherwise, add a new asset */
        mcarrpush(KORL_C_CAST(void*, context->allocatorHandle), context->stbDaAssets, (_Korl_AssetCache_Asset){0});
        asset = &arrlast(context->stbDaAssets);
        korl_memory_zero(asset, sizeof(*asset));// not _entirely_ sure this is necessary, but I've read that struct initialization using {0} might have portability issues?...
        asset->name = string_newUtf16(assetName);
    }
    switch(asset->state)
    {
    case _KORL_ASSET_CACHE_ASSET_STATE_INITIALIZED:{
        korl_assert(asset->fileDescriptor.flags == 0);
        const bool resultFileOpen = korl_file_open(KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY, 
                                                   string_getRawUtf16(asset->name), 
                                                   &(asset->fileDescriptor), 
                                                   asyncLoad);
        if(resultFileOpen)
        {
            asset->dateStampLastWrite = korl_file_getDateStampLastWrite(asset->fileDescriptor);
            asset->data.dataBytes     = korl_file_getTotalBytes(asset->fileDescriptor);
            asset->data.data          = korl_allocate(context->allocatorHandle, asset->data.dataBytes);
            if(asyncLoad)
            {
                asset->asyncIoHandle = korl_file_readAsync(asset->fileDescriptor, asset->data.data, asset->data.dataBytes);
                asset->state         = _KORL_ASSET_CACHE_ASSET_STATE_PENDING;
            }
            else
            {
                const bool resultFileRead = korl_file_read(asset->fileDescriptor, asset->data.data, asset->data.dataBytes);
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
            asset->asyncIoHandle = korl_file_readAsync(asset->fileDescriptor, asset->data.data, asset->data.dataBytes);
            break;}
        }
        break;}
    case _KORL_ASSET_CACHE_ASSET_STATE_LOADED:{
        returnLoadedData:
        *o_assetData = asset->data;
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
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
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
                const wchar_t*const rawUtf16AssetName = string_getRawUtf16(asset->name);
                korl_log(INFO, "Asset \"%ws\" has been hot-reloaded!  Running callbacks...", rawUtf16AssetName);
                callbackOnAssetHotReloaded(rawUtf16AssetName, asset->data);
                break;}
            case KORL_FILE_GET_ASYNC_IO_RESULT_PENDING:{
                break;}
            case KORL_FILE_GET_ASYNC_IO_RESULT_INVALID_HANDLE:{
                /* For whatever reason, the async handle was invalidated; just make 
                    another attempt to async load.  NOTE: we are assuming that the 
                    file descriptor is still valid.  If this breaks, we should do 
                    something about that. */
                asset->asyncIoHandle = korl_file_readAsync(asset->fileDescriptor, asset->data.data, asset->data.dataBytes);
                break;}
            }
            continue;
        }
        else if(asset->state != _KORL_ASSET_CACHE_ASSET_STATE_LOADED)
            continue;
        const wchar_t*const rawUtf16AssetName = string_getRawUtf16(asset->name);
        KorlPlatformDateStamp dateStampLatestFileWrite;
        if(   korl_file_getDateStampLastWriteFileName(KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY, 
                                                      rawUtf16AssetName, &dateStampLatestFileWrite)
           && KORL_TIME_DATESTAMP_COMPARE_RESULT_FIRST_TIME_EARLIER == korl_time_dateStampCompare(asset->dateStampLastWrite, dateStampLatestFileWrite))
        {
            korl_assert(asset->fileDescriptor.flags == 0);
            const bool resultFileOpen = korl_file_open(KORL_FILE_PATHTYPE_CURRENT_WORKING_DIRECTORY, 
                                                       string_getRawUtf16(asset->name), 
                                                       &(asset->fileDescriptor), 
                                                       true/*async*/);
            if(resultFileOpen)
            {
                korl_assert(!asset->asyncIoHandle);
                asset->data.dataBytes = korl_file_getTotalBytes(asset->fileDescriptor);
                if(0 == asset->data.dataBytes)
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
                        korl_log(WARNING, "asset \"%ws\" opened with 0 bytes; closing, then re-trying later...", string_getRawUtf16(asset->name));
                        asset->flags |= _KORL_ASSET_CACHE_ASSET_FLAG_RELOAD_WARNING_ISSUED;
                    }
                    korl_file_close(&(asset->fileDescriptor));
                    continue;
                }
                /* otherwise, it should be safe to assume that we opened the 
                    file in a valid state, so we can async load the contents */
                asset->dateStampLastWrite = dateStampLatestFileWrite;
                asset->state              = _KORL_ASSET_CACHE_ASSET_STATE_RELOADING;
                asset->data.data          = korl_reallocate(context->allocatorHandle, asset->data.data, asset->data.dataBytes);
                asset->asyncIoHandle      = korl_file_readAsync(asset->fileDescriptor, asset->data.data, asset->data.dataBytes);
            }
            else
                /* It should be okay for us to continue trying to open the file; 
                    this condition is likely to occur often when the editor 
                    program being used to modify the asset hasn't finished 
                    writing operations on it yet. */
                if(!(asset->flags & _KORL_ASSET_CACHE_ASSET_FLAG_RELOAD_WARNING_ISSUED))
                {
                    korl_log(INFO, "asset \"%ws\" failed to open; re-trying later...", string_getRawUtf16(asset->name));
                    asset->flags |= _KORL_ASSET_CACHE_ASSET_FLAG_RELOAD_WARNING_ISSUED;
                }
        }
    }
}
korl_internal void korl_assetCache_clearAllFileHandles(void)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    for(u$ a = 0; a < arrlenu(context->stbDaAssets); a++)
    {
        _Korl_AssetCache_Asset*const asset = &(context->stbDaAssets[a]);
        if(asset->fileDescriptor.flags)
            korl_file_close(&(asset->fileDescriptor));
    }
}
korl_internal void korl_assetCache_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &context->stbDaAssets, sizeof(context->stbDaAssets));
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &context->stringPool , sizeof(context->stringPool));
}
korl_internal bool korl_assetCache_saveStateRead(HANDLE hFile)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    if(!ReadFile(hFile, &context->stbDaAssets, sizeof(context->stbDaAssets), NULL/*bytes read*/, NULL/*no overlapped*/))
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
