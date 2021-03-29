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
