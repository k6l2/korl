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
			asset->kam->hKgtAllocatorRawFiles, fileByteSize + 1));
	asset->kam->korl->unlock(asset->kam->hLockAssetDataAllocator);
	/* read the raw file into memory */
	const bool successReadEntireFile = 
		asset->kam->korl->readEntireFile(hFile, rawFileData, fileByteSize);
	korlAssert(successReadEntireFile);
	// ensure the raw file data is terminated with a null character //
	rawFileData[fileByteSize] = '\0';
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
/** Unload & release all dynamic storage managed by the specified asset index, 
 * then recursively do the same for all assets use the specified asset index as 
 * a dependency.  This functionality assumes no cycles in the asset dependency 
 * DAG.  If there are, we will probably hang forever so don't do that!!! */
internal void _kgt_assetManager_free(
	KgtAssetManager* kam, size_t kamIndex)
{
	korlAssert(kamIndex < kam->maxAssetHandles);
	KgtAsset& asset = kam->assets[kamIndex];
	if(!asset.loaded)
	{
		KLOG(WARNING, "asset[%i] is already free!", kamIndex);
		return;
	}
	/* @speed: some assets don't actually use the asset data allocator, 
		so we can conditionally ignore locking code for those! */
	kam->korl->lock(kam->hLockAssetDataAllocator);
	kgt_asset_free(&asset, kam->hKgtAllocatorAssetData);
	kam->korl->unlock(kam->hLockAssetDataAllocator);
	/* @speed: this algorithm is garbage (recursive n^2), and would probably run 
		MUCH faster if we just did topographical sort or something */
	const KgtAssetHandle hAsset = _kgt_assetManager_makeHandle(kamIndex);
	for(size_t kid = 0; kid < kam->maxAssetHandles; kid++)
	{
		KgtAsset& assetOther = kam->assets[kid];
		if(kid == kamIndex || !assetOther.loaded || !assetOther.dependencyCount)
			continue;
		bool dependsOnKamIndex = false;
		for(u8 d = 0; d < assetOther.dependencyCount; d++)
		{
			if(assetOther.dependencies[d] == hAsset)
			{
				dependsOnKamIndex = true;
				break;
			}
		}
		if(dependsOnKamIndex)
			_kgt_assetManager_free(kam, kid);
	}
	asset.loaded             = false;
	asset.dependenciesLoaded = false;
	asset.dependencyCount    = 0;
}
internal void kgt_assetManager_free(
	KgtAssetManager* kam, KgtAssetIndex assetIndex)
{
	/* the first KgtAsset::Type::ENUM_COUNT asset handles are reserved for 
		KgtAssetIndex values! */
	const size_t kamIndex = static_cast<size_t>(assetIndex);
	korlAssert(kamIndex < kam->maxAssetHandles);
	_kgt_assetManager_free(kam, kamIndex);
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
	{
		asset.loaded = true;
		/* the asset was JUST loaded, so we have no idea whether or not all its 
			dependencies are also loaded; ergo we must check! */
		asset.dependenciesLoaded = false;
	}
	if(!asset.loaded)
		return false;
	/* iterate over all the asset's dependencies and recursively fully-load */
	for(u8 d = 0; d < asset.dependencyCount; d++)
		if(!_kgt_assetManager_fullyLoad(kam, asset.dependencies[d]))
			return false;
	if(!asset.dependenciesLoaded && asset.dependencyCount)
	{
		kgt_asset_onDependenciesLoaded(&asset, kam);
		asset.dependenciesLoaded = true;
	}
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

internal void kgt_assetManager_loadAllStaticAssets(KgtAssetManager* kam)
{
	for(size_t a = 0; a < KGT_ASSET_COUNT; a++)
		_kgt_assetManager_load(kam, _kgt_assetManager_makeHandle(a));
}
internal void kgt_assetManager_reloadChangedAssets(KgtAssetManager* kam)
{
	for(size_t a = 0; a < kam->maxAssetHandles; a++)
	{
		KgtAsset& asset = kam->assets[a];
		if(!asset.loaded)
			continue;
		if(a >= KGT_ASSET_COUNT)
		{
			KLOG(ERROR, "Dynamic assets not implemented yet!");
			continue;
		}
		/* build the file path of the KAsset */
		char filePathBuffer[256];
		const bool successBuildExeFilePath = 
			_kgt_assetManager_buildCurrentRelativeFilePath(
				filePathBuffer, CARRAY_SIZE(filePathBuffer), 
				kgtAssetFileNames[a]);
		korlAssert(successBuildExeFilePath);
		/* if the KAsset is changed on disk, we need to free the asset & begin 
			the async load procedures so it's available as fast as possible */
		if(kam->korl->isFileChanged(
			filePathBuffer, KorlApplicationDirectory::CURRENT, 
			asset.lastWriteTime))
		{
			_kgt_assetManager_free(kam, a);
			_kgt_assetManager_load(kam, _kgt_assetManager_makeHandle(a));
		}
	}
}
