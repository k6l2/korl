#version 450
#extension GL_GOOGLE_include_directive : require
#include "korl.glsl"
layout(set     = KORL_DESCRIPTOR_SET_MATERIALS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_UBO) 
    uniform Korl_UniformBufferObject_Material
{
    Korl_Material material;
};
layout(location = KORL_FRAGMENT_INPUT_COLOR)  in  vec4 fragmentColor;
layout(location = KORL_FRAGMENT_OUTPUT_COLOR) out vec4 outColor;
void main() 
{
    outColor = fragmentColor * material.color;
}
