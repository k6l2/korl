#include "kNetClient.h"
internal void kNetClientOnPreUnload(KNetClient* knc)
{
	if(knc->socket != KPL_INVALID_SOCKET_INDEX)
	{
		KLOG(INFO, "invalidating socketClient==%i", knc->socket);
		g_kpl->socketClose(knc->socket);
		knc->socket = KPL_INVALID_SOCKET_INDEX;
	}
}
internal bool kNetClientIsDisconnected(const KNetClient* knc)
{
	return knc->socket == KPL_INVALID_SOCKET_INDEX;
}
internal void kNetClientConnect(KNetClient* knc, 
                                const char* cStrServerAddress)
{
	knc->addressServer = 
		g_kpl->netResolveAddress(g_gs->clientAddressBuffer);
	knc->socket = g_kpl->socketOpenUdp(0);
	knc->connectionState = network::ConnectionState::ACCEPTING;
	knc->secondsSinceLastServerPacket = 0;
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
/**
 * @param o_reliableDataBufferCursor 
 *        Return value for the position in the reliable data buffer immediately 
 *        following the last used byte in the circular buffer.  It should be 
 *        safe to start storing the next reliable message at this location.  If 
 *        this value == `nullptr`, then it is unused.
 */
internal u16 kNetClientGetReliableDataBufferUsedBytes(
	KNetClient* knc, u8** o_reliableDataBufferCursor)
{
	u16 reliableDataBufferUsedBytes = 0;
	u8* reliableDataBufferCursor = 
		knc->reliableDataBuffer + knc->reliableDataBufferFrontMessageByteOffset;
	const u8*const reliableDataBufferEnd = 
		knc->reliableDataBuffer + CARRAY_SIZE(knc->reliableDataBuffer);
	for(u16 r = 0; r < knc->reliableDataBufferMessageCount; r++)
	/* for each reliable message, extract its byte length and move the buffer 
		cursor to the next contiguous message location */
	{
		/* extract message byte length */
		u8 messageBytesBuffer[2];
		for(size_t b = 0; b < CARRAY_SIZE(messageBytesBuffer); b++)
		{
			messageBytesBuffer[b] = reliableDataBufferCursor[0];
			reliableDataBufferCursor++;
			if(reliableDataBufferCursor >= reliableDataBufferEnd)
				reliableDataBufferCursor = knc->reliableDataBuffer;
		}
		const u8* messageBytesBufferCursor = messageBytesBuffer;
		const u16 messageByteCount = 
			kutil::netUnpackU16(&messageBytesBufferCursor, messageBytesBuffer, 
			                    CARRAY_SIZE(messageBytesBuffer));
		/* move buffer cursor to next possible message location & add this 
			message size to the total used byte count */
		reliableDataBufferUsedBytes += 
			sizeof(messageByteCount) + messageByteCount;
		reliableDataBufferCursor += messageByteCount;
		if(reliableDataBufferCursor >= reliableDataBufferEnd)
			reliableDataBufferCursor -= CARRAY_SIZE(knc->reliableDataBuffer);
		kassert(reliableDataBufferCursor >= knc->reliableDataBuffer);
		kassert(reliableDataBufferCursor < reliableDataBufferEnd);
	}
	kassert(reliableDataBufferUsedBytes <= 
	            CARRAY_SIZE(knc->reliableDataBuffer));
	if(o_reliableDataBufferCursor)
		*o_reliableDataBufferCursor = reliableDataBufferCursor;
	return reliableDataBufferUsedBytes;
}
internal void kNetClientDequeueReliableMessages(
	KNetClient* knc, u32 serverReportedReliableMessageRollingIndex)
{
	if(knc->reliableDataBufferMessageCount == 0)
	/* ensure that the next reliable message sent to server will have a newer 
		rolling index than what the server is reporting */
	{
		knc->reliableDataBufferFrontMessageRollingIndex = 
			serverReportedReliableMessageRollingIndex + 1;
		return;
	}
	if(serverReportedReliableMessageRollingIndex < 
		knc->reliableDataBufferFrontMessageRollingIndex)
	/* the server has not confirmed receipt of our latest reliable messages, so 
		there is nothing to dequeue */
	{
		return;
	}
	/* remove reliable messages that the server has reported to have obtained so 
		that we stop sending copies to the server */
	kassert(serverReportedReliableMessageRollingIndex < 
	        knc->reliableDataBufferFrontMessageRollingIndex + 
	            knc->reliableDataBufferMessageCount);
	const u8* reliableDataCursor = knc->reliableDataBuffer + 
		knc->reliableDataBufferFrontMessageByteOffset;
	const u8*const reliableDataBufferEnd = 
		knc->reliableDataBuffer + CARRAY_SIZE(knc->reliableDataBuffer);
	for(u32 rmi = knc->reliableDataBufferFrontMessageRollingIndex; 
		rmi <= serverReportedReliableMessageRollingIndex; rmi++)
	{
		/* we can't unpack the message size using netUnpack API because the 
			reliableDataBuffer is circular, so the bytes can wrap around */
		u8 netPackedMessageSizeBuffer[2];
		for(size_t b = 0; b < CARRAY_SIZE(netPackedMessageSizeBuffer); b++)
		{
			netPackedMessageSizeBuffer[b] = reliableDataCursor[0];
			reliableDataCursor++;
			if(reliableDataCursor >= reliableDataBufferEnd)
				reliableDataCursor -= CARRAY_SIZE(knc->reliableDataBuffer);
			kassert(reliableDataCursor >= knc->reliableDataBuffer);
			kassert(reliableDataCursor < reliableDataBufferEnd);
		}
		const u8* netPackedMessageSizeBufferCursor = netPackedMessageSizeBuffer;
		const u16 messageSize = 
			kutil::netUnpackU16(&netPackedMessageSizeBufferCursor, 
			                    netPackedMessageSizeBuffer, 
			                    CARRAY_SIZE(netPackedMessageSizeBuffer));
		/* now that we have the message size we can move the data cursor to the 
			next message location */
		reliableDataCursor += messageSize;
		if(reliableDataCursor >= reliableDataBufferEnd)
			reliableDataCursor -= CARRAY_SIZE(knc->reliableDataBuffer);
		kassert(reliableDataCursor >= knc->reliableDataBuffer);
		kassert(reliableDataCursor < reliableDataBufferEnd);
		knc->reliableDataBufferFrontMessageByteOffset = kmath::safeTruncateU16(
			reliableDataCursor - knc->reliableDataBuffer);
		knc->reliableDataBufferMessageCount--;
		knc->reliableDataBufferFrontMessageRollingIndex++;
		KLOG(INFO, "CLIENT: confirmed reliable message[%i]", rmi);
	}
}
internal void kNetClientStep(
	KNetClient* knc, f32 deltaSeconds, f32 netReceiveSeconds, 
	fnSig_kNetClientWriteState* clientWriteState,
	fnSig_kNetClientReadServerState* clientReadServerState)
{
	if(kNetClientIsDisconnected(&g_gs->kNetClient))
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
				               &dataCursor, knc->packetBuffer, 
				               CARRAY_SIZE(knc->packetBuffer));
				kutil::netPack(knc->rollingUnreliableStateIndex, 
				               &dataCursor, knc->packetBuffer, 
				               CARRAY_SIZE(knc->packetBuffer));
				knc->rollingUnreliableStateIndex++;
				kutil::netPack(knc->latestServerTimestamp, 
				               &dataCursor, knc->packetBuffer, 
				               CARRAY_SIZE(knc->packetBuffer));
				const u32 remainingPacketBufferSize = 
					kmath::safeTruncateU32(kncPacketBufferEnd - dataCursor);
				dataCursor += 
					clientWriteState(dataCursor, remainingPacketBufferSize);
			}break;
		}
		/* attempt to send the unreliable packet */
		{
			const size_t packetSize = kmath::safeTruncateU64(
				dataCursor - knc->packetBuffer);
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
			&& knc->reliableDataBufferMessageCount > 0)
		{
			dataCursor = knc->packetBuffer;
			u32 bytesPacked = 0;
			/* first, pack the datagram header & the # of reliable messages 
				we're about to send */
			bytesPacked += 
				kutil::netPack(static_cast<u8>(network::PacketType
				                   ::CLIENT_RELIABLE_MESSAGE_BUFFER), 
				               &dataCursor, knc->packetBuffer, 
				               CARRAY_SIZE(knc->packetBuffer));
			bytesPacked += 
				kutil::netPack(knc->reliableDataBufferFrontMessageRollingIndex,
				               &dataCursor, knc->packetBuffer, 
				               CARRAY_SIZE(knc->packetBuffer));
			bytesPacked += 
				kutil::netPack(knc->reliableDataBufferMessageCount,
				               &dataCursor, knc->packetBuffer, 
				               CARRAY_SIZE(knc->packetBuffer));
			/* copy the reliable messages into the knc->packetBuffer */
			/* because reliable message buffer is circular, we need to handle 
				two cases:
				- reliableDataBufferFront < reliableDataBufferCursor 
					=> one memcpy
				- reliableDataBufferFront >= reliableDataBufferCursor
					=> two memcpys */
			u8* reliableDataBufferCursor = nullptr;
			u16 reliableDataBufferUsedBytes = 
				kNetClientGetReliableDataBufferUsedBytes(
					knc, &reliableDataBufferCursor);
			const u8* reliableDataBufferFront = knc->reliableDataBuffer + 
				knc->reliableDataBufferFrontMessageByteOffset;
			const u8*const reliableDataBufferEnd = 
				knc->reliableDataBuffer + CARRAY_SIZE(knc->reliableDataBuffer);
			if(reliableDataBufferFront < reliableDataBufferCursor)
			{
				const u32 reliableBufferCopyBytes = kmath::safeTruncateU32(
					reliableDataBufferCursor - reliableDataBufferFront);
				memcpy(dataCursor, reliableDataBufferFront, 
				       reliableBufferCopyBytes);
				dataCursor += reliableBufferCopyBytes;
				bytesPacked += reliableBufferCopyBytes;
			}
			else
			{
				const u32 reliableBufferCopyBytesA = kmath::safeTruncateU32(
					reliableDataBufferEnd - reliableDataBufferFront);
				const u32 reliableBufferCopyBytesB = kmath::safeTruncateU32(
					reliableDataBufferCursor - knc->reliableDataBuffer);
				kassert(reliableBufferCopyBytesA + reliableBufferCopyBytesB == 
				        reliableDataBufferUsedBytes);
				memcpy(dataCursor, reliableDataBufferFront, 
				       reliableBufferCopyBytesA);
				dataCursor += reliableBufferCopyBytesA;
				bytesPacked += reliableBufferCopyBytesA;
				memcpy(dataCursor, knc->reliableDataBuffer, 
				       reliableBufferCopyBytesB);
				dataCursor += reliableBufferCopyBytesB;
				bytesPacked += reliableBufferCopyBytesB;
			}
			/* send the packetBuffer which now contains the reliable messages */
			const size_t packetSize = kmath::safeTruncateU64(
				dataCursor - knc->packetBuffer);
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
		const u8* packetBuffer = knc->packetBuffer;
		if(netAddress == knc->addressServer && netPort == 30942)
		{
			const network::PacketType packetType = 
				network::PacketType(*(packetBuffer++));
			switch(packetType)
			{
				case network::PacketType::SERVER_ACCEPT_CONNECTION:{
					if(knc->connectionState != 
						network::ConnectionState::ACCEPTING)
					{
						break;
					}
					knc->id = 
						kutil::netUnpackU16(&packetBuffer, knc->packetBuffer, 
						                    CARRAY_SIZE(knc->packetBuffer));
					knc->connectionState = 
						network::ConnectionState::CONNECTED;
					knc->secondsSinceLastServerPacket = 0;
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
					const u32 rollingUnreliableStateIndexServer = 
						kutil::netUnpackU32(&packetBuffer, knc->packetBuffer, 
						                    CARRAY_SIZE(knc->packetBuffer));
					if(rollingUnreliableStateIndexServer <= 
						knc->rollingUnreliableStateIndexServer)
					/* drop/ignore old/duplicate server state packets! */
					{
						break;
					}
					knc->rollingUnreliableStateIndexServer = 
						rollingUnreliableStateIndexServer;
					knc->latestServerTimestamp = 
						kutil::netUnpackU64(&packetBuffer, knc->packetBuffer, 
						                    CARRAY_SIZE(knc->packetBuffer));
					knc->serverReportedRoundTripTime = 
						kutil::netUnpackF32(&packetBuffer, knc->packetBuffer, 
						                    CARRAY_SIZE(knc->packetBuffer));
					const u32 serverReportedReliableMessageRollingIndex = 
						kutil::netUnpackU32(&packetBuffer, knc->packetBuffer, 
						                    CARRAY_SIZE(knc->packetBuffer));
					kNetClientDequeueReliableMessages(
						knc, serverReportedReliableMessageRollingIndex);
//					KLOG(INFO, "CLIENT: got server state!");
					const u32 packetBufferSize = 
						kmath::safeTruncateU32(bytesReceived - 1);
					clientReadServerState(packetBuffer, packetBufferSize);
					knc->secondsSinceLastServerPacket = 0;
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
	       && !kNetClientIsDisconnected(&g_gs->kNetClient));
}
internal void kNetClientQueueReliableMessage(
	KNetClient* knc, const u8* netPackedData, u16 netPackedDataBytes)
{
	/* make sure that we have enough room in the circular buffer queue for 
		`dataBytes` + sizeof(dataBytes) worth of bytes */
	const u8*const reliableDataBufferEnd = 
		knc->reliableDataBuffer + CARRAY_SIZE(knc->reliableDataBuffer);
	u8* reliableDataBufferCursor = nullptr;
	u16 reliableDataBufferUsedBytes = 
		kNetClientGetReliableDataBufferUsedBytes(
			knc, &reliableDataBufferCursor);
	if(CARRAY_SIZE(knc->reliableDataBuffer) - reliableDataBufferUsedBytes <
		(netPackedDataBytes + sizeof(netPackedDataBytes)))
	{
		KLOG(ERROR, "Reliable data buffer {%i/%i} overflow! "
		     "Cannot fit message size=%i!", reliableDataBufferUsedBytes, 
		     CARRAY_SIZE(knc->reliableDataBuffer), 
		     netPackedDataBytes + sizeof(netPackedDataBytes));
	}
	/* each reliable message begins with a number representing the length of the 
		message */
	u8 netPackedDataBytesBuffer[2];
	u8* netPackedDataBytesBufferCursor = netPackedDataBytesBuffer;
	const u32 packedDataBytesBytes = 
		kutil::netPack(netPackedDataBytes, &netPackedDataBytesBufferCursor, 
		               netPackedDataBytesBuffer, 2);
	kassert(packedDataBytesBytes == 2);
	/* now that we have `dataBytes` in network-byte-order, we can add them all 
		to the circular buffer queue */
	for(size_t c = 0; c < CARRAY_SIZE(netPackedDataBytesBuffer); c++)
	{
		reliableDataBufferCursor[0] = netPackedDataBytesBuffer[c];
		reliableDataBufferCursor++;
		if(reliableDataBufferCursor >= reliableDataBufferEnd)
			reliableDataBufferCursor -= CARRAY_SIZE(knc->reliableDataBuffer);
		kassert(reliableDataBufferCursor >= knc->reliableDataBuffer);
		kassert(reliableDataBufferCursor < reliableDataBufferEnd);
	}
	for(u16 b = 0; b < netPackedDataBytes; b++)
	{
		reliableDataBufferCursor[0] = netPackedData[b];
		reliableDataBufferCursor++;
		if(reliableDataBufferCursor >= reliableDataBufferEnd)
			reliableDataBufferCursor -= CARRAY_SIZE(knc->reliableDataBuffer);
		kassert(reliableDataBufferCursor >= knc->reliableDataBuffer);
		kassert(reliableDataBufferCursor < reliableDataBufferEnd);
	}
	/* finally, update the rest of the KNetClient's reliable message queue state 
		to reflect our newly added message.  For now, all we have to do is 
		increment the message count, since the other information is all 
		calculated on the fly.  */
	knc->reliableDataBufferMessageCount++;
}