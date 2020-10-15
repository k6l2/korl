#pragma once
#include "kutil.h"
namespace kgtNet
{
	using ServerClientId = u16;
	global_variable const ServerClientId SERVER_INVALID_CLIENT_ID = 
	                                                         ServerClientId(-1);
	global_variable const f32 VIRTUAL_CONNECTION_TIMEOUT_SECONDS = 1.f;
	enum class ConnectionState : u8
		{ DISCONNECTING
		, ACCEPTING
		, CONNECTED
	};
	enum class PacketType : u8
		/* packets sent from CLIENT to SERVER */
		{ CLIENT_CONNECT_REQUEST
		, CLIENT_UNRELIABLE_STATE
		, CLIENT_RELIABLE_MESSAGE_BUFFER
		, CLIENT_DISCONNECT_REQUEST
		/* packets sent from SERVER to CLIENT */
		, SERVER_ACCEPT_CONNECTION
		, SERVER_REJECT_CONNECTION
		, SERVER_UNRELIABLE_STATE
		, SERVER_RELIABLE_MESSAGE_BUFFER
		, SERVER_DISCONNECT
	};
}
