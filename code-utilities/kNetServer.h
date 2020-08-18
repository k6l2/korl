#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "kNetCommon.h"
using KNetServerClientId = u16;
global_variable const KNetServerClientId K_NET_SERVER_INVALID_CLIENT_ID = 
                                                         KNetServerClientId(-1);
struct KNetServerClientEntry
{
	KNetServerClientId id;
	KplNetAddress netAddress;
	u16 netPort;
	f32 timeSinceLastPacket;
	network::ConnectionState connectionState;
};
struct KNetServer
{
	KplSocketIndex socket = KPL_INVALID_SOCKET_INDEX;
	u16 port;
	/** `arrcap(clientArray)` is assumed to be equal to the `maxClients` 
	 * parameter passed to `kNetServerStart` */
	KNetServerClientEntry* clientArray;
};
internal bool kNetServerStart(KNetServer* kns, KgaHandle hKga, u8 maxClients);
internal void kNetServerStop(KNetServer* kns);
#define K_NET_SERVER_READ_CLIENT_STATE(name) \
	void name(KNetServerClientId clientId, const u8* packetBuffer, \
	          u32 packetBufferSize, void* userPointer)
/**
 * @return the # of bytes written to `packetBuffer`
 */
#define K_NET_SERVER_WRITE_STATE(name) \
	u32 name(u8* packetBuffer, u32 packetBufferSize, void* userPointer)
typedef K_NET_SERVER_READ_CLIENT_STATE(fnSig_kNetServerReadClientState);
typedef K_NET_SERVER_WRITE_STATE(fnSig_kNetServerWriteState);
internal void kNetServerStep(KNetServer* kns, f32 deltaSeconds, 
                             f32 netReceiveSeconds, 
                             const PlatformTimeStamp& timeStampFrameStart, 
                             fnSig_kNetServerReadClientState* fnReadClientState, 
                             fnSig_kNetServerWriteState* fnWriteState, 
                             void* userPointer);