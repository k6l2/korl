#pragma once
#define SEPARATE_ASSET_MODULES_COMPLETE 0
#include "kgtGameState.h"
struct GameState
{
	KgtGameState kgtGameState;
	v2f32 viewPosition;
};
global_variable GameState* g_gs;
