#pragma once
#include "TemplateGameState.h"
#include "camera3d.h"
#include "kVertex.h"
#include "kShape.h"
enum class HudState : u8
	{ NAVIGATING
	, ADDING_SHAPE
	, ADDING_BOX
	, ADDING_SPHERE
	, MODIFY_SHAPE_GRAB
	, MODIFY_SHAPE_ROTATE };
struct Actor
{
	v3f32 position;
	kQuaternion orientation = kQuaternion::IDENTITY;
	Shape shape;
};
struct GameState
{
	KmlTemplateGameState templateGameState;
	HudState hudState;
	Camera3d camera;
	/* Director */
	Actor* actors;
	bool wireframe;
	bool resolveShapeCollisions;
	/* add actor state */
	Shape addShape;
	v3f32 addShapePosition;
	/* actor manipulation state */
	/* The array index into `actors` == `selectedActorId` - 1.  A value of 0 
		indicates no actor is selected */
	u32 selectedActorId;
	/* when a modification command occurs, the original values of the selected 
		actor which are to be modified are stored here */
	f32 modifyShapeTempValues[4];
	f32 modifyShapePlaneDistanceFromCamera;
	v2i32 modifyShapeWindowPositionStart;
};
global_variable GameState* g_gs;
