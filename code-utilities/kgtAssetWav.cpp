#include "kgtAssetWav.h"
#include "kgtAssetManager.h"
internal KORL_CALLBACK_REQUEST_MEMORY(
	_kgt_assetWav_callbackRequestMemorySampleData)
{
	KgtAllocatorHandle hKgtAllocator = 
		reinterpret_cast<KgtAllocatorHandle>(userData);
	return kgtAllocAlloc(hKgtAllocator, requestedByteCount);
}
internal void kgt_assetWav_decode(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
	u8* data, u32 dataBytes, const char* ansiAssetName)
{
	a->kgtAssetWav.rawSound = 
		a->kam->korl->decodeAudioFile(
			data, dataBytes, _kgt_assetWav_callbackRequestMemorySampleData, 
			hKgtAllocatorAssetData, KorlAudioFileType::WAVE);
	korlAssert(a->kgtAssetWav.rawSound.sampleData);
	a->type = KgtAsset::Type::KGTASSETWAV;
}
internal void kgt_assetWav_free(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData)
{
	kgtAllocFree(hKgtAllocatorAssetData, a->kgtAssetWav.rawSound.sampleData);
	a->kgtAssetWav.rawSound.sampleData = nullptr;
}
internal RawSound kgt_assetWav_get(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	const KgtAsset* kgtAsset = assetIndex == KgtAssetIndex::ENUM_SIZE 
		? kgt_assetManager_getDefault(kam, KgtAsset::Type::KGTASSETWAV)
		: kgt_assetManager_get(kam, assetIndex);
	korlAssert(kgtAsset->type == KgtAsset::Type::KGTASSETWAV);
	return kgtAsset->kgtAssetWav.rawSound;
}