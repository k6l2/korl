#pragma once
#include "TemplateGameState.h"
struct GameState
{
	KmlTemplateGameState templateGameState;
	f32 seconds;
	v3f32* dynamicArrayActorPositions;
};
global_variable GameState* g_gs;
struct VertexNoTexture
{
	v3f32 position;
	Color4f32 color;
};
const global_variable KrbVertexAttributeOffsets 
	VERTEX_NO_TEXTURE_ATTRIBS = 
		{ .position_3f32 = offsetof(VertexNoTexture, position)
		, .color_4f32    = offsetof(VertexNoTexture, color)
		, .texCoord_2f32 = sizeof(VertexNoTexture) };
const global_variable v3f32 WORLD_UP = {0,0,1};
#define DRAW_LINES(mesh, vertexAttribs) \
	g_krb->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS(mesh, vertexAttribs) \
	g_krb->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)