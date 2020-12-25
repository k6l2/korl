#include "kgtSpriteFont.h"
#include "kgtAssetManager.h"
#include "krb-interface.h"
enum KgtSpriteFontMetaDataEntries : u32
	{ KGT_SPRITE_FONT_META_DECODE_ASSET_NAME_TEX
	, KGT_SPRITE_FONT_META_DECODE_ASSET_NAME_TEX_OUTLINE
	, KGT_SPRITE_FONT_META_DECODE_SIZE_X
	, KGT_SPRITE_FONT_META_DECODE_SIZE_Y
	, KGT_SPRITE_FONT_META_DECODE_PADDING_X
	, KGT_SPRITE_FONT_META_DECODE_PADDING_Y
	, KGT_SPRITE_FONT_META_DECODE_CHARACTERS
	, KGT_SPRITE_FONT_META_DECODE_ENTRY_COUNT
};
internal bool 
	kgtSpriteFontDecodeMeta(
		void* fileData, u32 fileBytes, const char* cStrAnsiAssetName, 
		KgtSpriteFontMetaData* o_sfMeta, 
		char* o_assetFileNameTexture, size_t assetFileNameTextureBufferSize, 
		char* o_assetFileNameTextureOutline, 
		size_t assetFileNameTextureOutlineBufferSize)
{
	*o_sfMeta = {};
	char*const fileCStr = reinterpret_cast<char*>(fileData);
	const char*const fileCStrEnd = fileCStr + fileBytes;
	korlAssert(fileCStr[fileBytes] == '\0');
	u32 propertiesFoundBitflags = 0;
	u8  propertiesFoundCount    = 0;
	// destructively read the file line by line //
	// source: https://stackoverflow.com/a/17983619
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
		/* parse the current line */
		char* idValueSeparator = strchr(currLine, ':');
		if(!idValueSeparator || idValueSeparator >= currLineEnd)
		{
			KLOG(ERROR, "Failed to find idValueSeparator for "
			     "currLine=='%s'!", currLine);
			return false;
		}
		*idValueSeparator = '\0';
		idValueSeparator++;
		const size_t valueStringSize = 
			kmath::safeTruncateU64(currLineEnd - idValueSeparator);
		if(strstr(currLine, "asset-file-name-texture-outline"))
		{
			propertiesFoundBitflags |= 
				1<<KGT_SPRITE_FONT_META_DECODE_ASSET_NAME_TEX_OUTLINE;
			propertiesFoundCount++;
			const size_t cStrValueTokenSize = 
				kutil::extractNonWhitespaceToken(
					&idValueSeparator, valueStringSize);
			if(cStrValueTokenSize >= assetFileNameTextureOutlineBufferSize - 1)
			{
				KLOG(ERROR, "String length==%i is too "
				     "long for asset file name buffer==%i!",
				     cStrValueTokenSize, assetFileNameTextureOutlineBufferSize);
				return false;
			}
			strcpy_s(
				o_assetFileNameTextureOutline, 
				assetFileNameTextureOutlineBufferSize, idValueSeparator);
		}
		else if(strstr(currLine, "asset-file-name-texture"))
		{
			propertiesFoundBitflags |= 
				1<<KGT_SPRITE_FONT_META_DECODE_ASSET_NAME_TEX;
			propertiesFoundCount++;
			const size_t cStrValueTokenSize = 
				kutil::extractNonWhitespaceToken(
					&idValueSeparator, valueStringSize);
			if(cStrValueTokenSize >= assetFileNameTextureBufferSize - 1)
			{
				KLOG(ERROR, "String length==%i is too "
				     "long for asset file name buffer==%i!",
				     cStrValueTokenSize, assetFileNameTextureBufferSize);
				return false;
			}
			strcpy_s(
				o_assetFileNameTexture, assetFileNameTextureBufferSize, 
				idValueSeparator);
		}
		else if(strstr(currLine, "size-x"))
		{
			propertiesFoundBitflags |= 1<<KGT_SPRITE_FONT_META_DECODE_SIZE_X;
			propertiesFoundCount++;
			o_sfMeta->monospaceSize.x = 
				kmath::safeTruncateU32(atoi(idValueSeparator));
		}
		else if(strstr(currLine, "size-y"))
		{
			propertiesFoundBitflags |= 1<<KGT_SPRITE_FONT_META_DECODE_SIZE_Y;
			propertiesFoundCount++;
			o_sfMeta->monospaceSize.y = 
				kmath::safeTruncateU32(atoi(idValueSeparator));
		}
		else if(strstr(currLine, "padding-x"))
		{
			propertiesFoundBitflags |= 1<<KGT_SPRITE_FONT_META_DECODE_PADDING_X;
			propertiesFoundCount++;
			o_sfMeta->texturePadding.x = 
				kmath::safeTruncateU32(atoi(idValueSeparator));
		}
		else if(strstr(currLine, "padding-y"))
		{
			propertiesFoundBitflags |= 1<<KGT_SPRITE_FONT_META_DECODE_PADDING_Y;
			propertiesFoundCount++;
			o_sfMeta->texturePadding.y = 
				kmath::safeTruncateU32(atoi(idValueSeparator));
		}
		else if(strstr(currLine, "characters"))
		{
			propertiesFoundBitflags |= 
				1<<KGT_SPRITE_FONT_META_DECODE_CHARACTERS;
			propertiesFoundCount++;
			const char*const strBegin = strchr (idValueSeparator, '"');
			const char*const strEnd   = strrchr(idValueSeparator, '"');
			korlAssert(strBegin);
			korlAssert(strEnd);
			korlAssert(strBegin != strEnd);
			o_sfMeta->charactersSize = 0;
			for(const char* currChar = strBegin + 1
				; currChar < strEnd; currChar++)
			{
				korlAssert(o_sfMeta->charactersSize < 
					CARRAY_SIZE(o_sfMeta->characters));
				o_sfMeta->characters[o_sfMeta->charactersSize++] = *currChar;
			}
		}
		/* advance to the next line, if there is one */
		if(nextLine) *nextLine = '\n';
		currLine = nextLine ? (nextLine + 1) : nullptr;
	}// while(currLine && *currLine)
	const bool success = 
		   (propertiesFoundCount    == KGT_SPRITE_FONT_META_DECODE_ENTRY_COUNT) 
		&& (propertiesFoundBitflags == 
			(1<<KGT_SPRITE_FONT_META_DECODE_ENTRY_COUNT) - 1);
	if(!success)
	{
		KLOG(ERROR, "Failed to pass safety check for '%s'!", cStrAnsiAssetName);
	}
	return success;
}
internal void 
	kgtSpriteFontDraw(
		const KgtSpriteFontMetaData*const sfm, const char* cStrText, 
		const v2f32& position, const v2f32& scale, const Color4f32& color, 
		const Color4f32& colorOutline, 
		const KrbApi*const krb, KgtAssetManager*const kam)
{
	korlAssert(!"@todo");
}