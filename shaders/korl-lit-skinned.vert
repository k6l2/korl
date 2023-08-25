// very useful source on how mesh skinning works: https://webglfundamentals.org/webgl/lessons/webgl-skinning.html
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
#define MAX_BONES 60
layout(set     = KORL_DESCRIPTOR_SET_STORAGE
      ,binding = KORL_DESCRIPTOR_SET_BINDING_STORAGE_UBO
      ,row_major) 
    uniform Korl_UniformBufferObject_Bones
{
    mat4 bones[MAX_BONES];
};
layout(push_constant, row_major) uniform Korl_UniformBufferObject_VertexPushConstants
{
    Korl_Vertex_PushConstants pushConstants;
};
layout(location = KORL_VERTEX_INPUT_POSITION)        in vec3 attributePosition;
layout(location = KORL_VERTEX_INPUT_UV)              in vec2 attributeUv;
layout(location = KORL_VERTEX_INPUT_NORMAL)          in vec3 attributeNormal;
layout(location = KORL_VERTEX_INPUT_JOINTS_0)        in vec4 attributeJoints0;// loaded using VK_FORMAT_R8G8B8A8_UNORM; max supported joints: 256
layout(location = KORL_VERTEX_INPUT_JOINT_WEIGHTS_0) in vec4 attributeJointWeights0;
layout(location = KORL_FRAGMENT_INPUT_COLOR)         out vec4 fragmentColor;
layout(location = KORL_FRAGMENT_INPUT_UV)            out vec2 fragmentUv;
layout(location = KORL_FRAGMENT_INPUT_VIEW_NORMAL)   out vec3 fragmentViewNormal;
layout(location = KORL_FRAGMENT_INPUT_VIEW_POSITION) out vec3 fragmentViewPosition;
void main()
{
    // note: all bones are expected to be already combined with their inverseBindMatrices
    // note: pushConstants.model is expected to be pre-multiplied with the root bone; ergo, all bones are in model-space; in other words, do _not_ use pushConstants.model anywhere in this shader
    const mat4 skinnedModel = bones[int(attributeJoints0[0] * 255)] * attributeJointWeights0[0]
                            + bones[int(attributeJoints0[1] * 255)] * attributeJointWeights0[1]
                            + bones[int(attributeJoints0[2] * 255)] * attributeJointWeights0[2]
                            + bones[int(attributeJoints0[3] * 255)] * attributeJointWeights0[3];
    const mat3 normalMatrix = mat3(transpose(inverse(skinnedModel)));
    fragmentColor        = vec4(1.0);
    fragmentUv           = attributeUv;
    fragmentViewNormal   = normalize(mat3(sceneProperties.view) * normalMatrix * attributeNormal);
    fragmentViewPosition = vec3(sceneProperties.view * skinnedModel * vec4(attributePosition, 1.0));
    gl_Position          = sceneProperties.projection * sceneProperties.view * skinnedModel * vec4(attributePosition, 1.0);
}
