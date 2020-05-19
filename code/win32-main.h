#pragma once
#include "global-defines.h"
#include "platform-game-interfaces.h"
internal PLATFORM_LOG(platformLog);
#define KLOG_INFO(formattedString, ...) platformLog(__FILENAME__, __LINE__, \
                                                  PlatformLogCategory::K_INFO, \
                                                  formattedString, \
                                                  ##__VA_ARGS__)
#define KLOG_WARNING(formattedString, ...) platformLog(__FILENAME__, __LINE__, \
                                               PlatformLogCategory::K_WARNING, \
                                               formattedString, \
                                               ##__VA_ARGS__)
#define KLOG_ERROR(formattedString, ...) platformLog(__FILENAME__, __LINE__, \
                                                 PlatformLogCategory::K_ERROR, \
                                                 formattedString, \
                                                 ##__VA_ARGS__)
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