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
internal KAssetHandle kamAddAsset(KAssetManager* assetManager, KAsset& kAsset)
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
	// Initialize the new asset //
	assets[result] = kAsset;
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
internal KAssetHandle kamAddWav(KAssetManager* assetManager, 
                                fnSig_platformLoadWav* platformLoadWav, 
                                const char* assetFileName)
{
	KAsset asset = {};
	asset.type            = KAssetType::RAW_SOUND;
	asset.assetFileName   = assetFileName;
	asset.assetData.sound = platformLoadWav(assetFileName, 
	                                        assetManager->assetDataAllocator);
	return kamAddAsset(assetManager, asset);
}
internal KAssetHandle kamAddOgg(KAssetManager* assetManager, 
                                fnSig_platformLoadOgg* platformLoadOgg, 
                                const char* assetFileName)
{
	KAsset asset = {};
	asset.type            = KAssetType::RAW_SOUND;
	asset.assetFileName   = assetFileName;
	asset.assetData.sound = platformLoadOgg(assetFileName, 
	                                        assetManager->assetDataAllocator);
	return kamAddAsset(assetManager, asset);
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
			KLOG(ERROR, "Attempted to free an unused asset handle!");
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
internal bool kamIsRawSound(KAssetManager* assetManager, KAssetHandle kah)
{
	if(kah < assetManager->maxAssetHandles)
	{
		KAsset*const assets = reinterpret_cast<KAsset*>(assetManager + 1);
		return assets[kah].type == KAssetType::RAW_SOUND;
	}
	KLOG(ERROR, "Attempted to access asset handle outside the range [0,%i)!",
	     assetManager->maxAssetHandles);
	return false;
}
internal RawSound kamGetRawSound(KAssetManager* assetManager,
                                 KAssetHandle kahSound)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(assetManager + 1);
	kassert(kahSound < assetManager->maxAssetHandles);
	kassert(assets[kahSound].type == KAssetType::RAW_SOUND);
	return assets[kahSound].assetData.sound;
}