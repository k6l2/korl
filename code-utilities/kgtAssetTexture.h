#pragma once
#include "kcppPolymorphicTaggedUnion.h"
#include "platform-game-interfaces.h"/* KorlTexture*, RawImage */
#include "gen_kgtAssets.h"/* KgtAssetIndex */
KCPP_POLYMORPHIC_TAGGED_UNION_EXTENDS(KgtAsset) struct KgtAssetTexture
{
	KorlTextureMetaData metaData;
	KrbTextureHandle hTexture;
};
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_decode) 
internal void 
	kgt_assetTexture_decode(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
		u8* data, u32 dataBytes, const char* ansiAssetName);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(
		kgt_asset_onDependenciesLoaded) 
internal void 
	kgt_assetTexture_onDependenciesLoaded(KgtAsset* a, KgtAssetManager* kam);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_free) 
internal void 
	kgt_assetTexture_free(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData);
internal KrbTextureHandle 
	kgt_assetTexture_get(KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal RawImage 
	kgt_assetTexture_getRawImage(
		KgtAssetManager* kam, KgtAssetIndex assetIndex);
internal v2u32 
	kgt_assetTexture_getSize(KgtAssetManager* kam, KgtAssetIndex assetIndex);
