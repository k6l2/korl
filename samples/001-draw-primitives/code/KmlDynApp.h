#pragma once
#include "kgtGameState.h"
struct GameState
{
	KgtGameState kgtGameState;
	f32 seconds;// for simple drawing animations
};
global_variable GameState* g_gs;
