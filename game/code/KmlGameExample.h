#pragma once
#include "kutil.h"
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
#include "serverExample.h"
#include "network_common.h"
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
	KplSocketIndex socketClient = KPL_INVALID_SOCKET_INDEX;
	char clientAddressBuffer[256];
	u8 clientPacketBuffer[KPL_MAX_DATAGRAM_SIZE];
	KplNetAddress clientAddressToServer;
	network::ConnectionState clientConnectionState;
	f32 clientSecondsSinceLastServerPacket;
	/* sample media testing state */
	v2f32 viewOffset2d;
	v2f32 shipWorldPosition;
	kQuaternion shipWorldOrientation = kQuaternion({0,0,1}, 0);
	KTapeHandle tapeBgmBattleTheme;
	KFlipBook kFbShip;
	KFlipBook kFbShipExhaust;
	int* testStbDynArr;
};
global_variable GameState* g_gs;
global_variable KmlPlatformApi* g_kpl;
global_variable fnSig_platformLog* platformLog;
global_variable fnSig_platformAssert* platformAssert;
global_variable KrbApi* g_krb;
internal void* kStbDsRealloc(void* allocatedAddress, size_t newAllocationSize);
internal void  kStbDsFree(void* allocatedAddress);