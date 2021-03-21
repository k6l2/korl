#pragma once
/* KgtAsset is a procedurally generated data type (via KCPP) */
#include "kcppPolymorphicTaggedUnion.h"
#include "gen_ptu_KgtAsset_includes.h"
#include "kgtAssetCommon.h"
struct KgtAssetManager;
KCPP_POLYMORPHIC_TAGGED_UNION struct KgtAsset
{
	#include "gen_ptu_KgtAsset.h"
	FileWriteTime lastWriteTime;
	bool loaded;
	JobQueueTicket jobTicketLoading;
	/* If an asset depends on other assets in order to function (example: 
		texture assets require a PNG asset to upload the image data to the GPU 
		to be used) then the asset must populate this array with all assets it 
		depends on.  The asset cannot be fully "loaded" until all dependencies 
		are fully loaded (meaning their dependencies are recursively all loaded 
		as well). */
	u8 dependencyCount;
	KgtAssetHandle dependencies[8];
	/* async job function convenience data */
	KgtAssetManager* kam;
	u32 kgtAssetIndex;
};
internal KgtAssetHandle 
	kgt_asset_makeHandle(KgtAssetIndex kai);
internal KgtAssetIndex 
	kgt_asset_findKgtAssetIndex(const char* assetName);
/** 
 * @param data
 * The raw asset data is mutable in case our decode procedures would like to 
 * destructively modify the contents to make parsing easier. */
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL internal void 
	kgt_asset_decode(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
		u8* data, u32 dataBytes, const char* ansiAssetName);
/** This should never be called for assets which have dependencyCount == 0.  For 
 * assets which have dependencies, this should be called only ONCE.  So if a 
 * derived KgtAsset has no dependencies, there is no need to override this 
 * function. */
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL internal void 
	kgt_asset_onDependenciesLoaded(KgtAsset* a, KgtAssetManager* kam);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL internal void 
	kgt_asset_free(KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData);