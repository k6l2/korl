#pragma once
#include "global-defines.h"
#include "krb-interface.h"
#include "generalAllocator.h"
struct GameState
{
	v2f32 viewOffset2d;
	v2f32 shipWorldPosition;
	f32 theraminHz = 294.f;
	f32 theraminSine = 0;
	f32 theraminVolume = 1000;
	KrbTextureHandle kthFighter;
	KgaHandle gAllocTransient;
};