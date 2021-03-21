#include "kgtAssetManager.h"
internal KgtAssetManager* kgt_assetManager_construct(
	KgtAllocatorHandle hKgtAllocator, KgtAssetHandle maxAssetHandles, 
	KgtAllocatorHandle hKgtAllocatorAssetData, const KorlPlatformApi* korl, 
	const KrbApi* krb)
{
	if(maxAssetHandles < KGT_ASSET_COUNT)
	{
		KLOG(ERROR, "maxAssetHandles(%i) < KGT_ASSET_COUNT(%i)!", 
			maxAssetHandles, KGT_ASSET_COUNT);
		return nullptr;
	}
	const size_t rawFilePoolBytes = kmath::megabytes(1);
	korlAssert(maxAssetHandles > 0);
	const size_t requiredBytes = sizeof(KgtAssetManager) + 
		sizeof(KgtAsset)*(maxAssetHandles - 1) + rawFilePoolBytes;
	KgtAssetManager*const result = reinterpret_cast<KgtAssetManager*>(
		kgtAllocAlloc(hKgtAllocator, requiredBytes));
	if(!result)
	{
		KLOG(ERROR, "Failed to allocate memory for KgtAssetManager!");
		return nullptr;
	}
	/* initialize the default assets in an un-loaded state */
	for(u32 d = 0; d < static_cast<u32>(KgtAsset::Type::ENUM_COUNT); d++)
		result->assetDescriptors[d] = {};
	/* initialize the assets in an un-loaded state */
	for(KgtAssetHandle hAsset = 0; hAsset < maxAssetHandles; hAsset++)
	{
		result->assets[hAsset] = {};
		result->assets[hAsset].type = KgtAsset::Type::ENUM_COUNT;
	}
	/* initialize the raw file data allocator */
	void*const rawFileAllocatorAddress = 
		reinterpret_cast<u8*>(result) + sizeof(KgtAssetManager) + 
		sizeof(KgtAsset)*maxAssetHandles;
	const KgtAllocatorHandle hKgtAllocatorRawFiles = kgtAllocInit(
		KgtAllocatorType::GENERAL, rawFileAllocatorAddress, rawFilePoolBytes);
	/* request access to a spinlock from the platform layer so we can keep 
		the asset data allocator safe between asset loading job threads */
	const KplLockHandle hLockAssetDataAllocator = korl->reserveLock();
	/* Initialize the rest of the data members of the asset manager.  NOTE: we 
		don't use initializer braces because MSVC wont initialize an array with 
		constant size 0 */
	result->korl                    = korl;
	result->krb                     = krb;
	result->maxAssetHandles         = maxAssetHandles;
	result->hKgtAllocatorAssetData  = hKgtAllocatorAssetData;
	result->hKgtAllocatorRawFiles   = hKgtAllocatorRawFiles;
	result->hLockAssetDataAllocator = hLockAssetDataAllocator;
	return result;
}
internal void kgt_assetManager_addAssetDescriptor(
	KgtAssetManager* kam, KgtAsset::Type type, const char*const fileExtension, 
	u8* rawDefaultAssetData, u32 rawDefaultAssetDataBytes)
{
	const u32 descriptorIndex = static_cast<u32>(type);
	korlAssert(descriptorIndex < static_cast<u32>(KgtAsset::Type::ENUM_COUNT));
	KgtAssetManager::AssetDescriptor& descriptor = 
		kam->assetDescriptors[descriptorIndex];
	/* copy the file extension to the descriptor */
	const errno_t errorResultCopyFileExtension = 
		strcpy_s(
			descriptor.fileExtension, CARRAY_SIZE(descriptor.fileExtension), 
			fileExtension);
	korlAssert(errorResultCopyFileExtension == 0);
	descriptor.fileExtensionSize = kmath::safeTruncateU8(
		strnlen_s(fileExtension, CARRAY_SIZE(descriptor.fileExtension)));
	korlAssert(descriptor.fileExtensionSize > 0);
	/* load the default asset using the provided raw asset data */
	descriptor.defaultAsset.kam = kam;
	descriptor.defaultAsset.type = type;
	kgt_asset_decode(
		&descriptor.defaultAsset, kam->hKgtAllocatorAssetData, 
		rawDefaultAssetData, rawDefaultAssetDataBytes, "default-asset");
	/* some default assets require default assets themselves, but there's no 
		reason to call the onDependenciesLoaded callback if the asset has no 
		dependencies */
	if(descriptor.defaultAsset.dependencyCount)
		kgt_asset_onDependenciesLoaded(&descriptor.defaultAsset, kam);
	descriptor.defaultAsset.loaded = true;
}
internal void _kgt_assetManager_matchAssetDescriptor(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	/* the first KgtAsset::Type::ENUM_COUNT asset handles are reserved for 
		KgtAssetIndex values! */
	const size_t kamIndex = static_cast<size_t>(assetIndex);
	korlAssert(kamIndex < kam->maxAssetHandles);
	KgtAsset& asset = kam->assets[kamIndex];
	/* if the asset type is already defined, we're done */
	if(asset.type != KgtAsset::Type::ENUM_COUNT)
		return;
	/* figure out which asset descriptor matches the file extension of the 
		KAsset located at `assetIndex`, then set the KgtAsset type of this index 
		to match the descriptor's type */
	const char* cStrAssetName = 
		kgtAssetFileNames[static_cast<size_t>(assetIndex)];
	/* @speed: generate an array of asset name lengths in KAsset program, then 
		pass those values into this function as a param so we don't even have to 
		call strlen! */
	const size_t cStrAssetNameLength = strnlen_s(cStrAssetName, 256);
	KgtAsset::Type assetType = KgtAsset::Type::ENUM_COUNT;
	for(u32 d = 0; d < static_cast<u32>(KgtAsset::Type::ENUM_COUNT); d++)
	{
		const KgtAssetManager::AssetDescriptor& descriptor = 
			kam->assetDescriptors[d];
		const int resultCompareExtension = 
			strcmp(
				descriptor.fileExtension, 
				cStrAssetName + cStrAssetNameLength 
					- descriptor.fileExtensionSize);
		if(resultCompareExtension == 0)
		{
			assetType = KgtAsset::Type(d);
			break;
		}
	}
	korlAssert(assetType < KgtAsset::Type::ENUM_COUNT);
	asset.type = assetType;
}
/** Construct a string in the provided buffer which represents the asset file 
 * path relative to the platform's current working directory.  The expectation 
 * is that the final deployed project is stored in the same directory as a 
 * folder called "assets" which contains all the files in the `kasset` database.  
 * By default, the `CURRENT` platform directory is always the same directory as 
 * the platform executable!  However, the caller can configure it to be whatever 
 * they want it to be.  For example, when debugging we can use the project 
 * directory instead, since this contains the original asset folder! */
internal bool _kgt_assetManager_buildCurrentRelativeFilePath(
	char* o_filePathBuffer, u32 filePathBufferBytes, const char* assetFileName)
{
	const int charactersWritten = 
		sprintf_s(
			o_filePathBuffer, filePathBufferBytes, "assets/%s", assetFileName);
	return charactersWritten > 0;
}
JOB_QUEUE_FUNCTION(_kgt_assetManager_asyncLoad)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	char filePathBuffer[256];
	const bool successBuildExeFilePath = 
		_kgt_assetManager_buildCurrentRelativeFilePath(
			filePathBuffer, CARRAY_SIZE(filePathBuffer), 
			kgtAssetFileNames[asset->kgtAssetIndex]);
	korlAssert(successBuildExeFilePath);
	/* loop forever until we obtain an exclusive handle to the file */
	KorlFileHandle hFile;
	while(!asset->kam->korl->getFileHandle(
		filePathBuffer, KorlApplicationDirectory::CURRENT, 
		KorlFileHandleUsage::READ_EXISTING, &hFile))
	{
		local_const f32 KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS = 0.25f;
		KLOG(INFO, "Waiting for asset '%s'...", filePathBuffer);
		asset->kam->korl->sleepFromTimeStamp(
			asset->kam->korl->getTimeStamp(), 
			KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS);
	}
	/* update the asset's last known write time */
	asset->lastWriteTime = 
		asset->kam->korl->getFileWriteTime(
			filePathBuffer, KorlApplicationDirectory::CURRENT);
	/* @speed: it's probably the case for MOST types of assets that you don't 
		actually have to read the entire asset into memory before decoding it!  
		It may be much less wasteful if we just decoded the asset while reading 
		the file sequentially, although this would mean holding the file handle 
		for a longer period of time */
	/* obtain the size of the raw file */
	const i32 resultGetFileByteSize = asset->kam->korl->getFileByteSize(hFile);
	korlAssert(resultGetFileByteSize > 0);
	const u32 fileByteSize = kmath::safeTruncateU32(resultGetFileByteSize);
	/* safely allocate data for the raw file */
	asset->kam->korl->lock(asset->kam->hLockAssetDataAllocator);
	u8*const rawFileData = reinterpret_cast<u8*>(
		kgtAllocAlloc(
			asset->kam->hKgtAllocatorRawFiles, fileByteSize));
	asset->kam->korl->unlock(asset->kam->hLockAssetDataAllocator);
	/* read the raw file into memory */
	const bool successReadEntireFile = 
		asset->kam->korl->readEntireFile(hFile, rawFileData, fileByteSize);
	korlAssert(successReadEntireFile);
	/* decode the KgtAsset safely into bulk data storage, then free the memory 
		holding the raw asset file */
	asset->kam->korl->lock(asset->kam->hLockAssetDataAllocator);
	kgt_asset_decode(
		asset, asset->kam->hKgtAllocatorAssetData, rawFileData, fileByteSize, 
		kgtAssetFileNames[asset->kgtAssetIndex]);
	kgtAllocFree(asset->kam->hKgtAllocatorRawFiles, rawFileData);
	asset->kam->korl->unlock(asset->kam->hLockAssetDataAllocator);
	/* release the file handle */
	asset->kam->korl->releaseFileHandle(hFile);
}
internal KgtAssetHandle _kgt_assetManager_makeHandle(size_t kamIndex)
{
	korlAssert(
		std::numeric_limits<KgtAssetHandle>::max() > 
		static_cast<KgtAssetHandle>(kamIndex));
	return static_cast<KgtAssetHandle>(kamIndex) + 1;
}
internal KgtAssetHandle kgt_assetManager_load(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	/* the first KgtAsset::Type::ENUM_COUNT asset handles are reserved for 
		KgtAssetIndex values! */
	const size_t kamIndex = static_cast<size_t>(assetIndex);
	korlAssert(kamIndex < kam->maxAssetHandles);
	KgtAsset& asset = kam->assets[kamIndex];
	if(asset.loaded)
		return _kgt_assetManager_makeHandle(kamIndex);
	_kgt_assetManager_matchAssetDescriptor(kam, assetIndex);
	/* start an asynchronous job which will attempt to load the asset (if there 
		is not already one running) */
	if(asset.jobTicketLoading)
		return _kgt_assetManager_makeHandle(kamIndex);
	KLOG(INFO, "Loading asset '%s'...", kgtAssetFileNames[kamIndex]);
	asset.kam = kam;
	asset.kgtAssetIndex = static_cast<u32>(assetIndex);
	asset.jobTicketLoading = 
		kam->korl->postJob(_kgt_assetManager_asyncLoad, &asset);
	return _kgt_assetManager_makeHandle(kamIndex);
}
internal void _kgt_assetManager_load(
	KgtAssetManager* kam, KgtAssetHandle hAsset)
{
	if(!hAsset)
		return;
	/* @robustness: create a "decode handle" function */
	const size_t kamIndex = hAsset - 1;
	korlAssert(kamIndex < kam->maxAssetHandles);
	KgtAsset& asset = kam->assets[kamIndex];
	if(asset.loaded)
		return;
	if(kamIndex < KGT_ASSET_COUNT)
		/* @speed: call this code once for all valid KgtAssetIndex values after 
			all the asset descriptors are added to the asset manager, and we 
			never actually have to run this code ever again, since KgtAssetIndex 
			data is determined at compile-time */
		_kgt_assetManager_matchAssetDescriptor(
			kam, static_cast<KgtAssetIndex>(kamIndex));
	else
		korlAssert(!"Not implemented!");
	/* start an asynchronous job which will attempt to load the asset (if there 
		is not already one running) */
	if(asset.jobTicketLoading)
		return;
	KLOG(INFO, "Loading asset '%s'...", kgtAssetFileNames[kamIndex]);
	asset.kam = kam;
	asset.kgtAssetIndex = kmath::safeTruncateU32(kamIndex);
	asset.jobTicketLoading = 
		kam->korl->postJob(_kgt_assetManager_asyncLoad, &asset);
}
internal void kgt_assetManager_free(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	/* the first KgtAsset::Type::ENUM_COUNT asset handles are reserved for 
		KgtAssetIndex values! */
	const size_t kamIndex = static_cast<size_t>(assetIndex);
	korlAssert(kamIndex < kam->maxAssetHandles);
	KgtAsset& asset = kam->assets[kamIndex];
	if(!asset.loaded)
	{
		KLOG(WARNING, "asset[%i] is already free!", kamIndex);
		return;
	}
	kam->korl->lock(kam->hLockAssetDataAllocator);
	kgt_asset_free(&asset, kam->hKgtAllocatorAssetData);
	kam->korl->unlock(kam->hLockAssetDataAllocator);
	asset.loaded = false;
}
#if 0
/** @return true if all dependencies of the asset are loaded */
internal bool _kgt_assetManager_loadDependencies(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	/* the first KgtAsset::Type::ENUM_COUNT asset handles are reserved for 
		KgtAssetIndex values! */
	const KgtAssetHandle kamIndex = static_cast<KgtAssetHandle>(assetIndex);
	korlAssert(kamIndex < kam->maxAssetHandles);
	KgtAsset& asset = kam->assets[kamIndex];
	if(asset.loaded)
		return false;
	for(u8 d = 0; d < asset.dependencyCount; d++)
		if(!_kgt_assetManager_loadDependencies(kam, ))
			return false;
	korlAssert(!"@todo");
	return true;
}
#endif//0
internal const KgtAsset* kgt_assetManager_get(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
		return nullptr;
	const KgtAssetHandle assetHandle = kgt_asset_makeHandle(assetIndex);
	return kgt_assetManager_get(kam, assetHandle);
#if 0
	/* ensure that the asset manager has begin async loading the asset if it has 
		not already done so */
	kgt_assetManager_load(kam, assetIndex);
	/* if the asset's loading job is finished, then we can mark the asset as 
		loaded (which ends the async load job) */
	if(kam->korl->jobDone(&(asset.jobTicketLoading)))
		asset.loaded = true;
	/* if the asset is loaded, we have to ensure that all the asset's 
		dependencies are loaded before returning the asset */
	const bool fullyLoaded = asset.loaded 
		? _kgt_assetManager_loadDependencies(kam, assetIndex)
		: false;
	/* if the asset isn't marked as loaded at this point, we should just return 
		the default asset */
	if(!fullyLoaded)
	{
		korlAssert(asset.type != KgtAsset::Type::ENUM_COUNT);
		const size_t descriptorIndex = static_cast<size_t>(asset.type);
		return &(kam->assetDescriptors[descriptorIndex].defaultAsset);
	}
	/* otherwise return the loaded asset */
	return &asset;
#endif//0
}
/** @return true if the asset & all its dependencies are loaded */
internal bool _kgt_assetManager_fullyLoad(
	KgtAssetManager* kam, KgtAssetHandle hAsset)
{
	/* ensure that the asset manager has begun async loading the asset if it has 
		not already done so */
	_kgt_assetManager_load(kam, hAsset);
	/* if the asset's loading job is finished, then we can mark the asset as 
		loaded (which ends the async load job) */
	/* @robustness: create a "decode handle" function */
	const size_t kamIndex = hAsset - 1;
	korlAssert(kamIndex < kam->maxAssetHandles);
	KgtAsset& asset = kam->assets[kamIndex];
	if(kam->korl->jobDone(&(asset.jobTicketLoading)))
		asset.loaded = true;
	if(!asset.loaded)
		return false;
	/* iterate over all the asset's dependencies and recursively fully-load */
	for(u8 d = 0; d < asset.dependencyCount; d++)
		if(!_kgt_assetManager_fullyLoad(kam, asset.dependencies[d]))
			return false;
	/* if we've made it this far, it must be the case that the asset it loaded & 
		all its dependencies are loaded, so we can return true! */
	return true;
}
internal const KgtAsset* kgt_assetManager_get(
	KgtAssetManager* kam, KgtAssetHandle hAsset)
{
	if(!hAsset)
		return nullptr;
	if(!_kgt_assetManager_fullyLoad(kam, hAsset))
	{
		/* return the default asset if the hAsset along with all its 
			dependencies are not ALL loaded */
		/* @robustness: create a "decode handle" function */
		const size_t kamIndex = hAsset - 1;
		korlAssert(kamIndex < kam->maxAssetHandles);
		const KgtAsset& asset = kam->assets[kamIndex];
		korlAssert(asset.type != KgtAsset::Type::ENUM_COUNT);
		const size_t descriptorIndex = static_cast<size_t>(asset.type);
		return &(kam->assetDescriptors[descriptorIndex].defaultAsset);
	}
	/* @robustness: create a "decode handle" function */
	const size_t kamIndex = hAsset - 1;
	return &kam->assets[kamIndex];
}
internal const KgtAsset* kgt_assetManager_getDefault(
	KgtAssetManager* kam, KgtAsset::Type assetType)
{
	if(assetType >= KgtAsset::Type::ENUM_COUNT)
		return nullptr;
	return &kam->assetDescriptors[static_cast<u32>(assetType)].defaultAsset;
}
#if 0
#include "z85-png-default.h"
#include "z85-wav-default.h"
#include "korl-texture.h"
#include <string>
enum class KgtAssetType : u8
	{ UNUSED
	, RAW_IMAGE
	, RAW_SOUND
	, FLIPBOOK_META
	, TEXTURE_META
	, SPRITE_FONT_META
	, BINARY_DATA };
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
		struct
		{
			KgtSpriteFontMetaData metaData;
			char texAssetName[128];
			char texAssetNameOutline[128];
			KgtAssetIndex kaiTex;
			KgtAssetIndex kaiTexOutline;
		} spriteFont;
		struct 
		{
			u8* data;
			u32 bytes;
		} binary;
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
	KgtAsset defaultAssetSpriteFont;
	KgtAllocatorHandle assetDataAllocator;
	KgtAllocatorHandle hKgaRawFiles;
	KplLockHandle hLockAssetDataAllocator;
	KrbApi* krb;
	//KgtAsset assets[];
};
/**
 * Construct a string in the provided buffer which represents the asset file 
 * path relative to the platform's current working directory.  The expectation 
 * is that the final deployed project is stored in the same directory as a 
 * folder called "assets" which contains all the files in the `kasset` database.  
 * By default, the `CURRENT` platform directory is always the same directory as 
 * the platform executable!  However, the caller can configure it to be whatever 
 * they want it to be.  For example, when debugging we can use the project 
 * directory instead, since this contains the original asset folder!
 */
internal bool 
	kgtAssetManagerBuildCurrentRelativeFilePath(
		char* o_filePathBuffer, u32 filePathBufferBytes, 
		const char* assetFileName)
{
	const int charactersWritten = 
		sprintf_s(o_filePathBuffer, filePathBufferBytes, 
		          "assets/%s", assetFileName);
	return charactersWritten > 0;
}
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
			defaultAssetImage.assetData.image.rawImage.pixelData, 
			defaultAssetImage.assetData.image.rawImage.pixelDataFormat )
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
	KgtAsset defaultAssetSpriteFont;
	defaultAssetSpriteFont.type = KgtAssetType::TEXTURE_META;
	defaultAssetSpriteFont.assetData.spriteFont.metaData = 
		{ .kaiTexture        = KgtAssetIndex::ENUM_SIZE
		, .kaiTextureOutline = KgtAssetIndex::ENUM_SIZE
		, .monospaceSize     = {8,8}
		, .texturePadding    = {0,0}
		, .charactersSize    = 0 };
	defaultAssetSpriteFont.assetData.spriteFont.kaiTex = 
		KgtAssetIndex::ENUM_SIZE;
	defaultAssetSpriteFont.assetData.spriteFont.kaiTexOutline = 
		KgtAssetIndex::ENUM_SIZE;
	strcpy_s(defaultAssetTexture.assetData.spriteFont.texAssetName, 
		CARRAY_SIZE(defaultAssetTexture.assetData.spriteFont.texAssetName),
		"texture-default");
	strcpy_s(defaultAssetTexture.assetData.spriteFont.texAssetNameOutline, 
		CARRAY_SIZE(
			defaultAssetTexture.assetData.spriteFont.texAssetNameOutline),
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
		, .defaultAssetSpriteFont       = defaultAssetSpriteFont
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
	korlAssert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(!asset->loaded)
	{
		KLOG(WARNING, "KgtAsset[%i] is already free!", assetHandle);
		return;
	}
	switch(asset->type)
	{
	case KgtAssetType::RAW_IMAGE: {
		kam->krb->deleteTexture(asset->assetData.image.krbTextureHandle);
		kgtAllocFree(kam->assetDataAllocator, 
		             asset->assetData.image.rawImage.pixelData);
		} break;
	case KgtAssetType::RAW_SOUND: {
		kgtAllocFree(kam->assetDataAllocator, 
		             asset->assetData.sound.sampleData);
		} break;
	case KgtAssetType::FLIPBOOK_META: {
		KgtAssetHandle kahTexture = static_cast<KgtAssetHandle>(
			asset->assetData.flipbook.metaData.kaiTexture);
		kgtAssetManagerFreeAsset(kam, kahTexture);
		} break;
	case KgtAssetType::TEXTURE_META: {
		kgtAssetManagerFreeAsset(kam, asset->assetData.texture.kaiImage);
		} break;
	case KgtAssetType::SPRITE_FONT_META: {
		kgtAssetManagerFreeAsset(kam, asset->assetData.spriteFont.kaiTex);
		kgtAssetManagerFreeAsset(
			kam, asset->assetData.spriteFont.kaiTexOutline);
		} break;
	case KgtAssetType::BINARY_DATA: {
		kgtAllocFree(kam->assetDataAllocator, asset->assetData.binary.data);
		} break;
	case KgtAssetType::UNUSED: 
	default: {
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
	korlAssert(kah < kam->maxAssetHandles);
	KgtAsset*const asset = assets + kah;
	switch(asset->type)
	{
	case KgtAssetType::RAW_IMAGE: {
		korlAssert(asset->assetData.image.krbTextureHandle == 
			krb::INVALID_TEXTURE_HANDLE);
		asset->assetData.image.krbTextureHandle = kam->krb->loadImage(
			asset->assetData.image.rawImage.sizeX, 
			asset->assetData.image.rawImage.sizeY, 
			asset->assetData.image.rawImage.pixelData, 
			asset->assetData.image.rawImage.pixelDataFormat );
		} break;
	case KgtAssetType::RAW_SOUND: {
		} break;
	case KgtAssetType::FLIPBOOK_META: {
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
	case KgtAssetType::TEXTURE_META: {
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
	case KgtAssetType::SPRITE_FONT_META: {
		const KgtAssetIndex kaiTexture = 
			kgtAssetManagerFindKgtAssetIndex(
				asset->assetData.spriteFont.texAssetName);
		const KgtAssetIndex kaiTextureOutline = 
			kgtAssetManagerFindKgtAssetIndex(
				asset->assetData.spriteFont.texAssetNameOutline);
		korlAssert(kaiTexture        < KgtAssetIndex::ENUM_SIZE);
		korlAssert(kaiTextureOutline < KgtAssetIndex::ENUM_SIZE);
		asset->assetData.spriteFont.kaiTex        = kaiTexture;
		asset->assetData.spriteFont.kaiTexOutline = kaiTextureOutline;
		asset->assetData.spriteFont.metaData.kaiTexture = kaiTexture;
		asset->assetData.spriteFont.metaData.kaiTextureOutline = 
			kaiTextureOutline;
		kgtAssetManagerPushAsset(kam, kaiTexture);
		kgtAssetManagerPushAsset(kam, kaiTextureOutline);
		} break;
	case KgtAssetType::BINARY_DATA: {
		/* allocate storage from assetDataAllocator */
		u8*const assetData = reinterpret_cast<u8*>(
			kgtAllocAlloc(kam->assetDataAllocator, 
			              asset->assetData.binary.bytes));
		/* copy the data over */
		memcpy(assetData, asset->assetData.binary.data, 
		       asset->assetData.binary.bytes);
		/* safely deallocate the hKgaRawFiles data */
		g_kpl->lock(kam->hLockAssetDataAllocator);
		kgtAllocFree(kam->hKgaRawFiles, asset->assetData.binary.data);
		g_kpl->unlock(kam->hLockAssetDataAllocator);
		/* set the new asset binary data pointer */
		asset->assetData.binary.data = assetData;
		} break;
	case KgtAssetType::UNUSED: {
		KLOG(ERROR, "UNUSED asset!");
		return;
		} break;
	}
	char filePathBuffer[256];
	const bool successBuildExeFilePath = 
		kgtAssetManagerBuildCurrentRelativeFilePath(
			filePathBuffer, CARRAY_SIZE(filePathBuffer), 
			kgtAssetFileNames[kah]);
	korlAssert(successBuildExeFilePath);
	asset->lastWriteTime = 
		g_kpl->getFileWriteTime(
			filePathBuffer, KorlApplicationDirectory::CURRENT);
	asset->loaded = true;
}
internal KgtAssetManagerByteArray 
	kgtAssetManagerGetBinary(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return { .data = nullptr, .bytes = 0 };
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	korlAssert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		korlAssert(asset->type == KgtAssetType::BINARY_DATA);
		return { .data  = asset->assetData.binary.data
		       , .bytes = asset->assetData.binary.bytes };
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			korlAssert(asset->type == KgtAssetType::BINARY_DATA);
			return { .data  = asset->assetData.binary.data
			       , .bytes = asset->assetData.binary.bytes };
		}
		return { .data = nullptr, .bytes = 0 };
	}
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
	korlAssert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		korlAssert(asset->type == KgtAssetType::RAW_SOUND);
		return asset->assetData.sound;
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			korlAssert(asset->type == KgtAssetType::RAW_SOUND);
			return asset->assetData.sound;
		}
		return kam->defaultAssetSound.assetData.sound;
	}
}
internal RawImage 
	kgtAssetManagerGetRawImage(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetImage.assetData.image.rawImage;
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	korlAssert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		if(asset->type == KgtAssetType::TEXTURE_META)
			return kgtAssetManagerGetRawImage(
				kam, asset->assetData.texture.kaiImage);
		else if(asset->type == KgtAssetType::RAW_IMAGE)
			return asset->assetData.image.rawImage;
		else
		{
			KLOG(ERROR, "Invalid asset type (%i)!", asset->type);
			return {};
		}
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			if(asset->type == KgtAssetType::TEXTURE_META)
				return kgtAssetManagerGetRawImage(
					kam, asset->assetData.texture.kaiImage);
			else if(asset->type == KgtAssetType::RAW_IMAGE)
				return asset->assetData.image.rawImage;
			else
			{
				KLOG(ERROR, "Invalid asset type (%i)!", asset->type);
				return {};
			}
		}
		return kam->defaultAssetImage.assetData.image.rawImage;
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
	korlAssert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		korlAssert(asset->type == KgtAssetType::RAW_IMAGE);
		return asset->assetData.image.krbTextureHandle;
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			korlAssert(asset->type == KgtAssetType::RAW_IMAGE);
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
	korlAssert(assetHandle < kam->maxAssetHandles);
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
	korlAssert(assetHandle < kam->maxAssetHandles);
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
/** this is a specialized internal function which ONLY works for RAW_IMAGE 
 * assets */
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
	korlAssert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		korlAssert(asset->type == KgtAssetType::RAW_IMAGE);
		return {asset->assetData.image.rawImage.sizeX,
		        asset->assetData.image.rawImage.sizeY};
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			korlAssert(asset->type == KgtAssetType::RAW_IMAGE);
			return {asset->assetData.image.rawImage.sizeX,
			        asset->assetData.image.rawImage.sizeY};
		}
		return {kam->defaultAssetImage.assetData.image.rawImage.sizeX,
		        kam->defaultAssetImage.assetData.image.rawImage.sizeY};
	}
}
internal v2u32 
	kgtAssetManagerGetImageSize(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return {kam->defaultAssetImage.assetData.image.rawImage.sizeX,
		        kam->defaultAssetImage.assetData.image.rawImage.sizeY};
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	korlAssert(assetHandle < kam->maxAssetHandles);
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
	korlAssert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		korlAssert(asset->type == KgtAssetType::FLIPBOOK_META);
		return asset->assetData.flipbook.metaData;
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			korlAssert(asset->type == KgtAssetType::FLIPBOOK_META);
			return asset->assetData.flipbook.metaData;
		}
		return kam->defaultAssetFlipbookMetaData.assetData.flipbook.metaData;
	}
}
internal const KgtSpriteFontMetaData& 
	kgtAssetManagerGetSpriteFontMetaData(
		KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	if(assetIndex >= KgtAssetIndex::ENUM_SIZE)
	{
		return kam->defaultAssetSpriteFont.assetData.spriteFont.metaData;
	}
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	korlAssert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	{
		kgtAssetManagerPushAsset(kam, assetIndex);
	}
	if(asset->loaded)
	{
		korlAssert(asset->type == KgtAssetType::SPRITE_FONT_META);
		return asset->assetData.spriteFont.metaData;
	}
	else
	{
		if(g_kpl->jobDone(&asset->jqTicketLoading))
		{
			kgtAssetManagerOnLoadingJobFinished(kam, assetHandle);
			korlAssert(asset->type == KgtAssetType::SPRITE_FONT_META);
			return asset->assetData.spriteFont.metaData;
		}
		return kam->defaultAssetSpriteFont.assetData.spriteFont.metaData;
	}
}
global_variable const f32 KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS = 0.25f;
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadPng)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	char filePathBuffer[256];
	const bool successBuildExeFilePath = 
		kgtAssetManagerBuildCurrentRelativeFilePath(
			filePathBuffer, CARRAY_SIZE(filePathBuffer), 
			kgtAssetFileNames[kai]);
	korlAssert(successBuildExeFilePath);
	while(!g_kpl->isFileAvailable(
			filePathBuffer, KorlApplicationDirectory::CURRENT))
	{
		KLOG(INFO, "Waiting for asset '%s'...", filePathBuffer);
		g_kpl->sleepFromTimeStamp(
			g_kpl->getTimeStamp(), KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS);
	}
	asset->assetData.image.rawImage = 
		g_kpl->loadPng(filePathBuffer, KorlApplicationDirectory::CURRENT, 
		               asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadWav)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	char filePathBuffer[256];
	const bool successBuildExeFilePath = 
		kgtAssetManagerBuildCurrentRelativeFilePath(
			filePathBuffer, CARRAY_SIZE(filePathBuffer), 
			kgtAssetFileNames[kai]);
	korlAssert(successBuildExeFilePath);
	while(!g_kpl->isFileAvailable(
			filePathBuffer, KorlApplicationDirectory::CURRENT))
	{
		KLOG(INFO, "Waiting for asset '%s'...", filePathBuffer);
		g_kpl->sleepFromTimeStamp(
			g_kpl->getTimeStamp(), KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS);
	}
	asset->assetData.sound = 
		g_kpl->loadWav(filePathBuffer, KorlApplicationDirectory::CURRENT, 
		               asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadOgg)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	char filePathBuffer[256];
	const bool successBuildExeFilePath = 
		kgtAssetManagerBuildCurrentRelativeFilePath(
			filePathBuffer, CARRAY_SIZE(filePathBuffer), 
			kgtAssetFileNames[kai]);
	korlAssert(successBuildExeFilePath);
	while(!g_kpl->isFileAvailable(
			filePathBuffer, KorlApplicationDirectory::CURRENT))
	{
		KLOG(INFO, "Waiting for asset '%s'...", filePathBuffer);
		g_kpl->sleepFromTimeStamp(
			g_kpl->getTimeStamp(), KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS);
	}
	asset->assetData.sound = 
		g_kpl->loadOgg(filePathBuffer, KorlApplicationDirectory::CURRENT, 
		               asset->kam->assetDataAllocator);
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadFlipbookMeta)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	char filePathBuffer[256];
	const bool successBuildExeFilePath = 
		kgtAssetManagerBuildCurrentRelativeFilePath(
			filePathBuffer, CARRAY_SIZE(filePathBuffer), 
			kgtAssetFileNames[kai]);
	korlAssert(successBuildExeFilePath);
	while(!g_kpl->isFileAvailable(
			filePathBuffer, KorlApplicationDirectory::CURRENT))
	{
		KLOG(INFO, "Waiting for asset '%s'...", filePathBuffer);
		g_kpl->sleepFromTimeStamp(
			g_kpl->getTimeStamp(), KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS);
	}
	const i32 assetByteSize = 
		g_kpl->getFileByteSize(
			filePathBuffer, KorlApplicationDirectory::CURRENT);
	if(assetByteSize < 0)
	{
		KLOG(ERROR, "Failed to get asset byte size of \"%s\"", 
		     filePathBuffer);
		return;
	}
	/* lock the asset manager's asset data allocator so we can safely allocate 
		data for the raw file.  The decoded FlipbookMeta structure is stored 
		entirely in the KgtAsset */
	g_kpl->lock(asset->kam->hLockAssetDataAllocator);
	void*const rawFileMemory = 
		kgtAllocAlloc(asset->kam->hKgaRawFiles, 
		              kmath::safeTruncateU32(assetByteSize) + 1);
	korlAssert(rawFileMemory);
	g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	/* defer cleanup of the raw file memory until after we utilize it */
	defer({
		g_kpl->lock(asset->kam->hLockAssetDataAllocator);
		kgtAllocFree(asset->kam->hKgaRawFiles, rawFileMemory);
		g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	});
	/* load the entire raw file into a `fileByteSize` chunk */
	const bool assetReadSuccess = 
		g_kpl->readEntireFile(
			filePathBuffer, KorlApplicationDirectory::CURRENT, rawFileMemory, 
			kmath::safeTruncateU32(assetByteSize));
	if(!assetReadSuccess)
	{
		KLOG(ERROR, "Failed to read asset \"%s\"!", filePathBuffer);
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
		     filePathBuffer);
		return;
	}
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadTextureMeta)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	char filePathBuffer[256];
	const bool successBuildExeFilePath = 
		kgtAssetManagerBuildCurrentRelativeFilePath(
			filePathBuffer, CARRAY_SIZE(filePathBuffer), 
			kgtAssetFileNames[kai]);
	korlAssert(successBuildExeFilePath);
	while(!g_kpl->isFileAvailable(
			filePathBuffer, KorlApplicationDirectory::CURRENT))
	{
		KLOG(INFO, "Waiting for asset '%s'...", filePathBuffer);
		g_kpl->sleepFromTimeStamp(
			g_kpl->getTimeStamp(), KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS);
	}
	const i32 assetByteSize = 
		g_kpl->getFileByteSize(
			filePathBuffer, KorlApplicationDirectory::CURRENT);
	if(assetByteSize < 0)
	{
		KLOG(ERROR, "Failed to get asset byte size of \"%s\"", 
		     filePathBuffer);
		return;
	}
	/* lock the asset manager's asset data allocator so we can safely allocate 
		data for the raw file.  The decoded asset structure is stored entirely 
		in the KgtAsset */
	g_kpl->lock(asset->kam->hLockAssetDataAllocator);
	void*const rawFileMemory = 
		kgtAllocAlloc(asset->kam->hKgaRawFiles, 
		              kmath::safeTruncateU32(assetByteSize) + 1);
	korlAssert(rawFileMemory);
	g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	/* defer cleanup of the raw file memory until after we utilize it */
	defer({
		g_kpl->lock(asset->kam->hLockAssetDataAllocator);
		kgtAllocFree(asset->kam->hKgaRawFiles, rawFileMemory);
		g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	});
	/* load the entire raw file into a `fileByteSize` chunk */
	const bool assetReadSuccess = 
		g_kpl->readEntireFile(
			filePathBuffer, KorlApplicationDirectory::CURRENT, rawFileMemory, 
			kmath::safeTruncateU32(assetByteSize));
	if(!assetReadSuccess)
	{
		KLOG(ERROR, "Failed to read asset \"%s\"!", filePathBuffer);
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
		     filePathBuffer);
		return;
	}
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadSpriteFontMeta)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	char filePathBuffer[256];
	const bool successBuildExeFilePath = 
		kgtAssetManagerBuildCurrentRelativeFilePath(
			filePathBuffer, CARRAY_SIZE(filePathBuffer), 
			kgtAssetFileNames[kai]);
	korlAssert(successBuildExeFilePath);
	while(!g_kpl->isFileAvailable(
			filePathBuffer, KorlApplicationDirectory::CURRENT))
	{
		KLOG(INFO, "Waiting for asset '%s'...", filePathBuffer);
		g_kpl->sleepFromTimeStamp(
			g_kpl->getTimeStamp(), KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS);
	}
	const i32 assetByteSize = 
		g_kpl->getFileByteSize(
			filePathBuffer, KorlApplicationDirectory::CURRENT);
	if(assetByteSize < 0)
	{
		KLOG(ERROR, "Failed to get asset byte size of \"%s\"", 
		     filePathBuffer);
		return;
	}
	/* lock the asset manager's asset data allocator so we can safely allocate 
		data for the raw file.  The decoded asset structure is stored entirely 
		in the KgtAsset */
	g_kpl->lock(asset->kam->hLockAssetDataAllocator);
	void*const rawFileMemory = 
		kgtAllocAlloc(asset->kam->hKgaRawFiles, 
		              kmath::safeTruncateU32(assetByteSize) + 1);
	korlAssert(rawFileMemory);
	g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	/* defer cleanup of the raw file memory until after we utilize it */
	defer({
		g_kpl->lock(asset->kam->hLockAssetDataAllocator);
		kgtAllocFree(asset->kam->hKgaRawFiles, rawFileMemory);
		g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	});
	/* load the entire raw file into a `fileByteSize` chunk */
	const bool assetReadSuccess = 
		g_kpl->readEntireFile(
			filePathBuffer, KorlApplicationDirectory::CURRENT, rawFileMemory, 
			kmath::safeTruncateU32(assetByteSize));
	if(!assetReadSuccess)
	{
		KLOG(ERROR, "Failed to read asset \"%s\"!", filePathBuffer);
		return;
	}
	/* null-terminate the file buffer */
	reinterpret_cast<u8*>(rawFileMemory)[assetByteSize] = 0;
	/* decode the file data into a struct we can use */
	const bool decodeSuccess = 
		kgtSpriteFontDecodeMeta(
			rawFileMemory, kmath::safeTruncateU32(assetByteSize), 
			kgtAssetFileNames[kai], &asset->assetData.spriteFont.metaData, 
			asset->assetData.spriteFont.texAssetName, 
			CARRAY_SIZE(asset->assetData.spriteFont.texAssetName), 
			asset->assetData.spriteFont.texAssetNameOutline, 
			CARRAY_SIZE(asset->assetData.spriteFont.texAssetNameOutline));
	if(!decodeSuccess)
	{
		KLOG(ERROR, "Failed to decode asset \"%s\"!", filePathBuffer);
		return;
	}
}
JOB_QUEUE_FUNCTION(kgtAssetManagerAsyncLoadBinary)
{
	KgtAsset*const asset = reinterpret_cast<KgtAsset*>(data);
	const size_t kai = asset->kgtAssetIndex;
	char filePathBuffer[256];
	const bool successBuildExeFilePath = 
		kgtAssetManagerBuildCurrentRelativeFilePath(
			filePathBuffer, CARRAY_SIZE(filePathBuffer), 
			kgtAssetFileNames[kai]);
	korlAssert(successBuildExeFilePath);
	while(!g_kpl->isFileAvailable(
			filePathBuffer, KorlApplicationDirectory::CURRENT))
	{
		KLOG(INFO, "Waiting for asset '%s'...", filePathBuffer);
		g_kpl->sleepFromTimeStamp(
			g_kpl->getTimeStamp(), KGT_ASSET_UNAVAILABLE_SLEEP_SECONDS);
	}
	const i32 assetByteSize = 
		g_kpl->getFileByteSize(
			filePathBuffer, KorlApplicationDirectory::CURRENT);
	if(assetByteSize < 0)
	{
		KLOG(ERROR, "Failed to get asset byte size of \"%s\"", 
		     filePathBuffer);
		return;
	}
	/* lock the asset manager's asset data allocator so we can safely allocate 
		data for the raw file.  The decoded asset structure is stored entirely 
		in the KgtAsset */
	g_kpl->lock(asset->kam->hLockAssetDataAllocator);
	void*const rawFileMemory = 
		kgtAllocAlloc(asset->kam->hKgaRawFiles, 
		              kmath::safeTruncateU32(assetByteSize) + 1);
	korlAssert(rawFileMemory);
	g_kpl->unlock(asset->kam->hLockAssetDataAllocator);
	/* load the entire raw file into a `fileByteSize` chunk */
	const bool assetReadSuccess = 
		g_kpl->readEntireFile(
			filePathBuffer, KorlApplicationDirectory::CURRENT, rawFileMemory, 
			kmath::safeTruncateU32(assetByteSize));
	if(!assetReadSuccess)
	{
		KLOG(ERROR, "Failed to read asset \"%s\"!", filePathBuffer);
		return;
	}
	/* null-terminate the file buffer */
	reinterpret_cast<u8*>(rawFileMemory)[assetByteSize] = 0;
	/* save a copy to this pointer in the asset so the main thread can copy this 
		data & deallocate the raw file pool memory when this job completes */
	asset->assetData.binary.data  = reinterpret_cast<u8*>(rawFileMemory);
	asset->assetData.binary.bytes = kmath::safeTruncateU32(assetByteSize + 1);
}
// @optimization (minor): just bake this info using `kasset`
enum class KgtAssetFileType
	{ PNG, WAV, OGG, FLIPBOOK_META, TEXTURE_META, SPRITE_FONT_META, UNKNOWN };
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
	else if(strstr(kgtAssetFileNames[static_cast<size_t>(assetIndex)], ".sfm"))
	{
		return KgtAssetFileType::SPRITE_FONT_META;
	}
	return KgtAssetFileType::UNKNOWN;
}
internal KgtAssetHandle 
	kgtAssetManagerPushAsset(KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	KgtAsset*const assets = reinterpret_cast<KgtAsset*>(kam + 1);
	const KgtAssetHandle assetHandle = static_cast<KgtAssetHandle>(assetIndex);
	korlAssert(assetHandle < kam->maxAssetHandles);
	KgtAsset*const asset = assets + assetHandle;
	if(asset->type == KgtAssetType::UNUSED)
	/* load the asset from the platform layer */
	{
		KLOG(INFO, "Loading asset '%s'...", kgtAssetFileNames[assetHandle]);
		const KgtAssetFileType assetFileType = 
			kgtAssetManagerAssetFileType(assetIndex);
		KgtAssetType assetType = KgtAssetType::UNUSED;
		fnSig_jobQueueFunction* jobQueueFunc = nullptr;
		switch(assetFileType)
		{
		case KgtAssetFileType::PNG: {
			asset->assetData.image = {};
			assetType    = KgtAssetType::RAW_IMAGE;
			jobQueueFunc = kgtAssetManagerAsyncLoadPng;
			} break;
		case KgtAssetFileType::WAV: {
			assetType    = KgtAssetType::RAW_SOUND;
			jobQueueFunc = kgtAssetManagerAsyncLoadWav;
			} break;
		case KgtAssetFileType::OGG: {
			assetType    = KgtAssetType::RAW_SOUND;
			jobQueueFunc = kgtAssetManagerAsyncLoadOgg;
			} break;
		case KgtAssetFileType::FLIPBOOK_META: {
			assetType    = KgtAssetType::FLIPBOOK_META;
			jobQueueFunc = kgtAssetManagerAsyncLoadFlipbookMeta;
			} break;
		case KgtAssetFileType::TEXTURE_META: {
			assetType    = KgtAssetType::TEXTURE_META;
			jobQueueFunc = kgtAssetManagerAsyncLoadTextureMeta;
			} break;
		case KgtAssetFileType::SPRITE_FONT_META: {
			assetType    = KgtAssetType::SPRITE_FONT_META;
			jobQueueFunc = kgtAssetManagerAsyncLoadSpriteFontMeta;
			} break;
		case KgtAssetFileType::UNKNOWN:
		default: {
			/* if an asset file type is not known, we can just assume the 
				contents will be interpreted as raw binary data & load it as 
				such */
			assetType = KgtAssetType::BINARY_DATA;
			jobQueueFunc = kgtAssetManagerAsyncLoadBinary;
			} break;
		}
		asset->loaded          = false;
		asset->kgtAssetIndex   = assetHandle;
		asset->kam             = kam;
		asset->type            = assetType;
		asset->jqTicketLoading = g_kpl->postJob(jobQueueFunc, asset);
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
		if(asset->type != KgtAssetType::UNUSED)
		{
			if(!asset->loaded)
			{
				if(g_kpl->jobDone(&asset->jqTicketLoading))
				{
					kgtAssetManagerOnLoadingJobFinished(kam, kah);
				}
				else
				{
					continue;
				}
			}
			char filePathBuffer[256];
			const bool successBuildExeFilePath = 
				kgtAssetManagerBuildCurrentRelativeFilePath(
					filePathBuffer, CARRAY_SIZE(filePathBuffer), 
					kgtAssetFileNames[kah]);
			korlAssert(successBuildExeFilePath);
			if(g_kpl->isFileChanged(
					filePathBuffer, KorlApplicationDirectory::CURRENT, 
					asset->lastWriteTime))
			{
				KLOG(INFO, "Unloading asset '%s'...", filePathBuffer);
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
#endif//0
