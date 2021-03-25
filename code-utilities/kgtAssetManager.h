#pragma once
#include "kutil.h"
#include "kgtAllocator.h"
#include "platform-game-interfaces.h"
/* KgtAssetManager data structure & API */
#include "gen_kgtAssets.h"
#include "kgtAsset.h"
struct KgtAssetManager
{
	const KorlPlatformApi* korl;
	const KrbApi* krb;
	KgtAssetHandle maxAssetHandles;
	/* this allocator is where all the decoded asset data is stored, such as the 
		array of pixels in a RawImage asset */
	KgtAllocatorHandle hKgtAllocatorAssetData;
	/* raw file data should be a transient storage where file data rests until 
		it is decoded into a useful KgtAsset */
	KgtAllocatorHandle hKgtAllocatorRawFiles;
	/* asset data allocation will be asynchronous, since it will occur on a 
		separate thread as an async job, so we require a lock for safety */
	KplLockHandle hLockAssetDataAllocator;
	struct AssetDescriptor
	{
		char fileExtension[8];
		u8 fileExtensionSize;
		KgtAsset defaultAsset;
		/* the KgtAsset::Type is implied based on the array index */
	} assetDescriptors[static_cast<u32>(KgtAsset::Type::ENUM_COUNT)];
	/* This array size is variable & determined in the construct function.  The 
		`maxAssetHandles` data member should equal the # of elements. */
	KgtAsset assets[1];
public:
	/* debugger symbols don't load without this */
	KgtAssetManager() {}
private:
	/* disable copy/assignment since our struct refers to dynamic memory */
	KgtAssetManager(const KgtAssetManager&);
	KgtAssetManager& operator=(const KgtAssetManager&);
};
internal KgtAssetManager* 
	kgt_assetManager_construct(
		KgtAllocatorHandle hKgtAllocator, KgtAssetHandle maxAssetHandles, 
		KgtAllocatorHandle hKgtAllocatorAssetData, 
		const KorlPlatformApi* korl, const KrbApi* krb);
/** The user of the AssetManager must specify what file extension corresponds to 
 * what KgtAsset::Type using this API.  We must also provide raw file data of 
 * the default asset of this type.  We don't need to provide the loading 
 * function here though, because this should be handled automatically by the 
 * KgtAsset polymorphic tagged union inheritance via virtual override!  Make 
 * sure to use this API immediately after construction & before any assets get 
 * loaded!!!  Caller must be careful to ensure this happens because this 
 * function does NOT keep the asset data allocator thread-safe. */
internal void 
	kgt_assetManager_addAssetDescriptor(
		KgtAssetManager* kam, KgtAsset::Type type, 
		const char*const fileExtension, 
		u8* rawDefaultAssetData, u32 rawDefaultAssetDataBytes);
internal KgtAssetHandle 
	kgt_assetManager_load(KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal void 
	kgt_assetManager_free(KgtAssetManager* kam, KgtAssetIndex assetIndex);
/** 
 * @return a default asset if `assetIndex` has not yet been loaded */
internal const KgtAsset*
	kgt_assetManager_get(KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal const KgtAsset*
	kgt_assetManager_get(KgtAssetManager* kam, KgtAssetHandle hAsset);
internal const KgtAsset* 
	kgt_assetManager_getDefault(KgtAssetManager* kam, KgtAsset::Type assetType);
internal void 
	kgt_assetManager_loadAllStaticAssets(KgtAssetManager* kam);
internal void 
	kgt_assetManager_reloadChangedAssets(KgtAssetManager* kam);
#if 0
/*
 * User code must define the following global variables to use this module:
 *  - KrbApi*          g_krb
 *  - KorlPlatformApi* g_kpl
 */
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
	kgtAssetManagerGetImageSize(
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
#endif//0
