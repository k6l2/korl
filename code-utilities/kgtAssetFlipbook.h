#pragma once
#include "kgtFlipBook.h"
#include "kcppPolymorphicTaggedUnion.h"
struct KgtAsset;
struct KgtAssetManager;
KCPP_POLYMORPHIC_TAGGED_UNION_EXTENDS(KgtAsset) struct KgtAssetFlipbook
{
	KgtFlipBookMetaData metaData;
};
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_decode) 
internal void 
	kgt_assetFlipbook_decode(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
		u8* data, u32 dataBytes, const char* ansiAssetName);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(
		kgt_asset_onDependenciesLoaded) 
internal void 
	kgt_assetFlipbook_onDependenciesLoaded(KgtAsset* a, KgtAssetManager* kam);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_free) 
internal void 
	kgt_assetFlipbook_free(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData);
internal KgtFlipBookMetaData 
	kgt_assetFlipbook_get(KgtAssetManager* kam, KgtAssetIndex assetIndex);