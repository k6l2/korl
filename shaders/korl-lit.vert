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
layout(location = 0) in vec3 attributePosition;
// layout(location = 1) in vec4 attributeColor;
layout(location = 2) in vec2 attributeUv;
layout(location = 5) in vec3 attributeNormal;
layout(location = 0) out vec4 fragmentColor;
layout(location = 1) out vec2 fragmentUv;
layout(location = 2) out vec3 fragmentViewNormal;
layout(location = 3) out vec3 fragmentViewPosition;
void main() 
{
    const mat3 normalMatrix = mat3(transpose(inverse(pushConstants.model)));
    gl_Position          = ubo.projection * ubo.view * pushConstants.model * vec4(attributePosition, 1.0);
    fragmentColor        = pushConstants.color;
    fragmentUv           = attributeUv;
    fragmentViewNormal   = normalize(mat3(ubo.view) * normalMatrix * attributeNormal);
    fragmentViewPosition = vec3(ubo.view * pushConstants.model * vec4(attributePosition, 1.0));
}
