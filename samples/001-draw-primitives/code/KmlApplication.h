#pragma once
#include "TemplateGameState.h"
struct GameState
{
	KmlTemplateGameState templateGameState;
	f32 seconds;// for simple drawing animations
};
global_variable GameState* g_gs;
/* in order to draw primitives, we must define a structure to pack the vertex 
	data of our choice, as well as the memory layout instructions for KRB */
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
/* convenience macros for our application */
#define DRAW_LINES(mesh, vertexAttribs) \
	g_krb->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS(mesh, vertexAttribs) \
	g_krb->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define SINEF_0_1(x) (0.5f*sinf(x)+0.5f)