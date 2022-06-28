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
typedef struct _Korl_AssetCache_Asset
{
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
    Korl_StringPool stringPool;///@TODO: serialize this in the savestate probably?
} _Korl_AssetCache_Context;
#undef _KORL_ASSETCACHE_ASSET_NAME_SIZE_MAX
#undef _KORL_ASSETCACHE_ASSET_COUNT_MAX
korl_global_variable _Korl_AssetCache_Context _korl_assetCache_context;
korl_internal void korl_assetCache_initialize(void)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    korl_memory_zero(context, sizeof(*context));
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_gigabytes(1), L"korl-assetCache", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, NULL/*let platform choose address*/);
    context->stringPool      = korl_stringPool_create(context->allocatorHandle);
}
korl_internal KORL_PLATFORM_ASSETCACHE_GET(korl_assetCache_get)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    const u$ assetNameSize = korl_memory_stringSize(assetName);
    const bool asyncLoad = flags & KORL_ASSETCACHE_GET_FLAGS_LAZY;
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
    if(asset)
    {
        /* if the asset is still async loading, we just have to wait */
        if(asset->asyncIoHandle)
        {
            const Korl_File_GetAsyncIoResult asyncIoResult = 
                korl_file_getAsyncIoResult(&asset->asyncIoHandle, !asyncLoad);
            switch(asyncIoResult)
            {
            case KORL_FILE_GET_ASYNC_IO_RESULT_DONE:{
                break;}// just fall-through as if the async handle was already invalidated
            case KORL_FILE_GET_ASYNC_IO_RESULT_PENDING:{
                return KORL_ASSETCACHE_GET_RESULT_STILL_LOADING;}
            case KORL_FILE_GET_ASYNC_IO_RESULT_INVALID_HANDLE:{
                goto kickOffLoadFile;}
            }
        }
    }
    else
    {
        /* otherwise, add a new asset */
        korl_assert(!KORL_MEMORY_POOL_ISFULL(context->assets));
        asset = KORL_MEMORY_POOL_ADD(context->assets);
        korl_memory_zero(asset, sizeof(*asset));
        asset->name = string_newUtf16(assetName);
    }
    if(asset->data.data)
    {
        *o_assetData = asset->data;
        return KORL_ASSETCACHE_GET_RESULT_LOADED;
    }
    /* asset data load has not been kicked off yet; if it was, the async io 
        handle would have been present & we would have caught it in the above 
        code */
kickOffLoadFile:
    korl_assert(!"@TODO"); 
    return KORL_ASSETCACHE_GET_RESULT_STILL_LOADING;
}
korl_internal void korl_assetCache_saveStateWrite(Korl_Memory_AllocatorHandle allocatorHandle, void** saveStateBuffer, u$* saveStateBufferBytes, u$* saveStateBufferBytesUsed)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    const u$ bytesRequired = sizeof(context->assets_korlMemoryPoolSize) 
                           + context->assets_korlMemoryPoolSize*sizeof(*context->assets);
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
    return true;
}
#undef _LOCAL_STRING_POOL_POINTER
