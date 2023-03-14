#pragma once
#include "korl-globalDefines.h"
typedef u64 Korl_Resource_Handle;
#define KORL_FUNCTION_korl_resource_fromFile(name)        Korl_Resource_Handle name(acu16 fileName, Korl_AssetCache_Get_Flags assetCacheGetFlags)
#define KORL_FUNCTION_korl_resource_texture_getSize(name) Korl_Math_V2u32      name(Korl_Resource_Handle resourceHandleTexture)
