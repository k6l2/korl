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
	const u8* data, u32 dataBytes)
{
	a->kgtAssetPng.rawImage = 
		a->kam->korl->decodePng(
			data, dataBytes, _kgt_assetPng_callbackRequestMemoryPixelData, 
			hKgtAllocatorAssetData);
	korlAssert(a->kgtAssetPng.rawImage.pixelData);
	a->type = KgtAsset::Type::KGTASSETPNG;
}