#pragma once
#include "global-defines.h"
#include "kGeneralAllocator.h"
#include "platform-game-interfaces.h"
#include "gen_kassets.h"
using KAssetHandle = u32;
global_variable const KAssetHandle INVALID_KASSET_HANDLE = ~KAssetHandle(0);
struct KAssetManager;
internal KAssetManager* kamConstruct(KgaHandle allocator, u32 maxAssetHandles, 
                                     KgaHandle assetDataAllocator, 
                                     KmlPlatformApi* kpl, KrbApi* krb);
internal void kamFreeAsset(KAssetManager* kam, KAssetHandle assetHandle);
internal void kamFreeAsset(KAssetManager* kam, KAssetIndex assetIndex);
internal bool kamIsRawSound(KAssetManager* kam, KAssetHandle kah);
internal RawSound kamGetRawSound(KAssetManager* kam, KAssetHandle kahSound);
internal KrbTextureHandle kamGetTexture(KAssetManager* kam, 
                                        KAssetIndex assetIndex);
///TODO: maybe rename this to kamGetImageSize, since texture & image can have 
///	very different implications...
internal v2u32 kamGetTextureSize(KAssetManager* kam, 
                                 KAssetIndex assetIndex);
internal FlipbookMetaData kamGetFlipbookMetaData(KAssetManager* kam, 
                                                 KAssetIndex assetIndex);
internal KAssetHandle kamPushAsset(KAssetManager* kam, KAssetIndex assetIndex);
internal bool kamIsLoadingImages(KAssetManager* kam);
internal bool kamIsLoadingSounds(KAssetManager* kam);
internal void kamUnloadChangedAssets(KAssetManager* kam);