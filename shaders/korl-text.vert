#version 450
layout(binding = 0, set = 0, row_major) uniform UniformBufferObject
{
    mat4 projection;
    mat4 view;
} ubo;
layout(push_constant, row_major) uniform UniformPushConstants
{
    mat4 model;
    vec4 color;
} pushConstants;
struct GlyphVertex
{
    vec4 position2d_uv;// position occupies .xy, uv occupies .zw
};
layout(binding = 0, set = 2) readonly buffer BufferGlyphMeshData
{
    GlyphVertex glyphVertices[];
} bufferGlyphMeshData;
// layout(location = 0) in vec2 attributePosition;
// layout(location = 1) in vec4 attributeColor;
//layout(location = 2) in vec2 attributeUv;
layout(location = 3) in vec2 instanceAttributeGlyphPosition;
layout(location = 4) in uint instanceAttributeGlyphIndex;
layout(location = 0) out vec4 fragmentColor;
layout(location = 1) out vec2 fragmentUv;
void main() 
{
    const uint glyphMeshVertexIndex = 4*instanceAttributeGlyphIndex + gl_VertexIndex;
    const GlyphVertex glyphVertex   = bufferGlyphMeshData.glyphVertices[glyphMeshVertexIndex];
    const vec2 modelPosition2d      = instanceAttributeGlyphPosition + glyphVertex.position2d_uv.xy;
    gl_Position   = ubo.projection * ubo.view * pushConstants.model * vec4(modelPosition2d, 0.0, 1.0);
    fragmentColor = pushConstants.color;
    fragmentUv    = glyphVertex.position2d_uv.zw;
}
