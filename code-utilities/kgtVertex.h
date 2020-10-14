#pragma once
struct KgtVertex
{
	v3f32 position;
	v2f32 textureNormal;
	Color4f32 color;
};
/* KRB vertex attribute specifications */
const global_variable KrbVertexAttributeOffsets 
	KGT_VERTEX_ATTRIBS = 
		{ .position_3f32 = offsetof(KgtVertex, position)
		, .color_4f32    = offsetof(KgtVertex, color)
		, .texCoord_2f32 = offsetof(KgtVertex, textureNormal) };
const global_variable KrbVertexAttributeOffsets 
	KGT_VERTEX_ATTRIBS_NO_COLOR = 
		{ .position_3f32 = offsetof(KgtVertex, position)
		, .color_4f32    = sizeof(KgtVertex)
		, .texCoord_2f32 = offsetof(KgtVertex, textureNormal) };
const global_variable KrbVertexAttributeOffsets 
	KGT_VERTEX_ATTRIBS_POSITION_ONLY = 
		{ .position_3f32 = offsetof(KgtVertex, position)
		, .color_4f32    = sizeof(KgtVertex)
		, .texCoord_2f32 = sizeof(KgtVertex) };
const global_variable KrbVertexAttributeOffsets 
	KGT_VERTEX_ATTRIBS_NO_TEXTURE = 
		{ .position_3f32 = offsetof(KgtVertex, position)
		, .color_4f32    = offsetof(KgtVertex, color)
		, .texCoord_2f32 = sizeof(KgtVertex) };
