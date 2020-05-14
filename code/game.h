#pragma once
#include "global-defines.h"
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "generalAllocator.h"
struct GameState
{
	fnSig_platformLog* log;
	v2f32 viewOffset2d;
	v2f32 shipWorldPosition;
	f32 theraminHz = 294.f;
	f32 theraminSine = 0;
	f32 theraminVolume = 1000;
	KrbTextureHandle kthFighter;
	KgaHandle gAllocTransient;
};
global_variable GameState* gameState;
#define KLOG_INFO(formattedString, ...) gameState->log(__FILENAME__, __LINE__, \
                                                PlatformLogCategory::K_INFO, \
                                                formattedString, \
                                                ##__VA_ARGS__)
#define KLOG_WARNING(formattedString, ...) gameState->log(__FILENAME__, \
                                               __LINE__, \
                                               PlatformLogCategory::K_WARNING, \
                                               formattedString, \
                                               ##__VA_ARGS__)
#define KLOG_ERROR(formattedString, ...) gameState->log(__FILENAME__, \
                                                 __LINE__, \
                                                 PlatformLogCategory::K_ERROR, \
                                                 formattedString, \
                                                 ##__VA_ARGS__)