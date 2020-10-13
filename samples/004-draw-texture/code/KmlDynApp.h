#pragma once
#include "TemplateGameState.h"
struct GameState
{
	KmlTemplateGameState templateGameState;
	v2f32 camPosition2d = {12,0};
};
global_variable GameState* g_gs;
global_variable const v3f32 WORLD_UP = {0,0,1};
