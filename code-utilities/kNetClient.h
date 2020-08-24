#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "kNetCommon.h"
#include "KNetReliableDataBuffer.h"
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
	KNetReliableDataBuffer reliableDataBuffer;
	/** the last received SERVER=>CLIENT reliable message index */
	u32 latestReceivedReliableMessageIndex;
} FORCE_SYMBOL_EXPORT;
internal bool kNetClientIsDisconnected(const KNetClient* knc);
internal void kNetClientConnect(KNetClient* knc, 
                                const char* cStrServerAddress);
internal void kNetClientBeginDisconnect(KNetClient* knc);
internal void kNetClientDropConnection(KNetClient* knc);
/**
 * @return the # of bytes written to `packetBuffer`
 */
#define K_NET_CLIENT_WRITE_STATE(name) \
	u32 name(u8* o_data, const u8* dataEnd)
#define K_NET_CLIENT_READ_SERVER_STATE(name) \
	void name(const u8* data, const u8* dataEnd)
/** 
 * @return the # of bytes read/unpacked from `data`
 */
#define K_NET_CLIENT_READ_RELIABLE_MESSAGE(name) \
	u32 name(const u8* data, const u8* dataEnd)
typedef K_NET_CLIENT_WRITE_STATE(fnSig_kNetClientWriteState);
typedef K_NET_CLIENT_READ_SERVER_STATE(fnSig_kNetClientReadServerState);
typedef K_NET_CLIENT_READ_RELIABLE_MESSAGE(fnSig_kNetClientReadReliableMessage);
internal void kNetClientStep(
	KNetClient* knc, f32 deltaSeconds, f32 netReceiveSeconds, 
	fnSig_kNetClientWriteState* fnWriteState,
	fnSig_kNetClientReadServerState* fnReadServerState, 
	fnSig_kNetClientReadReliableMessage* fnReadReliableMessage);
internal void kNetClientQueueReliableMessage(
	KNetClient* knc, const u8* netPackedData, u16 netPackedDataBytes);