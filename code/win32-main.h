#pragma once
#include "global-defines.h"
#if defined(KLOG_INFO)
	#undef KLOG_INFO
#endif
#if defined(KLOG_WARNING)
	#undef KLOG_WARNING
#endif
#if defined(KLOG_ERROR)
	#undef KLOG_ERROR
#endif
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
#include "platform-game-interfaces.h"
internal PLATFORM_LOG(platformLog);
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
internal PLATFORM_READ_ENTIRE_FILE(platformReadEntireFile);
internal PLATFORM_FREE_FILE_MEMORY(platformFreeFileMemory);