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
internal void kfbDraw(KrbApi *krb, KAssetManager *kam, KAssetCStr kAssetCStr,
                      KFlipBook& flipbook)
{
	// If the flipbook's meta data doesn't match the meta data of the flipbook 
	//	asset, then initialize the flipbook using the latest asset data. //
	const FlipbookMetaData fbmd = kamGetFlipbookMetaData(kam, kAssetCStr);
	if(flipbook.metaData != fbmd)
	{
		flipbook.metaData        = fbmd;
		flipbook.anchorRatioX    = fbmd.defaultAnchorRatioX;
		flipbook.anchorRatioY    = fbmd.defaultAnchorRatioY;
		flipbook.repeat          = fbmd.defaultRepeat;
		flipbook.reverse         = fbmd.defaultReverse;
		flipbook.secondsPerFrame = fbmd.defaultSecondsPerFrame;
	}
	// Determine the flipbook's page properties so we know what UV coordinates 
	//	to submit to the backend renderer. //
	u32 frameSizeX = flipbook.metaData.frameSizeX;
	u32 frameSizeY = flipbook.metaData.frameSizeY;
	u16 frameCount = flipbook.metaData.frameCount;
	const v2u32 texSize = 
		kamGetTextureSize(kam, flipbook.metaData.textureKAssetCStr);
	if(frameSizeX == 0)
	{
		frameSizeX = texSize.x;
	}
	if(frameSizeY == 0)
	{
		frameSizeY = texSize.y;
	}
	const u32 flipbookPageRows = texSize.y / frameSizeY;
	const u32 flipbookPageCols = texSize.x / frameSizeX;
	if(frameCount == 0)
	{
		frameCount = kmath::safeTruncateU16(flipbookPageRows*flipbookPageCols);
	}
	kassert(frameCount >= 1);
	// Determine which page/frame of the flipbook we need to draw so we know 
	//	what UV coordinates to submit to the backend renderer. //
#if 0
	const f32 animationLoopTotalSeconds = frameCount * flipbook.secondsPerFrame;
#endif // 0
	const u16 pageIndex = static_cast<u16>(static_cast<i32>(
		flipbook.currentAnimationLoopSeconds / flipbook.secondsPerFrame) %
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
	                           static_cast<f32>( pageCol     )/flipbookPageCols;
	const f32 pageTexCoordRight = 
	                           static_cast<f32>((pageCol + 1))/flipbookPageCols;
	// Submit draw commands to the render backend. //
	krb->useTexture(kamGetTexture(kam, flipbook.metaData.textureKAssetCStr));
	krb->drawQuadTextured({ static_cast<f32>(frameSizeX), 
	                        static_cast<f32>(frameSizeY) }, 
	                      {flipbook.anchorRatioX, flipbook.anchorRatioY},
	                      {pageTexCoordLeft, pageTexCoordUp},
	                      {pageTexCoordLeft, pageTexCoordDown},
	                      {pageTexCoordRight, pageTexCoordDown},
	                      {pageTexCoordRight, pageTexCoordUp});
}