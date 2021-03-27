#include "kgtAssetOggVorbis.h"
internal KORL_CALLBACK_REQUEST_MEMORY(
	_kgt_assetOggVorbis_callbackRequestMemorySampleData)
{
	KgtAllocatorHandle hKgtAllocator = 
		reinterpret_cast<KgtAllocatorHandle>(userData);
	return kgtAllocAlloc(hKgtAllocator, requestedByteCount);
}
internal void kgt_assetOggVorbis_decode(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData, 
	u8* data, u32 dataBytes, const char* ansiAssetName)
{
	a->kgtAssetOggVorbis.rawSound = 
		a->kam->korl->decodeAudioFile(
			data, dataBytes, 
			_kgt_assetOggVorbis_callbackRequestMemorySampleData, 
			hKgtAllocatorAssetData, KorlAudioFileType::OGG_VORBIS);
	korlAssert(a->kgtAssetOggVorbis.rawSound.sampleData);
	a->type = KgtAsset::Type::KGTASSETOGGVORBIS;
}
internal void kgt_assetOggVorbis_free(
	KgtAsset* a, KgtAllocatorHandle hKgtAllocatorAssetData)
{
	kgtAllocFree(
		hKgtAllocatorAssetData, a->kgtAssetOggVorbis.rawSound.sampleData);
	a->kgtAssetOggVorbis.rawSound.sampleData = nullptr;
}
internal RawSound kgt_assetOggVorbis_get(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	const KgtAsset* kgtAsset = assetIndex == KgtAssetIndex::ENUM_SIZE 
		? kgt_assetManager_getDefault(kam, KgtAsset::Type::KGTASSETOGGVORBIS)
		: kgt_assetManager_get(kam, assetIndex);
	korlAssert(kgtAsset->type == KgtAsset::Type::KGTASSETOGGVORBIS);
	return kgtAsset->kgtAssetOggVorbis.rawSound;
}