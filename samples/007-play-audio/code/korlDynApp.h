#pragma once
#include "kgtGameState.h"
struct GameState
{
	KgtGameState kgtGameState;
	bool musicLoop;
	f32 musicVolumeRatio = 0.5f;
	KgtTapeHandle htMusic;
	f32 sfxVolumeRatio = 0.7f;
};
global_variable GameState* g_gs;
