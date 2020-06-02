#pragma once
#include "global-defines.h"
#include "generalAllocator.h"
#include "platform-game-interfaces.h"
using KAssetHandle = u32;
global_variable const KAssetHandle INVALID_KASSET_HANDLE = ~KAssetHandle(0);
struct KAssetManager;
internal KAssetManager* kamConstruct(KgaHandle allocator, u32 maxAssetHandles, 
                                     KgaHandle assetDataAllocator, 
                                     PlatformApi* kpl);
#if 0
internal KAssetHandle kamAddWav(KAssetManager* kam, const char* assetFileName);
internal KAssetHandle kamAddOgg(KAssetManager* kam, const char* assetFileName);
internal KAssetHandle kamAddPng(KAssetManager* kam, const char* assetFileName);
#endif// 0
internal void kamFreeAsset(KAssetManager* kam, KAssetHandle assetHandle);
internal bool kamIsRawSound(KAssetManager* kam, KAssetHandle kah);
internal RawSound kamGetRawSound(KAssetManager* kam, KAssetHandle kahSound);
internal RawImage kamGetRawImage(KAssetManager* kam, KAssetHandle kahImage);
internal KAssetHandle kamPushAsset(KAssetManager* kam, KAssetCStr kAssetCStr);