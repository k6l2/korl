#include "kAssetManager.h"
enum class KAssetType : u8
{
	UNUSED,
	RAW_IMAGE,
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
	PlatformApi* kpl;
	//KAsset assets[];
};
internal KAssetManager* kamConstruct(KgaHandle allocator, u32 maxAssetHandles, 
                                     KgaHandle assetDataAllocator,
                                     PlatformApi* kpl)
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
			, .assetDataAllocator = assetDataAllocator
			, .kpl = kpl };
	}
	return result;
}
#if 0
internal KAssetHandle kamAddAsset(KAssetManager* kam, KAsset& kAsset)
{
	if(kam->usedAssetHandles >= kam->maxAssetHandles)
	{
		return INVALID_KASSET_HANDLE;
	}
	///TODO: check to see if this asset already exists in the manager, and if so
	///      just return a handle to it.
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	const KAssetHandle result = kam->nextUnusedHandle;
	kam->usedAssetHandles++;
	// Initialize the new asset //
	assets[result] = kAsset;
	// now that we've acquired a KAsset, we can prepare for the next call to 
	//	this function by computing the next unused asset slot //
	if(kam->usedAssetHandles < kam->maxAssetHandles)
	{
		for(KAssetHandle h = 0; h < kam->maxAssetHandles; h++)
		{
			const KAssetHandle nextHandleModMax = 
				(result + h) % kam->maxAssetHandles;
			if(assets[nextHandleModMax].type == KAssetType::UNUSED)
			{
				kam->nextUnusedHandle = nextHandleModMax;
				break;
			}
		}
		kassert(kam->nextUnusedHandle != result);
	}
	return result;
}
internal KAssetHandle kamAddWav(KAssetManager* kam, 
                                const char* assetFileName)
{
	KAsset asset = {};
	asset.type            = KAssetType::RAW_SOUND;
	asset.assetFileName   = assetFileName;
	asset.assetData.sound = kam->kpl->loadWav(assetFileName, 
	                                          kam->assetDataAllocator);
	return kamAddAsset(kam, asset);
}
internal KAssetHandle kamAddOgg(KAssetManager* kam, 
                                const char* assetFileName)
{
	KAsset asset = {};
	asset.type            = KAssetType::RAW_SOUND;
	asset.assetFileName   = assetFileName;
	asset.assetData.sound = kam->kpl->loadOgg(assetFileName, 
	                                          kam->assetDataAllocator);
	return kamAddAsset(kam, asset);
}
internal KAssetHandle kamAddPng(KAssetManager* kam, 
                                const char* assetFileName)
{
	KAsset asset = {};
	asset.type            = KAssetType::RAW_IMAGE;
	asset.assetFileName   = assetFileName;
	asset.assetData.image = kam->kpl->loadPng(assetFileName, 
	                                          kam->assetDataAllocator);
	return kamAddAsset(kam, asset);
}
#endif// 0
internal void kamFreeAsset(KAssetManager* kam, 
                           KAssetHandle assetHandle)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	switch(assets[assetHandle].type)
	{
		case KAssetType::RAW_IMAGE:
		{
			kassert(!"TODO: free image data from KBR!");
			kgaFree(kam->assetDataAllocator, 
			        assets[assetHandle].assetData.image.pixelData);
		} break;
		case KAssetType::RAW_SOUND:
		{
			kgaFree(kam->assetDataAllocator, 
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
	if(kam->usedAssetHandles == kam->maxAssetHandles)
	{
		kam->nextUnusedHandle = assetHandle;
	}
	kam->usedAssetHandles--;
}
internal bool kamIsRawSound(KAssetManager* kam, KAssetHandle kah)
{
	if(kah < kam->maxAssetHandles)
	{
		KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
		return assets[kah].type == KAssetType::RAW_SOUND;
	}
	KLOG(ERROR, "Attempted to access asset handle outside the range [0,%i)!",
	     kam->maxAssetHandles);
	return false;
}
internal RawSound kamGetRawSound(KAssetManager* kam,
                                 KAssetHandle kahSound)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	kassert(kahSound < kam->maxAssetHandles);
	kassert(assets[kahSound].type == KAssetType::RAW_SOUND);
	return assets[kahSound].assetData.sound;
}
internal RawImage kamGetRawImage(KAssetManager* kam,
                                 KAssetHandle kahImage)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	kassert(kahImage < kam->maxAssetHandles);
	kassert(assets[kahImage].type == KAssetType::RAW_IMAGE);
	return assets[kahImage].assetData.image;
}
internal KAssetHandle kamPushAsset(KAssetManager* kam, 
                                   KAssetCStr kAssetCStr)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	const KAssetHandle assetHandle = KASSET_INDEX(kAssetCStr);
	kassert(assetHandle < kam->maxAssetHandles);
	if(assets[assetHandle].type == KAssetType::UNUSED)
	{
		// load the asset from the platform layer //
		switch(KASSET_TYPE(kAssetCStr))
		{
			case KASSET_TYPE_PNG:
			{
				assets[assetHandle].type            = KAssetType::RAW_IMAGE;
				assets[assetHandle].assetFileName   = *kAssetCStr;
				assets[assetHandle].assetData.image = 
					kam->kpl->loadPng(*kAssetCStr, kam->assetDataAllocator);
			}break;
			case KASSET_TYPE_WAV:
			{
				assets[assetHandle].type            = KAssetType::RAW_SOUND;
				assets[assetHandle].assetFileName   = *kAssetCStr;
				assets[assetHandle].assetData.sound = 
					kam->kpl->loadWav(*kAssetCStr, kam->assetDataAllocator);
			}break;
			case KASSET_TYPE_OGG:
			{
				assets[assetHandle].type            = KAssetType::RAW_SOUND;
				assets[assetHandle].assetFileName   = *kAssetCStr;
				assets[assetHandle].assetData.sound = 
					kam->kpl->loadOgg(*kAssetCStr, kam->assetDataAllocator);
			}break;
			case KASSET_TYPE_UNKNOWN:
			default:
			{
				KLOG(ERROR, "Unknown KASSET_TYPE==%i for asset '%s'!",
				     KASSET_TYPE(kAssetCStr), *kAssetCStr);
				return kam->maxAssetHandles;
			}break;
		}
	}
	else
	{
		// asset is already loaded into memory, do nothing? //
	}
	return assetHandle;
}