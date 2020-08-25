#include "kNetClient.h"
internal bool kNetClientIsDisconnected(const KNetClient* knc)
{
	return knc->socket == KPL_INVALID_SOCKET_INDEX;
}
internal void kNetClientConnect(KNetClient* knc, 
                                const char* cStrServerAddress)
{
	knc->addressServer = g_kpl->netResolveAddress(cStrServerAddress);
	knc->socket = g_kpl->socketOpenUdp(0);
	knc->connectionState = network::ConnectionState::ACCEPTING;
	knc->secondsSinceLastServerPacket = 0;
	/* when the client connects to the server, initially there are ZERO reliable 
		messages queued to be sent to us, so we can safely set this value to 
		zero */
	knc->latestReceivedReliableMessageIndex = 0;
}
internal void kNetClientBeginDisconnect(KNetClient* knc)
{
	knc->connectionState = network::ConnectionState::DISCONNECTING;
	knc->secondsSinceLastServerPacket = 0;
}
internal void kNetClientDropConnection(KNetClient* knc)
{
	g_kpl->socketClose(knc->socket);
	knc->socket = KPL_INVALID_SOCKET_INDEX;
}
internal void kNetClientStep(
	KNetClient* knc, f32 deltaSeconds, f32 netReceiveSeconds, 
	fnSig_kNetClientWriteState* fnWriteState,
	fnSig_kNetClientReadServerState* fnReadServerState, 
	fnSig_kNetClientReadReliableMessage* fnReadReliableMessage)
{
	if(kNetClientIsDisconnected(knc))
	/* there's no need to perform any network logic if the client isn't even 
		virtually connected to the server */
	{
		return;
	}
	/* process CLIENT => SERVER communication */
	{
		u8* dataCursor = knc->packetBuffer;
		const u8*const kncPacketBufferEnd = 
			knc->packetBuffer + CARRAY_SIZE(knc->packetBuffer);
		switch(knc->connectionState)
		{
			case network::ConnectionState::DISCONNECTING:{
				/* send disconnect packets to the server until we receive a 
					disconnect aknowledgement from the server */
				*(dataCursor++) = static_cast<u8>(
					network::PacketType::CLIENT_DISCONNECT_REQUEST);
			}break;
			case network::ConnectionState::ACCEPTING:{
				/* send connection requests until we receive server state */
				*(dataCursor++) = static_cast<u8>(
					network::PacketType::CLIENT_CONNECT_REQUEST);
			}break;
			case network::ConnectionState::CONNECTED:{
				/* send unreliable client state every frame */
				kutil::netPack(static_cast<u8>(
				               network::PacketType::CLIENT_UNRELIABLE_STATE), 
				               &dataCursor, kncPacketBufferEnd);
				kutil::netPack(knc->rollingUnreliableStateIndex, 
				               &dataCursor, kncPacketBufferEnd);
				knc->rollingUnreliableStateIndex++;
				kutil::netPack(knc->latestServerTimestamp, 
				               &dataCursor, kncPacketBufferEnd);
				kutil::netPack(knc->latestReceivedReliableMessageIndex, 
				               &dataCursor, kncPacketBufferEnd);
				const u32 remainingPacketBufferSize = 
					kmath::safeTruncateU32(kncPacketBufferEnd - dataCursor);
				dataCursor += 
					fnWriteState(dataCursor, kncPacketBufferEnd);
			}break;
		}
		/* attempt to send the unreliable packet */
		{
			const size_t packetSize = 
				kmath::safeTruncateU64(dataCursor - knc->packetBuffer);
			kassert(packetSize <= CARRAY_SIZE(knc->packetBuffer));
			const i32 bytesSent = 
				g_kpl->socketSend(knc->socket, knc->packetBuffer, packetSize, 
				                  knc->addressServer, 30942);
			kassert(bytesSent >= 0);
		}
		/* if we're connected to the server & we have reliable messages to send, 
			attempt to send everything in our queue of reliable messages to the 
			server */
		if(knc->connectionState == network::ConnectionState::CONNECTED
			&& knc->reliableDataBuffer.messageCount > 0)
		{
			dataCursor = knc->packetBuffer;
			u32 packetSize = 
				kutil::netPack(static_cast<u8>(network::PacketType
				                   ::CLIENT_RELIABLE_MESSAGE_BUFFER), 
				               &dataCursor, kncPacketBufferEnd);
			packetSize += 
				kNetReliableDataBufferNetPack(
					&knc->reliableDataBuffer, &dataCursor, kncPacketBufferEnd);
			/* send the packetBuffer which now contains the reliable messages */
			kassert(packetSize <= CARRAY_SIZE(knc->packetBuffer));
			const i32 bytesSent = 
				g_kpl->socketSend(knc->socket, knc->packetBuffer, packetSize, 
				                  knc->addressServer, 30942);
			kassert(bytesSent >= 0);
		}
	}
	/* process CLIENT <= SERVER communication */
	knc->secondsSinceLastServerPacket += deltaSeconds;
	if(knc->secondsSinceLastServerPacket >= 
		network::VIRTUAL_CONNECTION_TIMEOUT_SECONDS)
	/* if the client has timed out, there's no point in continuing since there 
		is no more virtual connection to the server */
	{
		KLOG(INFO, "CLIENT: server connection timed out!");
		g_kpl->socketClose(knc->socket);
		knc->socket = KPL_INVALID_SOCKET_INDEX;
		knc->connectionState = 
			network::ConnectionState::DISCONNECTING;
		return;
	}
	/* since the virtual net connection is still live, look for packets being 
		sent from the server & process them */
	const PlatformTimeStamp timeStampNetReceive = g_kpl->getTimeStamp();
	do
	/* attempt to receive a server datagram at LEAST once per frame */
	{
		KplNetAddress netAddress;
		u16 netPort;
		const i32 bytesReceived = 
			g_kpl->socketReceive(knc->socket, 
			                     knc->packetBuffer, 
			                     CARRAY_SIZE(knc->packetBuffer), 
			                     &netAddress, &netPort);
		kassert(bytesReceived >= 0);
		if(bytesReceived == 0)
		/* there's no more data being sent to us; we're done. */
		{
			break;
		}
		const u8*      packetBuffer    = knc->packetBuffer;
		const u8*const packetBufferEnd = knc->packetBuffer + bytesReceived;
		u32 bytesUnpacked = 0;
		if(netAddress == knc->addressServer && netPort == 30942)
		{
			const network::PacketType packetType = 
				network::PacketType(*(packetBuffer++));
			bytesUnpacked++;
			switch(packetType)
			{
				case network::PacketType::SERVER_ACCEPT_CONNECTION:{
					if(knc->connectionState != 
						network::ConnectionState::ACCEPTING)
					{
						break;
					}
					bytesUnpacked += 
						kutil::netUnpack(&knc->id, &packetBuffer, 
						                 packetBufferEnd);
					knc->connectionState = 
						network::ConnectionState::CONNECTED;
					knc->secondsSinceLastServerPacket = 0;
					knc->reliableDataBuffer.frontMessageRollingIndex = 1;
					KLOG(INFO, "CLIENT: connected!");
				}break;
				case network::PacketType::SERVER_REJECT_CONNECTION:{
					KLOG(INFO, "CLIENT: rejected by server!");
					g_kpl->socketClose(knc->socket);
					knc->socket = KPL_INVALID_SOCKET_INDEX;
				}break;
				case network::PacketType::SERVER_UNRELIABLE_STATE:{
					if(knc->connectionState != 
						network::ConnectionState::CONNECTED)
					{
						break;
					}
					u32 rollingUnreliableStateIndexServer;
					bytesUnpacked += 
						kutil::netUnpack(&rollingUnreliableStateIndexServer, 
						                 &packetBuffer, packetBufferEnd);
					if(rollingUnreliableStateIndexServer <= 
						knc->rollingUnreliableStateIndexServer)
					/* drop/ignore old/duplicate server state packets! */
					{
						break;
					}
					knc->rollingUnreliableStateIndexServer = 
						rollingUnreliableStateIndexServer;
					bytesUnpacked += 
						kutil::netUnpack(&knc->latestServerTimestamp, 
						                 &packetBuffer, packetBufferEnd);
					bytesUnpacked += 
						kutil::netUnpack(&knc->serverReportedRoundTripTime, 
						                 &packetBuffer, packetBufferEnd);
					u32 serverReportedReliableMessageRollingIndex;
					bytesUnpacked += 
						kutil::netUnpack(
							&serverReportedReliableMessageRollingIndex, 
							&packetBuffer, packetBufferEnd);
					kNetReliableDataBufferDequeue(
						&knc->reliableDataBuffer, 
						serverReportedReliableMessageRollingIndex);
//					KLOG(INFO, "CLIENT: got server state!");
					fnReadServerState(packetBuffer, packetBufferEnd);
					knc->secondsSinceLastServerPacket = 0;
				}break;
				case network::PacketType::SERVER_RELIABLE_MESSAGE_BUFFER:{
					if(knc->connectionState != 
						network::ConnectionState::CONNECTED)
					/* we aren't even connected; ignore. */
					{
						break;
					}
					u32 frontMessageRollingIndex;
					u16 reliableMessageCount;
					bytesUnpacked += 
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
						knc->latestReceivedReliableMessageIndex)
					/* we have already read all these reliable messages, so we 
						don't have to continue */
					{
						break;
					}
//					KLOG(INFO, "CLIENT: new SERVER_RELIABLE_MESSAGE_BUFFER");
					/* at this point, we know that the client's server rolling 
						index MUST lie in the range of [front - 1, last) if 
						we've been reliably reading all the messages so far */
					kassert(knc->latestReceivedReliableMessageIndex >= 
					            static_cast<i64>(frontMessageRollingIndex) - 1);
					/* we must iterate over the reliable messages until we get 
						to a rolling index greater than the client's server 
						rolling index */
					for(u32 rmi = frontMessageRollingIndex; 
						rmi < frontMessageRollingIndex + reliableMessageCount; 
						rmi++)
					{
						u16 reliableMessageBytes; 
						const u32 reliableMessageBytesBytesUnpacked = 
							kutil::netUnpack(&reliableMessageBytes, 
							                 &packetBuffer, packetBufferEnd);
						if(reliableMessageBytes == 0)
						{
							KLOG(ERROR, "CLIENT: empty reliable message "
							     "received!  This should probably never "
							     "happen...");
						}
						if(rmi <= knc->latestReceivedReliableMessageIndex)
						{
							packetBuffer += reliableMessageBytes;
							continue;
						}
						const u32 bytesRead = 
							fnReadReliableMessage(
								packetBuffer, 
								packetBuffer + reliableMessageBytes);
						kassert(bytesRead == reliableMessageBytes);
						packetBuffer += reliableMessageBytes;
					}
					/* record the last reliable rolling index we have 
						successfully received from the server */
					knc->latestReceivedReliableMessageIndex = 
						lastMessageRollingIndex;
				}break;
				case network::PacketType::SERVER_DISCONNECT:{
					KLOG(INFO, "CLIENT: disconnected from server!");
					g_kpl->socketClose(knc->socket);
					knc->socket = KPL_INVALID_SOCKET_INDEX;
				}break;
				case network::PacketType::CLIENT_CONNECT_REQUEST:
				case network::PacketType::CLIENT_UNRELIABLE_STATE:
				case network::PacketType::CLIENT_RELIABLE_MESSAGE_BUFFER:
				case network::PacketType::CLIENT_DISCONNECT_REQUEST:
				default:{
					KLOG(ERROR, "CLIENT: invalid packet!");
				}break;
			}
		}
		/* @robustness: assert that the packetBuffer is at the exact position of 
			knc->packetBuffer + bytesReceived to ensure that we completely read 
			all the data */
	}while(g_kpl->secondsSinceTimeStamp(timeStampNetReceive) < 
	           netReceiveSeconds
	       && !kNetClientIsDisconnected(knc));
}
internal void kNetClientQueueReliableMessage(
	KNetClient* knc, const u8* netPackedData, u16 netPackedDataBytes)
{
	kNetReliableDataBufferQueueMessage(&knc->reliableDataBuffer, netPackedData, 
	                                   netPackedDataBytes);
}