#pragma once
#include "TemplateGameState.h"
struct GameState
{
	KmlTemplateGameState templateGameState;
	bool musicLoop;
	f32 musicVolumeRatio = 0.5f;
	KTapeHandle musicTapeHandle;
	f32 sfxVolumeRatio = 0.7f;
};
global_variable GameState* g_gs;