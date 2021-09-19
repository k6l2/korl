#version 450
layout(binding = 0) uniform UniformBufferObject
{
    mat4 projection;
    mat4 view;
} ubo;
layout(location = 0) in vec3 attributePosition;
layout(location = 1) in vec3 attributeColor;
layout(location = 0) out vec3 fragmentColor;
void main() 
{
    gl_Position = ubo.projection * ubo.view * vec4(attributePosition, 1.0);
    fragmentColor = attributeColor;
}
