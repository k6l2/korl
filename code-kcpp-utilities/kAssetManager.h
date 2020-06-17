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
internal void kamFreeAsset(KAssetManager* kam, KAssetHandle assetHandle);
internal void kamFreeAsset(KAssetManager* kam, KAssetCStr kAssetCStr);
internal bool kamIsRawSound(KAssetManager* kam, KAssetHandle kah);
internal RawSound kamGetRawSound(KAssetManager* kam, KAssetHandle kahSound);
internal KrbTextureHandle kamGetTexture(KAssetManager* kam, 
                                        KAssetCStr kAssetCStr);
///TODO: maybe rename this to kamGetImageSize, since texture & image can have 
///	very different implications...
internal v2u32 kamGetTextureSize(KAssetManager* kam, 
                                 KAssetCStr kAssetCStr);
internal FlipbookMetaData kamGetFlipbookMetaData(KAssetManager* kam, 
                                                 KAssetCStr kAssetCStr);
internal KAssetHandle kamPushAsset(KAssetManager* kam, KAssetCStr kAssetCStr);
internal bool kamIsLoadingImages(KAssetManager* kam);
internal bool kamIsLoadingSounds(KAssetManager* kam);
internal void kamUnloadChangedAssets(KAssetManager* kam);