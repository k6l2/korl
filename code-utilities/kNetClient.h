#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "kNetCommon.h"
struct KNetClient
{
	/** this value determines the state of the CLIENT<=>SERVER virtual 
	 * connection */
	network::ConnectionState connectionState;
	network::ServerClientId id = network::SERVER_INVALID_CLIENT_ID;
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
/**
 * @return the # of bytes written to `packetBuffer`
 */
#define K_NET_CLIENT_WRITE_STATE(name) \
	u32 name(u8* packetBuffer, u32 packetBufferSize)
#define K_NET_CLIENT_READ_SERVER_STATE(name) \
	void name(const u8* packetBuffer, u32 packetBufferSize)
typedef K_NET_CLIENT_WRITE_STATE(fnSig_kNetClientWriteState);
typedef K_NET_CLIENT_READ_SERVER_STATE(fnSig_kNetClientReadServerState);
internal void kNetClientStep(KNetClient* knc, f32 deltaSeconds, 
                        f32 netReceiveSeconds, 
                        fnSig_kNetClientWriteState* clientWriteState,
                        fnSig_kNetClientReadServerState* clientReadServerState);