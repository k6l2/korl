#pragma once
#include "kgtGameState.h"
struct GameState
{
	KgtGameState kgtGameState;
	struct 
	{
		v2f32 position;
		q32 orient = q32::IDENTITY;
	} samplePlayer;
};
global_variable GameState* g_gs;
