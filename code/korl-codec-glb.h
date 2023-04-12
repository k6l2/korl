#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_Codec_Gltf_Data
{
    u32 byteOffset;
    u32 size;
} Korl_Codec_Gltf_Data;
typedef struct Korl_Codec_Gltf
{
    struct
    {
        Korl_Codec_Gltf_Data rawUtf8Version;
    } asset;
    i32                  scene;
    Korl_Codec_Gltf_Data scenes;
    Korl_Codec_Gltf_Data nodes;
    Korl_Codec_Gltf_Data meshes;
} Korl_Codec_Gltf;
typedef struct Korl_Codec_Gltf_Scene
{
    Korl_Codec_Gltf_Data rawUtf8Name;
    Korl_Codec_Gltf_Data nodes;
} Korl_Codec_Gltf_Scene;
typedef struct Korl_Codec_Gltf_Node
{
    u32                  mesh;
    Korl_Codec_Gltf_Data rawUtf8Name;
} Korl_Codec_Gltf_Node;
typedef struct Korl_Codec_Gltf_Mesh
{
    Korl_Codec_Gltf_Data rawUtf8Name;
    Korl_Codec_Gltf_Data primitives;
} Korl_Codec_Gltf_Mesh;
typedef struct Korl_Codec_Gltf_Mesh_Primitive
{
    struct
    {
        i32 position;
        i32 normal;
        i32 texCoord0;
    } attributes;
    i32 indices;
    i32 material;
} Korl_Codec_Gltf_Mesh_Primitive;
korl_internal Korl_Codec_Gltf* korl_codec_glb_decode(const void* glbData, u$ glbDataBytes, Korl_Memory_AllocatorHandle resultAllocator);
