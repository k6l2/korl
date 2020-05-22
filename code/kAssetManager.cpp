#include "kAssetManager.h"
enum class KAssetType : u8
{
	UNUSED,
	RAW_SOUND
};
struct KAsset
{
	KAssetType type;
	u8 assetFileName_PADDING[7];
	const char* assetFileName;
	union 
	{
		RawImage image;
		RawSound sound;
	} assetData;
};
struct KAssetManager
{
	KAssetHandle usedAssetHandles;
	KAssetHandle maxAssetHandles;
	KAssetHandle nextUnusedHandle;
	u8 nextUnusedHandle_PADDING[4];
	KgaHandle assetDataAllocator;
	//KAsset assets[];
};
internal KAssetManager* kamConstruct(KgaHandle allocator, u32 maxAssetHandles, 
                                     KgaHandle assetDataAllocator)
{
	if(maxAssetHandles >= INVALID_KASSET_HANDLE)
	{
		return nullptr;
	}
	KAssetManager*const result = reinterpret_cast<KAssetManager*>(
		kgaAlloc(allocator, sizeof(KAssetManager) + 
		                        sizeof(KAsset)*maxAssetHandles));
	if(result)
	{
		*result = 
			{ .usedAssetHandles = 0
			, .maxAssetHandles = maxAssetHandles
			, .nextUnusedHandle = 0
			, .assetDataAllocator = assetDataAllocator };
	}
	return result;
}
internal KAssetHandle kamAddAsset(KAssetManager* assetManager, 
                                  fnSig_platformLoadWav* platformLoadWav, 
                                  const char* assetFileName)
{
	if(assetManager->usedAssetHandles >= assetManager->maxAssetHandles)
	{
		return INVALID_KASSET_HANDLE;
	}
	///TODO: check to see if this asset already exists in the manager, and if so
	///      just return a handle to it.
	KAsset*const assets = reinterpret_cast<KAsset*>(assetManager + 1);
	const KAssetHandle result = assetManager->nextUnusedHandle;
	assetManager->usedAssetHandles++;
	// Initialize the new RawSound asset //
	assets[result].type            = KAssetType::RAW_SOUND;
	assets[result].assetFileName   = assetFileName;
	assets[result].assetData.sound = 
		platformLoadWav(assetFileName, assetManager->assetDataAllocator);
	// now that we've acquired a KAsset, we can prepare for the next call to 
	//	this function by computing the next unused asset slot //
	if(assetManager->usedAssetHandles < assetManager->maxAssetHandles)
	{
		for(KAssetHandle h = 0; h < assetManager->maxAssetHandles; h++)
		{
			const KAssetHandle nextHandleModMax = 
				(result + h)%assetManager->maxAssetHandles;
			if(assets[nextHandleModMax].type == KAssetType::UNUSED)
			{
				assetManager->nextUnusedHandle = nextHandleModMax;
				break;
			}
		}
		kassert(assetManager->nextUnusedHandle != result);
	}
	return result;
}
internal void kamFreeAsset(KAssetManager* assetManager, 
                           KAssetHandle assetHandle)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(assetManager + 1);
	switch(assets[assetHandle].type)
	{
		case KAssetType::RAW_SOUND:
		{
			kgaFree(assetManager->assetDataAllocator, 
			        assets[assetHandle].assetData.sound.sampleData);
		} break;
		case KAssetType::UNUSED: 
		default:
		{
			KLOG_ERROR("Attempted to free an unused asset handle!");
			return;
		} break;
	}
	assets[assetHandle].type = KAssetType::UNUSED;
	if(assetManager->usedAssetHandles == assetManager->maxAssetHandles)
	{
		assetManager->nextUnusedHandle = assetHandle;
	}
	assetManager->usedAssetHandles--;
}