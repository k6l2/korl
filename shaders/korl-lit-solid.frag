#version 450
#extension GL_GOOGLE_include_directive : require
#include "korl.glsl"
layout(set     = KORL_DESCRIPTOR_SET_MATERIALS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_UBO) 
    uniform Korl_UniformBufferObject_Material
{
    Korl_Material material;
};
layout(set     = KORL_DESCRIPTOR_SET_MATERIALS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_BASE_TEXTURE) 
    uniform sampler2D baseTexture;
// layout(set     = KORL_DESCRIPTOR_SET_MATERIALS
//       ,binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_SPECULAR_TEXTURE) 
//     uniform sampler2D specularTexture;
// layout(set     = KORL_DESCRIPTOR_SET_MATERIALS
//       ,binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_EMISSIVE_TEXTURE) 
//     uniform sampler2D emissiveTexture;
layout(location = KORL_FRAGMENT_INPUT_UV)            in vec2 fragmentUv;
layout(location = KORL_FRAGMENT_INPUT_VIEW_NORMAL)   in vec3 fragmentViewNormal;
layout(location = KORL_FRAGMENT_INPUT_VIEW_POSITION) in vec3 fragmentViewPosition;
layout(location = KORL_FRAGMENT_INPUT_COLOR)         in vec4 fragmentColor;
layout(location = KORL_FRAGMENT_OUTPUT_COLOR) out vec4 outColor;
void main()
{
    /* we want to make the surface appear "brighter" if it is pointing towards 
        the camera (viewPosition=={0,0,0}) & "darker" if it is pointing away */
    /* to achieve this, all we need is the dot product between the fragment's 
        viewNormal & the fragment's viewPosition_to_camera */
    const vec3  viewPosition_to_camera = normalize(vec3(0) - fragmentViewPosition);
    const float brightness             = clamp(dot(normalize(fragmentViewNormal), viewPosition_to_camera), 0, 1);
    outColor = brightness * fragmentColor * vec4(texture(baseTexture, fragmentUv).rgb * material.colorFactorBase.rgb, 1);
}
