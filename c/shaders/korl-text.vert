#version 450
layout(binding = 0, row_major) uniform UniformBufferObject
{
    mat4 projection;
    mat4 view;
} ubo;
layout(push_constant, row_major) uniform UniformPushConstants
{
    mat4 model;
    ///@TODO: add a vec4 uniform fragment color
} pushConstants;
struct GlyphVertex
{
    vec2 position;
    vec2 uv;
};
layout(binding = 2) readonly buffer BufferGlyphMeshData
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
    GlyphVertex glyphVertex = bufferGlyphMeshData.glyphVertices[instanceAttributeGlyphIndex + gl_VertexIndex];
    gl_Position   = ubo.projection * ubo.view * pushConstants.model * vec4(glyphVertex.position, 0.0, 1.0);
    fragmentColor = vec4(1.0);
    fragmentUv    = glyphVertex.uv;
}
