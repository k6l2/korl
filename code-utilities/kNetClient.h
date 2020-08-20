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
	u32 rollingUnreliableStateIndexServer;
	PlatformTimeStamp latestServerTimestamp;
	u32 rollingUnreliableStateIndex;
	f32 serverReportedRoundTripTime;
	/** treated as a circular buffer queue of network::ReliableMessage.  
	 * We subtract the sizes of 
	 * - `reliableDataBufferMessageCount`
	 * - `reliableDataBufferFrontMessageRollingIndex`
	 * - the size of a PacketType 
	 * for the reliable packet header to ensure that 
	 * the entire buffer will fit in a single datagram.  */
	u8 reliableDataBuffer[KPL_MAX_DATAGRAM_SIZE - sizeof(u16) - sizeof(u32) - 
	                      sizeof(network::PacketType)];
	u16 reliableDataBufferMessageCount;
	u16 reliableDataBufferFrontMessageByteOffset;
	/** start at an index if 1 to ensure that the server's client rolling index 
	 * is out of date for the first reliable message we send, since on the 
	 * server we start at 0 */
	u32 reliableDataBufferFrontMessageRollingIndex = 1;
} FORCE_SYMBOL_EXPORT;
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
internal void kNetClientStep(
	KNetClient* knc, f32 deltaSeconds, f32 netReceiveSeconds, 
	fnSig_kNetClientWriteState* clientWriteState,
	fnSig_kNetClientReadServerState* clientReadServerState);
internal void kNetClientQueueReliableMessage(
	KNetClient* knc, const u8* netPackedData, u16 netPackedDataBytes);