#pragma once
#include "TemplateGameState.h"
#include "kFlipBook.h"
struct GameState
{
	KmlTemplateGameState templateGameState;
	/* flipbook animation state is stored in a simple struct which must be 
		initialized before using */
	KFlipBook kfbBlob;
};
global_variable GameState* g_gs;