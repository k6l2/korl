#pragma once
#include "kgtGameState.h"
struct GameState
{
	KgtGameState kgtGameState;
	v2f32 viewPosition;
};
global_variable GameState* g_gs;
