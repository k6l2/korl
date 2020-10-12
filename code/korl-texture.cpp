enum KorlTextureMetaEntries : u8
	{ KORL_TEXTURE_META_IMAGE_ASSET_NAME
	, KORL_TEXTURE_META_WRAP_X
	, KORL_TEXTURE_META_WRAP_Y
	, KORL_TEXTURE_META_FILTER_MIN
	, KORL_TEXTURE_META_FILTER_MAG
	, KORL_TEXTURE_META_ENTRY_COUNT };
/* @TODO: automatically generate the string array representations of enums using 
 * kcpp or some other such meta programming application */
global_variable const char*const KORL_TEXTURE_WRAP_MODE_STRINGS[] = 
	{ "clamp"
	, "repeat"
	, "repeat-mirrored" };
global_variable const char*const KORL_TEXTURE_FILTER_MODE_STRINGS[] = 
	{ "nearest"
	, "linear" };
static_assert(CARRAY_SIZE(KORL_TEXTURE_WRAP_MODE_STRINGS) == 
              static_cast<size_t>(KorlTextureWrapMode::ENUM_SIZE));
static_assert(CARRAY_SIZE(KORL_TEXTURE_FILTER_MODE_STRINGS) == 
              static_cast<size_t>(KorlTextureFilterMode::ENUM_SIZE));
internal bool 
	korlTextureMetaDecode(
		void* fileData, u32 fileBytes, const char* ansiAssetName, 
		KorlTextureMetaData* o_texMeta, char* o_ansiAssetNameImage, 
		size_t ansiAssetNameImageBufferSize)
{
	*o_texMeta = {};
	char*const fileCStr = reinterpret_cast<char*>(fileData);
	const char*const fileCStrEnd = fileCStr + fileBytes;
	kassert(fileCStr[fileBytes] == '\0');
	u32 foundEntryBitFlags = 0;
	u8  foundEntryCount    = 0;
	char* currLine = fileCStr;
	while(currLine && *currLine)
	{
		char* nextLine = strchr(currLine, '\n');
		if(nextLine >= fileCStr + fileBytes)
			nextLine = nullptr;
		const char*const currLineEnd = (nextLine
			? nextLine
			: fileCStrEnd);
		if(nextLine)
			*nextLine = '\0';
		char* idValueSeparator = strchr(currLine, ':');
		if(!idValueSeparator || idValueSeparator >= currLineEnd)
		{
			KLOG(ERROR, "Failed to find idValueSeparator for currLine=='%s'!", 
			     currLine);
			return false;
		}
		*idValueSeparator = '\0';
		idValueSeparator++;
		const size_t valueStringSize = kmath::safeTruncateU64(
			currLineEnd - idValueSeparator);
		if(strstr(currLine, "image-asset-file-name"))
		{
			foundEntryBitFlags |= 1<<KORL_TEXTURE_META_IMAGE_ASSET_NAME;
			foundEntryCount++;
			const size_t cStrValueTokenSize = 
				kutil::extractNonWhitespaceToken(
					&idValueSeparator, valueStringSize);
			/* ensure that the `value` string will not exceed the length of the 
			 * output buffer */
			if(cStrValueTokenSize >= ansiAssetNameImageBufferSize - 1)
			{
				KLOG(ERROR, "Asset file name length==%i is too "
				     "long for asset file name buffer==%i!",
				     cStrValueTokenSize, ansiAssetNameImageBufferSize);
				return false;
			}
			/* actually copy the `value` string to the output buffer */
			strcpy_s(o_ansiAssetNameImage, ansiAssetNameImageBufferSize,
			         idValueSeparator);
		}
		else if(strstr(currLine, "wrap-x"))
		{
			foundEntryBitFlags |= 1<<KORL_TEXTURE_META_WRAP_X;
			foundEntryCount++;
			const size_t cStrValueTokenSize = 
				kutil::extractNonWhitespaceToken(
					&idValueSeparator, valueStringSize);
			if(cStrValueTokenSize <= 0)
			{
				KLOG(ERROR, "Invalid wrap-x string");
				return false;
			}
			kutil::cStrToLowercase(idValueSeparator, cStrValueTokenSize);
			kutil::cStrRaiseUnderscores(idValueSeparator, cStrValueTokenSize);
			const size_t cStrMatchIndex = kutil::cStrCompareArray(
				idValueSeparator, KORL_TEXTURE_WRAP_MODE_STRINGS, 
				CARRAY_SIZE(KORL_TEXTURE_WRAP_MODE_STRINGS));
			if(cStrMatchIndex >= CARRAY_SIZE(KORL_TEXTURE_WRAP_MODE_STRINGS))
			{
				KLOG(ERROR, "Invalid wrap-x mode ('%s')", idValueSeparator);
				return false;
			}
			o_texMeta->wrapX = static_cast<KorlTextureWrapMode>(cStrMatchIndex);
		}
		else if(strstr(currLine, "wrap-y"))
		{
			foundEntryBitFlags |= 1<<KORL_TEXTURE_META_WRAP_Y;
			foundEntryCount++;
			const size_t cStrValueTokenSize = 
				kutil::extractNonWhitespaceToken(
					&idValueSeparator, valueStringSize);
			if(cStrValueTokenSize <= 0)
			{
				KLOG(ERROR, "Invalid wrap-y string");
				return false;
			}
			kutil::cStrToLowercase(idValueSeparator, cStrValueTokenSize);
			kutil::cStrRaiseUnderscores(idValueSeparator, cStrValueTokenSize);
			const size_t cStrMatchIndex = kutil::cStrCompareArray(
				idValueSeparator, KORL_TEXTURE_WRAP_MODE_STRINGS, 
				CARRAY_SIZE(KORL_TEXTURE_WRAP_MODE_STRINGS));
			if(cStrMatchIndex >= CARRAY_SIZE(KORL_TEXTURE_WRAP_MODE_STRINGS))
			{
				KLOG(ERROR, "Invalid wrap-y mode ('%s')", idValueSeparator);
				return false;
			}
			o_texMeta->wrapY = static_cast<KorlTextureWrapMode>(cStrMatchIndex);
		}
		else if(strstr(currLine, "filter-minify"))
		{
			foundEntryBitFlags |= 1<<KORL_TEXTURE_META_FILTER_MIN;
			foundEntryCount++;
			const size_t cStrValueTokenSize = 
				kutil::extractNonWhitespaceToken(
					&idValueSeparator, valueStringSize);
			if(cStrValueTokenSize <= 0)
			{
				KLOG(ERROR, "Invalid filter-minify string");
				return false;
			}
			kutil::cStrToLowercase(idValueSeparator, cStrValueTokenSize);
			kutil::cStrRaiseUnderscores(idValueSeparator, cStrValueTokenSize);
			const size_t cStrMatchIndex = kutil::cStrCompareArray(
				idValueSeparator, KORL_TEXTURE_FILTER_MODE_STRINGS, 
				CARRAY_SIZE(KORL_TEXTURE_FILTER_MODE_STRINGS));
			if(cStrMatchIndex >= CARRAY_SIZE(KORL_TEXTURE_FILTER_MODE_STRINGS))
			{
				KLOG(ERROR, "Invalid filter-minify mode ('%s')", 
				     idValueSeparator);
				return false;
			}
			o_texMeta->filterMinify = static_cast<KorlTextureFilterMode>(
				cStrMatchIndex);
		}
		else if(strstr(currLine, "filter-magnify"))
		{
			foundEntryBitFlags |= 1<<KORL_TEXTURE_META_FILTER_MAG;
			foundEntryCount++;
			const size_t cStrValueTokenSize = 
				kutil::extractNonWhitespaceToken(
					&idValueSeparator, valueStringSize);
			if(cStrValueTokenSize <= 0)
			{
				KLOG(ERROR, "Invalid filter-magnify string");
				return false;
			}
			kutil::cStrToLowercase(idValueSeparator, cStrValueTokenSize);
			kutil::cStrRaiseUnderscores(idValueSeparator, cStrValueTokenSize);
			const size_t cStrMatchIndex = kutil::cStrCompareArray(
				idValueSeparator, KORL_TEXTURE_FILTER_MODE_STRINGS, 
				CARRAY_SIZE(KORL_TEXTURE_FILTER_MODE_STRINGS));
			if(cStrMatchIndex >= CARRAY_SIZE(KORL_TEXTURE_FILTER_MODE_STRINGS))
			{
				KLOG(ERROR, "Invalid filter-magnify mode ('%s')", 
				     idValueSeparator);
				return false;
			}
			o_texMeta->filterMagnify = static_cast<KorlTextureFilterMode>(
				cStrMatchIndex);
		}
		if(nextLine) *nextLine = '\n';
		currLine = nextLine ? (nextLine + 1) : nullptr;
	}
	if(!(foundEntryBitFlags & 1<<KORL_TEXTURE_META_IMAGE_ASSET_NAME))
	/* if the meta data file didn't contain an entry for the image asset name, 
	 * then we assume the image asset name matches the texture meta data asset 
	 * name with a png file extension */
	{
		strcpy_s(o_ansiAssetNameImage, ansiAssetNameImageBufferSize, 
		         ansiAssetName);
		char* pFileExtension = strchr(o_ansiAssetNameImage, '.');
		if(!pFileExtension)
		{
			KLOG(ERROR, "Texture meta data asset file name '%s' contains no "
			     "file extension!  Expected '*.tex'!", ansiAssetName);
			return false;
		}
		const char imageFileExtension[] = "png";
		const char*const ansiAssetNameImageEnd = 
			o_ansiAssetNameImage + ansiAssetNameImageBufferSize;
		if(pFileExtension + CARRAY_SIZE(imageFileExtension) >= 
			ansiAssetNameImageEnd)
		{
			KLOG(ERROR, "ansiAssetNameImageBufferSize==%i can't contain "
			     "implicit image asset file name for texture meta data asset "
			     "'%s'!", ansiAssetNameImageBufferSize, ansiAssetName);
			return false;
		}
		for(size_t c = 0; c < CARRAY_SIZE(imageFileExtension); c++)
		{
			pFileExtension[1 + c] = imageFileExtension[c];
		}
		foundEntryCount++;
		foundEntryBitFlags |= 1<<KORL_TEXTURE_META_IMAGE_ASSET_NAME;
	}
	const bool success = 
		   (foundEntryCount    ==     KORL_TEXTURE_META_ENTRY_COUNT) 
		&& (foundEntryBitFlags == (1<<KORL_TEXTURE_META_ENTRY_COUNT) - 1);
	if(!success)
	{
		KLOG(ERROR, "Failed to pass safety check for '%s'!", ansiAssetName);
	}
	return success;
}
