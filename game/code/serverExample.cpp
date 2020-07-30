#include "serverExample.h"
#include "KmlGameExample.h"
internal void serverInitialize(ServerState* ss, f32 secondsPerFrame, 
                               KgaHandle hKgaPermanent, 
                               KgaHandle hKgaTransient, 
                               u64 permanentMemoryBytes, 
                               u64 transientMemoryBytes)
{
	kassert(!ss->running);
	ss->secondsPerFrame = secondsPerFrame;
	ss->serverJobTicket = g_kpl->postJob(nullptr, nullptr);
	/* allocate memory for the server */
	ss->permanentMemoryBytes = permanentMemoryBytes;
	ss->transientMemoryBytes = transientMemoryBytes;
	ss->permanentMemory = kgaAlloc(hKgaPermanent, ss->permanentMemoryBytes);
	ss->transientMemory = kgaAlloc(hKgaPermanent, ss->transientMemoryBytes);
	// initialize dynamic allocators //
	ss->hKgaPermanent = 
		kgaInit(reinterpret_cast<u8*>(ss->permanentMemory), 
		        ss->permanentMemoryBytes);
	ss->hKgaTransient = kgaInit(ss->transientMemory, ss->transientMemoryBytes);
	// construct a linear frame allocator //
	{
		const size_t kalFrameSize = kmath::megabytes(1);
		void*const kalFrameStartAddress = 
			kgaAlloc(ss->hKgaPermanent, kalFrameSize);
		ss->hKalFrame = kalInit(kalFrameStartAddress, kalFrameSize);
	}
	// Contruct/Initialize the game's AssetManager //
	ss->assetManager = 
		kamConstruct(ss->hKgaPermanent, KASSET_COUNT,
		             /* pass nullptr for the kRenderBackend to ensure that the 
		                server doesn't try to load any assets onto the GPU, 
		                since there's no point. */
		             ss->hKgaTransient, g_kpl, nullptr);
}
internal JOB_QUEUE_FUNCTION(serverUpdate)
{
	KLOG(INFO, "Server job START!");
	ServerState* ss = reinterpret_cast<ServerState*>(data);
	while(ss->running)
	{
		PlatformTimeStamp timeStampFrameStart = g_kpl->getTimeStamp();
		kalReset(ss->hKalFrame);
		g_kpl->sleepFromTimeStamp(timeStampFrameStart, ss->secondsPerFrame);
	}
	KLOG(INFO, "Server job END!");
}
internal ServerOperatingState serverOpState(ServerState* ss)
{
	if(g_kpl->jobValid(&ss->serverJobTicket))
	{
		if(ss->running)
			return ServerOperatingState::RUNNING;
		else
			return ServerOperatingState::STOPPING;
	}
	return ServerOperatingState::STOPPED;
}
internal void serverStart(ServerState* ss)
{
	if(ss->running || g_kpl->jobValid(&ss->serverJobTicket))
	{
		KLOG(WARNING, "Server already running or is stopping!");
		return;
	}
	ss->running = true;
	ss->serverJobTicket = g_kpl->postJob(serverUpdate, ss);
}
internal void serverStop(ServerState* ss)
{
	if(!ss->running)
	{
		KLOG(WARNING, "Server already stopped!");
		return;
	}
	ss->running = false;
}
internal void serverOnPreUnload(ServerState* ss)
{
	if(g_kpl->jobValid(&ss->serverJobTicket))
	{
		KLOG(INFO, "serverOnPreUnload: temporarily stopping server...");
		ss->running = false;
		ss->onGameReloadStartServer = true;
	}
}
internal void serverOnReload(ServerState* ss)
{
	if(ss->onGameReloadStartServer)
	{
		KLOG(INFO, "Restarting server job...");
		serverStart(ss);
	}
	ss->onGameReloadStartServer = false;
}