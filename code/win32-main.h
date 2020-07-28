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
	fnSig_gameInitialize* initialize;
	fnSig_gameOnReloadCode* onReloadCode;
	fnSig_gameRenderAudio* renderAudio;
	fnSig_gameUpdateAndDraw* updateAndDraw;
	HMODULE hLib;
	FILETIME dllLastWriteTime;
	bool isValid;
};
internal PLATFORM_LOG(platformLog);
internal PLATFORM_ASSERT(platformAssert);
internal PLATFORM_READ_ENTIRE_FILE(platformReadEntireFile);
internal PLATFORM_FREE_FILE_MEMORY(platformFreeFileMemory);
global_variable HWND g_mainWindow;