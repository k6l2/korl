#pragma once
#include "global-defines.h"
#include <windows.h>
struct W32OffscreenBuffer
{
	void* bitmapMemory;
	BITMAPINFO bitmapInfo;
	u32 width;
	u32 height;
	u32 pitch;
	u8 bytesPerPixel;
};
struct W32Dimension2d
{
	u32 width;
	u32 height;
};