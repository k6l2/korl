#pragma once
#include "global-defines.h"
#include "kGeneralAllocator.h"
#include "platform-game-interfaces.h"
using KAssetHandle = u32;
global_variable const KAssetHandle INVALID_KASSET_HANDLE = ~KAssetHandle(0);
struct KAssetManager;
internal KAssetManager* kamConstruct(KgaHandle allocator, u32 maxAssetHandles, 
                                     KgaHandle assetDataAllocator, 
                                     KmlPlatformApi* kpl, KrbApi* krb);
#if 0
internal KAssetHandle kamAddWav(KAssetManager* kam, const char* assetFileName);
internal KAssetHandle kamAddOgg(KAssetManager* kam, const char* assetFileName);
internal KAssetHandle kamAddPng(KAssetManager* kam, const char* assetFileName);
#endif// 0
internal void kamFreeAsset(KAssetManager* kam, KAssetHandle assetHandle);
internal void kamFreeAsset(KAssetManager* kam, KAssetCStr kAssetCStr);
internal bool kamIsRawSound(KAssetManager* kam, KAssetHandle kah);
internal RawSound kamGetRawSound(KAssetManager* kam, KAssetHandle kahSound);
#if 0
internal RawImage kamGetRawImage(KAssetManager* kam, KAssetHandle kahImage);
#endif// 0
internal KrbTextureHandle kamGetTexture(KAssetManager* kam, 
                                        KAssetCStr kAssetCStr);
internal v2u32 kamGetTextureSize(KAssetManager* kam, 
                                 KAssetCStr kAssetCStr);
internal FlipbookMetaData kamGetFlipbookMetaData(KAssetManager* kam, 
                                                 KAssetCStr kAssetCStr);
internal KAssetHandle kamPushAsset(KAssetManager* kam, KAssetCStr kAssetCStr);
internal bool kamIsLoadingImages(KAssetManager* kam);
internal bool kamIsLoadingSounds(KAssetManager* kam);
internal void kamUnloadChangedAssets(KAssetManager* kam);