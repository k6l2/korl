#pragma once
#include "kcppPolymorphicTaggedUnion.h"
#include "platform-game-interfaces.h"
struct KgtAsset;
KCPP_POLYMORPHIC_TAGGED_UNION_EXTENDS(KgtAsset) struct KgtAssetRawImage
{
	RawImage rawImage;
};
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_decode) 
internal void 
	kgt_assetRawImage_decode(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
		const u8* data, u32 dataBytes);