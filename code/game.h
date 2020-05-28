#pragma once
#include "global-defines.h"
#if defined(KLOG)
	#undef KLOG
	#define KLOG(platformLogCategory, formattedString, ...) g_log(\
	    __FILENAME__, __LINE__, PlatformLogCategory::K_##platformLogCategory, \
	    formattedString, ##__VA_ARGS__)
#endif
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "generalAllocator.h"
#include "kAssetManager.h"
#include "kAudioMixer.h"
struct GameState
{
	v2f32 viewOffset2d;
	v2f32 shipWorldPosition;
	kmath::Quaternion shipWorldOrientation;
	KAssetManager* assetManager;
	KAudioMixer* kAudioMixer;
	KAssetHandle kahSfxShoot;
	KAssetHandle kahSfxHit;
	KAssetHandle kahSfxExplosion;
	KAssetHandle kahBgmBattleTheme;
	KAssetHandle kahImgFighter;
	KrbTextureHandle kthFighter;
	KgaHandle kgaHPermanent;
	KgaHandle kgaHTransient;
};
global_variable GameState* g_gameState;
global_variable fnSig_platformLog* g_log;