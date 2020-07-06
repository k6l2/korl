#pragma once
#include "global-defines.h"
#include "platform-game-interfaces.h"
#include "gen_kassets.h"
struct KAssetManager;
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
internal void kfbInit(KFlipBook* kfb, KAssetManager* kam, 
                      KrbApi* krb, KAssetIndex assetIndex);
internal void kfbStep(KFlipBook* kfb, f32 deltaSeconds);
internal void kfbDraw(KFlipBook* kfb, const Color4f32& color);