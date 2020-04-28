#include "game.h"
internal void game_updateAndRender(GameOffscreenBuffer& buffer, 
                                   int offsetX, int offsetY)
{
	// render a weird gradient pattern to the offscreen buffer //
	u8* row = reinterpret_cast<u8*>(buffer.bitmapMemory);
	for (int y = 0; y < buffer.height; y++)
	{
		u32* pixel = reinterpret_cast<u32*>(row);
		for (int x = 0; x < buffer.width; x++)
		{
			// pixel format: 0xXxRrGgBb
			*pixel++ = ((u8)(x + offsetX) << 16) | ((u8)(y + offsetY) << 8);
		}
		row += buffer.pitch;
	}
}