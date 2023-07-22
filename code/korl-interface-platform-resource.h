#pragma once
#include "korl-globalDefines.h"
typedef u64 Korl_Resource_Handle;
enum Korl_Resource_BufferUsageFlags
    {KORL_RESOURCE_BUFFER_USAGE_FLAG_INDEX   = 1 << 0
    ,KORL_RESOURCE_BUFFER_USAGE_FLAG_VERTEX  = 1 << 1
    ,KORL_RESOURCE_BUFFER_USAGE_FLAG_STORAGE = 1 << 2
};
typedef struct Korl_Resource_CreateInfoBuffer
{
    u$  bytes;
    u32 usageFlags;// see: Korl_Resource_BufferUsageFlags
} Korl_Resource_CreateInfoBuffer;
#define KORL_FUNCTION_korl_resource_fromFile(name)        Korl_Resource_Handle name(acu16 fileName, Korl_AssetCache_Get_Flags assetCacheGetFlags)
#define KORL_FUNCTION_korl_resource_buffer_create(name)   Korl_Resource_Handle name(const struct Korl_Resource_CreateInfoBuffer* createInfo)
#define KORL_FUNCTION_korl_resource_destroy(name)         void                 name(Korl_Resource_Handle resourceHandle)
#define KORL_FUNCTION_korl_resource_update(name)          void                 name(Korl_Resource_Handle handle, const void* sourceData, u$ sourceDataBytes, u$ destinationByteOffset)
#define KORL_FUNCTION_korl_resource_getUpdateBuffer(name) void*                name(Korl_Resource_Handle handle, u$ byteOffset, u$* io_bytesRequested_bytesAvailable)
#define KORL_FUNCTION_korl_resource_texture_getSize(name) Korl_Math_V2u32      name(Korl_Resource_Handle resourceHandleTexture)
