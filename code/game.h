#pragma once
#include "global-defines.h"
#if defined(KLOG)
	#undef KLOG
	#define KLOG(platformLogCategory, formattedString, ...) g_kpl->log(\
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
	kmath::Quaternion shipWorldOrientation = kmath::quat({0,0,1}, 0);
	KAssetHandle kahImgShip;
	u8 kahImgShip_PADDING[4];
	KAssetManager* assetManager;
	KAudioMixer* kAudioMixer;
	KgaHandle kgaHPermanent;
	KgaHandle kgaHTransient;
};
global_variable PlatformApi* g_kpl;
global_variable GameState* g_gs;
global_variable KrbApi* g_krb;