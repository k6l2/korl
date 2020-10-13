#pragma once
#include "kutil.h"
#include "kAllocator.h"
#include "platform-game-interfaces.h"
#include "gen_kassets.h"
#include "kFlipBook.h"
using KAssetHandle = u32;
global_variable const KAssetHandle INVALID_KASSET_HANDLE = ~KAssetHandle(0);
struct KAssetManager;
internal KAssetManager* 
	kamConstruct(
		KAllocatorHandle allocator, u32 maxAssetHandles, 
		KAllocatorHandle assetDataAllocator, KmlPlatformApi* kpl, KrbApi* krb);
internal void 
	kamFreeAsset(KAssetManager* kam, KAssetHandle assetHandle);
internal void 
	kamFreeAsset(KAssetManager* kam, KAssetIndex assetIndex);
internal RawSound 
	kamGetRawSound(KAssetManager* kam, KAssetIndex assetIndex);
/**
 * This function can be called successfully for RAW_IMAGE & TEXTURE_META assets.
 */
internal KrbTextureHandle 
	kamGetTexture(KAssetManager* kam, KAssetIndex assetIndex);
/**
 * If a RAW_IMAGE asset is requested, this function will return the default 
 * texture meta data asset.
 */
internal KorlTextureMetaData
	kamGetTextureMetaData(KAssetManager* kam, KAssetIndex assetIndex);
/**
 * This function can be called successfully for RAW_IMAGE & TEXTURE_META assets.
 */
internal v2u32 
	kamGetImageSize(KAssetManager* kam, KAssetIndex assetIndex);
internal FlipbookMetaData 
	kamGetFlipbookMetaData(
		KAssetManager* kam, KAssetIndex assetIndex);
internal KAssetHandle 
	kamPushAsset(KAssetManager* kam, KAssetIndex assetIndex);
internal bool 
	kamIsLoadingImages(KAssetManager* kam);
internal bool 
	kamIsLoadingSounds(KAssetManager* kam);
/**
 * @return the # of assets that were unloaded
 */
internal u32 
	kamUnloadChangedAssets(KAssetManager* kam);
internal void 
	kamUnloadAllAssets(KAssetManager* kam);
internal void 
	kamPushAllKAssets(KAssetManager* kam);
