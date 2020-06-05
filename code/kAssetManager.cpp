#include "kAssetManager.h"
#include "z85-png-default.h"
#include "z85-wav-default.h"
enum class KAssetType : u8
{
	UNUSED,
	RAW_IMAGE,
	RAW_SOUND
};
struct KAsset
{
	FileWriteTime lastWriteTime;
	KAssetType type;
	bool loaded;
	u8 loaded_PADDING[2];
	JobQueueTicket jqTicketLoading;
	const char* assetFileName;
	union 
	{
		struct 
		{
			RawImage rawImage;
			KrbTextureHandle krbTextureHandle;
			u8 krbTextureHandle_PADDING[4];
		} image;
		RawSound sound;
	} assetData;
	// This pointer is here only for convenience with respect to asynchronous 
	//	job posting to the platform layer.
	KAssetManager* kam;
};
struct KAssetManager
{
	KAssetHandle usedAssetHandles;
	KAssetHandle maxAssetHandles;
	KAssetHandle nextUnusedHandle;
	u8 nextUnusedHandle_PADDING[4];
	KAsset defaultAssetImage;
	KAsset defaultAssetSound;
	KgaHandle assetDataAllocator;
	PlatformApi* kpl;
	KrbApi* krb;
	//KAsset assets[];
};
internal KAssetManager* kamConstruct(KgaHandle allocator, u32 maxAssetHandles, 
                                     KgaHandle assetDataAllocator,
                                     PlatformApi* kpl, KrbApi* krb)
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
		// Quickly load very tiny default assets which can be immediately used
		//	in place of any asset handle which has not yet been loaded from the
		//	platform and decoded into useful data for us yet! //
		KAsset defaultAssetImage;
		defaultAssetImage.type = KAssetType::RAW_IMAGE;
		defaultAssetImage.assetFileName = "z85_png_default";
		defaultAssetImage.assetData.image.rawImage = 
			kpl->decodeZ85Png(z85_png_default, 
			                  CARRAY_COUNT(z85_png_default) - 1,
			                  assetDataAllocator);
		defaultAssetImage.assetData.image.krbTextureHandle = krb->loadImage(
			defaultAssetImage.assetData.image.rawImage.sizeX,
			defaultAssetImage.assetData.image.rawImage.sizeY,
			defaultAssetImage.assetData.image.rawImage.pixelData);
		KAsset defaultAssetSound;
		defaultAssetSound.type = KAssetType::RAW_SOUND;
		defaultAssetSound.assetFileName = "z85_wav_default";
		defaultAssetSound.assetData.sound =
			kpl->decodeZ85Wav(z85_wav_default, 
			                  CARRAY_COUNT(z85_wav_default) - 1,
			                  assetDataAllocator);
		*result = 
			{ .usedAssetHandles = 0
			, .maxAssetHandles = maxAssetHandles
			, .nextUnusedHandle = 0
			, .defaultAssetImage = defaultAssetImage
			, .defaultAssetSound = defaultAssetSound
			, .assetDataAllocator = assetDataAllocator
			, .kpl = kpl
			, .krb = krb };
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
internal void kamFreeAsset(KAssetManager* kam, KAssetHandle assetHandle)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	kassert(assetHandle < kam->maxAssetHandles);
	KAsset*const asset = assets + assetHandle;
	kassert(asset->loaded);
	switch(asset->type)
	{
		case KAssetType::RAW_IMAGE:
		{
			kam->krb->deleteTexture(asset->assetData.image.krbTextureHandle);
			kgaFree(kam->assetDataAllocator, 
			        asset->assetData.image.rawImage.pixelData);
		} break;
		case KAssetType::RAW_SOUND:
		{
			kgaFree(kam->assetDataAllocator, 
			        asset->assetData.sound.sampleData);
		} break;
		case KAssetType::UNUSED: 
		default:
		{
			KLOG(ERROR, "Attempted to free an unused asset handle!");
			return;
		} break;
	}
	asset->type   = KAssetType::UNUSED;
	asset->loaded = false;
	if(kam->usedAssetHandles == kam->maxAssetHandles)
	{
		kam->nextUnusedHandle = assetHandle;
	}
	kam->usedAssetHandles--;
}
internal void kamFreeAsset(KAssetManager* kam, KAssetCStr kAssetCStr)
{
	const KAssetHandle assetHandle = KASSET_INDEX(kAssetCStr);
	kamFreeAsset(kam, assetHandle);
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
internal void kamOnLoadingJobFinished(KAssetManager* kam, KAssetHandle kah)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	kassert(kah < kam->maxAssetHandles);
	KAsset*const asset = assets + kah;
	switch(asset->type)
	{
		case KAssetType::RAW_IMAGE:
		{
			kassert(asset->assetData.image.krbTextureHandle == 
				krb::INVALID_TEXTURE_HANDLE);
			asset->assetData.image.krbTextureHandle = kam->krb->loadImage(
				asset->assetData.image.rawImage.sizeX,
				asset->assetData.image.rawImage.sizeY,
				asset->assetData.image.rawImage.pixelData);
		}break;
		case KAssetType::RAW_SOUND:
		{
		}break;
		case KAssetType::UNUSED:
		{
			KLOG(ERROR, "UNUSED asset!");
			return;
		}break;
	}
	asset->lastWriteTime = 
		kam->kpl->getAssetWriteTime(asset->assetFileName);
	asset->loaded = true;
}
internal RawSound kamGetRawSound(KAssetManager* kam,
                                 KAssetHandle kahSound)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	kassert(kahSound < kam->maxAssetHandles);
	KAsset*const asset = assets + kahSound;
	kassert(asset->type == KAssetType::RAW_SOUND);
	if(asset->loaded)
	{
		return asset->assetData.sound;
	}
	else
	{
		if(kam->kpl->jobDone(&asset->jqTicketLoading))
		{
			kamOnLoadingJobFinished(kam, kahSound);
			return asset->assetData.sound;
		}
		return kam->defaultAssetSound.assetData.sound;
	}
}
#if 0
internal RawImage kamGetRawImage(KAssetManager* kam,
                                 KAssetHandle kahImage)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	kassert(kahImage < kam->maxAssetHandles);
	kassert(assets[kahImage].type == KAssetType::RAW_IMAGE);
	if(assets[kahImage].loaded)
	{
		return assets[kahImage].assetData.image.rawImage;
	}
	else
	{
		return kam->defaultAssetImage.assetData.image.rawImage;
	}
}
#endif// 0
internal KrbTextureHandle kamGetTexture(KAssetManager* kam, 
                                        KAssetCStr kAssetCStr)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	const KAssetHandle assetHandle = KASSET_INDEX(kAssetCStr);
	kassert(assetHandle < kam->maxAssetHandles);
	KAsset*const asset = assets + assetHandle;
	if(asset->type == KAssetType::UNUSED)
	{
		kamPushAsset(kam, kAssetCStr);
	}
	if(asset->loaded)
	{
		return asset->assetData.image.krbTextureHandle;
	}
	else
	{
		if(kam->kpl->jobDone(&asset->jqTicketLoading))
		{
			kamOnLoadingJobFinished(kam, assetHandle);
			return asset->assetData.image.krbTextureHandle;
		}
		return kam->defaultAssetImage.assetData.image.krbTextureHandle;
	}
}
JOB_QUEUE_FUNCTION(asyncLoadPng)
{
	KAsset*const asset = reinterpret_cast<KAsset*>(data);
	while(!asset->kam->kpl->isAssetAvailable(asset->assetFileName))
	{
		KLOG(INFO, "Waiting for asset '%s'...", asset->assetFileName);
	}
	asset->assetData.image.rawImage = 
		asset->kam->kpl->loadPng(asset->assetFileName, 
		                         asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(asyncLoadWav)
{
	KAsset*const asset = reinterpret_cast<KAsset*>(data);
	while(!asset->kam->kpl->isAssetAvailable(asset->assetFileName))
	{
		KLOG(INFO, "Waiting for asset '%s'...", asset->assetFileName);
	}
	asset->assetData.sound = 
		asset->kam->kpl->loadWav(asset->assetFileName, 
		                         asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(asyncLoadOgg)
{
	KAsset*const asset = reinterpret_cast<KAsset*>(data);
	while(!asset->kam->kpl->isAssetAvailable(asset->assetFileName))
	{
		KLOG(INFO, "Waiting for asset '%s'...", asset->assetFileName);
	}
	asset->assetData.sound = 
		asset->kam->kpl->loadOgg(asset->assetFileName, 
		                         asset->kam->assetDataAllocator);
}
internal KAssetHandle kamPushAsset(KAssetManager* kam, 
                                   KAssetCStr kAssetCStr)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	const KAssetHandle assetHandle = KASSET_INDEX(kAssetCStr);
	kassert(assetHandle < kam->maxAssetHandles);
	KAsset*const asset = assets + assetHandle;
	if(asset->type == KAssetType::UNUSED)
	{
		// load the asset from the platform layer //
		asset->loaded = false;
		switch(KASSET_TYPE(kAssetCStr))
		{
			case KASSET_TYPE_PNG:
			{
				asset->type            = KAssetType::RAW_IMAGE;
				asset->assetFileName   = *kAssetCStr;
				asset->kam             = kam;
				asset->assetData.image = {};
				asset->jqTicketLoading = kam->kpl->postJob(asyncLoadPng, asset);
			}break;
			case KASSET_TYPE_WAV:
			{
				asset->type            = KAssetType::RAW_SOUND;
				asset->assetFileName   = *kAssetCStr;
				asset->kam             = kam;
				asset->jqTicketLoading = kam->kpl->postJob(asyncLoadWav, asset);
			}break;
			case KASSET_TYPE_OGG:
			{
				asset->type            = KAssetType::RAW_SOUND;
				asset->assetFileName   = *kAssetCStr;
				asset->kam             = kam;
				asset->jqTicketLoading = kam->kpl->postJob(asyncLoadOgg, asset);
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
internal bool kamIsLoadingAssets(KAssetManager* kam, KAssetType type)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	for(KAssetHandle kah = 0; kah < kam->maxAssetHandles; kah++)
	{
		KAsset*const asset = assets + kah;
		if(asset->type == type && !asset->loaded)
		{
			if(kam->kpl->jobDone(&asset->jqTicketLoading))
			{
				kamOnLoadingJobFinished(kam, kah);
				continue;
			}
			return true;
		}
	}
	return false;
}
internal bool kamIsLoadingImages(KAssetManager* kam)
{
	return kamIsLoadingAssets(kam, KAssetType::RAW_IMAGE);
}
internal bool kamIsLoadingSounds(KAssetManager* kam)
{
	return kamIsLoadingAssets(kam, KAssetType::RAW_SOUND);
}
internal void kamUnloadChangedAssets(KAssetManager* kam)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	for(KAssetHandle kah = 0; kah < kam->maxAssetHandles; kah++)
	{
		KAsset*const asset = assets + kah;
		if(asset->type != KAssetType::UNUSED && asset->loaded)
		{
			if(kam->kpl->isAssetChanged(asset->assetFileName, 
			                            asset->lastWriteTime))
			{
				kamFreeAsset(kam, kah);
			}
		}
	}
}