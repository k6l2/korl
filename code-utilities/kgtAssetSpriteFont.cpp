#include "kgtAssetSpriteFont.h"
#include "kgtAssetManager.h"
internal void kgt_assetSpriteFont_decode(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
	u8* data, u32 dataBytes, const char* ansiAssetName)
{
	char assetNameBufferTex[64];
	char assetNameBufferTexOutline[64];
	const bool successDecodeSpriteFont = 
		kgtSpriteFontDecodeMeta(
			data, dataBytes, ansiAssetName, &a->kgtAssetSpriteFont.metaData, 
			assetNameBufferTex, CARRAY_SIZE(assetNameBufferTex), 
			assetNameBufferTexOutline, CARRAY_SIZE(assetNameBufferTexOutline));
	if(!successDecodeSpriteFont)
	{
		KLOG(ERROR, "Decode sprite font meta data filed!");
		return;
	}
	/* match the texture asset names to KgtAssetIndex values */
	const KgtAssetIndex kaiTex = 
		kgt_asset_findKgtAssetIndex(assetNameBufferTex);
	const KgtAssetIndex kaiTexOutline = 
		kgt_asset_findKgtAssetIndex(assetNameBufferTexOutline);
	korlAssert(a->dependencyCount == 0);
	a->dependencies[a->dependencyCount++] = kgt_asset_makeHandle(kaiTex);
	a->dependencies[a->dependencyCount++] = kgt_asset_makeHandle(kaiTexOutline);
	/* populate the meta data's texture asset handles here */
	a->kgtAssetSpriteFont.metaData.kaiTexture        = kaiTex;
	a->kgtAssetSpriteFont.metaData.kaiTextureOutline = kaiTexOutline;
}
internal void kgt_assetSpriteFont_onDependenciesLoaded(
	KgtAsset* a, KgtAssetManager* kam)
{
	/* we don't own any dynamic resources, so do nothing */
}
internal void kgt_assetSpriteFont_free(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData)
{
	/* we don't own any dynamic resources, so do nothing */
}
internal KgtSpriteFontMetaData kgt_assetSpriteFont_get(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	const KgtAsset*const kgtAsset = assetIndex == KgtAssetIndex::ENUM_SIZE 
		? kgt_assetManager_getDefault(kam, KgtAsset::Type::KGTASSETSPRITEFONT)
		: kgt_assetManager_get(kam, assetIndex);
	korlAssert(kgtAsset->type == KgtAsset::Type::KGTASSETSPRITEFONT);
	return kgtAsset->kgtAssetSpriteFont.metaData;
}
