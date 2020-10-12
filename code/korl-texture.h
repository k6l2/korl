#pragma once
#include "kutil.h"
/**
 * Texture Meta Data asset files should all have the `.tex` file extension.
 * If `image-asset-file-name` is omitted from the asset file, then it is assumed 
 * to be the same asset file name of the texture meta data asset file with a 
 * `.png` file extension instead.
 * Enumeration values are strings which can be in any desired case, and '_' 
 * characters can optionally be '-' characters if desired.
 * ----- Example of a texture meta data asset file -----
image-asset-file-name : fighter-exhaust.png
wrap-x                : clamp
wrap-y                : clamp
filter-minify         : nearest
filter-magnify        : nearest
 */
enum class KorlTextureWrapMode : u8
	{ CLAMP
	, REPEAT
	, REPEAT_MIRRORED 
	, ENUM_SIZE };
enum class KorlTextureFilterMode : u8
	{ NEAREST
	, LINEAR 
	, ENUM_SIZE };
struct KorlTextureMetaData
{
	KorlTextureWrapMode wrapX;
	KorlTextureWrapMode wrapY;
	KorlTextureFilterMode filterMinify;
	KorlTextureFilterMode filterMagnify;
};
internal bool 
	korlTextureMetaDecode(
		void* fileData, u32 fileBytes, const char* ansiAssetName, 
		KorlTextureMetaData* o_texMeta, char* o_ansiAssetNameImage, 
		size_t ansiAssetNameImageBufferSize);
