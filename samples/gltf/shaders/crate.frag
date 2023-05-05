#version 450
#extension GL_GOOGLE_include_directive : require
#include "../../../shaders/korl-lighting-fragment.glsl"
layout(location = KORL_FRAGMENT_OUTPUT_COLOR) out vec4 outColor;
void main()
{
    const vec3 maskSpecularMap = step(vec3(0.99), vec3(1.0) - texture(specularTexture, fragmentUv).rgb);
    outColor = korl_glsl_fragment_computeLightColor(maskSpecularMap/*factorEmissive*/) * fragmentColor;
}
