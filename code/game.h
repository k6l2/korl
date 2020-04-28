#pragma once
#include "global-defines.h"
struct GameOffscreenBuffer
{
	void* bitmapMemory;
	u32 width;
	u32 height;
	u32 pitch;
	u8 bytesPerPixel;
};
internal void game_updateAndRender(GameOffscreenBuffer& buffer, 
                                   int offsetX, int offsetY);