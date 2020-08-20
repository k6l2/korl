#include "serverExample.h"
#include "KmlGameExample.h"
internal void serverInitialize(ServerState* ss, f32 secondsPerFrame, 
                               KgaHandle hKgaPermanentParent, 
                               KgaHandle hKgaTransientParent, 
                               u64 permanentMemoryBytes, 
                               u64 transientMemoryBytes, u16 port)
{
	kassert(!ss->running);
	*ss = {};
	ss->kNetServer.port = port;
	ss->secondsPerFrame = secondsPerFrame;
	ss->serverJobTicket = g_kpl->postJob(nullptr, nullptr);
	/* allocate memory for the server */
	void*const permanentMemory = 
		kgaAlloc(hKgaPermanentParent, permanentMemoryBytes);
	void*const transientMemory = 
		kgaAlloc(hKgaTransientParent, transientMemoryBytes);
	// initialize dynamic allocators //
	ss->hKgaPermanent = 
		kgaInit(reinterpret_cast<u8*>(permanentMemory), permanentMemoryBytes);
	ss->hKgaTransient = kgaInit(transientMemory, transientMemoryBytes);
	// construct a linear frame allocator //
	{
		const size_t kalFrameSize = kmath::megabytes(1);
		void*const kalFrameStartAddress = 
			kgaAlloc(ss->hKgaPermanent, kalFrameSize);
		ss->hKalFrame = kalInit(kalFrameStartAddress, kalFrameSize);
	}
	// Contruct/Initialize the server's AssetManager //
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
	if(!kNetServerStart(&ss->kNetServer, ss->hKgaPermanent, 4))
	{
		KLOG(ERROR, "Failed to start server on port %i!", ss->kNetServer.port);
		ss->running = false;
		return;
	}
	defer(kNetServerStop(&ss->kNetServer));
	while(ss->running)
	{
		const PlatformTimeStamp timeStampFrameStart = g_kpl->getTimeStamp();
		kalReset(ss->hKalFrame);
		kNetServerStep(&ss->kNetServer, ss->secondsPerFrame, 
		               0.1f*ss->secondsPerFrame, timeStampFrameStart, 
		               serverReadClient, serverWriteState, 
		               serverOnClientConnect, serverOnClientDisconnect, 
		               serverReadReliableMessage, ss);
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