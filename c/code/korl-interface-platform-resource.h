#pragma once
#include "korl-globalDefines.h"
typedef u64 Korl_Resource_Handle;
#define KORL_PLATFORM_RESOURCE_FROM_FILE(name) Korl_Resource_Handle name(acu16 fileName, Korl_AssetCache_Get_Flags assetCacheGetFlags)
