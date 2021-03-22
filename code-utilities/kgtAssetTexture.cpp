#include "kgtAssetManager.h"
internal void kgt_assetTexture_decode(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
	u8* data, u32 dataBytes, const char* ansiAssetName)
{
	char imageAssetNameBuffer[64];
	const bool successDecode = 
		korlTextureMetaDecode(
			data, dataBytes, ansiAssetName, 
			&a->kgtAssetTexture.metaData, 
			imageAssetNameBuffer, CARRAY_SIZE(imageAssetNameBuffer));
	korlAssert(successDecode);
	/* find which asset matches the image asset name, and add this asset as a 
		dependency */
	const KgtAssetIndex kaiImage = 
		kgt_asset_findKgtAssetIndex(imageAssetNameBuffer);
	korlAssert(a->dependencyCount == 0);
	a->dependencies[a->dependencyCount++] = kgt_asset_makeHandle(kaiImage);
}
internal void 
	kgt_assetTexture_onDependenciesLoaded(KgtAsset* a, KgtAssetManager* kam)
{
	/* get the image asset this texture depends on */
	korlAssert(a->dependencyCount > 0);
	const RawImage img = kgt_assetPng_get(kam, a->dependencies[0]);
	/* upload the image to the GPU */
	a->kgtAssetTexture.hTexture = 
		kam->krb->loadImage(
			img.sizeX, img.sizeY, img.pixelData, img.pixelDataFormat);
	kam->krb->configureTexture(
		a->kgtAssetTexture.hTexture, a->kgtAssetTexture.metaData);
}
internal void kgt_assetTexture_free(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData)
{
	a->kam->krb->deleteTexture(a->kgtAssetTexture.hTexture);
}
internal KrbTextureHandle 
	kgt_assetTexture_get(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	const KgtAsset*const kgtAsset = assetIndex == KgtAssetIndex::ENUM_SIZE 
		? kgt_assetManager_getDefault(kam, KgtAsset::Type::KGTASSETTEXTURE)
		: kgt_assetManager_get(kam, assetIndex);
	korlAssert(kgtAsset->type == KgtAsset::Type::KGTASSETTEXTURE);
	korlAssert(kgtAsset->kgtAssetTexture.hTexture);
	return kgtAsset->kgtAssetTexture.hTexture;
}