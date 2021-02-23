#include "kgtFlipBook.h"
#include "kgtAssetManager.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
enum KFlipbookMetaEntries : u32
	{ KFB_META_DECODE_FRAME_SIZE_X
	, KFB_META_DECODE_FRAME_SIZE_Y
	, KFB_META_DECODE_FRAME_COUNT
	, KFB_META_DECODE_TEX_ASSET_NAME
	, KFB_META_DECODE_DEFAULT_REPEAT
	, KFB_META_DECODE_DEFAULT_REVERSE
	, KFB_META_DECODE_DEFAULT_SEC_PER_FRAME
	, KFB_META_DECODE_DEFAULT_ANCHOR_X
	, KFB_META_DECODE_DEFAULT_ANCHOR_Y
	, KFB_META_DECODE_ENTRY_COUNT
};
internal bool kgtFlipBookDecodeMeta(
	void* fileData, u32 fileBytes, const char* cStrAnsiAssetName, 
	KgtFlipBookMetaData* o_fbMeta, char* o_texAssetFileName, 
	size_t texAssetFileNameBufferSize)
{
	*o_fbMeta = {};
	char*const fileCStr = reinterpret_cast<char*>(fileData);
	const char*const fileCStrEnd = fileCStr + fileBytes;
	korlAssert(fileCStr[fileBytes] == '\0');
	u32 fbPropertiesFoundBitflags = 0;
	u8  fbPropertiesFoundCount    = 0;
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
		// parse `currLine` for flipbook meta data //
		{
			char* idValueSeparator = strchr(currLine, ':');
			if(!idValueSeparator || idValueSeparator >= currLineEnd)
			{
				KLOG(ERROR, "Failed to find idValueSeparator for "
				     "currLine=='%s'!", currLine);
				return false;
			}
			*idValueSeparator = '\0';
			idValueSeparator++;
			const size_t valueStringSize = kmath::safeTruncateU64(
				currLineEnd - idValueSeparator);
			if(strstr(currLine, "frame-size-x"))
			{
				fbPropertiesFoundBitflags |= 1<<KFB_META_DECODE_FRAME_SIZE_X;
				fbPropertiesFoundCount++;
				o_fbMeta->frameSizeX = kmath::safeTruncateU32(
					atoi(idValueSeparator));
			}
			else if(strstr(currLine, "frame-size-y"))
			{
				fbPropertiesFoundBitflags |= 1<<KFB_META_DECODE_FRAME_SIZE_Y;
				fbPropertiesFoundCount++;
				o_fbMeta->frameSizeY = kmath::safeTruncateU32(
					atoi(idValueSeparator));
			}
			else if(strstr(currLine, "frame-count"))
			{
				fbPropertiesFoundBitflags |= 1<<KFB_META_DECODE_FRAME_COUNT;
				fbPropertiesFoundCount++;
				o_fbMeta->frameCount = 
					kmath::safeTruncateU16(atoi(idValueSeparator));
			}
			else if(strstr(currLine, "texture-asset-file-name"))
			{
				fbPropertiesFoundBitflags |= 1<<KFB_META_DECODE_TEX_ASSET_NAME;
				fbPropertiesFoundCount++;
				const size_t cStrValueTokenSize = 
					kutil::extractNonWhitespaceToken(
						&idValueSeparator, valueStringSize);
				if(cStrValueTokenSize >= texAssetFileNameBufferSize - 1)
				{
					KLOG(ERROR, "Texture asset file name length==%i is too "
					     "long for asset file name buffer==%i!",
					     cStrValueTokenSize, texAssetFileNameBufferSize);
					return false;
				}
				strcpy_s(o_texAssetFileName, texAssetFileNameBufferSize,
				         idValueSeparator);
			}
			else if(strstr(currLine, "default-repeat"))
			{
				fbPropertiesFoundBitflags |= 1<<KFB_META_DECODE_DEFAULT_REPEAT;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultRepeat = atoi(idValueSeparator) != 0;
			}
			else if(strstr(currLine, "default-reverse"))
			{
				fbPropertiesFoundBitflags |= 1<<KFB_META_DECODE_DEFAULT_REVERSE;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultReverse = atoi(idValueSeparator) != 0;
			}
			else if(strstr(currLine, "default-seconds-per-frame"))
			{
				fbPropertiesFoundBitflags |= 
					1<<KFB_META_DECODE_DEFAULT_SEC_PER_FRAME;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultSecondsPerFrame = 
					static_cast<f32>(atof(idValueSeparator));
			}
			else if(strstr(currLine, "default-anchor-ratio-x"))
			{
				fbPropertiesFoundBitflags |= 
					1<<KFB_META_DECODE_DEFAULT_ANCHOR_X;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultAnchorRatioX = 
					static_cast<f32>(atof(idValueSeparator));
			}
			else if(strstr(currLine, "default-anchor-ratio-y"))
			{
				fbPropertiesFoundBitflags |= 
					1<<KFB_META_DECODE_DEFAULT_ANCHOR_Y;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultAnchorRatioY = 
					static_cast<f32>(atof(idValueSeparator));
			}
		}
		if(nextLine) *nextLine = '\n';
		currLine = nextLine ? (nextLine + 1) : nullptr;
	}
	if(!(fbPropertiesFoundBitflags & 1<<KFB_META_DECODE_TEX_ASSET_NAME))
	/* if the meta data file didn't contain an entry for texture asset file 
		name, then we assume that the texture asset file name matches the 
		flipbook meta asset file name with a png file extension */
	{
		strcpy_s(o_texAssetFileName, texAssetFileNameBufferSize, 
		         cStrAnsiAssetName);
		char* pFileExtension = strchr(o_texAssetFileName, '.');
		if(!pFileExtension)
		{
			KLOG(ERROR, "Flipbook meta data asset file name '%s' contains no "
			     "file extension!", cStrAnsiAssetName);
			return false;
		}
		const char textureFileExtension[] = "tex";
		const char*const texAssetFileNameBufferEnd = 
			o_texAssetFileName + texAssetFileNameBufferSize;
		if(pFileExtension + CARRAY_SIZE(textureFileExtension) >= 
		        texAssetFileNameBufferEnd)
		{
			KLOG(ERROR, "texAssetFileNameBufferSize==%i can't contain implicit "
			     "texture asset file name for flipbook meta data asset '%s'!", 
			     texAssetFileNameBufferSize, cStrAnsiAssetName);
			return false;
		}
		for(size_t c = 0; c < CARRAY_SIZE(textureFileExtension); c++)
		{
			pFileExtension[1 + c] = textureFileExtension[c];
		}
		fbPropertiesFoundCount++;
		fbPropertiesFoundBitflags |= 1<<KFB_META_DECODE_TEX_ASSET_NAME;
	}
	const bool success = 
		   (fbPropertiesFoundCount    ==     KFB_META_DECODE_ENTRY_COUNT) 
		&& (fbPropertiesFoundBitflags == (1<<KFB_META_DECODE_ENTRY_COUNT) - 1);
	if(!success)
	{
		KLOG(ERROR, "Failed to pass safety check for '%s'!", cStrAnsiAssetName);
	}
	return success;
}
internal void kgtFlipBookInit(KgtFlipBook* kfb, KgtAssetIndex assetIndex)
{
	kfb->kaiMetaData     = assetIndex;
	kfb->cachedMetaData  = kgtAssetManagerGetFlipBookMetaData(
	                           g_kam, assetIndex);
	kfb->anchorRatioX    = kfb->cachedMetaData.defaultAnchorRatioX;
	kfb->anchorRatioY    = kfb->cachedMetaData.defaultAnchorRatioY;
	kfb->repeat          = kfb->cachedMetaData.defaultRepeat;
	kfb->reverse         = kfb->cachedMetaData.defaultReverse;
	kfb->secondsPerFrame = kfb->cachedMetaData.defaultSecondsPerFrame;
}
internal bool operator==(
	const KgtFlipBookMetaData& a, const KgtFlipBookMetaData& b)
{
	if(a.defaultAnchorRatioX != b.defaultAnchorRatioX)
		return false;
	if(a.defaultAnchorRatioY != b.defaultAnchorRatioY)
		return false;
	if(a.defaultRepeat != b.defaultRepeat)
		return false;
	if(a.defaultReverse != b.defaultReverse)
		return false;
	if(a.defaultSecondsPerFrame != b.defaultSecondsPerFrame)
		return false;
	if(a.frameCount != b.frameCount)
		return false;
	if(a.frameSizeX != b.frameSizeX)
		return false;
	if(a.frameSizeY != b.frameSizeY)
		return false;
	if(a.kaiTexture != b.kaiTexture)
		return false;
	return true;
}
internal bool operator!=(
	const KgtFlipBookMetaData& a, const KgtFlipBookMetaData& b)
{
	return !(a == b);
}
internal void kgtFlipBookGetPageProperties(
	KgtFlipBook* kfb, u32* o_pageSizeX, u32* o_pageSizeY, u16* o_pageCount, 
	u32* o_pageRows, u32* o_pageCols)
{
	*o_pageSizeX = kfb->cachedMetaData.frameSizeX;
	*o_pageSizeY = kfb->cachedMetaData.frameSizeY;
	*o_pageCount = kfb->cachedMetaData.frameCount;
	const KgtAssetIndex kaiTex = kfb->cachedMetaData.kaiTexture;
	const v2u32 texSize = kgtAssetManagerGetRawImageSize(g_kam, kaiTex);
	if(*o_pageSizeX == 0)
	{
		*o_pageSizeX = texSize.x;
	}
	if(*o_pageSizeY == 0)
	{
		*o_pageSizeY = texSize.y;
	}
	*o_pageRows = texSize.y / *o_pageSizeY;
	*o_pageCols = texSize.x / *o_pageSizeX;
	if(*o_pageCount == 0)
	{
		*o_pageCount = 
			kmath::safeTruncateU16((*o_pageRows) * (*o_pageCols));
	}
	korlAssert(*o_pageCount >= 1);
}
internal void kgtFlipBookDraw(KgtFlipBook* kfb, const ColorRgbaF32& color)
{
	// If the flipbook's meta data doesn't match the meta data of the flipbook 
	//	asset, then initialize the flipbook using the latest asset data. //
	const KgtAssetIndex kaiFbm = kfb->kaiMetaData;
	const KgtFlipBookMetaData fbmd = 
		kgtAssetManagerGetFlipBookMetaData(g_kam, kaiFbm);
	if(kfb->cachedMetaData != fbmd)
	{
		kfb->cachedMetaData  = fbmd;
		kfb->anchorRatioX    = fbmd.defaultAnchorRatioX;
		kfb->anchorRatioY    = fbmd.defaultAnchorRatioY;
		kfb->repeat          = fbmd.defaultRepeat;
		kfb->reverse         = fbmd.defaultReverse;
		kfb->secondsPerFrame = fbmd.defaultSecondsPerFrame;
	}
	// Determine the flipbook's page properties so we know what UV coordinates 
	//	to submit to the backend renderer. //
	u32 frameSizeX, frameSizeY, flipbookPageRows, flipbookPageCols;
	u16 frameCount;
	kgtFlipBookGetPageProperties(
		kfb, &frameSizeX, &frameSizeY, &frameCount, 
		&flipbookPageRows, &flipbookPageCols);
	// Determine which page/frame of the flipbook we need to draw so we know 
	//	what UV coordinates to submit to the backend renderer. //
#if 0
	const f32 animationLoopTotalSeconds = frameCount * kfb->secondsPerFrame;
#endif // 0
	const u16 pageIndex = static_cast<u16>(static_cast<i32>(
		kfb->currentAnimationLoopSeconds / kfb->secondsPerFrame) %
		frameCount);
	const u32 pageRow = pageIndex / flipbookPageCols;
	const u32 pageCol = pageIndex % flipbookPageCols;
	korlAssert(pageRow < flipbookPageRows);
	korlAssert(pageCol < flipbookPageCols);
	const f32 pageTexCoordUp    = 
		static_cast<f32>( pageRow     ) / static_cast<f32>(flipbookPageRows);
	const f32 pageTexCoordDown  = 
		static_cast<f32>((pageRow + 1)) / static_cast<f32>(flipbookPageRows);
	const f32 pageTexCoordLeft  = 
		static_cast<f32>(pageCol + (kfb->flipH ? 1 : 0)) / 
			static_cast<f32>(flipbookPageCols);
	const f32 pageTexCoordRight = 
		static_cast<f32>(pageCol + (kfb->flipH ? 0 : 1)) / 
			static_cast<f32>(flipbookPageCols);
	// Submit draw commands to the render backend. //
	const KgtAssetIndex kaiTex = kfb->cachedMetaData.kaiTexture;
	g_krb->useTexture(kgtAssetManagerGetTexture        (g_kam, kaiTex), 
	                  kgtAssetManagerGetTextureMetaData(g_kam, kaiTex));
	v2f32 texCoords[4] = {{pageTexCoordLeft, pageTexCoordUp},
	                      {pageTexCoordLeft, pageTexCoordDown},
	                      {pageTexCoordRight, pageTexCoordDown},
	                      {pageTexCoordRight, pageTexCoordUp}};
	ColorRgbaF32 colors[4] = {color,color,color,color};
	g_krb->drawQuadTextured(
		v2f32{ static_cast<f32>(frameSizeX)
		     , static_cast<f32>(frameSizeY) }.elements, 
		v2f32{kfb->anchorRatioX, kfb->anchorRatioY}.elements,
		colors, v2f32{pageTexCoordLeft, pageTexCoordUp}.elements, 
		v2f32{pageTexCoordRight, pageTexCoordDown}.elements);
}
internal void kgtFlipBookStep(KgtFlipBook* kfb, f32 deltaSeconds)
{
	// Determine the flipbook's page properties so we can figure out how long a 
	//	loop of the animation is in seconds //
	u32 frameSizeX, frameSizeY, flipbookPageRows, flipbookPageCols;
	u16 frameCount;
	kgtFlipBookGetPageProperties(
		kfb, &frameSizeX, &frameSizeY, &frameCount, 
		&flipbookPageRows, &flipbookPageCols);
	const f32 animationLoopTotalSeconds = 
		static_cast<f32>(frameCount) * kfb->secondsPerFrame;
	if(kmath::isNearlyZero(animationLoopTotalSeconds))
	{
		return;
	}
	// Animate the flipbook //
	kfb->currentAnimationLoopSeconds = 
		fmodf((kfb->currentAnimationLoopSeconds + deltaSeconds), 
		      animationLoopTotalSeconds);
}
