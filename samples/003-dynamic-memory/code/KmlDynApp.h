#pragma once
#include "kgtGameState.h"
struct GameState
{
	KgtGameState kgtGameState;
	f32 seconds;
	v3f32* dynamicArrayActorPositions;
};
global_variable GameState* g_gs;
global_variable const v3f32 WORLD_UP = {0,0,1};
