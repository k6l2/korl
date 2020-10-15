#pragma once
#include "kgtGameState.h"
#include "kgtNetClient.h"
#include "serverExample.h"
struct GameState
{
	KgtGameState kgtGameState;
	/* sample server state management */
	ServerState serverState;
	char clientAddressBuffer[256];
	KgtNetClient kNetClient;
	char clientReliableTextBuffer[256];
	u8 reliableMessageBuffer[KPL_MAX_DATAGRAM_SIZE];
	/* sample media testing state */
	KgtFlipBook kFbShip;
	KgtFlipBook kFbShipExhaust;
	Actor* actors;
};
global_variable GameState* g_gs;
