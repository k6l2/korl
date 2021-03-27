#include "kgtAssetFlipbook.h"
#include "kgtAssetManager.h"
internal void kgt_assetFlipbook_decode(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
	u8* data, u32 dataBytes, const char* ansiAssetName)
{
	char texAssetNameBuffer[64];
	const bool successDecodeFlipbook = 
		kgtFlipBookDecodeMeta(
			data, dataBytes, ansiAssetName, &a->kgtAssetFlipbook.metaData, 
			texAssetNameBuffer, CARRAY_SIZE(texAssetNameBuffer));
	/* find which asset matches the tex asset name, and add this asset as a 
		dependency */
	const KgtAssetIndex kaiTex = 
		kgt_asset_findKgtAssetIndex(texAssetNameBuffer);
	korlAssert(a->dependencyCount == 0);
	a->dependencies[a->dependencyCount++] = kgt_asset_makeHandle(kaiTex);
	/* since the flipbook meta decode function doesn't actually populate the 
		meta data's texture asset handle, we can just set it here */
	a->kgtAssetFlipbook.metaData.kaiTexture = kaiTex;
}
internal void kgt_assetFlipbook_onDependenciesLoaded(
	KgtAsset* a, KgtAssetManager* kam)
{
	/* nothing to do, since this asset doesn't have additional dynamic memory 
		requirements or anything like that */
}
internal void kgt_assetFlipbook_free(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData)
{
	/* once again, since this asset does not own any other dynamic resources (as 
		of now), we can just do nothing here safely */
}
internal KgtFlipBookMetaData kgt_assetFlipbook_get(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	const KgtAsset*const kgtAsset = assetIndex == KgtAssetIndex::ENUM_SIZE 
		? kgt_assetManager_getDefault(kam, KgtAsset::Type::KGTASSETFLIPBOOK)
		: kgt_assetManager_get(kam, assetIndex);
	korlAssert(kgtAsset->type == KgtAsset::Type::KGTASSETFLIPBOOK);
	return kgtAsset->kgtAssetFlipbook.metaData;
}