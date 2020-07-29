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
};
internal JobQueueTicket serverStart(ServerState* ss);