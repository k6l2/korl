#pragma once
#include "global-defines.h"
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "generalAllocator.h"
struct GameState
{
	v2f32 viewOffset2d;
	v2f32 shipWorldPosition;
#if 0
	f32 theraminHz = 294.f;
	f32 theraminSine = 0;
	f32 theraminVolume = 1000;
#endif //0
	KrbTextureHandle kthFighter;
	u8 kthFighter_PADDING[4];
	KgaHandle gAllocTransient;
};
global_variable GameState* g_gameState;
global_variable fnSig_platformLog* g_log;
#define KLOG_INFO(formattedString, ...) g_log(__FILENAME__, __LINE__, \
                                              PlatformLogCategory::K_INFO, \
                                              formattedString, \
                                              ##__VA_ARGS__)
#define KLOG_WARNING(formattedString, ...) g_log(__FILENAME__, __LINE__, \
                                               PlatformLogCategory::K_WARNING, \
                                               formattedString, \
                                               ##__VA_ARGS__)
#define KLOG_ERROR(formattedString, ...) g_log(__FILENAME__, __LINE__, \
                                               PlatformLogCategory::K_ERROR, \
                                               formattedString, \
                                               ##__VA_ARGS__)