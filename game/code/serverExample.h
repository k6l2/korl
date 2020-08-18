#pragma once
#include "kAssetManager.h"
#include "kAllocatorLinear.h"
#include "kNetCommon.h"
#include "kNetServer.h"
#include "exampleGameNet.h"
struct ServerState
{
	/* KML interface */
	bool running;
	JobQueueTicket serverJobTicket;
	bool onGameReloadStartServer;
	/* configuration */
	f32 secondsPerFrame;
	/* memory management */
	KgaHandle hKgaPermanent;
	KgaHandle hKgaTransient;
	KalHandle hKalFrame;
	KAssetManager* assetManager;
	/* data */
	KNetServer kNetServer;
	Actor actors[4];
};
enum class ServerOperatingState : u8
	{ RUNNING
	, STOPPING
	, STOPPED
};
internal void serverInitialize(ServerState* ss, f32 secondsPerFrame, 
                               KgaHandle hKgaPermanentParent, 
                               KgaHandle hKgaTransientParent, 
                               u64 permanentMemoryBytes, 
                               u64 transientMemoryBytes, u16 port);
internal ServerOperatingState serverOpState(ServerState* ss);
internal void serverStart(ServerState* ss);
internal void serverStop(ServerState* ss);
internal void serverOnPreUnload(ServerState* ss);
internal void serverOnReload(ServerState* ss);