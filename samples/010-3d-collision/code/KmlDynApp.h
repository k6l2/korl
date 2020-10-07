#pragma once
#include "TemplateGameState.h"
#include "camera3d.h"
#include "kVertex.h"
#include "kShape.h"
#define DEBUG_DELETE_LATER 1
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
#if DEBUG_DELETE_LATER
	/* GJK + EPA visualizations */
	v3f32 minkowskiDifferencePosition;
	Vertex minkowskiDifferencePointCloud[1000];
	u16 minkowskiDifferencePointCloudCount = 1000;
	kmath::GjkState gjkState;
	kmath::EpaState epaState;
#endif// DEBUG_DELETE_LATER
};
global_variable GameState* g_gs;
/* convenience macros for our application */
#define ALLOC_FRAME_ARRAY(type,elements) \
	reinterpret_cast<type*>(\
		kAllocAlloc(g_gs->templateGameState.hKalFrame,sizeof(type)*(elements)))
