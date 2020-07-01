#include "kAssetManager.h"
#include "z85-png-default.h"
#include "z85-wav-default.h"
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
	KgaHandle assetDataAllocator;
	KmlPlatformApi* kpl;
	KrbApi* krb;
	//KAsset assets[];
};
internal KAssetManager* kamConstruct(KgaHandle allocator, u32 maxAssetHandles, 
                                     KgaHandle assetDataAllocator,
                                     KmlPlatformApi* kpl, KrbApi* krb)
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
		defaultAssetSound.assetData.sound =
			kpl->decodeZ85Wav(z85_wav_default, 
			                  CARRAY_COUNT(z85_wav_default) - 1,
			                  assetDataAllocator);
		KAsset defaultAssetFlipbook;
		defaultAssetFlipbook.type = KAssetType::FLIPBOOK_META;
		defaultAssetFlipbook.assetData.flipbook.metaData = {};
		defaultAssetFlipbook.assetData.flipbook.metaData.textureKAssetIndex = 
			KASSET_COUNT;
		strcpy_s(defaultAssetFlipbook.assetData.flipbook.textureAssetFileName, 
			CARRAY_COUNT(
				defaultAssetFlipbook.assetData.flipbook.textureAssetFileName),
			"flipbook-default");
		*result = 
			{ .usedAssetHandles             = 0
			, .maxAssetHandles              = maxAssetHandles
			, .nextUnusedHandle             = 0
			, .defaultAssetImage            = defaultAssetImage
			, .defaultAssetSound            = defaultAssetSound
			, .defaultAssetFlipbookMetaData = defaultAssetFlipbook
			, .assetDataAllocator           = assetDataAllocator
			, .kpl = kpl
			, .krb = krb };
	}
	return result;
}
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
internal v2u32 kamGetTextureSize(KAssetManager* kam, 
                                 KAssetIndex assetIndex)
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
	kassert(asset->type == KAssetType::UNUSED || 
	        asset->type == KAssetType::RAW_IMAGE);
	if(asset->type == KAssetType::UNUSED)
	{
		kamPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		return {asset->assetData.image.rawImage.sizeX,
		        asset->assetData.image.rawImage.sizeY};
	}
	else
	{
		if(kam->kpl->jobDone(&asset->jqTicketLoading))
		{
			kamOnLoadingJobFinished(kam, assetHandle);
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
		return asset->assetData.flipbook.metaData;
	}
	else
	{
		if(kam->kpl->jobDone(&asset->jqTicketLoading))
		{
			kamOnLoadingJobFinished(kam, assetHandle);
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
	const bool loadFbmSuccess = 
		asset->kam->kpl->loadFlipbookMeta(
		          kAssetFileNames[kAssetId], 
		          &asset->assetData.flipbook.metaData,
		          asset->assetData.flipbook.textureAssetFileName,
		          CARRAY_COUNT(asset->assetData.flipbook.textureAssetFileName));
	kassert(loadFbmSuccess);
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
	{
		// load the asset from the platform layer //
		asset->loaded = false;
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
internal void kamUnloadChangedAssets(KAssetManager* kam)
{
	KAsset*const assets = reinterpret_cast<KAsset*>(kam + 1);
	for(KAssetHandle kah = 0; kah < kam->maxAssetHandles; kah++)
	{
		KAsset*const asset = assets + kah;
		if(asset->type != KAssetType::UNUSED && asset->loaded)
		{
			if(kam->kpl->isAssetChanged(kAssetFileNames[kah], 
			                            asset->lastWriteTime))
			{
				kamFreeAsset(kam, kah);
			}
		}
	}
}