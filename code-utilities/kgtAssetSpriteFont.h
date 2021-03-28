#pragma once
#include "kgtSpriteFont.h"
#include "kcppPolymorphicTaggedUnion.h"
struct KgtAsset;
struct KgtAssetManager;
KCPP_POLYMORPHIC_TAGGED_UNION_EXTENDS(KgtAsset) struct KgtAssetSpriteFont
{
	KgtSpriteFontMetaData metaData;
};
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_decode) 
internal void 
	kgt_assetSpriteFont_decode(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
		u8* data, u32 dataBytes, const char* ansiAssetName);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(
		kgt_asset_onDependenciesLoaded) 
internal void 
	kgt_assetSpriteFont_onDependenciesLoaded(KgtAsset* a, KgtAssetManager* kam);
KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(kgt_asset_free) 
internal void 
	kgt_assetSpriteFont_free(
		KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData);
internal KgtSpriteFontMetaData 
	kgt_assetSpriteFont_get(KgtAssetManager* kam, KgtAssetIndex assetIndex);
