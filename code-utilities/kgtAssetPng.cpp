#include "kgtAssetManager.h"
#include "kgtAssetPng.h"
internal KORL_CALLBACK_REQUEST_MEMORY(
	_kgt_assetPng_callbackRequestMemoryPixelData)
{
	KgtAllocatorHandle hKgtAllocator = 
		reinterpret_cast<KgtAllocatorHandle>(userData);
	return kgtAllocAlloc(hKgtAllocator, requestedByteCount);
}
internal void kgt_assetPng_decode(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
	u8* data, u32 dataBytes, const char* ansiAssetName)
{
	a->kgtAssetPng.rawImage = 
		a->kam->korl->decodePng(
			data, dataBytes, _kgt_assetPng_callbackRequestMemoryPixelData, 
			hKgtAllocatorAssetData);
	korlAssert(a->kgtAssetPng.rawImage.pixelData);
	a->type = KgtAsset::Type::KGTASSETPNG;
}
internal void kgt_assetPng_free(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData)
{
	kgtAllocFree(hKgtAllocatorAssetData, a->kgtAssetPng.rawImage.pixelData);
}
internal RawImage kgt_assetPng_get(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	const KgtAsset* kgtAsset = assetIndex == KgtAssetIndex::ENUM_SIZE 
		? kgt_assetManager_getDefault(kam, KgtAsset::Type::KGTASSETPNG)
		: kgt_assetManager_get(kam, assetIndex);
	korlAssert(kgtAsset->type == KgtAsset::Type::KGTASSETPNG);
	return kgtAsset->kgtAssetPng.rawImage;
}
internal RawImage kgt_assetPng_get(KgtAssetManager* kam, KgtAssetHandle hAsset)
{
	const KgtAsset*const kgtAsset = !hAsset 
		? kgt_assetManager_getDefault(kam, KgtAsset::Type::KGTASSETPNG)
		: kgt_assetManager_get(kam, hAsset);
	korlAssert(kgtAsset->type == KgtAsset::Type::KGTASSETPNG);
	return kgtAsset->kgtAssetPng.rawImage;
}