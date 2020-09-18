#pragma once
#include "TemplateGameState.h"
#define DEBUG_DELETE_LATER 1
enum class ShapeType : u8
	{ BOX
	, SPHERE };
union Shape
{
	ShapeType type;
	struct 
	{
		ShapeType type;
		v3f32 lengths;
	} box;
	struct 
	{
		ShapeType type;
		f32 radius;
	} sphere;
};
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
	bool orthographicView;
	v3f32 cameraPosition = {10,11,12};
	f32 cameraRadiansYaw = PI32*3/4;
	f32 cameraRadiansPitch = -PI32/4;
	HudState hudState;
	Shape addShape;
	v3f32 addShapePosition;
	kmath::GeneratedMeshVertex* generatedSphereMesh;
	size_t generatedSphereMeshVertexCount;
	Actor* actors;
	bool wireframe;
	/* The array index into `actors` == `selectedActorId` - 1.  A value of 0 
		indicates no actor is selected */
	u32 selectedActorId;
	/* when a modification command occurs, the original values of the selected 
		actor which are to be modified are stored here */
	f32 modifyShapeTempValues[4];
	f32 modifyShapePlaneDistanceFromCamera;
	v2i32 modifyShapeWindowPositionStart;
#if DEBUG_DELETE_LATER
	v3f32 eyeRayActorHitPosition;
	v3f32 testPosition;
	v3f32 testRadianAxis;
	f32 testRadians;
#endif// DEBUG_DELETE_LATER
};
global_variable GameState* g_gs;
/* KRB vertex attribute specification */
struct VertexNoTexture
{
	v3f32 position;
	Color4f32 color;
};
const global_variable KrbVertexAttributeOffsets 
	VERTEX_ATTRIBS_NO_TEXTURE = 
		{ .position_3f32 = offsetof(VertexNoTexture, position)
		, .color_4f32    = offsetof(VertexNoTexture, color)
		, .texCoord_2f32 = sizeof(VertexNoTexture) };
const global_variable KrbVertexAttributeOffsets 
	VERTEX_ATTRIBS_GENERATED_MESH = 
		{ .position_3f32 = offsetof(kmath::GeneratedMeshVertex, localPosition)
		, .color_4f32    = sizeof(kmath::GeneratedMeshVertex)
		, .texCoord_2f32 = 
			offsetof(kmath::GeneratedMeshVertex, textureNormal) };
const global_variable KrbVertexAttributeOffsets 
	VERTEX_ATTRIBS_GENERATED_MESH_NO_TEX = 
		{ .position_3f32 = offsetof(kmath::GeneratedMeshVertex, localPosition)
		, .color_4f32    = sizeof(kmath::GeneratedMeshVertex)
		, .texCoord_2f32 = sizeof(kmath::GeneratedMeshVertex) };
/* convenience macros for our application */
#define DRAW_LINES(mesh, vertexAttribs) \
	g_krb->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS(mesh, vertexAttribs) \
	g_krb->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define ALLOC_FRAME_ARRAY(type,elements) \
	reinterpret_cast<type*>(\
		kalAlloc(g_gs->templateGameState.hKalFrame,sizeof(type)*elements))