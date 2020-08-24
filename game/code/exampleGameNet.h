#pragma once
#include "kutil.h"
#include "kNetClient.h"
#include "kNetServer.h"
struct Actor
{
	network::ServerClientId clientId = network::SERVER_INVALID_CLIENT_ID;
	v2f32 shipWorldPosition;
	kQuaternion shipWorldOrientation;
};
enum class ReliableMessageType : u8
{
	CLIENT_INPUT_STATE,
	ANSI_TEXT_MESSAGE
};
struct ClientControlInput
{
	v2f32 controlMoveVector;
	bool controlResetPosition;
};
internal u16 reliableMessageAnsiTextPack(
	const char* nullTerminatedAnsiText, u8* dataBuffer, 
	const u8* dataBufferEnd);
internal u16 reliableMessageClientControlInputPack(
	const ClientControlInput& cci, u8* o_data, const u8* dataEnd);
internal K_NET_CLIENT_WRITE_STATE(gameWriteClientState);
internal K_NET_CLIENT_READ_SERVER_STATE(gameClientReadServerState);
internal K_NET_SERVER_READ_CLIENT_STATE(serverReadClient);
internal K_NET_SERVER_WRITE_STATE(serverWriteState);
internal K_NET_SERVER_ON_CLIENT_CONNECT(serverOnClientConnect);
internal K_NET_SERVER_ON_CLIENT_DISCONNECT(serverOnClientDisconnect);
internal K_NET_SERVER_READ_RELIABLE_MESSAGE(serverReadReliableMessage);
internal K_NET_CLIENT_READ_RELIABLE_MESSAGE(gameClientReadReliableMessage);