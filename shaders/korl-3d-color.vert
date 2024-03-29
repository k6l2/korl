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
layout(location = KORL_VERTEX_INPUT_POSITION) in vec3 attributePosition;
layout(location = KORL_VERTEX_INPUT_COLOR)    in vec4 attributeColor;
layout(location = KORL_FRAGMENT_INPUT_COLOR) out vec4 fragmentColor;
void main()
{
    gl_Position   = sceneProperties.projection * sceneProperties.view * pushConstants.model * vec4(attributePosition, 1.0);
    fragmentColor = attributeColor;
}
