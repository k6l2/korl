#include "kgtAssetRawImage.h"
internal KORL_CALLBACK_REQUEST_MEMORY(
	_kgt_assetRawImage_callbackRequestMemoryPixelData)
{
	KgtAllocatorHandle hKgtAllocator = 
		reinterpret_cast<KgtAllocatorHandle>(userData);
	return kgtAllocAlloc(hKgtAllocator, requestedByteCount);
}
internal void kgt_assetRawImage_decode(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
	const u8* data, u32 dataBytes)
{
	a->kgtAssetRawImage.rawImage = 
		g_kpl->decodePng(
			data, dataBytes, _kgt_assetRawImage_callbackRequestMemoryPixelData, 
			hKgtAllocatorAssetData);
	korlAssert(a->kgtAssetRawImage.rawImage.pixelData);
	a->type = KgtAsset::Type::KGTASSETRAWIMAGE;
}