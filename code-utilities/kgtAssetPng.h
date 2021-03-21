#pragma once
#include "kcppPolymorphicTaggedUnion.h"
#include "platform-game-interfaces.h"/* RawImage */
#include "gen_kgtAssets.h"/* KgtAssetIndex */
#include "kgtAssetCommon.h"/* KgtAssetHandle */
KCPP_POLYMORPHIC_TAGGED_UNION_EXTENDS(KgtAsset) struct KgtAssetPng
{
	RawImage rawImage;
};
struct KgtAsset;
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_decode) 
internal void 
	kgt_assetPng_decode(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
		u8* data, u32 dataBytes, const char* ansiAssetName);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_free) 
internal void 
	kgt_assetPng_free(KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData);
internal RawImage 
	kgt_assetPng_get(KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal RawImage 
	kgt_assetPng_get(KgtAssetManager* kam, KgtAssetHandle hAsset);