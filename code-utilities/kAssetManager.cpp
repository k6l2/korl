#include "kAssetManager.h"
#include "z85-png-default.h"
#include "z85-wav-default.h"
#include <string>
enum class KAssetType : u8
{
	UNUSED,
	RAW_IMAGE,
	RAW_SOUND,
	FLIPBOOK_META
};
struct KAsset
{
	FileWriteTime lastWriteTime;
	KAssetType type;
	bool loaded;
	JobQueueTicket jqTicketLoading;
	union 
	{
		struct 
		{
			RawImage rawImage;
			KrbTextureHandle krbTextureHandle;
		} image;
		RawSound sound;
		struct 
		{
			FlipbookMetaData metaData;
			char textureAssetFileName[128];
		} flipbook;
	} assetData;
	// This pointer is here only for convenience with respect to asynchronous 
	//	job posting to the platform layer.
	KAssetManager* kam;
	// Likewise, this is only for the convenience of the async job functions
	size_t kAssetIndex;
};
struct KAssetManager
{
	KAssetHandle usedAssetHandles;
	KAssetHandle maxAssetHandles;
	KAssetHandle nextUnusedHandle;
	u8 nextUnusedHandle_PADDING[4];
	KAsset defaultAssetImage;
	KAsset defaultAssetSound;
	KAsset defaultAssetFlipbookMetaData;
	KAllocatorHandle assetDataAllocator;
	KAllocatorHandle hKgaRawFiles;
	KplLockHandle hLockAssetDataAllocator;
	KmlPlatformApi* kpl;
	KrbApi* krb;
	//KAsset assets[];
};
internal KAssetManager* kamConstruct(
	KAllocatorHandle allocator, u32 maxAssetHandles, 
	KAllocatorHandle assetDataAllocator, KmlPlatformApi* kpl, KrbApi* krbApi)
{
	if(maxAssetHandles >= INVALID_KASSET_HANDLE)
	{
		return nullptr;
	}
	const size_t rawFilePoolBytes = kmath::megabytes(1);
	const size_t requiredBytes = sizeof(KAssetManager) + 
		sizeof(KAsset)*maxAssetHandles + rawFilePoolBytes;
	KAssetManager*const result = reinterpret_cast<KAssetManager*>(
		kAllocAlloc(allocator, requiredBytes));
	if(!result)
	{
		KLOG(ERROR, "Failed to allocate memory for KAssetManager!");
		return result;
	}
	void*const rawFileAllocatorAddress = 
		reinterpret_cast<u8*>(result) + sizeof(KAssetManager) + 
		sizeof(KAsset)*maxAssetHandles;
	KAllocatorHandle hKalRawFiles = kAllocInit(
		KAllocatorType::GENERAL, rawFileAllocatorAddress, rawFilePoolBytes);
	// Quickly load very tiny default assets which can be immediately used
	//	in place of any asset handle which has not yet been loaded from the
	//	platform and decoded into useful data for us yet! //
	KAsset defaultAssetImage;
	defaultAssetImage.type = KAssetType::RAW_IMAGE;
	defaultAssetImage.assetData.image.rawImage = 
		kpl->decodeZ85Png(z85_png_default, CARRAY_SIZE(z85_png_default) - 1,
		                  assetDataAllocator);
	defaultAssetImage.assetData.image.krbTextureHandle = krbApi
		? krbApi->loadImage(
			defaultAssetImage.assetData.image.rawImage.sizeX,
			defaultAssetImage.assetData.image.rawImage.sizeY,
			defaultAssetImage.assetData.image.rawImage.pixelData)
		: krb::INVALID_TEXTURE_HANDLE;
	KAsset defaultAssetSound;
	defaultAssetSound.type = KAssetType::RAW_SOUND;
	defaultAssetSound.assetData.sound =
		kpl->decodeZ85Wav(z85_wav_default, CARRAY_SIZE(z85_wav_default) - 1,
		                  assetDataAllocator);
	KAsset defaultAssetFlipbook;
	defaultAssetFlipbook.type = KAssetType::FLIPBOOK_META;
	defaultAssetFlipbook.assetData.flipbook.metaData = {};
	defaultAssetFlipbook.assetData.flipbook.metaData.textureKAssetIndex = 
		KASSET_COUNT;
	strcpy_s(defaultAssetFlipbook.assetData.flipbook.textureAssetFileName, 
		CARRAY_SIZE(
			defaultAssetFlipbook.assetData.flipbook.textureAssetFileName),
		"flipbook-default");
	/* request access to a spinlock from the platform layer so we can keep 
		the asset data allocator safe between asset loading job threads */
	const KplLockHandle hLockAssetDataAllocator = kpl->reserveLock();
	*result = 
		{ .usedAssetHandles             = 0
		, .maxAssetHandles              = maxAssetHandles
		, .nextUnusedHandle             = 0
		, .defaultAssetImage            = defaultAssetImage
		, .defaultAssetSound            = defaultAssetSound
		, .defaultAssetFlipbookMetaData = defaultAssetFlipbook
		, .assetDataAllocator           = assetDataAllocator
		, .hKgaRawFiles                 = hKalRawFiles
		, .hLockAssetDataAllocator      = hLockAssetDataAllocator
		, .kpl = kpl
		, .krb = krbApi };
	return result;
}
internal void kamFreeAsset(KAssetManager* kam, KAssetHandle assetHandle)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	kassert(assetHandle < kam->maxAssetHandles);
	KAsset*const asset = assets + assetHandle;
	if(!asset->loaded)
	{
		KLOG(WARNING, "KAsset[%i] is already free!", assetHandle);
		return;
	}
	switch(asset->type)
	{
		case KAssetType::RAW_IMAGE:
		{
			kam->krb->deleteTexture(asset->assetData.image.krbTextureHandle);
			kAllocFree(kam->assetDataAllocator, 
			           asset->assetData.image.rawImage.pixelData);
		} break;
		case KAssetType::RAW_SOUND:
		{
			kAllocFree(kam->assetDataAllocator, 
			           asset->assetData.sound.sampleData);
		} break;
		case KAssetType::FLIPBOOK_META:
		{
			KAssetHandle kahTexture = static_cast<KAssetHandle>(
				asset->assetData.flipbook.metaData.textureKAssetIndex);
			kamFreeAsset(kam, kahTexture);
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
internal void kamFreeAsset(KAssetManager* kam, KAssetIndex assetIndex)
{
	const KAssetHandle assetHandle = static_cast<KAssetHandle>(assetIndex);
	kamFreeAsset(kam, assetHandle);
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
		case KAssetType::FLIPBOOK_META:
		{
			char*const fbTexAssetFileName = 
				asset->assetData.flipbook.textureAssetFileName;
			size_t assetFileNameIdTex;
			for(assetFileNameIdTex = 0; assetFileNameIdTex < KASSET_COUNT; 
				assetFileNameIdTex++)
			{
				if(strcmp(kAssetFileNames[assetFileNameIdTex], 
				          fbTexAssetFileName) == 0)
				{
					break;
				}
			}
			kassert(assetFileNameIdTex < KASSET_COUNT);
			if(assetFileNameIdTex >= KASSET_COUNT)
			{
				KLOG(WARNING, 
				     "Flipbook meta textureAssetFileName='%s' not found!", 
				     asset->assetData.flipbook.textureAssetFileName);
			}
			else
			{
				asset->assetData.flipbook.metaData.textureKAssetIndex = 
					assetFileNameIdTex;
				kamPushAsset(kam, KAssetIndex(assetFileNameIdTex));
			}
		}break;
		case KAssetType::UNUSED:
		{
			KLOG(ERROR, "UNUSED asset!");
			return;
		}break;
	}
	asset->lastWriteTime = kam->kpl->getAssetWriteTime(kAssetFileNames[kah]);
	asset->loaded = true;
}
internal RawSound kamGetRawSound(KAssetManager* kam,
                                 KAssetIndex assetIndex)
{
	if(assetIndex >= KAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetSound.assetData.sound;
	}
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	const KAssetHandle assetHandle = static_cast<KAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KAsset*const asset = assets + assetHandle;
	if(asset->type == KAssetType::UNUSED)
	{
		kamPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		kassert(asset->type == KAssetType::RAW_SOUND);
		return asset->assetData.sound;
	}
	else
	{
		if(kam->kpl->jobDone(&asset->jqTicketLoading))
		{
			kamOnLoadingJobFinished(kam, assetHandle);
			kassert(asset->type == KAssetType::RAW_SOUND);
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
                                        KAssetIndex assetIndex)
{
	if(assetIndex >= KAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetImage.assetData.image.krbTextureHandle;
	}
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	const KAssetHandle assetHandle = static_cast<KAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KAsset*const asset = assets + assetHandle;
	if(asset->type == KAssetType::UNUSED)
	{
		kamPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		kassert(asset->type == KAssetType::RAW_IMAGE);
		return asset->assetData.image.krbTextureHandle;
	}
	else
	{
		if(kam->kpl->jobDone(&asset->jqTicketLoading))
		{
			kamOnLoadingJobFinished(kam, assetHandle);
			kassert(asset->type == KAssetType::RAW_IMAGE);
			return asset->assetData.image.krbTextureHandle;
		}
		return kam->defaultAssetImage.assetData.image.krbTextureHandle;
	}
}
internal v2u32 kamGetImageSize(KAssetManager* kam, KAssetIndex assetIndex)
{
	if(assetIndex >= KAssetIndex::ENUM_SIZE)
	{
		return {kam->defaultAssetImage.assetData.image.rawImage.sizeX,
		        kam->defaultAssetImage.assetData.image.rawImage.sizeY};
	}
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	const KAssetHandle assetHandle = static_cast<KAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KAsset*const asset = assets + assetHandle;
	if(asset->type == KAssetType::UNUSED)
	{
		kamPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		kassert(asset->type == KAssetType::RAW_IMAGE);
		return {asset->assetData.image.rawImage.sizeX,
		        asset->assetData.image.rawImage.sizeY};
	}
	else
	{
		if(kam->kpl->jobDone(&asset->jqTicketLoading))
		{
			kamOnLoadingJobFinished(kam, assetHandle);
			kassert(asset->type == KAssetType::RAW_IMAGE);
			return {asset->assetData.image.rawImage.sizeX,
			        asset->assetData.image.rawImage.sizeY};
		}
		return {kam->defaultAssetImage.assetData.image.rawImage.sizeX,
		        kam->defaultAssetImage.assetData.image.rawImage.sizeY};
	}
}
internal FlipbookMetaData kamGetFlipbookMetaData(KAssetManager* kam, 
                                                 KAssetIndex assetIndex)
{
	if(assetIndex >= KAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetFlipbookMetaData.assetData.flipbook.metaData;
	}
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	const KAssetHandle assetHandle = static_cast<KAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KAsset*const asset = assets + assetHandle;
	if(asset->type == KAssetType::UNUSED)
	{
		kamPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		kassert(asset->type == KAssetType::FLIPBOOK_META);
		return asset->assetData.flipbook.metaData;
	}
	else
	{
		if(kam->kpl->jobDone(&asset->jqTicketLoading))
		{
			kamOnLoadingJobFinished(kam, assetHandle);
			kassert(asset->type == KAssetType::FLIPBOOK_META);
			return asset->assetData.flipbook.metaData;
		}
		return kam->defaultAssetFlipbookMetaData.assetData.flipbook.metaData;
	}
}
JOB_QUEUE_FUNCTION(asyncLoadPng)
{
	KAsset*const asset = reinterpret_cast<KAsset*>(data);
	const size_t kAssetId = asset->kAssetIndex;
	while(!asset->kam->kpl->isAssetAvailable(kAssetFileNames[kAssetId]))
	{
		KLOG(INFO, "Waiting for asset '%s'...", kAssetFileNames[kAssetId]);
	}
	asset->assetData.image.rawImage = 
		asset->kam->kpl->loadPng(kAssetFileNames[kAssetId], 
		                         asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(asyncLoadWav)
{
	KAsset*const asset = reinterpret_cast<KAsset*>(data);
	const size_t kAssetId = asset->kAssetIndex;
	while(!asset->kam->kpl->isAssetAvailable(kAssetFileNames[kAssetId]))
	{
		KLOG(INFO, "Waiting for asset '%s'...", kAssetFileNames[kAssetId]);
	}
	asset->assetData.sound = 
		asset->kam->kpl->loadWav(kAssetFileNames[kAssetId], 
		                         asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(asyncLoadOgg)
{
	KAsset*const asset = reinterpret_cast<KAsset*>(data);
	const size_t kAssetId = asset->kAssetIndex;
	while(!asset->kam->kpl->isAssetAvailable(kAssetFileNames[kAssetId]))
	{
		KLOG(INFO, "Waiting for asset '%s'...", kAssetFileNames[kAssetId]);
	}
	asset->assetData.sound = 
		asset->kam->kpl->loadOgg(kAssetFileNames[kAssetId], 
		                         asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(asyncLoadFlipbookMeta)
{
	KAsset*const asset = reinterpret_cast<KAsset*>(data);
	const size_t kAssetId = asset->kAssetIndex;
	while(!asset->kam->kpl->isAssetAvailable(kAssetFileNames[kAssetId]))
	{
		KLOG(INFO, "Waiting for asset '%s'...", kAssetFileNames[kAssetId]);
	}
	const i32 assetByteSize = 
		asset->kam->kpl->getAssetByteSize(kAssetFileNames[kAssetId]);
	if(assetByteSize < 0)
	{
		KLOG(ERROR, "Failed to get asset byte size of \"%s\"", 
		     kAssetFileNames[kAssetId]);
		return;
	}
	/* lock the asset manager's asset data allocator so we can safely allocate 
		data for the raw file.  The decoded FlipbookMeta structure is stored 
		entirely in the KAsset */
	asset->kam->kpl->lock(asset->kam->hLockAssetDataAllocator);
	void*const rawFileMemory = 
		kAllocAlloc(asset->kam->hKgaRawFiles, 
		            kmath::safeTruncateU32(assetByteSize) + 1);
	kassert(rawFileMemory);
	asset->kam->kpl->unlock(asset->kam->hLockAssetDataAllocator);
	/* defer cleanup of the raw file memory until after we utilize it */
	defer({
		asset->kam->kpl->lock(asset->kam->hLockAssetDataAllocator);
		kAllocFree(asset->kam->hKgaRawFiles, rawFileMemory);
		asset->kam->kpl->unlock(asset->kam->hLockAssetDataAllocator);
	});
	/* load the entire raw file into a `fileByteSize` chunk */
	const bool assetReadSuccess = 
		asset->kam->kpl->readEntireAsset(
			kAssetFileNames[kAssetId], rawFileMemory, 
			kmath::safeTruncateU32(assetByteSize));
	if(!assetReadSuccess)
	{
		KLOG(ERROR, "Failed to read asset \"%s\"!", kAssetFileNames[kAssetId]);
		return;
	}
	reinterpret_cast<u8*>(rawFileMemory)[assetByteSize] = 0;
	/* decode the file data into a struct we can use */
	const bool flipbookMetaDecodeSuccess = 
		kfbDecodeMeta(
			rawFileMemory, kmath::safeTruncateU32(assetByteSize), 
			kAssetFileNames[kAssetId], &asset->assetData.flipbook.metaData, 
			asset->assetData.flipbook.textureAssetFileName, 
			CARRAY_SIZE(asset->assetData.flipbook.textureAssetFileName));
	if(!flipbookMetaDecodeSuccess)
	{
		KLOG(ERROR, "Failed to decode flipbook meta data \"%s\"!", 
		     kAssetFileNames[kAssetId]);
	    return;
	}
}
// @optimization (minor): just bake this info using `kasset`
enum class KAssetFileType
{
	PNG, WAV, OGG, FLIPBOOK_META, 
	UNKNOWN
};
internal KAssetFileType kamAssetFileType(KAssetIndex assetIndex)
{
	if(strstr(kAssetFileNames[static_cast<size_t>(assetIndex)], ".png"))
	{
		return KAssetFileType::PNG;
	}
	else if(strstr(kAssetFileNames[static_cast<size_t>(assetIndex)], ".wav"))
	{
		return KAssetFileType::WAV;
	}
	else if(strstr(kAssetFileNames[static_cast<size_t>(assetIndex)], ".ogg"))
	{
		return KAssetFileType::OGG;
	}
	else if(strstr(kAssetFileNames[static_cast<size_t>(assetIndex)], ".fbm"))
	{
		return KAssetFileType::FLIPBOOK_META;
	}
	return KAssetFileType::UNKNOWN;
}
internal KAssetHandle kamPushAsset(KAssetManager* kam, 
                                   KAssetIndex assetIndex)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	const KAssetHandle assetHandle = static_cast<KAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KAsset*const asset = assets + assetHandle;
	if(asset->type == KAssetType::UNUSED)
	/* load the asset from the platform layer */
	{
		KLOG(INFO, "Loading asset '%s'...", kAssetFileNames[assetHandle]);
		asset->loaded      = false;
		asset->kAssetIndex = assetHandle;
		switch(kamAssetFileType(assetIndex))
		{
			case KAssetFileType::PNG:
			{
				asset->type            = KAssetType::RAW_IMAGE;
				asset->kam             = kam;
				asset->assetData.image = {};
				asset->jqTicketLoading = kam->kpl->postJob(asyncLoadPng, asset);
			}break;
			case KAssetFileType::WAV:
			{
				asset->type            = KAssetType::RAW_SOUND;
				asset->kam             = kam;
				asset->jqTicketLoading = kam->kpl->postJob(asyncLoadWav, asset);
			}break;
			case KAssetFileType::OGG:
			{
				asset->type            = KAssetType::RAW_SOUND;
				asset->kam             = kam;
				asset->jqTicketLoading = kam->kpl->postJob(asyncLoadOgg, asset);
			}break;
			case KAssetFileType::FLIPBOOK_META:
			{
				asset->type            = KAssetType::FLIPBOOK_META;
				asset->kam             = kam;
				asset->jqTicketLoading = 
				                kam->kpl->postJob(asyncLoadFlipbookMeta, asset);
			}break;
			case KAssetFileType::UNKNOWN:
			default:
			{
				KLOG(ERROR, "Unknown KAssetFileType==%i for assetIndex==%i!",
				     static_cast<i32>(kamAssetFileType(assetIndex)), 
				     assetIndex);
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
///TODO: make the type parameter a bitfield so we can call this function once 
///      for multiple types!
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
	return kamIsLoadingAssets(kam, KAssetType::RAW_IMAGE) |
	       kamIsLoadingAssets(kam, KAssetType::FLIPBOOK_META);
}
internal bool kamIsLoadingSounds(KAssetManager* kam)
{
	return kamIsLoadingAssets(kam, KAssetType::RAW_SOUND);
}
internal u32 kamUnloadChangedAssets(KAssetManager* kam)
{
	u32 unloadedAssetCount = 0;
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	for(KAssetHandle kah = 0; kah < kam->maxAssetHandles; kah++)
	{
		KAsset*const asset = assets + kah;
		if(asset->type != KAssetType::UNUSED && asset->loaded)
		{
			if(kam->kpl->isAssetChanged(kAssetFileNames[kah], 
			                            asset->lastWriteTime))
			{
				KLOG(INFO, "Unloading asset '%s'...", kAssetFileNames[kah]);
				kamFreeAsset(kam, kah);
				unloadedAssetCount++;
			}
		}
	}
	return unloadedAssetCount;
}
internal void kamUnloadAllAssets(KAssetManager* kam)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	for(KAssetHandle kah = 0; kah < kam->maxAssetHandles; kah++)
	{
		KAsset*const asset = assets + kah;
		if(asset->type != KAssetType::UNUSED && asset->loaded)
		{
			KLOG(INFO, "Unloading asset '%s'...", kAssetFileNames[kah]);
			kamFreeAsset(kam, kah);
		}
	}
}
internal void kamPushAllKAssets(KAssetManager* kam)
{
	for(u32 kAssetId = 0; kAssetId < KASSET_COUNT; kAssetId++)
	{
		kamPushAsset(kam, KAssetIndex(kAssetId));
	}
}
