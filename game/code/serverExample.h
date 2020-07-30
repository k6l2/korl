#pragma once
#include "kAssetManager.h"
#include "kAllocatorLinear.h"
struct ServerState
{
	void* permanentMemory;
	u64   permanentMemoryBytes;
	void* transientMemory;
	u64   transientMemoryBytes;
	KgaHandle hKgaPermanent;
	KgaHandle hKgaTransient;
	KalHandle hKalFrame;
	KAssetManager* assetManager;
	bool running;
	JobQueueTicket serverJobTicket;
	bool onGameReloadStartServer;
	f32 secondsPerFrame;
};
enum class ServerOperatingState : u8
	{ RUNNING
	, STOPPING
	, STOPPED
};
internal void serverInitialize(ServerState* ss, KgaHandle hKgaPermanent, 
                               KgaHandle hKgaTransient, 
                               u64 permanentMemoryBytes, 
                               u64 transientMemoryBytes);
internal ServerOperatingState serverOpState(ServerState* ss);
internal void serverStart(ServerState* ss);
internal void serverStop(ServerState* ss);
internal void serverOnPreUnload(ServerState* ss);
internal void serverOnReload(ServerState* ss);