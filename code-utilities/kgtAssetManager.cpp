#include "kgtAssetManager.h"
#include "z85-png-default.h"
#include "z85-wav-default.h"
#include "korl-texture.h"
#include <string>
enum class KgtAssetType : u8
	{ UNUSED
	, RAW_IMAGE
	, RAW_SOUND
	, FLIPBOOK_META
	, TEXTURE_META };
struct KgtAsset
{
	FileWriteTime lastWriteTime;
	KgtAssetType type;
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
			KgtFlipBookMetaData metaData;
			char textureAssetFileName[128];
		} flipbook;
		struct
		{
			KorlTextureMetaData metaData;
			char imageAssetName[128];
			KgtAssetIndex kaiImage;
		} texture;
	} assetData;
	/* async job function convenience data */
	KgtAssetManager* kam;
	size_t kgtAssetIndex;
};
struct KgtAssetManager
{
	KgtAssetHandle usedAssetHandles;
	KgtAssetHandle maxAssetHandles;
	KgtAssetHandle nextUnusedHandle;
	u8 nextUnusedHandle_PADDING[4];
	KgtAsset defaultAssetImage;
	KgtAsset defaultAssetSound;
	KgtAsset defaultAssetFlipbookMetaData;
	KgtAsset defaultAssetTextureMetaData;
	KgtAllocatorHandle assetDataAllocator;
	KgtAllocatorHandle hKgaRawFiles;
	KplLockHandle hLockAssetDataAllocator;
	KrbApi* krb;
	//KgtAsset assets[];
};
internal KgtAssetManager* 
	kgtAssetManagerConstruct(
		KgtAllocatorHandle allocator, u32 maxAssetHandles, 
		KgtAllocatorHandle assetDataAllocator, KrbApi* krbApi)
{
	if(maxAssetHandles < KGT_ASSET_COUNT)
	{
		KLOG(ERROR, "maxAssetHandles(%i) < KGT_ASSET_COUNT(%i)!", 
		     maxAssetHandles, KGT_ASSET_COUNT);
		return nullptr;
	}
	const size_t rawFilePoolBytes = kmath::megabytes(1);
	const size_t requiredBytes = sizeof(KgtAssetManager) + 
		sizeof(KgtAsset)*maxAssetHandles + rawFilePoolBytes;
	KgtAssetManager*const result = reinterpret_cast<KgtAssetManager*>(
		kgtAllocAlloc(allocator, requiredBytes));
	if(!result)
	{
		KLOG(ERROR, "Failed to allocate memory for KgtAssetManager!");
		return result;
	}
	void*const rawFileAllocatorAddress = 
		reinterpret_cast<u8*>(result) + sizeof(KgtAssetManager) + 
		sizeof(KgtAsset)*maxAssetHandles;
	KgtAllocatorHandle hKalRawFiles = kgtAllocInit(
		KgtAllocatorType::GENERAL, rawFileAllocatorAddress, rawFilePoolBytes);
	// Quickly load very tiny default assets which can be immediately used
	//	in place of any asset handle which has not yet been loaded from the
	//	platform and decoded into useful data for us yet! //
	KgtAsset defaultAssetImage;
	defaultAssetImage.type = KgtAssetType::RAW_IMAGE;
	defaultAssetImage.assetData.image.rawImage = 
		g_kpl->decodeZ85Png(z85_png_default, CARRAY_SIZE(z85_png_default) - 1,
		                    assetDataAllocator);
	defaultAssetImage.assetData.image.krbTextureHandle = krbApi
		? krbApi->loadImage(
			defaultAssetImage.assetData.image.rawImage.sizeX,
			defaultAssetImage.assetData.image.rawImage.sizeY,
			defaultAssetImage.assetData.image.rawImage.pixelData)
		: krb::INVALID_TEXTURE_HANDLE;
	KgtAsset defaultAssetSound;
	defaultAssetSound.type = KgtAssetType::RAW_SOUND;
	defaultAssetSound.assetData.sound =
		g_kpl->decodeZ85Wav(z85_wav_default, CARRAY_SIZE(z85_wav_default) - 1,
		                    assetDataAllocator);
	KgtAsset defaultAssetFlipbook;
	defaultAssetFlipbook.type = KgtAssetType::FLIPBOOK_META;
	defaultAssetFlipbook.assetData.flipbook.metaData = {};
	defaultAssetFlipbook.assetData.flipbook.metaData.kaiTexture = 
		KgtAssetIndex::ENUM_SIZE;
	strcpy_s(defaultAssetFlipbook.assetData.flipbook.textureAssetFileName, 
		CARRAY_SIZE(
			defaultAssetFlipbook.assetData.flipbook.textureAssetFileName),
		"flipbook-default");
	KgtAsset defaultAssetTexture;
	defaultAssetTexture.type = KgtAssetType::TEXTURE_META;
	defaultAssetTexture.assetData.texture.metaData = {};
	defaultAssetTexture.assetData.texture.kaiImage = KgtAssetIndex::ENUM_SIZE;
	strcpy_s(defaultAssetTexture.assetData.texture.imageAssetName, 
		CARRAY_SIZE(defaultAssetTexture.assetData.texture.imageAssetName),
		"texture-default");
	/* request access to a spinlock from the platform layer so we can keep 
		the asset data allocator safe between asset loading job threads */
	const KplLockHandle hLockAssetDataAllocator = g_kpl->reserveLock();
	*result = 
		{ .usedAssetHandles             = 0
		, .maxAssetHandles              = maxAssetHandles
		, .nextUnusedHandle             = 0
		, .defaultAssetImage            = defaultAssetImage
		, .defaultAssetSound            = defaultAssetSound
		, .defaultAssetFlipbookMetaData = defaultAssetFlipbook
		, .defaultAssetTextureMetaData  = defaultAssetTexture
		, .assetDataAllocator           = assetDataAllocator
		, .hKgaRawFiles                 = hKalRawFiles
		, .hLockAssetDataAllocator      = hLockAssetDataAllocator
		, .krb = krbApi };
	return result;
}
internal void 
	kgtAssetManagerFreeAsset(KgtAssetManager* kam, KgtAssetHandle assetHandle)
{
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	kassert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(!asset->loaded)
	{
		KLOG(WARNING, "KgtAsset[%i] is already free!", assetHandle);
		return;
	}
	switch(asset->type)
	{
		case KgtAssetType::RAW_IMAGE:
		{
			kam->krb->deleteTexture(asset->assetData.image.krbTextureHandle);
			kgtAllocFree(kam->assetDataAllocator, 
			             asset->assetData.image.rawImage.pixelData);
		} break;
		case KgtAssetType::RAW_SOUND:
		{
			kgtAllocFree(kam->assetDataAllocator, 
			             asset->assetData.sound.sampleData);
		} break;
		case KgtAssetType::FLIPBOOK_META:
		{
			KgtAssetHandle kahTexture = static_cast<KgtAssetHandle>(
				asset->assetData.flipbook.metaData.kaiTexture);
			kgtAssetManagerFreeAsset(kam, kahTexture);
		} break;
		case KgtAssetType::TEXTURE_META:
		{
			kgtAssetManagerFreeAsset(kam, asset->assetData.texture.kaiImage);
		} break;
		case KgtAssetType::UNUSED: 
		default:
		{
			KLOG(ERROR, "Attempted to free an unused asset handle!");
			return;
		} break;
	}
	asset->type   = KgtAssetType::UNUSED;
	asset->loaded = false;
	if(kam->usedAssetHandles == kam->maxAssetHandles)
	{
		kam->nextUnusedHandle = assetHandle;
	}
	kam->usedAssetHandles--;
}
internal void 
	kgtAssetManagerFreeAsset(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	kgtAssetManagerFreeAsset(kam, assetHandle);
}
internal KgtAssetIndex 
	kgtAssetManagerFindKgtAssetIndex(const char* assetName)
{
	for(size_t id = 0; id < KGT_ASSET_COUNT; id++)
	{
		if(strcmp(kgtAssetFileNames[id], assetName) == 0)
		{
			return static_cast<KgtAssetIndex>(id);
		}
	}
	return KgtAssetIndex::ENUM_SIZE;
}
internal void 
	kgtAssetManagerOnLoadingJobFinished(
		KgtAssetManager* kam, KgtAssetHandle kah)
{
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	kassert(kah < kam->maxAssetHandles);
	KgtAsset*const asset = assets + kah;
	switch(asset->type)
	{
		case KgtAssetType::RAW_IMAGE:
		{
			kassert(asset->assetData.image.krbTextureHandle == 
				krb::INVALID_TEXTURE_HANDLE);
			asset->assetData.image.krbTextureHandle = kam->krb->loadImage(
				asset->assetData.image.rawImage.sizeX,
				asset->assetData.image.rawImage.sizeY,
				asset->assetData.image.rawImage.pixelData);
		}break;
		case KgtAssetType::RAW_SOUND:
		{
		}break;
		case KgtAssetType::FLIPBOOK_META:
		{
			const KgtAssetIndex kgtAssetIdTex = 
				kgtAssetManagerFindKgtAssetIndex(
					asset->assetData.flipbook.textureAssetFileName);
			if(kgtAssetIdTex >= KgtAssetIndex::ENUM_SIZE)
				KLOG(ERROR, 
				     "Flipbook meta textureAssetFileName='%s' not found!", 
				     asset->assetData.flipbook.textureAssetFileName);
			else
			{
				asset->assetData.flipbook.metaData.kaiTexture = 
					kgtAssetIdTex;
				kgtAssetManagerPushAsset(kam, kgtAssetIdTex);
			}
		}break;
		case KgtAssetType::TEXTURE_META:
		{
			const KgtAssetIndex kgtAssetIdImage = 
				kgtAssetManagerFindKgtAssetIndex(
					asset->assetData.texture.imageAssetName);
			if(kgtAssetIdImage >= KgtAssetIndex::ENUM_SIZE)
				KLOG(ERROR, "texture meta image asset name ('%s') not found!", 
				     asset->assetData.texture.imageAssetName);
			else
			{
				asset->assetData.texture.kaiImage = kgtAssetIdImage;
				kgtAssetManagerPushAsset(kam, kgtAssetIdImage);
			}
		} break;
		case KgtAssetType::UNUSED:
		{
			KLOG(ERROR, "UNUSED asset!");
			return;
		}break;
	}
	asset->lastWriteTime = g_kpl->getAssetWriteTime(kgtAssetFileNames[kah]);
	asset->loaded = true;
}
internal RawSound 
	kgtAssetManagerGetRawSound(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetSound.assetData.sound;
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		kassert(asset->type == KgtAssetType::RAW_SOUND);
		return asset->assetData.sound;
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			kassert(asset->type == KgtAssetType::RAW_SOUND);
			return asset->assetData.sound;
		}
		return kam->defaultAssetSound.assetData.sound;
	}
}
internal KrbTextureHandle 
	kgtAssetManagerGetRawImageTexture(
		KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetImage.assetData.image.krbTextureHandle;
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		kassert(asset->type == KgtAssetType::RAW_IMAGE);
		return asset->assetData.image.krbTextureHandle;
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			kassert(asset->type == KgtAssetType::RAW_IMAGE);
			return asset->assetData.image.krbTextureHandle;
		}
		return kam->defaultAssetImage.assetData.image.krbTextureHandle;
	}
}
internal KrbTextureHandle 
	kgtAssetManagerGetTexture(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetImage.assetData.image.krbTextureHandle;
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		if(asset->type == KgtAssetType::TEXTURE_META)
			return kgtAssetManagerGetRawImageTexture(
				kam, asset->assetData.texture.kaiImage);
		else if(asset->type == KgtAssetType::RAW_IMAGE)
			return asset->assetData.image.krbTextureHandle;
		else
		{
			KLOG(ERROR, "Invalid asset type (%i)!", asset->type);
			return 0;
		}
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			if(asset->type == KgtAssetType::TEXTURE_META)
				return kgtAssetManagerGetRawImageTexture(
					kam, asset->assetData.texture.kaiImage);
			else if(asset->type == KgtAssetType::RAW_IMAGE)
				return asset->assetData.image.krbTextureHandle;
			else
			{
				KLOG(ERROR, "Invalid asset type (%i)!", asset->type);
				return 0;
			}
		}
		return kam->defaultAssetImage.assetData.image.krbTextureHandle;
	}
}
internal KorlTextureMetaData
	kgtAssetManagerGetTextureMetaData(
		KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetTextureMetaData.assetData.texture.metaData;
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		if(asset->type == KgtAssetType::TEXTURE_META)
			return asset->assetData.texture.metaData;
		else if(asset->type == KgtAssetType::RAW_IMAGE)
			return kam->defaultAssetTextureMetaData.assetData.texture.metaData;
		else
		{
			KLOG(ERROR, "Invalid asset type (%i)!", asset->type);
			return kam->defaultAssetTextureMetaData.assetData.texture.metaData;
		}
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			if(asset->type == KgtAssetType::TEXTURE_META)
				return asset->assetData.texture.metaData;
			else if(asset->type == KgtAssetType::RAW_IMAGE)
				return 
					kam->defaultAssetTextureMetaData.assetData.texture.metaData;
			else
			{
				KLOG(ERROR, "Invalid asset type (%i)!", asset->type);
				return 
					kam->defaultAssetTextureMetaData.assetData.texture.metaData;
			}
		}
		return kam->defaultAssetTextureMetaData.assetData.texture.metaData;
	}
}
internal v2u32 
	kgtAssetManagerGetRawImageSize(
		KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return {kam->defaultAssetImage.assetData.image.rawImage.sizeX,
		        kam->defaultAssetImage.assetData.image.rawImage.sizeY};
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		kassert(asset->type == KgtAssetType::RAW_IMAGE);
		return {asset->assetData.image.rawImage.sizeX,
		        asset->assetData.image.rawImage.sizeY};
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			kassert(asset->type == KgtAssetType::RAW_IMAGE);
			return {asset->assetData.image.rawImage.sizeX,
			        asset->assetData.image.rawImage.sizeY};
		}
		return {kam->defaultAssetImage.assetData.image.rawImage.sizeX,
		        kam->defaultAssetImage.assetData.image.rawImage.sizeY};
	}
}
internal v2u32 
	kgtAssetMangerGetImageSize(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return {kam->defaultAssetImage.assetData.image.rawImage.sizeX,
		        kam->defaultAssetImage.assetData.image.rawImage.sizeY};
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		if(asset->type == KgtAssetType::TEXTURE_META)
			return kgtAssetManagerGetRawImageSize(
				kam, asset->assetData.texture.kaiImage);
		else if(asset->type == KgtAssetType::RAW_IMAGE)
			return {asset->assetData.image.rawImage.sizeX,
			        asset->assetData.image.rawImage.sizeY};
		else
		{
			KLOG(ERROR, "Invalid asset type (%i)!", asset->type);
			return {0,0};
		}
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			if(asset->type == KgtAssetType::TEXTURE_META)
				return kgtAssetManagerGetRawImageSize(
					kam, asset->assetData.texture.kaiImage);
			else if(asset->type == KgtAssetType::RAW_IMAGE)
				return {asset->assetData.image.rawImage.sizeX,
				        asset->assetData.image.rawImage.sizeY};
			else
			{
				KLOG(ERROR, "Invalid asset type (%i)!", asset->type);
				return {0,0};
			}
		}
		return {kam->defaultAssetImage.assetData.image.rawImage.sizeX,
		        kam->defaultAssetImage.assetData.image.rawImage.sizeY};
	}
}
internal KgtFlipBookMetaData 
	kgtAssetManagerGetFlipBookMetaData(
		KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetFlipbookMetaData.assetData.flipbook.metaData;
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		kassert(asset->type == KgtAssetType::FLIPBOOK_META);
		return asset->assetData.flipbook.metaData;
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			kassert(asset->type == KgtAssetType::FLIPBOOK_META);
			return asset->assetData.flipbook.metaData;
		}
		return kam->defaultAssetFlipbookMetaData.assetData.flipbook.metaData;
	}
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadPng)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	while(!g_kpl->isAssetAvailable(kgtAssetFileNames[kai]))
	{
		KLOG(INFO, "Waiting for asset '%s'...", kgtAssetFileNames[kai]);
	}
	asset->assetData.image.rawImage = 
		g_kpl->loadPng(kgtAssetFileNames[kai], 
		               asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadWav)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	while(!g_kpl->isAssetAvailable(kgtAssetFileNames[kai]))
	{
		KLOG(INFO, "Waiting for asset '%s'...", kgtAssetFileNames[kai]);
	}
	asset->assetData.sound = 
		g_kpl->loadWav(kgtAssetFileNames[kai], 
		               asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadOgg)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	while(!g_kpl->isAssetAvailable(kgtAssetFileNames[kai]))
	{
		KLOG(INFO, "Waiting for asset '%s'...", kgtAssetFileNames[kai]);
	}
	asset->assetData.sound = 
		g_kpl->loadOgg(kgtAssetFileNames[kai], 
		               asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadFlipbookMeta)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	while(!g_kpl->isAssetAvailable(kgtAssetFileNames[kai]))
	{
		KLOG(INFO, "Waiting for asset '%s'...", kgtAssetFileNames[kai]);
	}
	const i32 assetByteSize = 
		g_kpl->getAssetByteSize(kgtAssetFileNames[kai]);
	if(assetByteSize < 0)
	{
		KLOG(ERROR, "Failed to get asset byte size of \"%s\"", 
		     kgtAssetFileNames[kai]);
		return;
	}
	/* lock the asset manager's asset data allocator so we can safely allocate 
		data for the raw file.  The decoded FlipbookMeta structure is stored 
		entirely in the KgtAsset */
	g_kpl->lock(asset->kam->hLockAssetDataAllocator);
	void*const rawFileMemory = 
		kgtAllocAlloc(asset->kam->hKgaRawFiles, 
		              kmath::safeTruncateU32(assetByteSize) + 1);
	kassert(rawFileMemory);
	g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	/* defer cleanup of the raw file memory until after we utilize it */
	defer({
		g_kpl->lock(asset->kam->hLockAssetDataAllocator);
		kgtAllocFree(asset->kam->hKgaRawFiles, rawFileMemory);
		g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	});
	/* load the entire raw file into a `fileByteSize` chunk */
	const bool assetReadSuccess = 
		g_kpl->readEntireAsset(
			kgtAssetFileNames[kai], rawFileMemory, 
			kmath::safeTruncateU32(assetByteSize));
	if(!assetReadSuccess)
	{
		KLOG(ERROR, "Failed to read asset \"%s\"!", kgtAssetFileNames[kai]);
		return;
	}
	reinterpret_cast<u8*>(rawFileMemory)[assetByteSize] = 0;
	/* decode the file data into a struct we can use */
	const bool flipbookMetaDecodeSuccess = 
		kgtFlipBookDecodeMeta(
			rawFileMemory, kmath::safeTruncateU32(assetByteSize), 
			kgtAssetFileNames[kai], &asset->assetData.flipbook.metaData, 
			asset->assetData.flipbook.textureAssetFileName, 
			CARRAY_SIZE(asset->assetData.flipbook.textureAssetFileName));
	if(!flipbookMetaDecodeSuccess)
	{
		KLOG(ERROR, "Failed to decode flipbook meta data \"%s\"!", 
		     kgtAssetFileNames[kai]);
		return;
	}
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadTextureMeta)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	while(!g_kpl->isAssetAvailable(kgtAssetFileNames[kai]))
	{
		KLOG(INFO, "Waiting for asset '%s'...", kgtAssetFileNames[kai]);
	}
	const i32 assetByteSize = 
		g_kpl->getAssetByteSize(kgtAssetFileNames[kai]);
	if(assetByteSize < 0)
	{
		KLOG(ERROR, "Failed to get asset byte size of \"%s\"", 
		     kgtAssetFileNames[kai]);
		return;
	}
	/* lock the asset manager's asset data allocator so we can safely allocate 
		data for the raw file.  The decoded asset structure is stored entirely 
		in the KgtAsset */
	g_kpl->lock(asset->kam->hLockAssetDataAllocator);
	void*const rawFileMemory = 
		kgtAllocAlloc(asset->kam->hKgaRawFiles, 
		            kmath::safeTruncateU32(assetByteSize) + 1);
	kassert(rawFileMemory);
	g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	/* defer cleanup of the raw file memory until after we utilize it */
	defer({
		g_kpl->lock(asset->kam->hLockAssetDataAllocator);
		kgtAllocFree(asset->kam->hKgaRawFiles, rawFileMemory);
		g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	});
	/* load the entire raw file into a `fileByteSize` chunk */
	const bool assetReadSuccess = 
		g_kpl->readEntireAsset(
			kgtAssetFileNames[kai], rawFileMemory, 
			kmath::safeTruncateU32(assetByteSize));
	if(!assetReadSuccess)
	{
		KLOG(ERROR, "Failed to read asset \"%s\"!", kgtAssetFileNames[kai]);
		return;
	}
	/* null-terminate the file buffer */
	reinterpret_cast<u8*>(rawFileMemory)[assetByteSize] = 0;
	/* decode the file data into a struct we can use */
	const bool textureMetaDecodeSuccess = 
		korlTextureMetaDecode(
			rawFileMemory, kmath::safeTruncateU32(assetByteSize), 
			kgtAssetFileNames[kai], &asset->assetData.texture.metaData, 
			asset->assetData.texture.imageAssetName, 
			CARRAY_SIZE(asset->assetData.texture.imageAssetName));
	if(!textureMetaDecodeSuccess)
	{
		KLOG(ERROR, "Failed to decode texture meta data \"%s\"!", 
		     kgtAssetFileNames[kai]);
		return;
	}
}
// @optimization (minor): just bake this info using `kasset`
enum class KgtAssetFileType
{
	PNG, WAV, OGG, FLIPBOOK_META, TEXTURE_META, 
	UNKNOWN
};
internal KgtAssetFileType 
	kgtAssetManagerAssetFileType(KgtAssetIndex assetIndex)
{
	if(strstr(kgtAssetFileNames[static_cast<size_t>(assetIndex)], ".png"))
	{
		return KgtAssetFileType::PNG;
	}
	else if(strstr(kgtAssetFileNames[static_cast<size_t>(assetIndex)], ".wav"))
	{
		return KgtAssetFileType::WAV;
	}
	else if(strstr(kgtAssetFileNames[static_cast<size_t>(assetIndex)], ".ogg"))
	{
		return KgtAssetFileType::OGG;
	}
	else if(strstr(kgtAssetFileNames[static_cast<size_t>(assetIndex)], ".fbm"))
	{
		return KgtAssetFileType::FLIPBOOK_META;
	}
	else if(strstr(kgtAssetFileNames[static_cast<size_t>(assetIndex)], ".tex"))
	{
		return KgtAssetFileType::TEXTURE_META;
	}
	return KgtAssetFileType::UNKNOWN;
}
internal KgtAssetHandle 
	kgtAssetManagerPushAsset(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	kassert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	/* load the asset from the platform layer */
	{
		KLOG(INFO, "Loading asset '%s'...", kgtAssetFileNames[assetHandle]);
		asset->loaded        = false;
		asset->kgtAssetIndex = assetHandle;
		const KgtAssetFileType assetFileType = 
			kgtAssetManagerAssetFileType(assetIndex);
		switch(assetFileType)
		{
			case KgtAssetFileType::PNG:
			{
				asset->type            = KgtAssetType::RAW_IMAGE;
				asset->kam             = kam;
				asset->assetData.image = {};
				asset->jqTicketLoading = 
					g_kpl->postJob(kgtAssetManagerAsyncLoadPng, asset);
			}break;
			case KgtAssetFileType::WAV:
			{
				asset->type            = KgtAssetType::RAW_SOUND;
				asset->kam             = kam;
				asset->jqTicketLoading = 
					g_kpl->postJob(kgtAssetManagerAsyncLoadWav, asset);
			}break;
			case KgtAssetFileType::OGG:
			{
				asset->type            = KgtAssetType::RAW_SOUND;
				asset->kam             = kam;
				asset->jqTicketLoading = 
					g_kpl->postJob(kgtAssetManagerAsyncLoadOgg, asset);
			}break;
			case KgtAssetFileType::FLIPBOOK_META:
			{
				asset->type            = KgtAssetType::FLIPBOOK_META;
				asset->kam             = kam;
				asset->jqTicketLoading = 
					g_kpl->postJob(kgtAssetManagerAsyncLoadFlipbookMeta, asset);
			}break;
			case KgtAssetFileType::TEXTURE_META:
			{
				asset->type            = KgtAssetType::TEXTURE_META;
				asset->kam             = kam;
				asset->jqTicketLoading = 
					g_kpl->postJob(kgtAssetManagerAsyncLoadTextureMeta, asset);
			}break;
			case KgtAssetFileType::UNKNOWN:
			default:
			{
				KLOG(ERROR, "Unknown KgtAssetFileType==%i for assetIndex==%i!",
				     static_cast<i32>(assetFileType), assetIndex);
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
internal bool 
	kgtAssetManagerIsLoadingAssets(KgtAssetManager* kam, KgtAssetType type)
{
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	for(KgtAssetHandle kah = 0; kah < kam->maxAssetHandles; kah++)
	{
		KgtAsset*const asset = assets + kah;
		if(asset->type == type && !asset->loaded)
		{
			if(g_kpl->jobDone(&asset->jqTicketLoading))
			{
				kgtAssetManagerOnLoadingJobFinished(kam, kah);
				continue;
			}
			return true;
		}
	}
	return false;
}
internal bool kgtAssetManagerIsLoadingImages(KgtAssetManager* kam)
{
	return kgtAssetManagerIsLoadingAssets(kam, KgtAssetType::RAW_IMAGE) 
	     | kgtAssetManagerIsLoadingAssets(kam, KgtAssetType::FLIPBOOK_META) 
	     | kgtAssetManagerIsLoadingAssets(kam, KgtAssetType::TEXTURE_META);
}
internal bool kgtAssetManagerIsLoadingSounds(KgtAssetManager* kam)
{
	return kgtAssetManagerIsLoadingAssets(kam, KgtAssetType::RAW_SOUND);
}
internal u32 kgtAssetManagerUnloadChangedAssets(KgtAssetManager* kam)
{
	u32 unloadedAssetCount = 0;
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	for(KgtAssetHandle kah = 0; kah < kam->maxAssetHandles; kah++)
	{
		KgtAsset*const asset = assets + kah;
		if(asset->type != KgtAssetType::UNUSED && asset->loaded)
		{
			if(g_kpl->isAssetChanged(kgtAssetFileNames[kah], 
			                         asset->lastWriteTime))
			{
				KLOG(INFO, "Unloading asset '%s'...", kgtAssetFileNames[kah]);
				kgtAssetManagerFreeAsset(kam, kah);
				unloadedAssetCount++;
			}
		}
	}
	return unloadedAssetCount;
}
internal void kgtAssetManagerUnloadAllAssets(KgtAssetManager* kam)
{
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	for(KgtAssetHandle kah = 0; kah < kam->maxAssetHandles; kah++)
	{
		KgtAsset*const asset = assets + kah;
		if(asset->type != KgtAssetType::UNUSED && asset->loaded)
		{
			KLOG(INFO, "Unloading asset '%s'...", kgtAssetFileNames[kah]);
			kgtAssetManagerFreeAsset(kam, kah);
		}
	}
}
internal void kgtAssetManagerPushAllKgtAssets(KgtAssetManager* kam)
{
	for(u32 kai = 0; kai < KGT_ASSET_COUNT; kai++)
	{
		kgtAssetManagerPushAsset(kam, KgtAssetIndex(kai));
	}
}
