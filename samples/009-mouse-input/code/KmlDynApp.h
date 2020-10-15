#pragma once
#include "kgtGameState.h"
#include "kgtCamera3d.h"
struct GameState
{
	KgtGameState kgtGameState;
	KgtCamera3d camera;
	v3f32 clickLocation;
	f32 clickCircleSize = 5;
};
global_variable GameState* g_gs;
