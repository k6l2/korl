#pragma once
#include "kgtGameState.h"
struct GameState
{
	KgtGameState kgtGameState;
	v2f32 viewPosition;
	char textBuffer[256];
};
global_variable GameState* g_gs;
