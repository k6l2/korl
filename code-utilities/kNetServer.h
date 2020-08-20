#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "kNetCommon.h"
struct KNetServerClientEntry
{
	network::ServerClientId id;
	KplNetAddress netAddress;
	u16 netPort;
	f32 timeSinceLastPacket;
	network::ConnectionState connectionState;
	u32 rollingUnreliableStateIndex;
	f32 roundTripTime;
	u32 reliableDataBufferFrontMessageRollingIndex;
};
struct KNetServer
{
	KplSocketIndex socket = KPL_INVALID_SOCKET_INDEX;
	u16 port;
	/** `arrcap(clientArray)` is assumed to be equal to the `maxClients` 
	 * parameter passed to `kNetServerStart`.  KNetServer will never allow more 
	 * than `maxClients` # of clients to connect to the server.  */
	KNetServerClientEntry* clientArray;
	/** assuming the client will drop server state indices below their latest 
	 * index, a 32-bit var rolling 60 frames/second will become unstable if left 
	 * running for several years non-stop - which is perfectly fine by me */
	u32 rollingUnreliableStateIndex;
} FORCE_SYMBOL_EXPORT;
internal bool kNetServerStart(KNetServer* kns, KgaHandle hKga, u8 maxClients);
internal void kNetServerStop(KNetServer* kns);
#define K_NET_SERVER_READ_CLIENT_STATE(name) \
	void name(network::ServerClientId clientId, const u8* packetBuffer, \
	          u32 packetBufferSize, void* userPointer)
/**
 * @return the # of bytes written to `packetBuffer`
 */
#define K_NET_SERVER_WRITE_STATE(name) \
	u32 name(u8* packetBuffer, u32 packetBufferSize, void* userPointer)
#define K_NET_SERVER_ON_CLIENT_CONNECT(name) \
	void name(network::ServerClientId clientId, void* userPointer)
#define K_NET_SERVER_ON_CLIENT_DISCONNECT(name) \
	void name(network::ServerClientId clientId, void* userPointer)
#define K_NET_SERVER_READ_RELIABLE_MESSAGE(name) \
	void name(network::ServerClientId clientId, const u8* netDataBuffer, \
	          u32 netDataBufferSize, void* userPointer)
typedef K_NET_SERVER_READ_CLIENT_STATE(fnSig_kNetServerReadClientState);
typedef K_NET_SERVER_WRITE_STATE(fnSig_kNetServerWriteState);
typedef K_NET_SERVER_ON_CLIENT_CONNECT(fnSig_kNetServerOnClientConnect);
typedef K_NET_SERVER_ON_CLIENT_DISCONNECT(fnSig_kNetServerOnClientDisconnect);
typedef K_NET_SERVER_READ_RELIABLE_MESSAGE(fnSig_kNetServerReadReliableMessage);
internal void kNetServerStep(
	KNetServer* kns, f32 deltaSeconds, f32 netReceiveSeconds, 
	const PlatformTimeStamp& timeStampFrameStart, 
	fnSig_kNetServerReadClientState* fnReadClientState, 
	fnSig_kNetServerWriteState* fnWriteState, 
	fnSig_kNetServerOnClientConnect* fnOnClientConnect, 
	fnSig_kNetServerOnClientDisconnect* fnOnClientDisconnect, 
	fnSig_kNetServerReadReliableMessage* fnReadReliableMessage, 
	void* userPointer);