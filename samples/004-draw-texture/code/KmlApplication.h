#pragma once
#include "TemplateGameState.h"
struct GameState
{
	KmlTemplateGameState templateGameState;
	v2f32 camPosition2d = {12,0};
};
global_variable GameState* g_gs;
struct Vertex
{
	v3f32 position;
	Color4f32 color;
	v2f32 textureCoord;
};
/* In order to draw textures with KRB primitive drawing functions, we must 
	specify that our vertex data contains texture coordinates.  Color is 
	optional, and if omitted defaults to white */
const global_variable KrbVertexAttributeOffsets VERTEX_ATTRIBS = 
	{ .position_3f32 = offsetof(Vertex, position)
	, .color_4f32    = sizeof(Vertex)
	, .texCoord_2f32 = offsetof(Vertex, textureCoord) };
const global_variable KrbVertexAttributeOffsets VERTEX_ATTRIBS_NO_TEXTURE = 
	{ .position_3f32 = offsetof(Vertex, position)
	, .color_4f32    = offsetof(Vertex, color)
	, .texCoord_2f32 = sizeof(Vertex) };
const global_variable v3f32 WORLD_UP = {0,0,1};
#define DRAW_LINES(mesh, vertexAttribs) \
	g_krb->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS(mesh, vertexAttribs) \
	g_krb->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)