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
layout(location = KORL_FRAGMENT_INPUT_COLOR) out vec4 fragmentColor;
void main()
{
    gl_Position   = vpTransforms.projection * vpTransforms.view * pushConstants.model * vec4(attributePosition, 1.0);
    fragmentColor = vec4(1.0);
}
