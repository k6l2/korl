#version 450
layout(binding = 0, set = 1, row_major) uniform UniformBufferObject_Lights
{
    vec3 viewPosition;// @TODO: HACK: see hack in korl-vulkan.c
    vec4 color;
} uboLights;
// layout(binding = 0, set = 2) uniform sampler2D textureSampler;
layout(location = 0) in vec4 fragmentColor;
layout(location = 1) in vec2 fragmentUv;
layout(location = 2) in vec3 fragmentViewNormal;
layout(location = 3) in vec3 fragmentViewPosition;
layout(location = 0) out vec4 outColor;
void main() 
{
    const vec3  fragmentViewNormal_normal             = normalize(fragmentViewNormal);// re-normalize this interpolated value
    const vec3  viewPosition_fragment_to_light_normal = normalize(uboLights.viewPosition - fragmentViewPosition);
    const float diffuseStrength                       = max(dot(fragmentViewNormal_normal, viewPosition_fragment_to_light_normal), 0.0);
    const float ambientStrength                       = 0.1;
    const vec3  colorLight                            = (ambientStrength + diffuseStrength) * uboLights.color.rgb;
    outColor = vec4(colorLight,1) * fragmentColor;
    // outColor = fragmentColor*texture(textureSampler, fragmentUv);
}
