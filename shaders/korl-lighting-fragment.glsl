#include "korl.glsl"
const uint KORL_LIGHT_TYPE_POINT       = 1;
const uint KORL_LIGHT_TYPE_DIRECTIONAL = 2;
const uint KORL_LIGHT_TYPE_SPOT        = 3;
struct Korl_Light_Color
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
struct Korl_Light_Attenuation
{
    float constant;
    float linear;
    float quadratic;
};
struct Korl_Light_Cutoff
{
    float inner;
    float outer;
};
struct Korl_Light
{
    uint                   type;// _must_ == `KORL_LIGHT_TYPE_*`
    // @TODO: HACK: see hack in korl-vulkan.c; view-space transform destroys symmetry between the GLSL/C Light structs
    vec3                   viewPosition;
    vec3                   viewDirection;// _not_ used in point lights
    Korl_Light_Color       color;
    Korl_Light_Attenuation attenuation;// used for point lights
    Korl_Light_Cutoff      cutoffs;// used for spot lights
};
layout(set     = KORL_DESCRIPTOR_SET_LIGHTS
      ,binding = KORL_DESCRIPTOR_SET_BINDING_LIGHTS_SSBO) 
    readonly buffer Korl_UniformBufferObject_Lights
{
    uint       lightsSize;
    Korl_Light lights[];
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
layout(location = KORL_FRAGMENT_INPUT_UV)            in vec2 fragmentUv;
layout(location = KORL_FRAGMENT_INPUT_VIEW_NORMAL)   in vec3 fragmentViewNormal;
layout(location = KORL_FRAGMENT_INPUT_VIEW_POSITION) in vec3 fragmentViewPosition;
vec3 _korl_glsl_fragment_computeLightColor(Korl_Light light, const in vec2 uvEmissive, const in vec3 factorEmissive)
{
    //@TODO: test with a single point light before splitting this implementation by light.type
    /* ambient */
    const vec3  lightAmbient = light.color.ambient * texture(baseTexture, fragmentUv).rgb;
    /* diffuse */
    const vec3  fragmentViewNormal_normal             = normalize(fragmentViewNormal);// re-normalize this interpolated value
    const vec3  viewPosition_fragment_to_light_normal = normalize(light.viewPosition - fragmentViewPosition);
    const float diffuseStrength                       = max(dot(fragmentViewNormal_normal, viewPosition_fragment_to_light_normal), 0.0);
    const vec3  lightDiffuse                          = diffuseStrength * light.color.diffuse * texture(baseTexture, fragmentUv).rgb;
    /* specular */
    const vec3  view_fragment_to_camera_normal = normalize(-fragmentViewPosition);
    const vec3  reflect_direction              = reflect(-viewPosition_fragment_to_light_normal, fragmentViewNormal_normal);
    const float specularStrength               = pow(max(dot(view_fragment_to_camera_normal, reflect_direction), 0.0), material.shininess);
    const vec3  lightSpecular                  = specularStrength * light.color.specular * texture(specularTexture, fragmentUv).rgb * material.colorFactorSpecular.rgb * vec3(material.colorFactorSpecular.a);
    /* emissive */
    const vec3  lightEmissive = texture(emissiveTexture, uvEmissive).rgb * material.colorFactorEmissive * factorEmissive;
    /**/
    return lightAmbient + lightDiffuse + lightSpecular + lightEmissive;
}
vec4 korl_glsl_fragment_computeLightColor(const in vec2 uvEmissive, const in vec3 factorEmissive)
{
    vec3 lightColorTotal = vec3(0);
    for(uint i = 0; i < lightsSize; i++)
        lightColorTotal += _korl_glsl_fragment_computeLightColor(lights[i], uvEmissive, factorEmissive);
    return vec4(lightColorTotal, material.colorFactorBase.a);
}
