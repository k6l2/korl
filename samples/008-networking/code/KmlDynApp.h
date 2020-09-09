#pragma once
#include "TemplateGameState.h"
#include "kNetClient.h"
#include "serverExample.h"
struct GameState
{
	KmlTemplateGameState templateGameState;
	/* sample server state management */
	ServerState serverState;
	char clientAddressBuffer[256];
	KNetClient kNetClient;
	char clientReliableTextBuffer[256];
	u8 reliableMessageBuffer[KPL_MAX_DATAGRAM_SIZE];
	/* sample media testing state */
	KFlipBook kFbShip;
	KFlipBook kFbShipExhaust;
	Actor* actors;
};
global_variable GameState* g_gs;
/* graphics system vertex data customization */
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