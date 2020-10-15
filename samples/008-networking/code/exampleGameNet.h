#pragma once
#include "kutil.h"
#include "kgtNetClient.h"
#include "kgtNetServer.h"
global_variable const u16 GAME_NET_SERVER_LISTEN_PORT = 30942;
struct Actor
{
	kgtNet::ServerClientId clientId = kgtNet::SERVER_INVALID_CLIENT_ID;
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
internal KGT_NET_CLIENT_WRITE_STATE(gameWriteClientState);
internal KGT_NET_CLIENT_READ_SERVER_STATE(gameClientReadServerState);
internal KGT_NET_SERVER_READ_CLIENT_STATE(serverReadClient);
internal KGT_NET_SERVER_WRITE_STATE(serverWriteState);
internal KGT_NET_SERVER_ON_CLIENT_CONNECT(serverOnClientConnect);
internal KGT_NET_SERVER_ON_CLIENT_DISCONNECT(serverOnClientDisconnect);
internal KGT_NET_SERVER_READ_RELIABLE_MESSAGE(serverReadReliableMessage);
internal KGT_NET_CLIENT_READ_RELIABLE_MESSAGE(gameClientReadReliableMessage);
