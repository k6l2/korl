/**
 * The asset manager's job consists of the following operations:
 *    - load files (assets) from disk
 *    - manage memory of loaded files
 *        - eventually internally defragment memory which assets occupy
 *        - eventually support the ability to free assets which are no longer in 
 *          use (either automatically, or via some manual API)
 *    - coordinate with other components to transform loaded raw files into more 
 *      usable data (example: transform raw image into a texture)
 *    - eventually support the ability to do all this asynchronously
 *    - eventually support the ability to load files from an archive
 *    - eventually support one or more asset compression codecs
 * There is only ever going to be one asset manager for the entire application, 
 * so it seems okay to simplify the API by keeping an internal instance of the 
 * asset manager struct (for now at least...).
 */
#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_AssetCache_AssetData
{
    void* data;
    u32 dataBytes;
} Korl_AssetCache_AssetData;
typedef enum Korl_AssetCache_Get_Flags
{
    KORL_ASSETCACHE_GET_FLAGS_NONE = 0, 
    /**
     * Tell the asset manager that it is not necessary to load the asset 
     * immediately.  This will allow us to load the asset asynchronously.  If 
     * this flag is set, the caller is responsible for calling "get" on 
     * subsequent frames to see if the asset has finished loading.
     */
    KORL_ASSETCACHE_GET_FLAGS_LAZY = 1<<0
} Korl_AssetCache_Get_Flags;
/**
 * Because we know that the asset manager can follow the singleton pattern, we 
 * also know that "destruction" of the asset manager is only going to happen 
 * once when the program ends.  Ergo, we can simplify this API significantly by 
 * just omitting the traditional OOP "destruction" function.  The memory 
 * resources used by the asset manager will be freed automatically when the 
 * program ends!
 */
korl_internal void korl_assetCache_initialize(void);
/**
 * Use this function to obtain an asset.  If the asset has not yet been loaded, 
 * the asset manager will use an appropriate strategy to load the asset 
 * automatically.
 * \return If the asset was not loaded, the result's \c data pointer will be 
 * \c NULL .  The caller can be assured that if the 
 * \c KORL_ASSETCACHE_GET_FLAGS_LAZY flag is not set, the asset is guaranteed 
 * to be loaded when the function call is complete.
 */
korl_internal Korl_AssetCache_AssetData korl_assetCache_get(
    const wchar_t*const assetName, Korl_AssetCache_Get_Flags flags);
