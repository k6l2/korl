#pragma once
#include "TemplateGameState.h"
/* In order to draw primitives, we must define a structure to pack the vertex 
	data of our choice, as well as the memory layout instructions for KRB.  This 
	utility header provides definitions of such constructs for your 
	convenience! */
#include "kVertex.h"
struct GameState
{
	KmlTemplateGameState templateGameState;
	f32 seconds;// for simple drawing animations
};
global_variable GameState* g_gs;
