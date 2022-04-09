#include "korl-assetCache.h"
#include "korl-memory.h"
#include "korl-file.h"
#define _KORL_ASSETCACHE_ASSET_COUNT_MAX 1024
#define _KORL_ASSETCACHE_ASSET_NAME_SIZE_MAX 64
typedef struct _Korl_AssetCache_Asset
{
    Korl_AssetCache_AssetData assetData;
    wchar_t name[_KORL_ASSETCACHE_ASSET_NAME_SIZE_MAX];
} _Korl_AssetCache_Asset;
typedef struct _Korl_AssetCache_Context
{
    KORL_MEMORY_POOL_DECLARE(_Korl_AssetCache_Asset, assets, _KORL_ASSETCACHE_ASSET_COUNT_MAX);
    /** this allocator will store all the data for the raw assets */
    Korl_Memory_Allocator allocator;
} _Korl_AssetCache_Context;
#undef _KORL_ASSETCACHE_ASSET_NAME_SIZE_MAX
#undef _KORL_ASSETCACHE_ASSET_COUNT_MAX
_Korl_AssetCache_Context _korl_assetCache_context;
korl_internal void korl_assetCache_initialize(void)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    korl_memory_nullify(context, sizeof(*context));
    context->allocator = korl_memory_createAllocatorLinear(korl_math_gigabytes(1));
}
korl_internal Korl_AssetCache_AssetData korl_assetCache_get(
    const wchar_t*const assetName, Korl_AssetCache_Get_Flags flags)
{
    _Korl_AssetCache_Context*const context = &_korl_assetCache_context;
    KORL_ZERO_STACK(Korl_AssetCache_AssetData, resultAsset);
    const u$ assetNameSize = korl_memory_stringSize(assetName);
    korl_assert(assetNameSize < korl_arraySize(((_Korl_AssetCache_Asset*)0)->name));
    /* see if the asset is already loaded, and if so return it */
    {
        u$ a = 0;
        for(; a < KORL_MEMORY_POOL_SIZE(context->assets); a++)
        {
            _Korl_AssetCache_Asset*const asset = &context->assets[a];
            if(korl_memory_stringCompare(asset->name, assetName) == 0)
            {
                resultAsset = asset->assetData;
                break;
            }
        }
        if(a < KORL_MEMORY_POOL_SIZE(context->assets))
            return context->assets[a].assetData;
    }
    //KORL-ISSUE-000-000-000
    //KORL-ISSUE-000-000-001
    korl_assert(!KORL_MEMORY_POOL_ISFULL(context->assets));
    korl_assert(
        korl_file_load(
            assetName, KORL_FILE_PATHTYPE_CURRENTWORKINGDIRECTORY, 
            &context->allocator, &resultAsset.data, &resultAsset.dataBytes));
    /* since the asset has just been loaded, add it to the asset manager */
    _Korl_AssetCache_Asset*const newAsset = KORL_MEMORY_POOL_ADD(context->assets);
    const i$ charactersCopied = 
        korl_memory_stringCopy(assetName, newAsset->name, korl_arraySize(newAsset->name));
    newAsset->assetData = resultAsset;
    return resultAsset;
}
