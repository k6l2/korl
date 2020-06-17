#include "kFlipBook.h"
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
	if(a.textureKAssetCStr != b.textureKAssetCStr)
		return false;
	return true;
}
internal bool operator!=(const FlipbookMetaData& a, const FlipbookMetaData& b)
{
	return !(a == b);
}
internal void kfbGetPageProperties(KFlipBook* kfb, KAssetManager *kam, 
                                   u32* o_pageSizeX, u32* o_pageSizeY, 
                                   u16* o_pageCount,
                                   u32* o_pageRows, u32* o_pageCols)
{
	*o_pageSizeX = kfb->metaData.frameSizeX;
	*o_pageSizeY = kfb->metaData.frameSizeY;
	*o_pageCount = kfb->metaData.frameCount;
	const v2u32 texSize = 
		kamGetTextureSize(kam, kfb->metaData.textureKAssetCStr);
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
internal void kfbDraw(KFlipBook* kfb, KrbApi *krb, KAssetManager *kam, 
                      KAssetCStr kAssetCStr)
{
	// If the flipbook's meta data doesn't match the meta data of the flipbook 
	//	asset, then initialize the flipbook using the latest asset data. //
	const FlipbookMetaData fbmd = kamGetFlipbookMetaData(kam, kAssetCStr);
	if(kfb->metaData != fbmd)
	{
		kfb->metaData        = fbmd;
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
	kfbGetPageProperties(kfb, kam, &frameSizeX, &frameSizeY, &frameCount, 
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
	krb->useTexture(kamGetTexture(kam, kfb->metaData.textureKAssetCStr));
	krb->drawQuadTextured({ static_cast<f32>(frameSizeX), 
	                        static_cast<f32>(frameSizeY) }, 
	                      {kfb->anchorRatioX, kfb->anchorRatioY},
	                      {pageTexCoordLeft, pageTexCoordUp},
	                      {pageTexCoordLeft, pageTexCoordDown},
	                      {pageTexCoordRight, pageTexCoordDown},
	                      {pageTexCoordRight, pageTexCoordUp});
}
internal void kfbStep(KFlipBook* kfb, KAssetManager *kam, f32 deltaSeconds)
{
	// Determine the flipbook's page properties so we can figure out how long a 
	//	loop of the animation is in seconds //
	u32 frameSizeX, frameSizeY, flipbookPageRows, flipbookPageCols;
	u16 frameCount;
	kfbGetPageProperties(kfb, kam, &frameSizeX, &frameSizeY, &frameCount, 
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