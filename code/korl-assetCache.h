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
#include "korl-interface-platform-assetCache.h"
#include "korl-windows-globalDefines.h"
/** Because we know that the asset manager can follow the singleton pattern, we 
 * also know that "destruction" of the asset manager is only going to happen 
 * once when the program ends.  Ergo, we can simplify this API significantly by 
 * just omitting the traditional OOP "destruction" function.  The memory 
 * resources used by the asset manager will be freed automatically when the 
 * program ends! */
korl_internal void korl_assetCache_initialize(void);
/** Iterate over all assets in the cache which are in the LOADED state, and 
 * check their corresponding file modification timestamp on disk.  If the last 
 * time modified is newer than when it was when we loaded the file, we must: 
 * - kick off another load of the file
 * - call-back to the caller so that other code modules can be notified of this 
 *   event, since code modules which consume assets must then process the assets 
 *   in various ways (generate database diffs, uploading textures to the GPU, 
 *   etc...) */
#define KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(name) void name(acu16 rawUtf16AssetName, Korl_AssetCache_AssetData assetData)
typedef KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(fnSig_korl_assetCache_onAssetHotReloadedCallback);
korl_internal void korl_assetCache_checkAssetObsolescence(fnSig_korl_assetCache_onAssetHotReloadedCallback* callbackOnAssetHotReloaded);
korl_internal void korl_assetCache_clearAllFileHandles(void);
korl_internal void korl_assetCache_defragment(Korl_Memory_AllocatorHandle stackAllocator);
korl_internal u32  korl_assetCache_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer);
korl_internal void korl_assetCache_memoryStateRead(const u8* memoryState);
