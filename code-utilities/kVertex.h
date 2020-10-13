#pragma once
struct Vertex
{
	v3f32 position;
	v2f32 textureNormal;
	Color4f32 color;
};
/* KRB vertex attribute specifications */
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
/* useful drawing macros */
#define DRAW_POINTS(krbApi, mesh, vertexAttribs) \
	(krbApi)->drawPoints(\
		mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_LINES(krbApi, mesh, vertexAttribs) \
	(krbApi)->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS(krbApi, mesh, vertexAttribs) \
	(krbApi)->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS_DYNAMIC(krbApi, mesh, meshSize, vertexAttribs) \
	(krbApi)->drawTris(mesh, meshSize, sizeof(mesh[0]), vertexAttribs)
