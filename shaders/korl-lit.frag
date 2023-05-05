#version 450
#extension GL_GOOGLE_include_directive : require
#include "korl-lighting-fragment.glsl"
layout(location = KORL_FRAGMENT_OUTPUT_COLOR) out vec4 outColor;
void main()
{
    outColor = korl_glsl_fragment_computeLightColor(vec3(1)/*factorEmissive*/) * fragmentColor;
}
