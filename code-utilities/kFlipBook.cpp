#include "kFlipBook.h"
#include "kAssetManager.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
internal bool kfbDecodeMeta(void* fileData, u32 fileBytes, 
                            FlipbookMetaData* o_fbMeta, 
                            char* o_texAssetFileName, 
                            size_t texAssetFileNameBufferSize)
{
	*o_fbMeta = {};
	char*const fileCStr = reinterpret_cast<char*>(fileData);
	u16 fbPropertiesFoundBitflags = 0;
	u8 fbPropertiesFoundCount = 0;
	// destructively read the file line by line //
	// source: https://stackoverflow.com/a/17983619
	char* currLine = fileCStr;
	while(currLine)
	{
		char* nextLine = strchr(currLine, '\n');
		if(nextLine) *nextLine = '\0';
		// parse `currLine` for flipbook meta data //
		{
			char* idValueSeparator = strchr(currLine, ':');
			if(!idValueSeparator)
			{
				return false;
			}
			*idValueSeparator = '\0';
			idValueSeparator++;
			if(strstr(currLine, "frame-size-x"))
			{
				fbPropertiesFoundBitflags |= 1<<0;
				fbPropertiesFoundCount++;
				o_fbMeta->frameSizeX = kmath::safeTruncateU32(
					atoi(idValueSeparator));
			}
			else if(strstr(currLine, "frame-size-y"))
			{
				fbPropertiesFoundBitflags |= 1<<1;
				fbPropertiesFoundCount++;
				o_fbMeta->frameSizeY = kmath::safeTruncateU32(
					atoi(idValueSeparator));
			}
			else if(strstr(currLine, "frame-count"))
			{
				fbPropertiesFoundBitflags |= 1<<2;
				fbPropertiesFoundCount++;
				o_fbMeta->frameCount = 
					kmath::safeTruncateU16(atoi(idValueSeparator));
			}
			else if(strstr(currLine, "texture-asset-file-name"))
			{
				fbPropertiesFoundBitflags |= 1<<3;
				fbPropertiesFoundCount++;
				while(*idValueSeparator && isspace(*idValueSeparator))
				{
					idValueSeparator++;
				}
				for(char* spaceSearchChar = idValueSeparator + 1; 
					*spaceSearchChar; spaceSearchChar++)
				{
					if(isspace(*spaceSearchChar))
					{
						*spaceSearchChar = '\0';
						break;
					}
				}
				const size_t valueStrLength = strlen(idValueSeparator);
				if(valueStrLength >= texAssetFileNameBufferSize - 1)
				{
					return false;
				}
				strcpy_s(o_texAssetFileName, texAssetFileNameBufferSize,
				         idValueSeparator);
			}
			else if(strstr(currLine, "default-repeat"))
			{
				fbPropertiesFoundBitflags |= 1<<4;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultRepeat = atoi(idValueSeparator) != 0;
			}
			else if(strstr(currLine, "default-reverse"))
			{
				fbPropertiesFoundBitflags |= 1<<5;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultReverse = atoi(idValueSeparator) != 0;
			}
			else if(strstr(currLine, "default-seconds-per-frame"))
			{
				fbPropertiesFoundBitflags |= 1<<6;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultSecondsPerFrame = 
					static_cast<f32>(atof(idValueSeparator));
			}
			else if(strstr(currLine, "default-anchor-ratio-x"))
			{
				fbPropertiesFoundBitflags |= 1<<7;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultAnchorRatioX = 
					static_cast<f32>(atof(idValueSeparator));
			}
			else if(strstr(currLine, "default-anchor-ratio-y"))
			{
				fbPropertiesFoundBitflags |= 1<<8;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultAnchorRatioY = 
					static_cast<f32>(atof(idValueSeparator));
			}
		}
		if(nextLine) *nextLine = '\n';
		currLine = nextLine ? (nextLine + 1) : nullptr;
	}
	return (fbPropertiesFoundCount == 9) && 
		(fbPropertiesFoundBitflags == (1<<fbPropertiesFoundCount) - 1);
}
internal void kfbInit(KFlipBook* kfb, KAssetManager* kam, 
                      KrbApi* krb, KAssetIndex assetIndex)
{
	kfb->kam = kam;
	kfb->krb = krb;
	kfb->kAssetIndexMetaData = static_cast<size_t>(assetIndex);
	const size_t kAssetIndexMetaData = kfb->kAssetIndexMetaData;
	kfb->cachedMetaData = kamGetFlipbookMetaData(kfb->kam, assetIndex);
	kfb->anchorRatioX    = kfb->cachedMetaData.defaultAnchorRatioX;
	kfb->anchorRatioY    = kfb->cachedMetaData.defaultAnchorRatioY;
	kfb->repeat          = kfb->cachedMetaData.defaultRepeat;
	kfb->reverse         = kfb->cachedMetaData.defaultReverse;
	kfb->secondsPerFrame = kfb->cachedMetaData.defaultSecondsPerFrame;
}
internal bool operator==(const FlipbookMetaData& a, const FlipbookMetaData& b)
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
	if(a.textureKAssetIndex != b.textureKAssetIndex)
		return false;
	return true;
}
internal bool operator!=(const FlipbookMetaData& a, const FlipbookMetaData& b)
{
	return !(a == b);
}
internal void kfbGetPageProperties(KFlipBook* kfb, 
                                   u32* o_pageSizeX, u32* o_pageSizeY, 
                                   u16* o_pageCount,
                                   u32* o_pageRows, u32* o_pageCols)
{
	*o_pageSizeX = kfb->cachedMetaData.frameSizeX;
	*o_pageSizeY = kfb->cachedMetaData.frameSizeY;
	*o_pageCount = kfb->cachedMetaData.frameCount;
	const KAssetIndex kAssetIdTex = 
		KAssetIndex(kfb->cachedMetaData.textureKAssetIndex);
	const v2u32 texSize = kamGetImageSize(kfb->kam, kAssetIdTex);
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
	kassert(*o_pageCount >= 1);
}
internal void kfbDraw(KFlipBook* kfb, const Color4f32& color)
{
	// If the flipbook's meta data doesn't match the meta data of the flipbook 
	//	asset, then initialize the flipbook using the latest asset data. //
	const KAssetIndex kAssetIdFb = KAssetIndex(kfb->kAssetIndexMetaData);
	const FlipbookMetaData fbmd = kamGetFlipbookMetaData(kfb->kam, kAssetIdFb);
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
	kfbGetPageProperties(kfb, &frameSizeX, &frameSizeY, &frameCount, 
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
	kassert(pageRow < flipbookPageRows);
	kassert(pageCol < flipbookPageCols);
	const f32 pageTexCoordUp    = 
	                           static_cast<f32>( pageRow     )/flipbookPageRows;
	const f32 pageTexCoordDown  = 
	                           static_cast<f32>((pageRow + 1))/flipbookPageRows;
	const f32 pageTexCoordLeft  = 
	          static_cast<f32>(pageCol + (kfb->flipH ? 1 : 0))/flipbookPageCols;
	const f32 pageTexCoordRight = 
	          static_cast<f32>(pageCol + (kfb->flipH ? 0 : 1))/flipbookPageCols;
	// Submit draw commands to the render backend. //
	const KAssetIndex kAssetIdTex = 
		KAssetIndex(kfb->cachedMetaData.textureKAssetIndex);
	kfb->krb->useTexture(kamGetTexture(kfb->kam, kAssetIdTex));
	v2f32 texCoords[4] = {{pageTexCoordLeft, pageTexCoordUp},
	                      {pageTexCoordLeft, pageTexCoordDown},
	                      {pageTexCoordRight, pageTexCoordDown},
	                      {pageTexCoordRight, pageTexCoordUp}};
	Color4f32 colors[4] = {color,color,color,color};
	kfb->krb->drawQuadTextured({ static_cast<f32>(frameSizeX), 
	                             static_cast<f32>(frameSizeY) }, 
	                           {kfb->anchorRatioX, kfb->anchorRatioY},
	                           texCoords, colors);
}
internal void kfbStep(KFlipBook* kfb, f32 deltaSeconds)
{
	// Determine the flipbook's page properties so we can figure out how long a 
	//	loop of the animation is in seconds //
	u32 frameSizeX, frameSizeY, flipbookPageRows, flipbookPageCols;
	u16 frameCount;
	kfbGetPageProperties(kfb, &frameSizeX, &frameSizeY, &frameCount, 
	                     &flipbookPageRows, &flipbookPageCols);
	const f32 animationLoopTotalSeconds = frameCount * kfb->secondsPerFrame;
	if(kmath::isNearlyZero(animationLoopTotalSeconds))
	{
		return;
	}
	// Animate the flipbook //
	kfb->currentAnimationLoopSeconds = 
		fmodf((kfb->currentAnimationLoopSeconds + deltaSeconds), 
		      animationLoopTotalSeconds);
}