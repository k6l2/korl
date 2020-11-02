#pragma once
#include "kgtGameState.h"
#include "kgtCamera3d.h"
#include "kgtShape.h"
struct Actor
{
	v3f32 position;
	q32 orientation = q32::IDENTITY;
	KgtShape shape;
};
struct GameState
{
	KgtGameState kgtGameState;
	KgtCamera3d camera;
	Actor actors[2];
	/* GJK + EPA visualizations */
	v3f32 minkowskiDifferencePosition;
	KgtVertex minkowskiDifferencePointCloud[1000];
	u16 minkowskiDifferencePointCloudCount = 1000;
	kmath::GjkState gjkState;
	kmath::EpaState epaState;
};
global_variable GameState* g_gs;
