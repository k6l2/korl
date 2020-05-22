#pragma once
#include "global-defines.h"
#include "generalAllocator.h"
#include "platform-game-interfaces.h"
using KAssetHandle = u32;
global_variable const KAssetHandle INVALID_KASSET_HANDLE = ~KAssetHandle(0);
struct KAssetManager;
internal KAssetManager* kamConstruct(KgaHandle allocator, u32 maxAssetHandles, 
                                     KgaHandle assetDataAllocator);
internal KAssetHandle kamAddAsset(KAssetManager* assetManager, 
                                  fnSig_platformLoadWav* platformLoadWav, 
                                  const char* assetFileName);
internal void kamFreeAsset(KAssetManager* assetManager, 
                           KAssetHandle assetHandle);
internal bool kamIsRawSound(KAssetManager* assetManager, KAssetHandle kah);
internal RawSound kamGetRawSound(KAssetManager* assetManager,
                                 KAssetHandle kahSound);