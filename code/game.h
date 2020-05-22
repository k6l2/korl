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
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "generalAllocator.h"
#include "kAssetManager.h"
#include "kAudioMixer.h"
struct GameState
{
	v2f32 viewOffset2d;
	v2f32 shipWorldPosition;
#if 0
	f32 theraminHz = 294.f;
	f32 theraminSine = 0;
	f32 theraminVolume = 1000;
	KrbTextureHandle kthFighter;
	u8 kthFighter_PADDING[4];
#endif //0
	KAssetManager* assetManager;
	KAudioMixer* kAudioMixer;
	KAssetHandle kahSfxShoot;
	KAssetHandle kahSfxHit;
	KAssetHandle kahSfxExplosion;
	u8 kahSfxExplosion_PADDING[4];
	KgaHandle kgaHPermanent;
	KgaHandle kgaHTransient;
};
global_variable GameState* g_gameState;
global_variable fnSig_platformLog* g_log;