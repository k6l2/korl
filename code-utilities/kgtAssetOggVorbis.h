#pragma once
#include "kcppPolymorphicTaggedUnion.h"
#include "platform-game-interfaces.h"/* RawSound */
#include "gen_kgtAssets.h"/* KgtAssetIndex */
#include "kgtAssetCommon.h"/* KgtAssetHandle */
KCPP_POLYMORPHIC_TAGGED_UNION_EXTENDS(KgtAsset) struct KgtAssetOggVorbis
{
	RawSound rawSound;
};
struct KgtAsset;
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_decode) 
internal void 
	kgt_assetOggVorbis_decode(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
		u8* data, u32 dataBytes, const char* ansiAssetName);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_free) 
internal void 
	kgt_assetOggVorbis_free(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData);
internal RawSound 
	kgt_assetOggVorbis_get(KgtAssetManager* kam, KgtAssetIndex assetIndex);