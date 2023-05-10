#version 450
#extension GL_GOOGLE_include_directive : require
#include "../../../shaders/korl-lighting-fragment.glsl"
layout(set     = KORL_DESCRIPTOR_SET_SCENE_TRANSFORMS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_SCENE_TRANSFORMS_UBO_VP
      ,row_major) 
    uniform Korl_UniformBufferObject_SceneProperties
{
    Korl_SceneProperties sceneProperties;
};
layout(location = KORL_FRAGMENT_INPUT_COLOR) in vec4 fragmentColor;
layout(location = KORL_FRAGMENT_OUTPUT_COLOR) out vec4 outColor;
void main()
{
    const vec3  maskSpecularMap = step(vec3(0.99), vec3(1.0) - texture(specularTexture, fragmentUv).rgb);
    const vec2  uvEmissive      = fragmentUv - vec2(0, sceneProperties.seconds * 0.5);// scroll the emissive map V-coordinate to simulate "digital rain"
    const float emissiveWave    = abs(sin(2 * sceneProperties.seconds));
    outColor = korl_glsl_fragment_computeLightColor(uvEmissive, emissiveWave * maskSpecularMap/*factorEmissive*/) * fragmentColor;
}
