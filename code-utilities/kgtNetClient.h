#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "kgtNetCommon.h"
#include "kgtNetReliableDataBuffer.h"
struct KgtNetClient
{
	/** this value determines the state of the CLIENT<=>SERVER virtual 
	 * connection */
	kgtNet::ConnectionState connectionState;
	kgtNet::ServerClientId id = kgtNet::SERVER_INVALID_CLIENT_ID;
	KplSocketIndex socket = KPL_INVALID_SOCKET_INDEX;
	u8 packetBuffer[KPL_MAX_DATAGRAM_SIZE];
	KplNetAddress addressServer;
	u16 serverListenPort;
	f32 secondsSinceLastServerPacket;
	u32 rollingUnreliableStateIndexServer;
	PlatformTimeStamp latestServerTimestamp;
	u32 rollingUnreliableStateIndex;
	f32 serverReportedRoundTripTime;
	KgtNetReliableDataBuffer reliableDataBuffer;
	/** the last received SERVER=>CLIENT reliable message index */
	u32 latestReceivedReliableMessageIndex;
} FORCE_SYMBOL_EXPORT;
internal bool 
	kgtNetClientIsDisconnected(const KgtNetClient* knc);
internal void 
	kgtNetClientConnect(
		KgtNetClient* knc, const char* cStrServerAddress, u16 serverListenPort);
internal void 
	kgtNetClientBeginDisconnect(KgtNetClient* knc);
internal void 
	kgtNetClientDropConnection(KgtNetClient* knc);
/**
 * @return the # of bytes written to `packetBuffer`
 */
#define KGT_NET_CLIENT_WRITE_STATE(name) \
	u32 name(u8* o_data, const u8* dataEnd)
#define KGT_NET_CLIENT_READ_SERVER_STATE(name) \
	void name(const u8* data, const u8* dataEnd)
/** 
 * @return the # of bytes read/unpacked from `data`
 */
#define KGT_NET_CLIENT_READ_RELIABLE_MESSAGE(name) \
	u32 name(const u8* data, const u8* dataEnd)
typedef KGT_NET_CLIENT_WRITE_STATE(funcKgtNetClientWriteState);
typedef KGT_NET_CLIENT_READ_SERVER_STATE(funcKgtNetClientReadServerState);
typedef KGT_NET_CLIENT_READ_RELIABLE_MESSAGE(
	funcKgtNetClientReadReliableMessage);
internal void 
	kgtNetClientStep(
		KgtNetClient* knc, f32 deltaSeconds, f32 netReceiveSeconds, 
		funcKgtNetClientWriteState* fnWriteState,
		funcKgtNetClientReadServerState* fnReadServerState, 
		funcKgtNetClientReadReliableMessage* fnReadReliableMessage);
internal void 
	kgtNetClientQueueReliableMessage(
		KgtNetClient* knc, const u8* netPackedData, u16 netPackedDataBytes);
