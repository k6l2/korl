#pragma once
#include "kutil.h"
namespace network
{
	global_variable const f32 VIRTUAL_CONNECTION_TIMEOUT_SECONDS = 1.f;
	enum class ConnectionState : u8
		{ NOT_CONNECTED///@TODO: rename to DISCONNECTING?
		, ACCEPTING
		, CONNECTED
	};
	enum class PacketType : u8
		/* packets sent from CLIENT to SERVER */
		{ CLIENT_CONNECT_REQUEST
		, CLIENT_STATE
		, CLIENT_DISCONNECT_REQUEST
		/* packets sent from SERVER to CLIENT */
		, SERVER_ACCEPT_CONNECTION
		, SERVER_REJECT_CONNECTION
		, SERVER_STATE
		, SERVER_DISCONNECT
	};
}