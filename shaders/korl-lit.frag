#version 450
#extension GL_GOOGLE_include_directive : require
#include "korl.glsl"
layout(set     = KORL_DESCRIPTOR_SET_LIGHTS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_LIGHTS_UBO
      ,row_major) 
    uniform Korl_UniformBufferObject_Lights
{
    Korl_Light light;
};
layout(set     = KORL_DESCRIPTOR_SET_MATERIALS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_UBO) 
    uniform Korl_UniformBufferObject_Material
{
    Korl_Material material;
};
// layout(set = KORL_DESCRIPTOR_SET_MATERIALS, binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_TEXTURE) uniform sampler2D textureSampler;
layout(location = KORL_FRAGMENT_INPUT_COLOR)         in vec4 fragmentColor;
layout(location = KORL_FRAGMENT_INPUT_UV)            in vec2 fragmentUv;
layout(location = KORL_FRAGMENT_INPUT_VIEW_NORMAL)   in vec3 fragmentViewNormal;
layout(location = KORL_FRAGMENT_INPUT_VIEW_POSITION) in vec3 fragmentViewPosition;
layout(location = KORL_FRAGMENT_OUTPUT_COLOR) out vec4 outColor;
void main() 
{
    /* ambient */
    const vec3  lightAmbient                          = material.ambient * light.colorAmbient;
    /* diffuse */
    const vec3  fragmentViewNormal_normal             = normalize(fragmentViewNormal);// re-normalize this interpolated value
    const vec3  viewPosition_fragment_to_light_normal = normalize(light.viewPosition - fragmentViewPosition);
    const float diffuseStrength                       = max(dot(fragmentViewNormal_normal, viewPosition_fragment_to_light_normal), 0.0);
    const vec3  lightDiffuse                          = diffuseStrength * material.diffuse * light.colorDiffuse;
    /* specular */
    const vec3  view_fragment_to_camera_normal        = normalize(-fragmentViewPosition);
    const vec3  reflect_direction                     = reflect(-viewPosition_fragment_to_light_normal, fragmentViewNormal_normal);
    const float specularStrength                      = pow(max(dot(view_fragment_to_camera_normal, reflect_direction), 0.0), material.shininess);
    const vec3  lightSpecular                         = specularStrength * material.specular * light.colorSpecular;
    /**/
    outColor = vec4(lightAmbient + lightDiffuse + lightSpecular, 1) * fragmentColor;
    // outColor = fragmentColor*texture(textureSampler, fragmentUv);
}
