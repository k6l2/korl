#pragma once
#include "global-defines.h"
#include "platform-game-interfaces.h"
#include "kAssetManager.h"
struct KFlipBook
{
	FlipbookMetaData metaData;
	f32 secondsPerFrame;
	f32 anchorRatioX;
	f32 anchorRatioY;
	f32 currentAnimationLoopSeconds;
	bool repeat;
	bool reverse;
	u8 reverse_PADDING[6];
};
internal void kfbDraw(KrbApi *krb, KAssetManager *kam, KAssetCStr kAssetCStr,
                      KFlipBook& flipbook);