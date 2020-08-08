#include "serverExample.h"
#include "KmlGameExample.h"
internal void serverInitialize(ServerState* ss, f32 secondsPerFrame, 
                               KgaHandle hKgaPermanent, 
                               KgaHandle hKgaTransient, 
                               u64 permanentMemoryBytes, 
                               u64 transientMemoryBytes, u16 port)
{
	kassert(!ss->running);
	*ss = {};
	ss->port = port;
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
	KplSocketIndex socket = g_kpl->socketOpenUdp(ss->port);
	if(socket == KPL_INVALID_SOCKET_INDEX)
	{
		KLOG(ERROR, "Failed to open a network socket on port %i!", ss->port);
		ss->running = false;
		return;
	}
	defer(g_kpl->socketClose(socket));
	while(ss->running)
	{
		PlatformTimeStamp timeStampFrameStart = g_kpl->getTimeStamp();
		kalReset(ss->hKalFrame);
		///TODO: keep pulling data from the socket until we get no data, while 
		///	simultaneously staying within ss->secondsPerFrame to prevent the 
		/// server thread from stalling from too much data
		/* check to see if we've gotten any data from the socket */
		u8 netBuffer[KPL_MAX_DATAGRAM_SIZE];
		KplNetAddress netAddressClient;
		u16 netPortClient;
		const i32 dataReceived = 
			g_kpl->socketReceive(socket, netBuffer, CARRAY_SIZE(netBuffer), 
			                     &netAddressClient, &netPortClient);
		kassert(dataReceived >= 0);
		if(dataReceived > 0)
		/* if we've gotten data from the socket, we need to parse the data into 
			`NetPacket`s */
		{
			u8* packetBuffer = netBuffer;
			network::PacketType packetType = 
				network::PacketType(*(packetBuffer++));
			switch(packetType)
			{
				case network::PacketType::CLIENT_CONNECT_REQUEST: {
					KLOG(INFO, "SERVER: received CLIENT_CONNECT_REQUEST");
					/* if this client is already connected, ignore this 
						packet */
					bool clientAlreadyConnected = false;
					for(size_t c = 0; c < CARRAY_SIZE(ss->clients); c++)
					{
						if(ss->clients[c].netAddress == netAddressClient
							&& ss->clients[c].netPort == netPortClient)
						{
							clientAlreadyConnected = true;
							break;
						}
					}
					if(clientAlreadyConnected)
					{
						break;
					}
					/* search for an unoccupied client slot on the server to 
						place this new client */
					size_t openClientIndex = 0;
					for(; openClientIndex < CARRAY_SIZE(ss->clients); 
						openClientIndex++)
					{
						if(ss->clients[openClientIndex].connectionState == 
							network::ConnectionState::NOT_CONNECTED)
						{
							break;
						}
					}
					if(openClientIndex >= CARRAY_SIZE(ss->clients))
					/* send connection rejected packet */
					{
						kassert(!"TODO");
					}
					/* add this new client to the unoccupied client slot */
					ss->clients[openClientIndex].netAddress = netAddressClient;
					ss->clients[openClientIndex].netPort    = netPortClient;
					ss->clients[openClientIndex].connectionState = 
						network::ConnectionState::ACCEPTING;
					ss->clients[openClientIndex].timeSinceLastPacket = 0;
				}break;
				case network::PacketType::CLIENT_STATE: {
					KLOG(INFO, "SERVER: received CLIENT_STATE");
				}break;
				case network::PacketType::SERVER_ACCEPT_CONNECTION: 
				case network::PacketType::SERVER_STATE: 
				default:{
					KLOG(ERROR, "SERVER: received invalid packet type (%i)!", 
					     static_cast<i32>(packetType));
				}break;
			}
#if 0
			local_persist const size_t TEST_PACKET_SIZE = 
				sizeof(i32) + sizeof(i16);
			kassert(dataReceived == TEST_PACKET_SIZE);
			u8* packetBuffer = netBuffer;
			i32 clientTestInt = 
				kutil::netUnpackI32(&packetBuffer, netBuffer, 
				                    kmath::safeTruncateU32(dataReceived));
			i32 clientTestShort = 
				kutil::netUnpackI16(&packetBuffer, netBuffer, 
				                    kmath::safeTruncateU32(dataReceived));
			kassert(packetBuffer - netBuffer == TEST_PACKET_SIZE);
			/* as a diagnostic, log the contents of the packet we just 
				received */
			KLOG(INFO, "clientTestInt=%i", clientTestInt);
			KLOG(INFO, "clientTestShort=%i", clientTestShort);
#endif // 0
		}
		for(size_t c = 0; c < CARRAY_SIZE(ss->clients); c++)
		/* process connected clients */
		{
			switch(ss->clients[c].connectionState)
			{
				case network::ConnectionState::NOT_CONNECTED:
					continue;
				case network::ConnectionState::ACCEPTING: {
					netBuffer[0] = static_cast<u8>(
						network::PacketType::SERVER_ACCEPT_CONNECTION);
					const i32 bytesSent = 
						g_kpl->socketSend(socket, netBuffer, 1, 
						                  ss->clients[c].netAddress, 
						                  ss->clients[c].netPort);
					kassert(bytesSent >= 0);
				}break;
				case network::ConnectionState::CONNECTED: {
					netBuffer[0] = static_cast<u8>(
						network::PacketType::SERVER_STATE);
					const i32 bytesSent = 
						g_kpl->socketSend(socket, netBuffer, 1, 
						                  ss->clients[c].netAddress, 
						                  ss->clients[c].netPort);
					kassert(bytesSent >= 0);
				}break;
			}
		}
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