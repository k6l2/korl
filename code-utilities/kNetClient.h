#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "kNetCommon.h"
struct KNetClient
{
	/** this value determines the state of the CLIENT<=>SERVER virtual 
	 * connection */
	network::ConnectionState connectionState;
	KplSocketIndex socket = KPL_INVALID_SOCKET_INDEX;
	u8 packetBuffer[KPL_MAX_DATAGRAM_SIZE];
	KplNetAddress addressServer;
	f32 secondsSinceLastServerPacket;
};
internal void kNetClientOnPreUnload(KNetClient* knc);
internal bool kNetClientIsDisconnected(const KNetClient* knc);
internal void kNetClientConnect(KNetClient* knc, 
                                const char* cStrServerAddress);
internal void kNetClientBeginDisconnect(KNetClient* knc);
internal void kNetClientDropConnection(KNetClient* knc);
internal void kNetClientStep(KNetClient* knc, f32 deltaSeconds, 
                             f32 netReceiveSeconds);