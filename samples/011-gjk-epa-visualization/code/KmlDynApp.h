#pragma once
#include "TemplateGameState.h"
#include "camera3d.h"
#include "kShape.h"
struct Actor
{
	v3f32 position;
	kQuaternion orientation = kQuaternion::IDENTITY;
	Shape shape;
};
struct GameState
{
	KmlTemplateGameState templateGameState;
	Camera3d camera;
	Actor actors[2];
	/* GJK + EPA visualizations */
	v3f32 minkowskiDifferencePosition;
	Vertex minkowskiDifferencePointCloud[1000];
	u16 minkowskiDifferencePointCloudCount = 1000;
	kmath::GjkState gjkState;
	kmath::EpaState epaState;
};
global_variable GameState* g_gs;
