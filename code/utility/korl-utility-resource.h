/** utilities for KORL built-in resource descriptors */
#pragma once
#include "korl-globalDefines.h"
/** KORL built-in resource descriptor names ***********************************/
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_SHADER     = "korl-rd-shader";
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_GFX_BUFFER = "korl-rd-gfx-buffer";
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE    = "korl-rd-texture";
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_FONT       = "korl-rd-font";// composed of GFX_BUFFER & TEXTURE resources
/** KORL built-in resource descriptor CreateInfos *****************************/
enum Korl_Resource_GfxBuffer_UsageFlags
    {KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_INDEX   = 1 << 0
    ,KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_VERTEX  = 1 << 1
    ,KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_STORAGE = 1 << 2
};
typedef struct Korl_Resource_GfxBuffer_CreateInfo
{
    u$  bytes;
    u32 usageFlags;// see: Korl_Resource_GfxBuffer_UsageFlags
} Korl_Resource_GfxBuffer_CreateInfo;
typedef struct Korl_Resource_Texture_CreateInfo
{
    Korl_Math_V2u32 size;
} Korl_Resource_Texture_CreateInfo;
