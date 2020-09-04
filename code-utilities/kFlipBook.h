#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "gen_kassets.h"
struct KAssetManager;
/* 
FlipbookMetaData asset files are just text.  They must contain ONE of EACH 
	property of the struct.  Each property must be contained on a new line.
	The property identifier (left side) MUST match exactly to the example below.  
	The identifier & value (right side) are separated by a ':' character.  Any 
	whitespace aside from '\n' characters are allowed at any time.  The file 
	extention MUST be ".fbm".
A `frame-count` of 0 means the texture asset is "fully saturated" with flipbook 
	pages.  This means the true frame count is equivilant to: 
	`flipbookPageRows*flipbookPageCols` where 
		`flipbookPageRows` == textureAsset.sizeY / `frame-size-y` and
		`flipbookPageCols` == textureAsset.sizeX / `frame-size-x`
A `frame-size-x/y` of {0,0} means the entire texture asset is a single frame.
`default-anchor-ratio-x/y` is relative to the upper-left corner of the texture 
	asset.
----- Example of a flipbook meta data asset text file: -----
frame-size-x              : 48
frame-size-y              : 48
frame-count               : 6
texture-asset-file-name   : fighter-exhaust.png
default-repeat            : 1
default-reverse           : 0
default-seconds-per-frame : 0.05
default-anchor-ratio-x    : 0.5
default-anchor-ratio-y    : 0.5
*/
struct FlipbookMetaData
{
	u32 frameSizeX;
	u32 frameSizeY;
	u16 frameCount;
	bool defaultRepeat;
	bool defaultReverse;
	f32 defaultSecondsPerFrame;
	size_t textureKAssetIndex;
	f32 defaultAnchorRatioX;
	f32 defaultAnchorRatioY;
};
struct KFlipBook
{
	KAssetManager* kam;
	KrbApi* krb;
	size_t kAssetIndexMetaData;
	FlipbookMetaData cachedMetaData;
	f32 secondsPerFrame;
	f32 anchorRatioX;
	f32 anchorRatioY;
	f32 currentAnimationLoopSeconds;
	bool repeat;
	bool reverse;
	bool flipH;
};
internal bool kfbDecodeMeta(void* fileData, u32 fileBytes, 
                            FlipbookMetaData* o_fbMeta, 
                            char* o_texAssetFileName, 
                            size_t texAssetFileNameBufferSize);
internal void kfbInit(KFlipBook* kfb, KAssetManager* kam, 
                      KrbApi* krb, KAssetIndex assetIndex);
internal void kfbStep(KFlipBook* kfb, f32 deltaSeconds);
internal void kfbDraw(KFlipBook* kfb, const Color4f32& color);