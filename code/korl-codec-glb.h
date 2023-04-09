#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_Codec_Gltf_Scene
{
    u8 name[16];
    struct
    {
        u32* indices;
        u32  indicesSize;
    } nodes;
} Korl_Codec_Gltf_Scene;
typedef struct Korl_Codec_Gltf
{
    Korl_Memory_AllocatorHandle allocator;
    struct
    {
        u8 versionRawUtf8[8];
    } asset;
    i32 scene;
    struct
    {
        Korl_Codec_Gltf_Scene* array;
        u32                    arraySize;
    } scenes;
} Korl_Codec_Gltf;
korl_internal void             korl_codec_gltf_free(Korl_Codec_Gltf* context);
korl_internal Korl_Codec_Gltf* korl_codec_glb_decode(const void* glbData, u$ glbDataBytes, Korl_Memory_AllocatorHandle resultAllocator);
