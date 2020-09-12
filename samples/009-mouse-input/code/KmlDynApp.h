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
	v3f32 clickLocation;
#if DEBUG_DELETE_LATER
	v3f32 lastEyeRay[2] = {{0,0,1000}, {}};
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
/* convenience macros for our application */
#define DRAW_LINES(mesh, vertexAttribs) \
	g_krb->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)