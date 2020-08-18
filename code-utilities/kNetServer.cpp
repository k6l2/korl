#include "kNetServer.h"
internal bool kNetServerStart(KNetServer* kns, KgaHandle hKga, u8 maxClients)
{
	kassert(kns->socket == KPL_INVALID_SOCKET_INDEX);
	kns->socket = g_kpl->socketOpenUdp(kns->port);
	if(kns->socket == KPL_INVALID_SOCKET_INDEX)
	{
		KLOG(ERROR, "Failed to open a network socket on port %i!", kns->port);
		return false;
	}
	kns->clientArray = reinterpret_cast<KNetServerClientEntry*>(
		arrinit(sizeof(KNetServerClientEntry), hKga, maxClients));
	return true;
}
internal void kNetServerStop(KNetServer* kns)
{
	kassert(kns->socket != KPL_INVALID_SOCKET_INDEX);
	g_kpl->socketClose(kns->socket);
	kns->socket = KPL_INVALID_SOCKET_INDEX;
	arrfree(kns->clientArray);
}
internal void kNetServerStep(KNetServer* kns, f32 deltaSeconds, 
                       f32 netReceiveSeconds, 
                       const PlatformTimeStamp& timeStampFrameStart, 
                       fnSig_kNetServerReadClientState* fnReadClientState, 
                       fnSig_kNetServerWriteState* fnWriteState, 
                       fnSig_kNetServerOnClientConnect* fnOnClientConnect, 
                       fnSig_kNetServerOnClientDisconnect* fnOnClientDisconnect, 
                       void* userPointer)
{
	u8 netBuffer[KPL_MAX_DATAGRAM_SIZE];
	do
	/* attempt to receive a network datagram at LEAST once per frame */
	{
		/* check to see if we've gotten any data from the socket */
		KplNetAddress netAddressClient;
		u16 netPortClient;
		const i32 bytesReceived = 
			g_kpl->socketReceive(kns->socket, netBuffer, CARRAY_SIZE(netBuffer), 
			                     &netAddressClient, &netPortClient);
		kassert(bytesReceived >= 0);
		if(bytesReceived == 0)
			/* if the server socket has no more data on it, we're done with 
				network data for this frame */
			break;
		/* match the address::port in the list of clients */
		size_t clientIndex = arrcap(kns->clientArray);
		for(size_t c = 0; c < arrlenu(kns->clientArray); c++)
		{
			if(kns->clientArray[c].netAddress == netAddressClient
				&& kns->clientArray[c].netPort == netPortClient)
			{
				clientIndex = c;
				break;
			}
		}
		/* since we've gotten data from the socket, we need to parse the 
			data into `NetPacket`s */
		const u8* packetBuffer = netBuffer;
		const network::PacketType packetType = 
			network::PacketType(*(packetBuffer++));
		switch(packetType)
		{
			case network::PacketType::CLIENT_CONNECT_REQUEST: {
				KLOG(INFO, "SERVER: received CLIENT_CONNECT_REQUEST");
				/* if this client is already connected, ignore this packet */
				const bool clientAlreadyConnected = 
					clientIndex < arrcap(kns->clientArray);
				if(clientAlreadyConnected)
				{
					break;
				}
				if(arrlenu(kns->clientArray) >= arrcap(kns->clientArray))
				/* send connection rejected packet if we're at capacity */
				{
					netBuffer[0] = static_cast<u8>(
						network::PacketType::SERVER_REJECT_CONNECTION);
					const i32 bytesSent = 
						g_kpl->socketSend(kns->socket, netBuffer, 1, 
						                  netAddressClient, netPortClient);
					kassert(bytesSent >= 0);
					break;
				}
				/* choose a unique id # to identify this client because the 
					client array is dynamic, ergo we cannot reference clients 
					via their clientArray index */
				network::ServerClientId cid = 0;
				for(; cid < network::SERVER_INVALID_CLIENT_ID; cid++)
				/* this loop sucks, but should perform fine for low clientArray 
					capacities so who cares? */
				{
					bool cidTaken = false;
					for(size_t c = 0; c < arrlenu(kns->clientArray); c++)
					{
						if(kns->clientArray[c].id == cid)
						{
							cidTaken = true;
							break;
						}
					}
					if(!cidTaken)
					{
						break;
					}
				}
				kassert(cid < network::SERVER_INVALID_CLIENT_ID);
				/* add this new client to the client array */
				KLOG(INFO, "SERVER: accepting new client (cid==%i)...", cid);
				const KNetServerClientEntry newClient = 
					{ .id                  = cid
					, .netAddress          = netAddressClient
					, .netPort             = netPortClient
					, .timeSinceLastPacket = 0
					, .connectionState     = network::ConnectionState::ACCEPTING
				};
				arrput(kns->clientArray, newClient);
			}break;
			case network::PacketType::CLIENT_STATE: {
//				KLOG(INFO, "SERVER: received CLIENT_STATE");
				if(clientIndex >= arrcap(kns->clientArray))
				/* received client state for someone who isn't even connected */
				{
					break;
				}
				if(kns->clientArray[clientIndex].connectionState == 
					network::ConnectionState::NOT_CONNECTED)
				/* if a client is disconnecting, ignore this packet */
				{
					break;
				}
				if(kns->clientArray[clientIndex].connectionState ==
					network::ConnectionState::ACCEPTING)
				{
					kns->clientArray[clientIndex].connectionState =
						network::ConnectionState::CONNECTED;
					fnOnClientConnect(kns->clientArray[clientIndex].id, 
					                  userPointer);
					KLOG(INFO, "SERVER: new client connected! cid==%i", 
					     kns->clientArray[clientIndex].id);
				}
				/* if the client is connected, update server state based on 
					this client's data */
				const u32 packetBufferSize = 
					kmath::safeTruncateU32(bytesReceived - 1);
				fnReadClientState(kns->clientArray[clientIndex].id, 
				                  packetBuffer, packetBufferSize, userPointer);
				kns->clientArray[clientIndex].timeSinceLastPacket = 0;
			}break;
			case network::PacketType::CLIENT_DISCONNECT_REQUEST: {
				if(clientIndex >= arrcap(kns->clientArray))
				/* client isn't even connected; ignore. */
				{
					break;
				}
				/* mark client as `disconnecting` so we can send disconnect 
					packets for a certain period of time */
				if(kns->clientArray[clientIndex].connectionState != 
					network::ConnectionState::NOT_CONNECTED)
				{
					KLOG(INFO, "SERVER: received CLIENT_DISCONNECT_REQUEST "
					     "(cid=%i)", kns->clientArray[clientIndex].id);
					fnOnClientDisconnect(kns->clientArray[clientIndex].id, 
					                     userPointer);
					kns->clientArray[clientIndex].connectionState = 
						network::ConnectionState::NOT_CONNECTED;
					kns->clientArray[clientIndex].timeSinceLastPacket = 0;
				}
			}break;
			/* packets that are supposed to be sent FROM a server should 
				NEVER be sent TO a server */
			case network::PacketType::SERVER_ACCEPT_CONNECTION: 
			case network::PacketType::SERVER_REJECT_CONNECTION: 
			case network::PacketType::SERVER_STATE: 
			case network::PacketType::SERVER_DISCONNECT: 
			default:{
				KLOG(ERROR, "SERVER: received invalid packet type (%i)!", 
				     static_cast<i32>(packetType));
			}break;
		}
	}while(g_kpl->secondsSinceTimeStamp(timeStampFrameStart) < 
	           netReceiveSeconds);
	for(size_t c = 0; c < arrlenu(kns->clientArray); c++)
	/* process connected clients */
	{
		kassert(kns->clientArray[c].netAddress != KPL_INVALID_ADDRESS);
		kns->clientArray[c].timeSinceLastPacket += deltaSeconds;
		if(kns->clientArray[c].timeSinceLastPacket >= 
			network::VIRTUAL_CONNECTION_TIMEOUT_SECONDS)
		/* drop client if we haven't received any packets within the timeout 
			threshold */
		{
			/* send a courtesy disconnect packet before dropping the client 
				completely */
			{
				netBuffer[0] = static_cast<u8>(
					network::PacketType::SERVER_DISCONNECT);
				const i32 bytesSent = 
					g_kpl->socketSend(kns->socket, netBuffer, 1, 
					                  kns->clientArray[c].netAddress, 
					                  kns->clientArray[c].netPort);
				kassert(bytesSent >= 0);
			}
			if(kns->clientArray[c].connectionState != 
				network::ConnectionState::NOT_CONNECTED)
			{
				fnOnClientDisconnect(kns->clientArray[c].id, userPointer);
				KLOG(INFO, "SERVER: client(cid=%i) dropped.", 
				     kns->clientArray[c].id);
			}
			else
			{
				KLOG(INFO, "SERVER: ending disconnected client(cid=%i) comms.", 
				     kns->clientArray[c].id);
			}
			kns->clientArray[c].netAddress      = KPL_INVALID_ADDRESS;
			kns->clientArray[c].netPort         = 0;
			kns->clientArray[c].connectionState = 
				network::ConnectionState::NOT_CONNECTED;
			continue;
		}
		switch(kns->clientArray[c].connectionState)
		{
			case network::ConnectionState::NOT_CONNECTED: {
				netBuffer[0] = static_cast<u8>(
					network::PacketType::SERVER_DISCONNECT);
				const i32 bytesSent = 
					g_kpl->socketSend(kns->socket, netBuffer, 1, 
					                  kns->clientArray[c].netAddress, 
					                  kns->clientArray[c].netPort);
				kassert(bytesSent >= 0);
			}break;
			case network::ConnectionState::ACCEPTING: {
				u8* packetBuffer = netBuffer;
				kutil::netPack(static_cast<u8>(
					network::PacketType::SERVER_ACCEPT_CONNECTION), 
					&packetBuffer, netBuffer, CARRAY_SIZE(netBuffer));
				kutil::netPack(kns->clientArray[c].id, &packetBuffer, netBuffer, 
				               CARRAY_SIZE(netBuffer));
				const size_t packetSize = kmath::safeTruncateU64(
					packetBuffer - netBuffer);
				const i32 bytesSent = 
					g_kpl->socketSend(kns->socket, netBuffer, packetSize, 
					                  kns->clientArray[c].netAddress, 
					                  kns->clientArray[c].netPort);
				kassert(bytesSent >= 0);
			}break;
			case network::ConnectionState::CONNECTED: {
				u8* packetBuffer = netBuffer;
				kutil::netPack(
					static_cast<u8>(network::PacketType::SERVER_STATE), 
					&packetBuffer, netBuffer, CARRAY_SIZE(netBuffer));
				packetBuffer += 
					fnWriteState(packetBuffer, CARRAY_SIZE(netBuffer) - 1, 
					             userPointer);
				const size_t packetSize = kmath::safeTruncateU64(
					packetBuffer - netBuffer);
				const i32 bytesSent = 
					g_kpl->socketSend(kns->socket, netBuffer, packetSize, 
					                  kns->clientArray[c].netAddress, 
					                  kns->clientArray[c].netPort);
				kassert(bytesSent >= 0);
			}break;
		}
	}
	/* cleanup clients which were disconnected by deleting them from the dynamic 
		client array */
	for(size_t c = arrlenu(kns->clientArray) - 1; c < arrlenu(kns->clientArray); 
		c--)
	{
		if(kns->clientArray[c].netAddress == KPL_INVALID_ADDRESS)
			arrdel(kns->clientArray, c);
	}
}