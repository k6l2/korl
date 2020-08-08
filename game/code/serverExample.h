#pragma once
#include "kAssetManager.h"
#include "kAllocatorLinear.h"
#include "network_common.h"
struct ServerClientEntry
{
	KplNetAddress netAddress;
	u16 netPort;
	f32 timeSinceLastPacket;
	network::ConnectionState connectionState;
};
struct ServerState
{
	/* KML interface */
	bool running;
	JobQueueTicket serverJobTicket;
	bool onGameReloadStartServer;
	/* memory management */
	void* permanentMemory;
	u64   permanentMemoryBytes;
	void* transientMemory;
	u64   transientMemoryBytes;
	KgaHandle hKgaPermanent;
	KgaHandle hKgaTransient;
	KalHandle hKalFrame;
	KAssetManager* assetManager;
	/* configuration */
	f32 secondsPerFrame;
	u16 port;
	/* data */
	ServerClientEntry clients[4];
};
enum class ServerOperatingState : u8
	{ RUNNING
	, STOPPING
	, STOPPED
};
internal void serverInitialize(ServerState* ss, f32 secondsPerFrame, 
                               KgaHandle hKgaPermanent, 
                               KgaHandle hKgaTransient, 
                               u64 permanentMemoryBytes, 
                               u64 transientMemoryBytes, u16 port);
internal ServerOperatingState serverOpState(ServerState* ss);
internal void serverStart(ServerState* ss);
internal void serverStop(ServerState* ss);
internal void serverOnPreUnload(ServerState* ss);
internal void serverOnReload(ServerState* ss);