/** derived from: 
 * https://catlikecoding.com/unity/tutorials/advanced-rendering/flat-and-wireframe-shading/Flat-and-Wireframe-Shading.pdf 
 * http://www2.imm.dtu.dk/pubdb/edoc/imm4884.pdf 
 * Note that we use `material.colorFactorSpecular` to determine the color of the 
 * wireframe.  In addition, if the user provides a colorFactorBase whose `a` 
 * component == 0, the non-wireframe fragments of the triangles are discarded 
 * (masked opacity).  Otherwise, the non-wireframe fragments are filled with the 
 * colorFactorBase.  */
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
layout(location = KORL_FRAGMENT_INPUT_UV)                   in vec2 fragmentUv;
layout(location = KORL_FRAGMENT_INPUT_VIEW_NORMAL)          in vec3 fragmentViewNormal;
layout(location = KORL_FRAGMENT_INPUT_VIEW_POSITION)        in vec3 fragmentViewPosition;
layout(location = KORL_FRAGMENT_INPUT_COLOR)                in vec4 fragmentColor;
layout(location = KORL_FRAGMENT_INPUT_BARYCENTRIC_POSITION) in vec2 fragmentBarycentricPosition;
layout(location = KORL_FRAGMENT_OUTPUT_COLOR) out vec4 outColor;
void main()
{
    /* we want to make the surface appear "brighter" if it is pointing towards 
        the camera (viewPosition=={0,0,0}) & "darker" if it is pointing away */
    /* to achieve this, all we need is the dot product between the fragment's 
        viewNormal & the fragment's viewPosition_to_camera */
    const vec3  viewPosition_to_camera   = normalize(vec3(0) - fragmentViewPosition);
    const float brightness               = clamp(dot(normalize(fragmentViewNormal), viewPosition_to_camera), 0.2, 1);
    const vec3  barycentricColor         = vec3(fragmentBarycentricPosition, 1 - fragmentBarycentricPosition.x - fragmentBarycentricPosition.y);
    const vec3  barycentricDerivatives   = fwidth(barycentricColor);
    const float wireframeThickness       = 1;
    const float wireframeSmoothing       = 1;
    const vec3  dBarycentricStepped      = smoothstep(barycentricDerivatives *  wireframeThickness
                                                     ,barycentricDerivatives * (wireframeThickness + wireframeSmoothing)
                                                     ,barycentricColor);
    const float minBarycentric           = min(dBarycentricStepped.x, min(dBarycentricStepped.y, dBarycentricStepped.z));
    if(minBarycentric > 0 && material.colorFactorBase.a == 0)
        discard;
    outColor = brightness 
               * fragmentColor 
               * (  vec4(vec3(    minBarycentric), 1) * vec4(texture(baseTexture, fragmentUv).rgb * material.colorFactorBase.rgb, 1)
                  + vec4(vec3(1 - minBarycentric), 1) * material.colorFactorSpecular);
}
