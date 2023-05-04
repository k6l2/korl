#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
#include "korl-interface-platform-memory.h"
typedef struct Korl_Codec_Gltf_Data
{
    u32 byteOffset;
    //@TODO: just rename this to "bytes"?
    u32 size;// in bytes
} Korl_Codec_Gltf_Data;
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
} Korl_Codec_Gltf;
typedef struct Korl_Codec_Gltf_Scene
{
    Korl_Codec_Gltf_Data rawUtf8Name;
    Korl_Codec_Gltf_Data nodes;
} Korl_Codec_Gltf_Scene;
typedef struct Korl_Codec_Gltf_Node
{
    Korl_Codec_Gltf_Data rawUtf8Name;
    i32                  mesh;
} Korl_Codec_Gltf_Node;
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
typedef struct Korl_Codec_Gltf_Mesh_Primitive
{
    Korl_Codec_Gltf_Mesh_Primitive_Mode mode;
    struct
    {
        i32 position;
        i32 normal;
        i32 texCoord0;
    } attributes;
    i32 indices;
    i32 material;
} Korl_Codec_Gltf_Mesh_Primitive;
korl_global_const Korl_Codec_Gltf_Mesh_Primitive KORL_CODEC_GLTF_MESH_PRIMITIVE_DEFAULT = {.mode = KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLES
                                                                                          ,.attributes = {.position = -1, .normal = -1, .texCoord0 = -1}
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
    u32                                    bufferView;
    Korl_Codec_Gltf_Accessor_Type          type;
    Korl_Codec_Gltf_Accessor_ComponentType componentType;
    u32                                    count;
    Korl_Math_Aabb3f32                     aabb;
} Korl_Codec_Gltf_Accessor;
typedef struct Korl_Codec_Gltf_BufferView
{
    u32 buffer;
    u32 byteLength;
    u32 byteOffset;
} Korl_Codec_Gltf_BufferView;
typedef struct Korl_Codec_Gltf_Buffer
{
    u32 byteLength;
} Korl_Codec_Gltf_Buffer;
typedef struct Korl_Codec_Gltf_Material
{
    Korl_Codec_Gltf_Data rawUtf8Name;
    struct
    {
        i32 specularColorTextureIndex;
    } KHR_materials_specular;
    struct
    {
        i32 baseColorTextureIndex;
    } pbrMetallicRoughness;
} Korl_Codec_Gltf_Material;
korl_global_const Korl_Codec_Gltf_Material KORL_CODEC_GLTF_MATERIAL_DEFAULT = {.KHR_materials_specular = {.specularColorTextureIndex = -1}
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
//@TODO: create "DEFAULT" struct instances of all the above structs that have default glTF values according to the spec, then pass those default structs to the "newArray" API in the decoder
korl_internal Korl_Codec_Gltf* korl_codec_glb_decode(const void* glbData, u$ glbDataBytes, Korl_Memory_AllocatorHandle resultAllocator);
korl_internal Korl_Codec_Gltf_Mesh*           korl_codec_gltf_getMeshes(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Mesh_Primitive* korl_codec_gltf_getMeshPrimitives(const Korl_Codec_Gltf* context, const Korl_Codec_Gltf_Mesh* mesh);
korl_internal Korl_Codec_Gltf_Accessor*       korl_codec_gltf_getAccessors(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_BufferView*     korl_codec_gltf_getBufferViews(const Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf_Buffer*         korl_codec_gltf_getBuffers(const Korl_Codec_Gltf* context);
korl_internal u32                             korl_codec_gltf_accessor_getStride(const Korl_Codec_Gltf_Accessor* context);