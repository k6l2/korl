#include "korl.glsl"
struct Korl_Light
{
    vec3 viewPosition;// @TODO: HACK: see hack in korl-vulkan.c
    vec3 colorAmbient;
    vec3 colorDiffuse;
    vec3 colorSpecular;
};
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
layout(set     = KORL_DESCRIPTOR_SET_MATERIALS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_BASE_TEXTURE) 
    uniform sampler2D baseTexture;
layout(set     = KORL_DESCRIPTOR_SET_MATERIALS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_SPECULAR_TEXTURE) 
    uniform sampler2D specularTexture;
layout(set     = KORL_DESCRIPTOR_SET_MATERIALS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_MATERIALS_EMISSIVE_TEXTURE) 
    uniform sampler2D emissiveTexture;
layout(location = KORL_FRAGMENT_INPUT_COLOR)         in vec4 fragmentColor;
layout(location = KORL_FRAGMENT_INPUT_UV)            in vec2 fragmentUv;
layout(location = KORL_FRAGMENT_INPUT_VIEW_NORMAL)   in vec3 fragmentViewNormal;
layout(location = KORL_FRAGMENT_INPUT_VIEW_POSITION) in vec3 fragmentViewPosition;
vec4 korl_glsl_fragment_computeLightColor(const in vec3 factorEmissive)
{
    /* ambient */
    const vec3  lightAmbient = light.colorAmbient * texture(baseTexture, fragmentUv).rgb;
    /* diffuse */
    const vec3  fragmentViewNormal_normal             = normalize(fragmentViewNormal);// re-normalize this interpolated value
    const vec3  viewPosition_fragment_to_light_normal = normalize(light.viewPosition - fragmentViewPosition);
    const float diffuseStrength                       = max(dot(fragmentViewNormal_normal, viewPosition_fragment_to_light_normal), 0.0);
    const vec3  lightDiffuse                          = diffuseStrength * light.colorDiffuse * texture(baseTexture, fragmentUv).rgb;
    /* specular */
    const vec3  view_fragment_to_camera_normal = normalize(-fragmentViewPosition);
    const vec3  reflect_direction              = reflect(-viewPosition_fragment_to_light_normal, fragmentViewNormal_normal);
    const float specularStrength               = pow(max(dot(view_fragment_to_camera_normal, reflect_direction), 0.0), material.shininess);
    const vec3  lightSpecular                  = specularStrength * light.colorSpecular * texture(specularTexture, fragmentUv).rgb * material.colorFactorSpecular.rgb * vec3(material.colorFactorSpecular.a);
    /* emissive */
    const vec3  lightEmissive = texture(emissiveTexture, fragmentUv).rgb * material.colorFactorEmissive * factorEmissive;
    /**/
    return vec4(lightAmbient + lightDiffuse + lightSpecular + lightEmissive, material.colorFactorBase.a);
}
