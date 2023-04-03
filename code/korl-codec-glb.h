#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_Codec_Gltf
{
    Korl_Memory_AllocatorHandle allocator;
    struct
    {
        u8 versionRawUtf8[8];
    } asset;
    i32 scene;
} Korl_Codec_Gltf;
korl_internal Korl_Codec_Gltf* korl_codec_glb_decode(const void* glbData, u$ glbDataBytes, Korl_Memory_AllocatorHandle resultAllocator);
