#pragma once
struct Vertex
{
	v3f32 position;
	v2f32 textureNormal;
	Color4f32 color;
};
/* KRB vertex attribute specifications */
const global_variable KrbVertexAttributeOffsets 
	VERTEX_ATTRIBS = 
		{ .position_3f32 = offsetof(Vertex, position)
		, .color_4f32    = offsetof(Vertex, color)
		, .texCoord_2f32 = offsetof(Vertex, textureNormal) };
const global_variable KrbVertexAttributeOffsets 
	VERTEX_ATTRIBS_NO_COLOR = 
		{ .position_3f32 = offsetof(Vertex, position)
		, .color_4f32    = sizeof(Vertex)
		, .texCoord_2f32 = offsetof(Vertex, textureNormal) };
const global_variable KrbVertexAttributeOffsets 
	VERTEX_ATTRIBS_POSITION_ONLY = 
		{ .position_3f32 = offsetof(Vertex, position)
		, .color_4f32    = sizeof(Vertex)
		, .texCoord_2f32 = sizeof(Vertex) };
const global_variable KrbVertexAttributeOffsets 
	VERTEX_ATTRIBS_NO_TEXTURE = 
		{ .position_3f32 = offsetof(Vertex, position)
		, .color_4f32    = offsetof(Vertex, color)
		, .texCoord_2f32 = sizeof(Vertex) };
