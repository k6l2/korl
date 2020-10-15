#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "kgtNetCommon.h"
#include "kgtNetReliableDataBuffer.h"
struct KgtNetServerClientEntry
{
	kgtNet::ServerClientId id;
	KplNetAddress netAddress;
	u16 netPort;
	f32 timeSinceLastPacket;
	kgtNet::ConnectionState connectionState;
	u32 rollingUnreliableStateIndex;
	f32 roundTripTime;
	/** the last received CLIENT=>SERVER reliable message index */
	u32 latestReceivedReliableMessageIndex;
	KgtNetReliableDataBuffer reliableDataBuffer;
};
struct KgtNetServer
{
	KplSocketIndex socket = KPL_INVALID_SOCKET_INDEX;
	u16 port;
	/** `arrcap(clientArray)` is assumed to be equal to the `maxClients` 
	 * parameter passed to `kNetServerStart`.  KNetServer will never allow more 
	 * than `maxClients` # of clients to connect to the server.  */
	KgtNetServerClientEntry* clientArray;
	/** assuming the client will drop server state indices below their latest 
	 * index, a 32-bit var rolling 60 frames/second will become unstable if left 
	 * running for several years non-stop - which is perfectly fine by me */
	u32 rollingUnreliableStateIndex;
} FORCE_SYMBOL_EXPORT;
internal bool 
	kgtNetServerStart(
		KgtNetServer* kns, KgtAllocatorHandle hKal, u8 maxClients);
internal void 
	kgtNetServerStop(KgtNetServer* kns);
#define KGT_NET_SERVER_READ_CLIENT_STATE(name) \
	void name(kgtNet::ServerClientId clientId, const u8* data, \
	          const u8* dataEnd, void* userPointer)
/**
 * @return the # of bytes written to `packetBuffer`
 */
#define KGT_NET_SERVER_WRITE_STATE(name) \
	u32 name(u8* data, const u8* dataEnd, void* userPointer)
#define KGT_NET_SERVER_ON_CLIENT_CONNECT(name) \
	void name(kgtNet::ServerClientId clientId, void* userPointer)
#define KGT_NET_SERVER_ON_CLIENT_DISCONNECT(name) \
	void name(kgtNet::ServerClientId clientId, void* userPointer)
/**
 * @return the # of bytes read/unpacked from `netData`
 */
#define KGT_NET_SERVER_READ_RELIABLE_MESSAGE(name) \
	u32 name(kgtNet::ServerClientId clientId, const u8* netData, \
	         const u8* netDataEnd, void* userPointer)
typedef KGT_NET_SERVER_READ_CLIENT_STATE(funcKgtNetServerReadClientState);
typedef KGT_NET_SERVER_WRITE_STATE(funcKgtNetServerWriteState);
typedef KGT_NET_SERVER_ON_CLIENT_CONNECT(funcKgtNetServerOnClientConnect);
typedef KGT_NET_SERVER_ON_CLIENT_DISCONNECT(funcKgtNetServerOnClientDisconnect);
typedef KGT_NET_SERVER_READ_RELIABLE_MESSAGE(
	funcKgtNetServerReadReliableMessage);
internal void 
	kgtNetServerStep(
		KgtNetServer* kns, f32 deltaSeconds, f32 netReceiveSeconds, 
		const PlatformTimeStamp& timeStampFrameStart, 
		funcKgtNetServerReadClientState* fnReadClientState, 
		funcKgtNetServerWriteState* fnWriteState, 
		funcKgtNetServerOnClientConnect* fnOnClientConnect, 
		funcKgtNetServerOnClientDisconnect* fnOnClientDisconnect, 
		funcKgtNetServerReadReliableMessage* fnReadReliableMessage, 
		void* userPointer);
/**
 * @param ignoreClientId 
 *     If this value is not `network::SERVER_INVALID_CLIENT_ID`, then the client 
 *     with this Id will not receive the `netPackedData`.  Otherwise, 
 *     `netPackedData` is queued to be sent reliably to all currently connected 
 *     clients.
 */
internal void 
	kgtNetServerQueueReliableMessage(
		KgtNetServer* kns, const u8* netPackedData, u16 netPackedDataBytes, 
		kgtNet::ServerClientId ignoreClientId);
