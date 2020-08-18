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
internal void kNetClientStep(KNetClient* knc, f32 deltaSeconds,
                         f32 netReceiveSeconds, 
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
		u8* packetBuffer = knc->packetBuffer;
		const u8*const kncPacketBufferEnd = 
			knc->packetBuffer + CARRAY_SIZE(knc->packetBuffer);
		switch(knc->connectionState)
		{
			case network::ConnectionState::DISCONNECTING:{
				/* send disconnect packets to the server until we receive a 
					disconnect aknowledgement from the server */
				*(packetBuffer++) = static_cast<u8>(
					network::PacketType::CLIENT_DISCONNECT_REQUEST);
			}break;
			case network::ConnectionState::ACCEPTING:{
				/* send connection requests until we receive server state */
				*(packetBuffer++) = static_cast<u8>(
					network::PacketType::CLIENT_CONNECT_REQUEST);
			}break;
			case network::ConnectionState::CONNECTED:{
				/* send client state every frame */
				kutil::netPack(static_cast<u8>(
				               network::PacketType::CLIENT_UNRELIABLE_STATE), 
				               &packetBuffer, knc->packetBuffer, 
				               CARRAY_SIZE(knc->packetBuffer));
				kutil::netPack(knc->rollingUnreliableStateIndex, 
				               &packetBuffer, knc->packetBuffer, 
				               CARRAY_SIZE(knc->packetBuffer));
				knc->rollingUnreliableStateIndex++;
				kutil::netPack(knc->latestServerTimestamp, 
				               &packetBuffer, knc->packetBuffer, 
				               CARRAY_SIZE(knc->packetBuffer));
				const u32 remainingPacketBufferSize = 
					kmath::safeTruncateU32(kncPacketBufferEnd - packetBuffer);
				packetBuffer += 
					clientWriteState(packetBuffer, remainingPacketBufferSize);
			}break;
		}
		const size_t packetSize = kmath::safeTruncateU64(
			packetBuffer - knc->packetBuffer);
		const i32 bytesSent = 
			g_kpl->socketSend(knc->socket, knc->packetBuffer, packetSize, 
			                  knc->addressServer, 30942);
		kassert(bytesSent >= 0);
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