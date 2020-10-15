#pragma once
#include "kgtAssetManager.h"
#include "kgtAllocator.h"
#include "kgtNetCommon.h"
#include "kgtNetServer.h"
#include "exampleGameNet.h"
struct ServerState
{
	/* KML interface */
	enum class RunState : u8
		{ STOP
		, RUN
		, RESTART
	} runState;
	JobQueueTicket serverJobTicket;
	/* configuration */
	f32 secondsPerFrame;
	/* memory management */
	KgtAllocatorHandle hKalPermanent;
	KgtAllocatorHandle hKalTransient;
	KgtAllocatorHandle hKalFrame;
	KgtAssetManager* assetManager;
	/* data */
	KgtNetServer kNetServer;
	bool serverStartCausedByRestart;
	Actor actors[4];
};
enum class ServerOperatingState : u8
	{ RUNNING
	, STOPPING
	, STOPPED
};
internal void serverInitialize(ServerState* ss, f32 secondsPerFrame, 
                               KgtAllocatorHandle hKgaPermanentParent, 
                               KgtAllocatorHandle hKgaTransientParent, 
                               u64 permanentMemoryBytes, 
                               u64 transientMemoryBytes, u16 port);
internal ServerOperatingState serverOpState(ServerState* ss);
internal void serverStart(ServerState* ss);
internal void serverStop(ServerState* ss);
internal void serverOnPreUnload(ServerState* ss);
internal void serverOnReload(ServerState* ss);
