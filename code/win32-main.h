#pragma once
#include "global-defines.h"
#include "platform-game-interfaces.h"
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
struct GameCode
{
	fnSig_GameRenderAudio* renderAudio;
	fnSig_GameUpdateAndDraw* updateAndDraw;
	HMODULE hLib;
	FILETIME dllLastWriteTime;
	bool isValid;
};