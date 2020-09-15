#pragma once
#include "TemplateGameState.h"
#define DEBUG_DELETE_LATER 1
struct GameState
{
	KmlTemplateGameState templateGameState;
	bool orthographicView;
	v3f32 cameraPosition = {10,11,12};
	f32 cameraRadiansYaw = PI32*3/4;
	f32 cameraRadiansPitch = -PI32/4;
#if DEBUG_DELETE_LATER
	f32 radius;
	u32 latitudes = 3;
	u32 longitudes = 3;
#endif//DEBUG_DELETE_LATER
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
/* convenience macros for our application */
#define DRAW_LINES(mesh, vertexAttribs) \
	g_krb->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS(mesh, vertexAttribs) \
	g_krb->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define ALLOC_FRAME_ARRAY(type,elements) \
	reinterpret_cast<type*>(\
		kalAlloc(g_gs->templateGameState.hKalFrame,sizeof(type)*elements))