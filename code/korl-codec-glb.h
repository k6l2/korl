/**
 * # Blender Export Notes
 * 
 *  - select `Include > Data > Custom Properties` to support custom korl-codec-glb & korl-resource-scene3d functionality (see: KORL-specific Custom Properties)
 *  - select `Material > PBR Extensions > Export Original PBR Specular` to support specular factors & maps in KORL materials
 *  - optional; recommended; DE-select `Transform > +Y Up` to maintain Blender workspace versors
 *  - optional; recommended; select `Remember Export Settings` so that you only have to do this once per Blender project
 * 
 * # KORL-specific Custom Properties
 * 
 * - Materials
 *   - korl-shader-vertex   : string
 *     - asset cache file path of a compiled SPIR-V shader (.spv) file
 *   - korl-shader-fragment : string
 *     - asset cache file path of a compiled SPIR-V shader (.spv) file
 * 
 * Useful docs:
 * https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
 * https://docs.blender.org/manual/en/latest/addons/import_export/scene_gltf2.html
 */
#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-utility-math.h"
#include "korl-interface-platform-memory.h"
typedef struct Korl_Codec_Gltf_Data
{
    u32 byteOffset;
    u32 size;// as in the array size; _not_ bytes
} Korl_Codec_Gltf_Data;
/** This struct will be stored as a single contiguous allocation in memory, 
 * where the byte offsets of its members refer to data immediately following 
 * this struct */
typedef struct Korl_Codec_Gltf
{
    u32 bytes;// # of bytes required for this struct, as well as all memory referred to by byteOffsets stored in this struct
    struct
    {
        Korl_Codec_Gltf_Data rawUtf8Version;
    } asset;
    i32                  scene;
    Korl_Codec_Gltf_Data scenes;
    Korl_Codec_Gltf_Data nodes;
    Korl_Codec_Gltf_Data meshes;
    Korl_Codec_Gltf_Data accessors;
    Korl_Codec_Gltf_Data bufferViews;
    Korl_Codec_Gltf_Data buffers;
    Korl_Codec_Gltf_Data materials;
    Korl_Codec_Gltf_Data textures;
    Korl_Codec_Gltf_Data images;
    Korl_Codec_Gltf_Data samplers;
    Korl_Codec_Gltf_Data skins;
    Korl_Codec_Gltf_Data animations;
} Korl_Codec_Gltf;
typedef struct Korl_Codec_Gltf_Scene
{
    Korl_Codec_Gltf_Data rawUtf8Name;
    Korl_Codec_Gltf_Data nodes;
} Korl_Codec_Gltf_Scene;
typedef struct Korl_Codec_Gltf_Node
{
    /** all of the members of this struct are _optional_ */
    Korl_Codec_Gltf_Data rawUtf8Name;
    i32                  mesh;
    i32                  skin;
    Korl_Codec_Gltf_Data children;// u32[]; array of node indices
    Korl_Math_V3f32      translation;
    Korl_Math_Quaternion rotation;
    Korl_Math_V3f32      scale;
} Korl_Codec_Gltf_Node;
korl_global_const Korl_Codec_Gltf_Node KORL_CODEC_GLTF_NODE_DEFAULT = {.mesh = -1, .skin = -1, .rotation = {0,0,0,1}, .scale = {1,1,1}};
typedef struct Korl_Codec_Gltf_Mesh
{
    Korl_Codec_Gltf_Data rawUtf8Name;
    Korl_Codec_Gltf_Data primitives;
} Korl_Codec_Gltf_Mesh;
typedef enum Korl_Codec_Gltf_Mesh_Primitive_Mode
    /* glTF-2.0 spec 5.24.4: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_mesh_primitive_mode */
    {KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_POINTS         = 0
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_LINES          = 1
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_LINE_LOOP      = 2
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_LINE_STRIP     = 3
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLES      = 4// DEFAULT
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLE_STRIP = 5
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLE_FAN   = 6
} Korl_Codec_Gltf_Mesh_Primitive_Mode;
enum Korl_Codec_Gltf_Mesh_Primitive_Attribute
    {KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_POSITION
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_NORMAL
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_TEXCOORD_0
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_JOINTS_0
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_JOINT_WEIGHTS_0
    ,KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_ENUM_COUNT
};
typedef struct Korl_Codec_Gltf_Mesh_Primitive
{
    Korl_Codec_Gltf_Mesh_Primitive_Mode mode;
    i32                                 attributes[KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_ENUM_COUNT];// Accessor index
    i32                                 indices;// Accessor index
    i32                                 material;// Material index
} Korl_Codec_Gltf_Mesh_Primitive;
korl_global_const Korl_Codec_Gltf_Mesh_Primitive KORL_CODEC_GLTF_MESH_PRIMITIVE_DEFAULT = {.mode = KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLES
                                                                                          ,.indices = -1, .material = -1};
typedef enum Korl_Codec_Gltf_Accessor_ComponentType
    /* these values are derived from the glTF-2.0 spec 3.6.2.2 */
    {KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_I8  = 5120
    ,KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U8  = 5121
    ,KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_I16 = 5122
    ,KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U16 = 5123
    ,KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U32 = 5125
    ,KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_F32 = 5126
} Korl_Codec_Gltf_Accessor_ComponentType;
typedef enum Korl_Codec_Gltf_Accessor_Type
    {KORL_CODEC_GLTF_ACCESSOR_TYPE_SCALAR
    ,KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC2
    ,KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC3
    ,KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC4
    ,KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT2
    ,KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT3
    ,KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT4
} Korl_Codec_Gltf_Accessor_Type;
typedef struct Korl_Codec_Gltf_Accessor
{
    i32                                    bufferView;// < 0 => all values are 0, and the attribute is tightly-packed
    Korl_Codec_Gltf_Accessor_Type          type;
    Korl_Codec_Gltf_Accessor_ComponentType componentType;
    u32                                    count;
    Korl_Codec_Gltf_Data                   min;// array size _must_ correspond to the # of elements defined by `type`, if defined
    Korl_Codec_Gltf_Data                   max;// array size _must_ correspond to the # of elements defined by `type`, if defined
} Korl_Codec_Gltf_Accessor;
korl_global_const Korl_Codec_Gltf_Accessor KORL_CODEC_GLTF_ACCESSOR_DEFAULT = {.bufferView = -1};
typedef struct Korl_Codec_Gltf_BufferView
{
    u32 buffer;
    u32 byteLength;
    u32 byteOffset;
    u8  byteStride;// glTF-2.0 spec 5.11.4: validRange=[4, 252]; 0 => attribute is tightly packed; for Accessor byte stride, call `korl_codec_gltf_accessor_getStride` instead of using this member directly
} Korl_Codec_Gltf_BufferView;
typedef struct Korl_Codec_Gltf_Buffer
{
    Korl_Codec_Gltf_Data stringUri;// 0 => undefined; "Uniform Resource Identifier"; even though this field implies this string is simply a file path (relative to this gltf file), it can also be a an embedded base64 string, representing the buffer's data itself
    u32                  byteLength;
    i32                  glbByteOffset;// only valid for the buffer backed by GLB chunk 1 binary data; the byte offset location of the buffer within the original GLB file
} Korl_Codec_Gltf_Buffer;
korl_global_const Korl_Codec_Gltf_Buffer KORL_CODEC_GLTF_BUFFER_DEFAULT = {.glbByteOffset = -1};
typedef enum Korl_Codec_Gltf_Material_AlphaMode
    {KORL_CODEC_GLTF_MATERIAL_ALPHA_MODE_OPAQUE// default
    ,KORL_CODEC_GLTF_MATERIAL_ALPHA_MODE_MASK
    ,KORL_CODEC_GLTF_MATERIAL_ALPHA_MODE_BLEND
} Korl_Codec_Gltf_Material_AlphaMode;
typedef struct Korl_Codec_Gltf_Material
{
    Korl_Codec_Gltf_Data               rawUtf8Name;
    Korl_Codec_Gltf_Material_AlphaMode alphaMode;
    f32                                alphaCutoff;
    bool                               doubleSided;
    struct
    {
        i32             specularColorTextureIndex;
        Korl_Math_V4f32 factors;// {x,y,z} => specularColorFactor; {w} => specularFactor
    } KHR_materials_specular;// extension doc: https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_specular/README.md
    struct
    {
        i32 baseColorTextureIndex;
    } pbrMetallicRoughness;
    struct
    {
        Korl_Codec_Gltf_Data rawUtf8KorlShaderVertex;
        Korl_Codec_Gltf_Data rawUtf8KorlShaderFragment;
        bool                 korlIsUnlit;// by default we assume all GLTF materials use a default lighting model; if true, we use default unlit built-in shaders (if none are provided via other properties), otherwise we use default lit built-in shaders
    } extras;
} Korl_Codec_Gltf_Material;
korl_global_const Korl_Codec_Gltf_Material KORL_CODEC_GLTF_MATERIAL_DEFAULT = {.alphaCutoff            = 0.5f
                                                                              ,.KHR_materials_specular = {.specularColorTextureIndex = -1, .factors = {1,1,1,1}}
                                                                              ,.pbrMetallicRoughness   = {.baseColorTextureIndex     = -1}};
typedef struct Korl_Codec_Gltf_Texture
{
    i32 sampler;
    i32 source;// Image index
} Korl_Codec_Gltf_Texture;
korl_global_const Korl_Codec_Gltf_Texture KORL_CODEC_GLTF_TEXTURE_DEFAULT = {.sampler = -1, .source = -1};
typedef enum Korl_Codec_Gltf_MimeType
    {KORL_CODEC_GLTF_MIME_TYPE_UNDEFINED
    ,KORL_CODEC_GLTF_MIME_TYPE_IMAGE_JPEG
    ,KORL_CODEC_GLTF_MIME_TYPE_IMAGE_PNG
} Korl_Codec_Gltf_MimeType;
typedef struct Korl_Codec_Gltf_Image
{
    Korl_Codec_Gltf_Data     rawUtf8Name;
    i32                      bufferView;
    Korl_Codec_Gltf_MimeType mimeType;
} Korl_Codec_Gltf_Image;
typedef enum Korl_Codec_Gltf_Sampler_MagFilter
    /* glTF 2.0 spec 5.26.1 */
    {KORL_CODEC_GLTF_SAMPLER_MAG_FILTER_NEAREST = 9728
    ,KORL_CODEC_GLTF_SAMPLER_MAG_FILTER_LINEAR  = 9729
} Korl_Codec_Gltf_Sampler_MagFilter;
typedef enum Korl_Codec_Gltf_Sampler_MinFilter
    /* glTF 2.0 spec 5.26.2 */
    {KORL_CODEC_GLTF_SAMPLER_MIN_FILTER_NEAREST                = 9728
    ,KORL_CODEC_GLTF_SAMPLER_MIN_FILTER_LINEAR                 = 9729
    ,KORL_CODEC_GLTF_SAMPLER_MIN_FILTER_NEAREST_MIPMAP_NEAREST = 9984
    ,KORL_CODEC_GLTF_SAMPLER_MIN_FILTER_LINEAR_MIPMAP_NEAREST  = 9985
    ,KORL_CODEC_GLTF_SAMPLER_MIN_FILTER_NEAREST_MIPMAP_LINEAR  = 9986
    ,KORL_CODEC_GLTF_SAMPLER_MIN_FILTER_LINEAR_MIPMAP_LINEAR   = 9987
} Korl_Codec_Gltf_Sampler_MinFilter;
typedef struct Korl_Codec_Gltf_Sampler
{
    Korl_Codec_Gltf_Sampler_MagFilter magFilter;
    Korl_Codec_Gltf_Sampler_MinFilter minFilter;
} Korl_Codec_Gltf_Sampler;
korl_global_const Korl_Codec_Gltf_Sampler KORL_CODEC_GLTF_SAMPLER_DEFAULT = {.magFilter = KORL_CODEC_GLTF_SAMPLER_MAG_FILTER_LINEAR
                                                                            ,.minFilter = KORL_CODEC_GLTF_SAMPLER_MIN_FILTER_LINEAR};
typedef struct Korl_Codec_Gltf_Skin
{
    Korl_Codec_Gltf_Data rawUtf8Name;        // optional
    Korl_Codec_Gltf_Data joints;             // _required_; u32[]; array of node indices
    i32                  inverseBindMatrices;// optional; accessor index
} Korl_Codec_Gltf_Skin;
korl_global_const Korl_Codec_Gltf_Skin KORL_CODEC_GLTF_SKIN_DEFAULT = {.inverseBindMatrices = -1};
typedef struct Korl_Codec_Gltf_Animation
{
    Korl_Codec_Gltf_Data rawUtf8Name;// optional
    Korl_Codec_Gltf_Data channels;   // _required_
    Korl_Codec_Gltf_Data samplers;   // _required_
} Korl_Codec_Gltf_Animation;
typedef enum Korl_Codec_Gltf_Animation_Channel_Target_Path
    {KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_TRANSLATION
    ,KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_ROTATION
    ,KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_SCALE
    ,KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_WEIGHTS
} Korl_Codec_Gltf_Animation_Channel_Target_Path;
typedef struct Korl_Codec_Gltf_Animation_Channel
{
    u32 sampler;// _required_
    struct
    {
        Korl_Codec_Gltf_Animation_Channel_Target_Path path;// _required_
        i32                                           node;// optional
    } target;// _required_
} Korl_Codec_Gltf_Animation_Channel;
korl_global_const Korl_Codec_Gltf_Animation_Channel KORL_CODEC_GLTF_ANIMATION_CHANNEL_DEFAULT = {.target = {.node = -1}};
typedef enum Korl_Codec_Gltf_Animation_Sampler_Interpolation
    {KORL_CODEC_GLTF_ANIMATION_SAMPLER_INTERPOLATION_LINEAR// default
    ,KORL_CODEC_GLTF_ANIMATION_SAMPLER_INTERPOLATION_STEP
    ,KORL_CODEC_GLTF_ANIMATION_SAMPLER_INTERPOLATION_CUBIC_SPLINE
} Korl_Codec_Gltf_Animation_Sampler_Interpolation;
typedef struct Korl_Codec_Gltf_Animation_Sampler
{
    u32                                             input;        // _required_; Accessor index; contains an array of keyframe timestamps
    Korl_Codec_Gltf_Animation_Sampler_Interpolation interpolation;// optional
    u32                                             output;       // _required_; Accessor index; contains keyframe output values
} Korl_Codec_Gltf_Animation_Sampler;
korl_internal Korl_Codec_Gltf*                   korl_codec_glb_decode(const void* glbData, u$ glbDataBytes, Korl_Memory_AllocatorHandle resultAllocator);
korl_internal acu8                               korl_codec_gltf_getUtf8(const Korl_Codec_Gltf* context, Korl_Codec_Gltf_Data gltfData);
korl_internal Korl_Codec_Gltf_Mesh*              korl_codec_gltf_getMeshes(const Korl_Codec_Gltf* context);
korl_internal acu8                               korl_codec_gltf_mesh_getName(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Mesh* mesh);
korl_internal Korl_Codec_Gltf_Mesh_Primitive*    korl_codec_gltf_mesh_getPrimitives(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Mesh* mesh);
korl_internal Korl_Codec_Gltf_Accessor*          korl_codec_gltf_getAccessors(const Korl_Codec_Gltf* context);
korl_internal u8                                 korl_codec_gltf_accessor_getComponentCount(const Korl_Codec_Gltf_Accessor* context);
korl_internal u32                                korl_codec_gltf_accessor_getStride(const Korl_Codec_Gltf_Accessor* context, const Korl_Codec_Gltf_BufferView* bufferViewArray);
korl_internal const f32*                         korl_codec_gltf_accessor_getMin(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Accessor* accessor);
korl_internal const f32*                         korl_codec_gltf_accessor_getMax(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Accessor* accessor);
korl_internal Korl_Codec_Gltf_BufferView*        korl_codec_gltf_getBufferViews(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Buffer*            korl_codec_gltf_getBuffers(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Texture*           korl_codec_gltf_getTextures(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Image*             korl_codec_gltf_getImages(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Sampler*           korl_codec_gltf_getSamplers(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Material*          korl_codec_gltf_getMaterials(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Skin*              korl_codec_gltf_getSkins(const Korl_Codec_Gltf* context);
korl_internal const u32*                         korl_codec_gltf_skin_getJointIndices(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Skin*const skin);
korl_internal Korl_Codec_Gltf_Node*              korl_codec_gltf_skin_getJointNode(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Skin*const skin, u32 jointIndex);
korl_internal Korl_Codec_Gltf_Node*              korl_codec_gltf_getNodes(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Node*              korl_codec_gltf_node_getChild(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Node*const node, u32 childIndex);
korl_internal Korl_Codec_Gltf_Animation*         korl_codec_gltf_getAnimations(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Animation_Sampler* korl_codec_gltf_getAnimationSamplers(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Animation* animation);
korl_internal Korl_Codec_Gltf_Animation_Channel* korl_codec_gltf_getAnimationChannels(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Animation* animation);
