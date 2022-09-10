#version 450
// layout(binding = 0, row_major) uniform UniformBufferObject
// {
//     mat4 projection;
//     mat4 view;
//     mat4 model;
// } ubo;
layout(location = 0) in vec3 attributePosition;
layout(location = 1) in vec4 attributeColor;
//layout(location = 2) in vec2 attributeUv;
layout(location = 0) out vec4 fragmentColor;
//layout(location = 1) out vec2 fragmentUv;
void main() 
{
    // gl_Position = ubo.projection * ubo.view * ubo.model * vec4(attributePosition, 1.0);
    gl_Position = vec4(attributePosition, 1.0);
    fragmentColor = attributeColor;
    //fragmentUv = attributeUv;
}
