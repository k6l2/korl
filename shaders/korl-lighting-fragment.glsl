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
    vec3                   viewPosition;// NOTE: in KORL C code, the position & direction are in world-space, but for convenience we transform them into view-space for GLSL
    vec3                   viewDirection;// _not_ used in point lights; see NOTE for `viewPosition`
    Korl_Light_Color       color;
    Korl_Light_Attenuation attenuation;// used for point lights
    Korl_Light_Cutoff      cutoffCosines;// used for spot lights
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
    const vec3 fragmentViewNormal_normal = normalize(fragmentViewNormal);// re-normalize this interpolated value
    vec3  viewPosition_fragment_to_light_normal;
    float attenuation;
    float factorDiffuseSpecular;
    switch(light.type)
    {
    case KORL_LIGHT_TYPE_SPOT:{
        const vec3  viewPosition_fragment_to_light        = light.viewPosition - fragmentViewPosition;
        const float viewPosition_fragment_to_light_length = length(viewPosition_fragment_to_light);
        viewPosition_fragment_to_light_normal = viewPosition_fragment_to_light / viewPosition_fragment_to_light_length;
        attenuation                           = 1 / (  light.attenuation.constant 
                                                     + light.attenuation.linear * viewPosition_fragment_to_light_length 
                                                     + light.attenuation.quadratic * pow(viewPosition_fragment_to_light_length, 2));
        const float cosine_fragToLight_inverseLightDir = dot(viewPosition_fragment_to_light_normal, normalize(-light.viewDirection));
        const float cosineCutoffDiff                   = light.cutoffCosines.inner - light.cutoffCosines.outer;
        factorDiffuseSpecular                          = clamp((cosine_fragToLight_inverseLightDir - light.cutoffCosines.outer) / cosineCutoffDiff, 0, 1);
        break;}
    case KORL_LIGHT_TYPE_POINT:{
        const vec3  viewPosition_fragment_to_light        = light.viewPosition - fragmentViewPosition;
        const float viewPosition_fragment_to_light_length = length(viewPosition_fragment_to_light);
        viewPosition_fragment_to_light_normal = viewPosition_fragment_to_light / viewPosition_fragment_to_light_length;
        attenuation                           = 1 / (  light.attenuation.constant 
                                                     + light.attenuation.linear * viewPosition_fragment_to_light_length 
                                                     + light.attenuation.quadratic * pow(viewPosition_fragment_to_light_length, 2));
        factorDiffuseSpecular                 = 1;
        break;}
    case KORL_LIGHT_TYPE_DIRECTIONAL:{
        viewPosition_fragment_to_light_normal = normalize(-light.viewDirection);
        attenuation                           = 1;
        factorDiffuseSpecular                 = 1;
        break;}
    }
    /* ambient */
    const vec3  lightAmbient = light.color.ambient * texture(baseTexture, fragmentUv).rgb * material.colorFactorBase.rgb;
    /* diffuse */
    const float diffuseStrength = max(dot(fragmentViewNormal_normal, viewPosition_fragment_to_light_normal), 0.0);
    const vec3  lightDiffuse    = diffuseStrength * light.color.diffuse * texture(baseTexture, fragmentUv).rgb * material.colorFactorBase.rgb;
    /* specular */
    const vec3  view_fragment_to_camera_normal = normalize(-fragmentViewPosition);
    const vec3  reflect_direction              = reflect(-viewPosition_fragment_to_light_normal, fragmentViewNormal_normal);
    const float specularStrength               = pow(max(dot(view_fragment_to_camera_normal, reflect_direction), 0.0), material.shininess);
    const vec3  lightSpecular                  = specularStrength * light.color.specular * texture(specularTexture, fragmentUv).rgb * material.colorFactorSpecular.rgb * vec3(material.colorFactorSpecular.a);
    /* emissive */
    const vec3  lightEmissive = texture(emissiveTexture, uvEmissive).rgb * material.colorFactorEmissive * factorEmissive;
    /**/
    return attenuation * (lightAmbient + factorDiffuseSpecular * (lightDiffuse + lightSpecular)) + lightEmissive;
}
vec4 korl_glsl_fragment_computeLightColor(const in vec2 uvEmissive, const in vec3 factorEmissive)
{
    vec3 lightColorTotal = vec3(0);
    for(uint i = 0; i < lightsSize; i++)
        lightColorTotal += _korl_glsl_fragment_computeLightColor(lights[i], uvEmissive, factorEmissive);
    return vec4(lightColorTotal, material.colorFactorBase.a);
}
