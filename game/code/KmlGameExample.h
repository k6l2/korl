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
#include "kGeneralAllocator.h"
#include "kAssetManager.h"
#include "kAudioMixer.h"
#include "kFlipBook.h"
#include "kAllocatorLinear.h"
#define STBDS_REALLOC(context,ptr,size) kStbDsRealloc(ptr,size)
#define STBDS_FREE(context,ptr)         kStbDsFree(ptr)
#include "stb/stb_ds.h"
struct GameState
{
	KAssetManager* assetManager;
	KAudioMixer* kAudioMixer;
	KgaHandle hKgaPermanent;
	KgaHandle hKgaTransient;
	KalHandle hKalFrame;
	v2f32 viewOffset2d;
	v2f32 shipWorldPosition;
	kmath::Quaternion shipWorldOrientation = kmath::quat({0,0,1}, 0);
	KTapeHandle tapeBgmBattleTheme;
	KFlipBook kFbShip;
	KFlipBook kFbShipExhaust;
	int* testStbDynArr;
};
global_variable GameState* g_gs;
global_variable KmlPlatformApi* g_kpl;
global_variable KrbApi* g_krb;
internal void* kStbDsRealloc(void* allocatedAddress, size_t newAllocationSize);
internal void  kStbDsFree(void* allocatedAddress);