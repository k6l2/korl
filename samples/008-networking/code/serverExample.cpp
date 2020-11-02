#include "serverExample.h"
#include "korlDynApp.h"
internal void serverInitialize(ServerState* ss, f32 secondsPerFrame, 
                               KgtAllocatorHandle hKgaPermanentParent, 
                               KgtAllocatorHandle hKgaTransientParent, 
                               u64 permanentMemoryBytes, 
                               u64 transientMemoryBytes, u16 port)
{
	kassert(ss->runState == ServerState::RunState::STOP);
	*ss = {};
	ss->kNetServer.port = port;
	ss->secondsPerFrame = secondsPerFrame;
	ss->serverJobTicket = g_kpl->postJob(nullptr, nullptr);
	/* allocate memory for the server */
	void*const permanentMemory = 
		kgtAllocAlloc(hKgaPermanentParent, permanentMemoryBytes);
	void*const transientMemory = 
		kgtAllocAlloc(hKgaTransientParent, transientMemoryBytes);
	// initialize dynamic allocators //
	ss->hKalPermanent = kgtAllocInit(
		KgtAllocatorType::GENERAL, permanentMemory, permanentMemoryBytes);
	ss->hKalTransient = kgtAllocInit(
		KgtAllocatorType::GENERAL, transientMemory, transientMemoryBytes);
	// construct a linear frame allocator //
	{
		const size_t kalFrameSize = kmath::megabytes(1);
		void*const kalFrameStartAddress = 
			kgtAllocAlloc(ss->hKalPermanent, kalFrameSize);
		ss->hKalFrame = kgtAllocInit(
			KgtAllocatorType::LINEAR, kalFrameStartAddress, kalFrameSize);
	}
	// Contruct/Initialize the server's AssetManager //
	ss->assetManager = kgtAssetManagerConstruct(
		ss->hKalPermanent, KGT_ASSET_COUNT,
		/* pass nullptr for the kRenderBackend to ensure that the 
		   server doesn't try to load any assets onto the GPU, 
		   since there's no point. */
		ss->hKalTransient, nullptr);
}
internal JOB_QUEUE_FUNCTION(serverUpdate)
{
	KLOG(INFO, "Server job START!");
	ServerState* ss = reinterpret_cast<ServerState*>(data);
	if(!ss->serverStartCausedByRestart)
	{
		if(!kgtNetServerStart(&ss->kNetServer, ss->hKalPermanent, 4))
		{
			KLOG(ERROR, "Failed to start server on port %i!", 
			     ss->kNetServer.port);
			ss->runState = ServerState::RunState::STOP;
			return;
		}
		for(size_t a = 0; a < CARRAY_SIZE(ss->actors); a++)
		{
			ss->actors[a].clientId = kgtNet::SERVER_INVALID_CLIENT_ID;
		}
	}
	defer(
		if(ss->runState != ServerState::RunState::RESTART)
			kgtNetServerStop(&ss->kNetServer)
	);
	while(ss->runState == ServerState::RunState::RUN)
	{
		const PlatformTimeStamp timeStampFrameStart = g_kpl->getTimeStamp();
		kgtAllocReset(ss->hKalFrame);
		kgtNetServerStep(&ss->kNetServer, ss->secondsPerFrame, 
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
		if(ss->runState == ServerState::RunState::RUN)
			return ServerOperatingState::RUNNING;
		else
			return ServerOperatingState::STOPPING;
	}
	return ServerOperatingState::STOPPED;
}
internal void serverStart(ServerState* ss)
{
	if(ss->runState == ServerState::RunState::RUN 
		|| g_kpl->jobValid(&ss->serverJobTicket))
	{
		KLOG(WARNING, "Server already running or is stopping!");
		return;
	}
	ss->serverStartCausedByRestart = 
		(ss->runState == ServerState::RunState::RESTART);
	ss->runState = ServerState::RunState::RUN;
	ss->serverJobTicket = g_kpl->postJob(serverUpdate, ss);
}
internal void serverStop(ServerState* ss)
{
	if(ss->runState == ServerState::RunState::STOP)
	{
		KLOG(WARNING, "Server already stopped!");
		return;
	}
	ss->runState = ServerState::RunState::STOP;
}
internal void serverOnPreUnload(ServerState* ss)
{
	if(g_kpl->jobValid(&ss->serverJobTicket))
	{
		KLOG(INFO, "serverOnPreUnload: temporarily stopping server...");
		ss->runState = ServerState::RunState::RESTART;
	}
}
internal void serverOnReload(ServerState* ss)
{
	if(ss->runState == ServerState::RunState::RESTART)
	{
		KLOG(INFO, "Restarting server job...");
		serverStart(ss);
	}
}
