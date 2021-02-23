/*
 * User code must define the following global variables to use this module:
 * - KrbApi* g_krb
 * - KgtAssetManager* g_kam
 */
#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "gen_kgtAssets.h"
/* 
FlipbookMetaData asset files are just text.  They must contain ONE of EACH 
	property of the struct (with one exception, see below).  Each property must 
	be contained on a new line.  The property identifier (left side) MUST match 
	exactly to the example below.  The identifier & value (right side) are 
	separated by a ':' character.  Any whitespace aside from '\n' characters are 
	allowed at any time.  The file extention MUST be ".fbm".
If `texture-asset-file-name` is not present in the FlipbookMetaData asset file, 
	it is assumed that the texture asset file is equivilant to the 
	FlipbookMetaData asset file, except with a ".tex" file extension instead.
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
frame-size-y              : 0
frame-count               : 6
texture-asset-file-name   : fighter-exhaust.tex
default-repeat            : 1
default-reverse           : 0
default-seconds-per-frame : 0.05
default-anchor-ratio-x    : 0.5
default-anchor-ratio-y    : 0.5
*/
struct KgtFlipBookMetaData
{
	u32 frameSizeX;
	u32 frameSizeY;
	u16 frameCount;
	bool defaultRepeat;
	bool defaultReverse;
	f32 defaultSecondsPerFrame;
	KgtAssetIndex kaiTexture;
	f32 defaultAnchorRatioX;
	f32 defaultAnchorRatioY;
};
struct KgtFlipBook
{
	KgtAssetIndex kaiMetaData;
	KgtFlipBookMetaData cachedMetaData;
	f32 secondsPerFrame;
	f32 anchorRatioX;
	f32 anchorRatioY;
	f32 currentAnimationLoopSeconds;
	bool repeat;
	bool reverse;
	bool flipH;
};
/**
 * This function decodes & populates all members of FlipbookMetaData excluding 
 * kaiTexture, since this data must be determined by the asset manager 
 * itself using o_texAssetFileName.
 */
internal bool 
	kgtFlipBookDecodeMeta(
		void* fileData, u32 fileBytes, const char* cStrAnsiAssetName, 
		KgtFlipBookMetaData* o_fbMeta, char* o_texAssetFileName, 
		size_t texAssetFileNameBufferSize);
internal void 
	kgtFlipBookInit(KgtFlipBook* kfb, KgtAssetIndex assetIndex);
internal void 
	kgtFlipBookStep(KgtFlipBook* kfb, f32 deltaSeconds);
internal void 
	kgtFlipBookDraw(KgtFlipBook* kfb, const ColorRgbaF32& color);
