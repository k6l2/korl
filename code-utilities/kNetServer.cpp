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
internal void kNetServerStep(
	KNetServer* kns, f32 deltaSeconds, f32 netReceiveSeconds, 
	const PlatformTimeStamp& timeStampFrameStart, 
	fnSig_kNetServerReadClientState* fnReadClientState, 
	fnSig_kNetServerWriteState* fnWriteState, 
	fnSig_kNetServerOnClientConnect* fnOnClientConnect, 
	fnSig_kNetServerOnClientDisconnect* fnOnClientDisconnect, 
	fnSig_kNetServerReadReliableMessage* fnReadReliableMessage, 
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
		const u8*      packetBuffer    = netBuffer;
		const u8*const packetBufferEnd = netBuffer + bytesReceived;
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
					, .rollingUnreliableStateIndex        = 0
					, .roundTripTime                      = 0
					, .latestReceivedReliableMessageIndex = 0
				};
				arrput(kns->clientArray, newClient);
			}break;
			case network::PacketType::CLIENT_UNRELIABLE_STATE: {
				u32 bytesUnpacked = 0;
//				KLOG(INFO, "SERVER: received CLIENT_UNRELIABLE_STATE");
				if(clientIndex >= arrcap(kns->clientArray))
				/* received client state for someone who isn't even connected */
				{
					break;
				}
				if(kns->clientArray[clientIndex].connectionState == 
					network::ConnectionState::DISCONNECTING)
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
				/* check the unreliable packet index, and drop this packet if we 
					find that it's old/duplicated data */
				u32 rollingUnreliableStateIndex;
				bytesUnpacked += 
					kutil::netUnpack(&rollingUnreliableStateIndex, 
					                 &packetBuffer, packetBufferEnd);
				if(kns->clientArray[clientIndex].rollingUnreliableStateIndex >= 
					rollingUnreliableStateIndex)
				{
					break;
				}
				kns->clientArray[clientIndex].rollingUnreliableStateIndex = 
					rollingUnreliableStateIndex;
				/* we can calculate the client's round-trip-time here using the 
					client's last known server timestamp */
				PlatformTimeStamp clientLastKnownServerTimestamp;
				bytesUnpacked += 
					kutil::netUnpack(&clientLastKnownServerTimestamp, 
					                 &packetBuffer, packetBufferEnd);
				kns->clientArray[clientIndex].roundTripTime = 
					g_kpl->secondsSinceTimeStamp(
						clientLastKnownServerTimestamp);
				if(kns->clientArray[clientIndex].roundTripTime > 
					network::VIRTUAL_CONNECTION_TIMEOUT_SECONDS)
				/* there's no meaning in huge round trip times, since the client 
					would technically be dropped from the connection anyways */
				{
					kns->clientArray[clientIndex].roundTripTime = 
						network::VIRTUAL_CONNECTION_TIMEOUT_SECONDS;
				}
//				KLOG(INFO, "SERVER: client (cid==%i) rtt=%fms", 
//				     kns->clientArray[clientIndex].id, 
//				     kns->clientArray[clientIndex].roundTripTime * 1000);
				/* dequeue reliable messages based on the reliable message index 
					reported to have been received by the client */
				u32 clientReportedReliableMessageRollingIndex;
				bytesUnpacked += 
					kutil::netUnpack(
						&clientReportedReliableMessageRollingIndex, 
						&packetBuffer, packetBufferEnd);
				kNetReliableDataBufferDequeue(
					&(kns->clientArray[clientIndex].reliableDataBuffer), 
					clientReportedReliableMessageRollingIndex);
				/* if the client is connected, update server state based on 
					this client's data */
				fnReadClientState(kns->clientArray[clientIndex].id, 
				                  packetBuffer, packetBufferEnd, userPointer);
				kns->clientArray[clientIndex].timeSinceLastPacket = 0;
			}break;
			case network::PacketType::CLIENT_RELIABLE_MESSAGE_BUFFER: {
				if(clientIndex >= arrcap(kns->clientArray))
				/* client isn't even connected; ignore. */
				{
					break;
				}
				if(kns->clientArray[clientIndex].connectionState != 
					network::ConnectionState::CONNECTED)
				/* only handle reliable data for CONNECTED clients */
				{
					break;
				}
				u32 frontMessageRollingIndex;
				u16 reliableMessageCount;
				const u32 bytesUnpacked = 
					kNetReliableDataBufferUnpackMeta(
						&packetBuffer, packetBufferEnd, 
						&frontMessageRollingIndex, &reliableMessageCount);
				/* using this information, we can determine if we need to:
					- discard the packet
					- read a subset of the messages
					- read ALL the messages */
				const u32 lastMessageRollingIndex = 
					frontMessageRollingIndex + reliableMessageCount - 1;
				if(lastMessageRollingIndex <= 
						kns->clientArray[clientIndex]
							.latestReceivedReliableMessageIndex)
				/* we have already read all these reliable messages, so we don't 
					have to continue */
				{
					break;
				}
#if INTERNAL_BUILD && 0
				if(kns->clientArray[clientIndex].debugTestDropReliableData)
				/* test the reliable packet system a bit by purposely dropping 
					reliable data */
				{
					kns->clientArray[clientIndex].debugTestDropReliableData--;
					break;
				}
				kns->clientArray[clientIndex].debugTestDropReliableData = 1000;
#endif// INTERNAL_BUILD
//				KLOG(INFO, "SERVER: new CLIENT_RELIABLE_MESSAGE_BUFFER");
				/* at this point, we know that the server client's rolling index 
					MUST lie in the range of [front - 1, last) if we've been 
					reliably reading all the messages so far */
				kassert(kns->clientArray[clientIndex]
				        .latestReceivedReliableMessageIndex >= 
				            static_cast<i64>(frontMessageRollingIndex) - 1);
				/* we must iterate over the reliable messages until we get to a 
					rolling index greater than the server client's rolling 
					index */
				for(u32 rmi = frontMessageRollingIndex; 
					rmi < frontMessageRollingIndex + reliableMessageCount; 
					rmi++)
				{
					u16 reliableMessageBytes; 
					const u32 reliableMessageBytesBytesUnpacked = 
						kutil::netUnpack(&reliableMessageBytes, &packetBuffer, 
						                 packetBufferEnd);
					if(reliableMessageBytes == 0)
					{
						KLOG(ERROR, "SERVER: empty reliable message received!  "
						     "This should probably never happen...");
					}
					if(rmi <= kns->clientArray[clientIndex]
				                  .latestReceivedReliableMessageIndex)
					{
						packetBuffer += reliableMessageBytes;
						continue;
					}
					const u32 bytesRead = 
						fnReadReliableMessage(
							kns->clientArray[clientIndex].id, packetBuffer, 
							packetBuffer + reliableMessageBytes, userPointer);
					kassert(bytesRead == reliableMessageBytes);
					packetBuffer += reliableMessageBytes;
				}
				/* record the last reliable rolling index we have successfully 
					received from the client */
				kns->clientArray[clientIndex]
					.latestReceivedReliableMessageIndex = 
						lastMessageRollingIndex;
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
					network::ConnectionState::DISCONNECTING)
				{
					KLOG(INFO, "SERVER: received CLIENT_DISCONNECT_REQUEST "
					     "(cid=%i)", kns->clientArray[clientIndex].id);
					fnOnClientDisconnect(kns->clientArray[clientIndex].id, 
					                     userPointer);
					kns->clientArray[clientIndex].connectionState = 
						network::ConnectionState::DISCONNECTING;
					kns->clientArray[clientIndex].timeSinceLastPacket = 0;
				}
			}break;
			/* packets that are supposed to be sent FROM a server should 
				NEVER be sent TO a server */
			case network::PacketType::SERVER_ACCEPT_CONNECTION: 
			case network::PacketType::SERVER_REJECT_CONNECTION: 
			case network::PacketType::SERVER_UNRELIABLE_STATE: 
			case network::PacketType::SERVER_RELIABLE_MESSAGE_BUFFER: 
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
				network::ConnectionState::DISCONNECTING)
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
				network::ConnectionState::DISCONNECTING;
			continue;
		}
		switch(kns->clientArray[c].connectionState)
		{
			case network::ConnectionState::DISCONNECTING: {
				netBuffer[0] = static_cast<u8>(
					network::PacketType::SERVER_DISCONNECT);
				const i32 bytesSent = 
					g_kpl->socketSend(kns->socket, netBuffer, 1, 
					                  kns->clientArray[c].netAddress, 
					                  kns->clientArray[c].netPort);
				kassert(bytesSent >= 0);
			}break;
			case network::ConnectionState::ACCEPTING: {
				u8*            packetBuffer    = netBuffer;
				const u8*const packetBufferEnd = 
					netBuffer + CARRAY_SIZE(netBuffer);
				kutil::netPack(static_cast<u8>(
					network::PacketType::SERVER_ACCEPT_CONNECTION), 
					&packetBuffer, packetBufferEnd);
				kutil::netPack(kns->clientArray[c].id, &packetBuffer, 
				               packetBufferEnd);
				const size_t packetSize = kmath::safeTruncateU64(
					packetBuffer - netBuffer);
				const i32 bytesSent = 
					g_kpl->socketSend(kns->socket, netBuffer, packetSize, 
					                  kns->clientArray[c].netAddress, 
					                  kns->clientArray[c].netPort);
				kassert(bytesSent >= 0);
			}break;
			case network::ConnectionState::CONNECTED: {
				/* send the server's unreliable state each frame */
				u8*            packetBuffer    = netBuffer;
				const u8*const packetBufferEnd = 
					netBuffer + CARRAY_SIZE(netBuffer);
				kutil::netPack(static_cast<u8>(
					network::PacketType::SERVER_UNRELIABLE_STATE), 
					&packetBuffer, packetBufferEnd);
				kutil::netPack(kns->rollingUnreliableStateIndex, &packetBuffer, 
				               packetBufferEnd);
				kutil::netPack(g_kpl->getTimeStamp(), &packetBuffer, 
				               packetBufferEnd);
				kutil::netPack(kns->clientArray[c].roundTripTime, &packetBuffer, 
				               packetBufferEnd);
				kutil::netPack(kns->clientArray[c]
					.latestReceivedReliableMessageIndex,
					&packetBuffer, packetBufferEnd);
				packetBuffer += 
					fnWriteState(packetBuffer, packetBufferEnd, userPointer);
				const size_t packetSize = kmath::safeTruncateU64(
					packetBuffer - netBuffer);
				const i32 bytesSent = 
					g_kpl->socketSend(kns->socket, netBuffer, packetSize, 
					                  kns->clientArray[c].netAddress, 
					                  kns->clientArray[c].netPort);
				kassert(bytesSent >= 0);
				/* if this client has reliable messages queued up, then we need 
					to attempt to send a reliable datagram */
				if(kns->clientArray[c].reliableDataBuffer.messageCount > 0)
				{
					packetBuffer = netBuffer;
					u32 reliablePacketSize = 
						kutil::netPack(static_cast<u8>(network::PacketType
						                   ::SERVER_RELIABLE_MESSAGE_BUFFER), 
						               &packetBuffer, packetBufferEnd);
					reliablePacketSize += 
						kNetReliableDataBufferNetPack(
							&kns->clientArray[c].reliableDataBuffer, 
							&packetBuffer, packetBufferEnd);
					/* send the packetBuffer which now contains the reliable 
						messages */
					kassert(reliablePacketSize <= CARRAY_SIZE(netBuffer));
					const i32 reliableBytesSent = 
						g_kpl->socketSend(kns->socket, netBuffer, 
						                  reliablePacketSize, 
						                  kns->clientArray[c].netAddress, 
						                  kns->clientArray[c].netPort);
					kassert(reliableBytesSent >= 0);
				}
			}break;
		}
	}
	kns->rollingUnreliableStateIndex++;
	/* cleanup clients which were disconnected by deleting them from the dynamic 
		client array */
	for(size_t c = arrlenu(kns->clientArray) - 1; c < arrlenu(kns->clientArray); 
		c--)
	{
		if(kns->clientArray[c].netAddress == KPL_INVALID_ADDRESS)
			arrdel(kns->clientArray, c);
	}
}
internal void kNetServerQueueReliableMessage(
	KNetServer* kns, const u8* netPackedData, u16 netPackedDataBytes, 
	network::ServerClientId ignoreClientId)
{
	for(size_t c = 0; c < arrlenu(kns->clientArray); c++)
	{
		if(kns->clientArray[c].id == ignoreClientId)
			continue;
		if(kns->clientArray[c].connectionState != 
				network::ConnectionState::CONNECTED)
			continue;
		kNetReliableDataBufferQueueMessage(
			&kns->clientArray[c].reliableDataBuffer, netPackedData, 
			netPackedDataBytes);
	}
}