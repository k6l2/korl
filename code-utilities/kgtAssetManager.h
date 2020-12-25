/*
 * User code must define the following global variables to use this module:
 *  - KrbApi*          g_krb
 *  - KorlPlatformApi* g_kpl
 */
#pragma once
#include "kutil.h"
#include "kgtAllocator.h"
#include "platform-game-interfaces.h"
#include "gen_kgtAssets.h"
#include "kgtFlipBook.h"
#include "kgtSpriteFont.h"
using KgtAssetHandle = u32;
struct KgtAssetManager;
struct KgtAssetManagerByteArray
{
	const u8* data;
	u32 bytes;
};
internal KgtAssetManager* 
	kgtAssetManagerConstruct(
		KgtAllocatorHandle allocator, u32 maxAssetHandles, 
		KgtAllocatorHandle assetDataAllocator, KrbApi* krbApi);
internal void 
	kgtAssetManagerFreeAsset(KgtAssetManager* kam, KgtAssetHandle assetHandle);
internal void 
	kgtAssetManagerFreeAsset(KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal KgtAssetManagerByteArray 
	kgtAssetManagerGetBinary(KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal RawSound 
	kgtAssetManagerGetRawSound(KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal RawImage 
	kgtAssetManagerGetRawImage(KgtAssetManager* kam, KgtAssetIndex assetIndex);
/**
 * This function can be called successfully for RAW_IMAGE & TEXTURE_META assets.
 */
internal KrbTextureHandle 
	kgtAssetManagerGetTexture(KgtAssetManager* kam, KgtAssetIndex assetIndex);
/**
 * If a RAW_IMAGE asset is requested, this function will return the default 
 * texture meta data asset.
 */
internal KorlTextureMetaData
	kgtAssetManagerGetTextureMetaData(
		KgtAssetManager* kam, KgtAssetIndex assetIndex);
/**
 * This function can be called successfully for RAW_IMAGE & TEXTURE_META assets.
 */
internal v2u32 
	kgtAssetManagerGetRawImageSize(
		KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal KgtFlipBookMetaData 
	kgtAssetManagerGetFlipBookMetaData(
		KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal const KgtSpriteFontMetaData& 
	kgtAssetManagerGetSpriteFontMetaData(
		KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal KgtAssetHandle 
	kgtAssetManagerPushAsset(KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal bool 
	kgtAssetManagerIsLoadingImages(KgtAssetManager* kam);
internal bool 
	kgtAssetManagerIsLoadingSounds(KgtAssetManager* kam);
/**
 * @return the # of assets that were unloaded
 */
internal u32 
	kgtAssetManagerUnloadChangedAssets(KgtAssetManager* kam);
internal void 
	kgtAssetManagerUnloadAllAssets(KgtAssetManager* kam);
internal void 
	kgtAssetManagerPushAllKgtAssets(KgtAssetManager* kam);
