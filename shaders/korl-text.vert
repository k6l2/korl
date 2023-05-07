#version 450
#extension GL_GOOGLE_include_directive : require
#include "korl.glsl"
layout(set     = KORL_DESCRIPTOR_SET_SCENE_TRANSFORMS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_SCENE_TRANSFORMS_UBO_VP
      ,row_major) 
    uniform Korl_UniformBufferObject_SceneProperties
{
    Korl_SceneProperties sceneProperties;
};
layout(push_constant, row_major) uniform Korl_UniformBufferObject_VertexPushConstants
{
    Korl_Vertex_PushConstants pushConstants;
};
struct GlyphVertex
{
    vec4 position2d_uv;// position occupies .xy, uv occupies .zw
};
layout(set     = KORL_DESCRIPTOR_SET_STORAGE
      ,binding = KORL_DESCRIPTOR_SET_BINDING_STORAGE_SSBO) 
    readonly buffer BufferGlyphMeshData
{
    GlyphVertex glyphVertices[];
};
layout(location = KORL_VERTEX_INPUT_INSTANCE_POSITION) in vec2 instanceAttributeGlyphPosition;// KORL-ISSUE-000-000-151: re-use KORL_VERTEX_INPUT_POSITION location? it would be nice if we had a way to configure any given attribute as per-vertex/instance in korl-vulkan
layout(location = KORL_VERTEX_INPUT_INSTANCE_UINT)     in uint instanceAttributeGlyphIndex;
layout(location = KORL_FRAGMENT_INPUT_COLOR) out vec4 fragmentColor;
layout(location = KORL_FRAGMENT_INPUT_UV)    out vec2 fragmentUv;
void main()
{
    const uint glyphMeshVertexIndex = 4*instanceAttributeGlyphIndex + gl_VertexIndex;
    const GlyphVertex glyphVertex   = glyphVertices[glyphMeshVertexIndex];
    const vec2 modelPosition2d      = instanceAttributeGlyphPosition + glyphVertex.position2d_uv.xy;
    gl_Position   = sceneProperties.projection * sceneProperties.view * pushConstants.model * vec4(modelPosition2d, 0.0, 1.0);
    fragmentColor = vec4(1.0);
    fragmentUv    = glyphVertex.position2d_uv.zw;
}
