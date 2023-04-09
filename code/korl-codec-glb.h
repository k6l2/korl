#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_Codec_Gltf_Scene
{
    u32 byteOffsetRawUtf8Name;
    // struct
    // {
    //     u32 indices;
    //     u32 indicesSize;
    // } nodes;
} Korl_Codec_Gltf_Scene;
typedef struct Korl_Codec_Gltf
{
    struct
    {
        u32 byteOffsetRawUtf8Version;
        u32 byteOffsetRawUtf8VersionEnd;
    } asset;
    i32 scene;
    // struct
    // {
    //     Korl_Codec_Gltf_Scene* array;
    //     u32                    arraySize;
    // } scenes;
} Korl_Codec_Gltf;
korl_internal Korl_Codec_Gltf* korl_codec_glb_decode(const void* glbData, u$ glbDataBytes, Korl_Memory_AllocatorHandle resultAllocator);
