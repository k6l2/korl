#pragma once
#include "kgtGameState.h"
#include "kgtFlipBook.h"
struct GameState
{
	KgtGameState kgtGameState;
	/* flipbook animation state is stored in a simple struct which must be 
		initialized before using */
	KgtFlipBook kfbBlob;
};
global_variable GameState* g_gs;
