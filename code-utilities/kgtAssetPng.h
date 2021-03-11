#pragma once
#include "kcppPolymorphicTaggedUnion.h"
#include "platform-game-interfaces.h"/* RawImage */
#include "gen_kgtAssets.h"/* KgtAssetIndex */
KCPP_POLYMORPHIC_TAGGED_UNION_EXTENDS(KgtAsset) struct KgtAssetPng
{
	RawImage rawImage;
};
struct KgtAsset;
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_decode) 
internal void 
	kgt_assetPng_decode(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
		const u8* data, u32 dataBytes);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_free) 
internal void 
	kgt_assetPng_free(KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData);
internal RawImage 
	kgt_assetPng_get(struct KgtAssetManager* kam, KgtAssetIndex assetIndex);