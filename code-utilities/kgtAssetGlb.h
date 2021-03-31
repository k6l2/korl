#pragma once
#include "kutil.h"
#include "kgtAllocator.h"
#include "kcppPolymorphicTaggedUnion.h"
struct KgtAsset;
struct KgtAssetManager;
KCPP_POLYMORPHIC_TAGGED_UNION_EXTENDS(KgtAsset) struct KgtAssetGlb
{
};
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_decode) 
internal void 
	kgt_assetGlb_decode(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
		u8* data, u32 dataBytes, const char* ansiAssetName);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(
		kgt_asset_onDependenciesLoaded) 
internal void 
	kgt_assetGlb_onDependenciesLoaded(KgtAsset* a, KgtAssetManager* kam);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_free) 
internal void 
	kgt_assetGlb_free(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData);
#if 0
internal ????? 
	kgt_assetGlb_get(KgtAssetManager* kam, KgtAssetIndex assetIndex);
#endif//0