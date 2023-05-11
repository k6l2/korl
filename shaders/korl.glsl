#define KORL_DESCRIPTOR_SET_SCENE_TRANSFORMS 0
#define KORL_DESCRIPTOR_SET_LIGHTS           1
#define KORL_DESCRIPTOR_SET_STORAGE          2
#define KORL_DESCRIPTOR_SET_MATERIALS        3
#define KORL_DESCRIPTOR_SET_BINDING_SCENE_TRANSFORMS_UBO_VP    0
#define KORL_DESCRIPTOR_SET_BINDING_LIGHTS_SSBO                0
#define KORL_DESCRIPTOR_SET_BINDING_STORAGE_SSBO               0
#define KORL_DESCRIPTOR_SET_BINDING_MATERIALS_UBO              0
#define KORL_DESCRIPTOR_SET_BINDING_MATERIALS_BASE_TEXTURE     1
#define KORL_DESCRIPTOR_SET_BINDING_MATERIALS_SPECULAR_TEXTURE 2
#define KORL_DESCRIPTOR_SET_BINDING_MATERIALS_EMISSIVE_TEXTURE 3
#define KORL_VERTEX_INPUT_POSITION          0
#define KORL_VERTEX_INPUT_COLOR             1
#define KORL_VERTEX_INPUT_UV                2
#define KORL_VERTEX_INPUT_INSTANCE_POSITION 3// KORL-ISSUE-000-000-151: re-use KORL_VERTEX_INPUT_POSITION location?
#define KORL_VERTEX_INPUT_INSTANCE_UINT     4
#define KORL_VERTEX_INPUT_NORMAL            5
#define KORL_FRAGMENT_INPUT_COLOR         0
#define KORL_FRAGMENT_INPUT_UV            1
#define KORL_FRAGMENT_INPUT_VIEW_NORMAL   2
#define KORL_FRAGMENT_INPUT_VIEW_POSITION 3
#define KORL_FRAGMENT_OUTPUT_COLOR 0
struct Korl_SceneProperties
{
    mat4  projection;
    mat4  view;
    float seconds;
};
struct Korl_Vertex_PushConstants
{
    mat4 model;
};
struct Korl_Material
{
    vec4  colorFactorBase;
    vec3  colorFactorEmissive;
    vec4  colorFactorSpecular;
    float shininess;
};