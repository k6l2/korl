#include "kgtSpriteFont.h"
#include "kgtAssetManager.h"
#include "krb-interface.h"
#include "kgtAssetSpriteFont.h"
#include "kgtAssetTexture.h"
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
		KgtAssetIndex kaiSpriteFontMeta, const char* cStrText, 
		const v2f32& position, const v2f32& anchor, const v2f32& scale, 
		const RgbaF32& color, const RgbaF32& colorOutline, 
		const KrbApi*const krb, KgtAssetManager*const kam)
{
	const KgtSpriteFontMetaData& sfm = 
		kgt_assetSpriteFont_get(kam, kaiSpriteFontMeta);
	const v2u32 fontSpriteSheetSize = 
		kgt_assetTexture_getSize(kam, sfm.kaiTexture);
	/* build the triangle mesh to draw each character */
	KgtVertex charTriMesh[6];
	// upper-left triangle //
	charTriMesh[0].position = v3f32::ZERO;
	charTriMesh[1].position = 
		{ static_cast<f32>(sfm.monospaceSize.x)
		, static_cast<f32>(sfm.monospaceSize.y), 0};
	charTriMesh[2].position = {0, static_cast<f32>(sfm.monospaceSize.y), 0};
	// lower-right triangle //
	charTriMesh[3].position = 
		{ static_cast<f32>(sfm.monospaceSize.x)
		, static_cast<f32>(sfm.monospaceSize.y), 0};
	charTriMesh[4].position = v3f32::ZERO;
	charTriMesh[5].position = {static_cast<f32>(sfm.monospaceSize.x), 0, 0};
	/* @speed: instead of doing one draw call per character, we can batch all 
		the characters into a single mesh and call draw once per texture.  We 
		can just take a memory allocation callback as a parameter to allocate a 
		temporary buffer to build the mesh data in */
	const v2f32 textAabbSize = 
		kgtSpriteFontComputeAabbSize(kaiSpriteFontMeta, cStrText, scale, kam);
	const v2f32 anchorOffset = 
		{ -anchor.x * textAabbSize.x
		,  anchor.y * textAabbSize.y };
	v2f32 currentPosition = position;
	currentPosition.y -= scale.y * static_cast<f32>(sfm.monospaceSize.y);
	for(; *cStrText; cStrText++)
	{
		if(*cStrText == '\n')
		{
			currentPosition.x = position.x;
			currentPosition.y -= 
				scale.y * static_cast<f32>(sfm.monospaceSize.y);
			continue;
		}
		/* advance to the next space at the end of this iteration */
		defer(currentPosition.x += 
			scale.x * static_cast<f32>(sfm.monospaceSize.x));
		/* determine which character in the font sprite sheet we need to use */
		u8 c = 0;
		for(; c < sfm.charactersSize; c++)
		{
			if(sfm.characters[c] == *cStrText)
				break;
		}
		if(c >= sfm.charactersSize)
			continue;
		/* calculate the UV coordinates to use for this character */
		const v2u32 charPixelUL = 
			{ sfm.texturePadding.x + 
				c*(sfm.monospaceSize.x + 2*sfm.texturePadding.x)
			// @assumption: there is only a single row of characters
			, sfm.texturePadding.y };
		const v2u32 charPixelDR = charPixelUL + sfm.monospaceSize;
		const v2f32 charTexNormUL = 
			{ static_cast<f32>(charPixelUL.x) / 
				static_cast<f32>(fontSpriteSheetSize.x)
			, static_cast<f32>(charPixelUL.y) / 
				static_cast<f32>(fontSpriteSheetSize.y) };
		const v2f32 charTexNormDR = 
			{ static_cast<f32>(charPixelDR.x) / 
				static_cast<f32>(fontSpriteSheetSize.x)
			, static_cast<f32>(charPixelDR.y) / 
				static_cast<f32>(fontSpriteSheetSize.y) };
		/* update the character mesh with these UV normals */
		// upper-left triangle //
		charTriMesh[0].textureNormal = {charTexNormUL.x, charTexNormDR.y};
		charTriMesh[1].textureNormal = {charTexNormDR.x, charTexNormUL.y};
		charTriMesh[2].textureNormal = {charTexNormUL.x, charTexNormUL.y};
		// down-right triangle //
		charTriMesh[3].textureNormal = {charTexNormDR.x, charTexNormUL.y};
		charTriMesh[4].textureNormal = {charTexNormUL.x, charTexNormDR.y};
		charTriMesh[5].textureNormal = {charTexNormDR.x, charTexNormDR.y};
		/* draw the outline mesh first */
		krb->setModelXform2d(
			currentPosition + anchorOffset, q32::IDENTITY, scale);
		krb->setDefaultColor(colorOutline);
		KGT_USE_IMAGE(sfm.kaiTextureOutline);
		KGT_DRAW_TRIS(charTriMesh, KGT_VERTEX_ATTRIBS_NO_COLOR);
		/* draw the normal text texture on top of the outline */
		krb->setDefaultColor(color);
		KGT_USE_IMAGE(sfm.kaiTexture);
		KGT_DRAW_TRIS(charTriMesh, KGT_VERTEX_ATTRIBS_NO_COLOR);
	}
}
internal v2f32 
	kgtSpriteFontComputeAabbSize(
		KgtAssetIndex kaiSpriteFontMeta, const char* cStrText, 
		const v2f32& scale, KgtAssetManager*const kam)
{
	const KgtSpriteFontMetaData& sfm = 
		kgt_assetSpriteFont_get(kam, kaiSpriteFontMeta);
	v2f32 resultAabbSize = {0, scale.y * static_cast<f32>(sfm.monospaceSize.y)};
	f32 currentLineSizeX = 0;
	for(; *cStrText; cStrText++)
	{
		if(*cStrText == '\n')
		{
			currentLineSizeX = 0;
			resultAabbSize.y += scale.y * static_cast<f32>(sfm.monospaceSize.y);
			continue;
		}
		currentLineSizeX += scale.x * static_cast<f32>(sfm.monospaceSize.x);
		if(currentLineSizeX > resultAabbSize.x)
			resultAabbSize.x = currentLineSizeX;
	}
	return resultAabbSize;
}
internal v2f32 
	kgtSpriteFontComputeAabbTopLeft(
		KgtAssetIndex kaiSpriteFontMeta, const v2f32& position, 
		const v2f32& scale, KgtAssetManager*const kam)
{
	const KgtSpriteFontMetaData& sfm = 
		kgt_assetSpriteFont_get(kam, kaiSpriteFontMeta);
	return position + scale*v2u32{0, sfm.monospaceSize.y};
}
internal void
	kgtSpriteFontBatch(
		KgtAssetIndex kaiSpriteFontMeta, const char* cStrText, 
		const v2f32& position, const v2f32& anchor, const v2f32& scale, 
		const RgbaF32& color, const RgbaF32& colorOutline, 
		KgtAssetManager*const kam, 
		kgtSpriteFontCallbackAddVertex* callbackAddVertex, 
		void* callbackAddVertexUserData, v2f32* io_aabbMin, v2f32* io_aabbMax)
{
	const KgtSpriteFontMetaData& sfm = 
		kgt_assetSpriteFont_get(kam, kaiSpriteFontMeta);
	const v2u32 fontSpriteSheetSize = 
		kgt_assetTexture_getSize(kam, sfm.kaiTexture);
	/* build the triangle mesh to draw each character */
	KgtVertex charTriMesh[6];
	// upper-left triangle //
	charTriMesh[0].position = v3f32::ZERO;
	charTriMesh[1].position = 
		{ static_cast<f32>(sfm.monospaceSize.x)
		, static_cast<f32>(sfm.monospaceSize.y), 0};
	charTriMesh[2].position = {0, static_cast<f32>(sfm.monospaceSize.y), 0};
	// lower-right triangle //
	charTriMesh[3].position = 
		{ static_cast<f32>(sfm.monospaceSize.x)
		, static_cast<f32>(sfm.monospaceSize.y), 0};
	charTriMesh[4].position = v3f32::ZERO;
	charTriMesh[5].position = {static_cast<f32>(sfm.monospaceSize.x), 0, 0};
	const v2f32 textAabbSize = 
		kgtSpriteFontComputeAabbSize(kaiSpriteFontMeta, cStrText, scale, kam);
	const v2f32 anchorOffset = 
		{ -anchor.x * textAabbSize.x
		,  anchor.y * textAabbSize.y };
	if(io_aabbMin)
	{
		const v2f32 aabbMin = position;
		if(aabbMin.x < io_aabbMin->x)
			io_aabbMin->x = aabbMin.x;
		if(aabbMin.y < io_aabbMin->y)
			io_aabbMin->y = aabbMin.y;
	}
	if(io_aabbMax)
	{
		const v2f32 aabbMax = position + textAabbSize;
		if(aabbMax.x > io_aabbMax->x)
			io_aabbMax->x = aabbMax.x;
		if(aabbMax.y > io_aabbMax->y)
			io_aabbMax->y = aabbMax.y;
	}
	v2f32 currentPosition = position;
	currentPosition.y -= scale.y * static_cast<f32>(sfm.monospaceSize.y);
	for(; *cStrText; cStrText++)
	{
		if(*cStrText == '\n')
		{
			currentPosition.x = position.x;
			currentPosition.y -= 
				scale.y * static_cast<f32>(sfm.monospaceSize.y);
			continue;
		}
		/* advance to the next space at the end of this iteration */
		defer(currentPosition.x += 
			scale.x * static_cast<f32>(sfm.monospaceSize.x));
		/* determine which character in the font sprite sheet we need to use */
		u8 c = 0;
		for(; c < sfm.charactersSize; c++)
		{
			if(sfm.characters[c] == *cStrText)
				break;
		}
		if(c >= sfm.charactersSize)
			continue;
		/* calculate the UV coordinates to use for this character */
		const v2u32 charPixelUL = 
			{ sfm.texturePadding.x + 
				c*(sfm.monospaceSize.x + 2*sfm.texturePadding.x)
			// @assumption: there is only a single row of characters
			, sfm.texturePadding.y };
		const v2u32 charPixelDR = charPixelUL + sfm.monospaceSize;
		const v2f32 charTexNormUL = 
			{ static_cast<f32>(charPixelUL.x) / 
				static_cast<f32>(fontSpriteSheetSize.x)
			, static_cast<f32>(charPixelUL.y) / 
				static_cast<f32>(fontSpriteSheetSize.y) };
		const v2f32 charTexNormDR = 
			{ static_cast<f32>(charPixelDR.x) / 
				static_cast<f32>(fontSpriteSheetSize.x)
			, static_cast<f32>(charPixelDR.y) / 
				static_cast<f32>(fontSpriteSheetSize.y) };
		/* update the character mesh with these UV normals */
		// upper-left triangle //
		charTriMesh[0].textureNormal = {charTexNormUL.x, charTexNormDR.y};
		charTriMesh[1].textureNormal = {charTexNormDR.x, charTexNormUL.y};
		charTriMesh[2].textureNormal = {charTexNormUL.x, charTexNormUL.y};
		// down-right triangle //
		charTriMesh[3].textureNormal = {charTexNormDR.x, charTexNormUL.y};
		charTriMesh[4].textureNormal = {charTexNormUL.x, charTexNormDR.y};
		charTriMesh[5].textureNormal = {charTexNormDR.x, charTexNormDR.y};
		/* batch the vertices for this character */
		for(u32 v = 0; v < 6; v++)
		{
			const v2f32 position2d = currentPosition + anchorOffset + (scale * 
				v2f32{charTriMesh[v].position.x, charTriMesh[v].position.y});
			callbackAddVertex(
				callbackAddVertexUserData, position2d, 
				charTriMesh[v].textureNormal, color, colorOutline);
		}
	}
}
internal void 
	kgtSpriteFontDrawBatch(
		KgtAssetIndex kaiSpriteFontMeta, const v2f32& positionOffset, 
		const void* vertices, const void* verticesOutline, 
		u32 vertexCount, u32 vertexStride, 
		const KrbVertexAttributeOffsets& vertexAttribOffsets, 
		const KrbApi*const krb, KgtAssetManager*const kam)
{
	const KgtSpriteFontMetaData& sfm = 
		kgt_assetSpriteFont_get(kam, kaiSpriteFontMeta);
	/* draw the outline mesh first */
	krb->setModelXform2d(positionOffset, q32::IDENTITY, {1,1});
	KGT_USE_IMAGE(sfm.kaiTextureOutline);
	g_krb->drawTris(
		verticesOutline, vertexCount, vertexStride, vertexAttribOffsets);
	/* draw the normal text texture on top of the outline */
	KGT_USE_IMAGE(sfm.kaiTexture);
	g_krb->drawTris(vertices, vertexCount, vertexStride, vertexAttribOffsets);
}
