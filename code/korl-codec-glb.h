#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_Codec_Gltf_Scene
{
    struct
    {
        u32 byteOffset;
        u32 size;
    } rawUtf8Name;
    struct
    {
        u32 byteOffset;
        u32 size;
    } nodes;
} Korl_Codec_Gltf_Scene;
typedef struct Korl_Codec_Gltf
{
    struct
    {
        struct
        {
            u32 byteOffset;
            u32 size;
        } rawUtf8Version;
    } asset;
    i32 scene;
    struct
    {
        u32 byteOffset;
        u32 size;
    } scenes;
} Korl_Codec_Gltf;
korl_internal Korl_Codec_Gltf* korl_codec_glb_decode(const void* glbData, u$ glbDataBytes, Korl_Memory_AllocatorHandle resultAllocator);
