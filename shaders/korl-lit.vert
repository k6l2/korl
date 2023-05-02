#version 450
#extension GL_GOOGLE_include_directive : require
#include "korl.glsl"
layout(set     = KORL_DESCRIPTOR_SET_SCENE_TRANSFORMS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_SCENE_TRANSFORMS_UBO_VP
      ,row_major) 
    uniform Korl_UniformBufferObject_VpTransforms
{
    Korl_VpTransforms vpTransforms;
};
layout(push_constant, row_major) uniform Korl_UniformBufferObject_VertexPushConstants
{
    Korl_Vertex_PushConstants pushConstants;
};
layout(location = KORL_VERTEX_INPUT_POSITION) in vec3 attributePosition;
// layout(location = KORL_VERTEX_INPUT_COLOR)    in vec4 attributeColor;
layout(location = KORL_VERTEX_INPUT_UV)       in vec2 attributeUv;
layout(location = KORL_VERTEX_INPUT_NORMAL)   in vec3 attributeNormal;
layout(location = KORL_FRAGMENT_INPUT_COLOR)         out vec4 fragmentColor;
layout(location = KORL_FRAGMENT_INPUT_UV)            out vec2 fragmentUv;
layout(location = KORL_FRAGMENT_INPUT_VIEW_NORMAL)   out vec3 fragmentViewNormal;
layout(location = KORL_FRAGMENT_INPUT_VIEW_POSITION) out vec3 fragmentViewPosition;
void main() 
{
    const mat3 normalMatrix = mat3(transpose(inverse(pushConstants.model)));
    fragmentColor        = vec4(1.0);
    fragmentUv           = attributeUv;
    fragmentViewNormal   = normalize(mat3(vpTransforms.view) * normalMatrix * attributeNormal);
    fragmentViewPosition = vec3(vpTransforms.view * pushConstants.model * vec4(attributePosition, 1.0));
    gl_Position          = vpTransforms.projection * vpTransforms.view * pushConstants.model * vec4(attributePosition, 1.0);
}
