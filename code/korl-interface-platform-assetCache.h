#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_AssetCache_AssetData
{
    void* data;
    u32   dataBytes;
} Korl_AssetCache_AssetData;
typedef enum Korl_AssetCache_Get_Flags
    { KORL_ASSETCACHE_GET_FLAGS_NONE = 0
    /** Tell the asset manager that it is not necessary to load the asset 
     * immediately.  This will allow us to load the asset asynchronously.  If 
     * this flag is set, the caller is responsible for calling "get" on 
     * subsequent frames to see if the asset has finished loading if the call to 
     * "assetCache_get" returns KORL_ASSETCACHE_GET_RESULT_LOADED. */
    , KORL_ASSETCACHE_GET_FLAG_LAZY     = 1<<0
    /** Indicates that the associated asset is _required_ to be loaded into 
     * application memory in order for proper operation to take place.  
     * Recommended usage: 
     * - do _not_ use this for large/cosmetic assets! (PNGs, WAVs, OGGs, etc...)
     * - use on small text/script/database files that dictate simulation 
     *   behaviors (JSON, INI, etc...) */
    // Actually... Is a CRITICAL flag even necessary?  Why not just assume that all assets that don't have the LAZY flag are all critical assets???
    // , KORL_ASSETCACHE_GET_FLAG_CRITICAL = 1<<1
} Korl_AssetCache_Get_Flags;
typedef enum Korl_AssetCache_Get_Result
    { KORL_ASSETCACHE_GET_RESULT_LOADED
    , KORL_ASSETCACHE_GET_RESULT_PENDING
} Korl_AssetCache_Get_Result;
/**
 * Use this function to obtain an asset.  If the asset has not yet been loaded, 
 * the asset cache will use an appropriate strategy to load the asset from disk 
 * automatically.
 */
#define KORL_FUNCTION_korl_assetCache_get(name) Korl_AssetCache_Get_Result name(const wchar_t*const assetName, Korl_AssetCache_Get_Flags flags, Korl_AssetCache_AssetData* o_assetData)
