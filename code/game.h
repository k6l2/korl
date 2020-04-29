#pragma once
#include "global-defines.h"
struct GameGraphicsBuffer
{
	void* bitmapMemory;
	u32 width;
	u32 height;
	u32 pitch;
	u8 bytesPerPixel;
};
struct GameAudioBuffer
{
	SoundSample* memory;
	u32 lockedSampleCount;
	u32 soundSampleHz;
	u8 numSoundChannels;
};
internal void game_renderAudio(GameAudioBuffer& audioBuffer, f32 theraminHz);
internal void game_updateAndDraw(GameGraphicsBuffer& graphicsBuffer, 
                                 int offsetX, int offsetY);