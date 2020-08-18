#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "krb-interface.h"
#include "kGeneralAllocator.h"
#include "kAssetManager.h"
#include "kAudioMixer.h"
#include "kFlipBook.h"
#include "kAllocatorLinear.h"
#include "kNetClient.h"
#include "serverExample.h"
#define STBDS_REALLOC(context,ptr,size) kStbDsRealloc(ptr,size,context)
#define STBDS_FREE(context,ptr)         kStbDsFree(ptr,context)
#define STBDS_ASSERT(x)                 g_kpl->assert(x)
#include "stb/stb_ds.h"
internal void* kStbDsRealloc(void* allocatedAddress, size_t newAllocationSize, 
                             void* context);
internal void kStbDsFree(void* allocatedAddress, void* context);
struct ClientControlInput
{
	v2f32 controlMoveVector;
	bool controlResetPosition;
};
struct GameState
{
	KAssetManager* assetManager;
	KAudioMixer* kAudioMixer;
	/* memory allocators */
	KgaHandle hKgaPermanent;
	KgaHandle hKgaTransient;
	KalHandle hKalFrame;
	/* sample server state management */
	ServerState serverState;
	char clientAddressBuffer[256];
	KNetClient kNetClient;
	/* sample media testing state */
	KTapeHandle tapeBgmBattleTheme;
	KFlipBook kFbShip;
	KFlipBook kFbShipExhaust;
	Actor* actors;
	ClientControlInput controlInput;
};
global_variable GameState* g_gs;
global_variable KmlPlatformApi* g_kpl;
global_variable fnSig_platformLog* platformLog;
global_variable fnSig_platformAssert* platformAssert;
global_variable KrbApi* g_krb;