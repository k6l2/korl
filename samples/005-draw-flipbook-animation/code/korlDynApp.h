#pragma once
#define SEPARATE_ASSET_MODULES_COMPLETE 0
#include "kgtGameState.h"
struct GameState
{
	KgtGameState kgtGameState;
	/* flipbook animation state is stored in a simple struct which must be 
		initialized before using */
	KgtFlipBook kfbBlob;
};
global_variable GameState* g_gs;
