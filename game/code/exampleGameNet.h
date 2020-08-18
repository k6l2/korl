#pragma once
#include "kutil.h"
#include "kNetClient.h"
#include "kNetServer.h"
struct Actor
{
	KNetServerClientId clientId = K_NET_SERVER_INVALID_CLIENT_ID;
	v2f32 shipWorldPosition;
	kQuaternion shipWorldOrientation;
};
internal K_NET_CLIENT_WRITE_STATE(gameWriteClientState);
internal K_NET_CLIENT_READ_SERVER_STATE(gameClientReadServerState);
internal K_NET_SERVER_READ_CLIENT_STATE(serverReadClient);
internal K_NET_SERVER_WRITE_STATE(serverWriteState);