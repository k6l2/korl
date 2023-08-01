/**
 * Example code of user file asset registration:
 * @TODO: do we want to include this?  this does not cover, for example, the user generating a \c "korl-resource-manifest.h" file by enumerating all the files of a directory, like what we needed to do for the \c farm project, although it doesn't _prevent_ this functionality as well...
 * 
 * // "korl-resource-manifest.h"
 * 
 * #ifndef KORL_RESOURCE_MACRO_OPERATION
 * #    define KORL_RESOURCE_MACRO_OPERATION(projectRootPath, enumerationIdentifier)
 * #endif
 * KORL_RESOURCE_MACRO_OPERATION("build/shaders/korl-2d.vert.spv", BUILD_SHADERS_KORL_2D_VERT_SPV)
 * // more lines like the one above this comment ...
 * #undef KORL_RESOURCE_MACRO_OPERATION
 * 
 * // somewhere in user code
 * 
 * enum
 * {
 *     #define KORL_RESOURCE_MACRO_OPERATION(projectRootPath, enumerationIdentifier) KORL_RESOURCE_##enumerationIdentifier,
 *     #include "korl-resource-manifest.h"
 *     #undef KORL_RESOURCE_MACRO_OPERATION
 *     KORL_RESOURCE_ENUM_COUNT
 * };
 * 
 * // elsewhere in user code
 * Korl_Resource_Handle resourceHandles[KORL_RESOURCE_ENUM_COUNT];// stored in persistent (code module or heap) memory somewhere
 * 
 * // elsewhere in user code; perhaps during program initialization
 * #define KORL_RESOURCE_MACRO_OPERATION(projectRootPath, enumerationIdentifier) resourceHandles[KORL_RESOURCE_##enumerationIdentifier] = korl_resource_fromFile(L""##projectRootPath);
 * #include "korl-resource-manifest.h"
 * #undef KORL_RESOURCE_MACRO_OPERATION
 */
#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-pool.h"
typedef Korl_Pool_Handle Korl_Resource_Handle;
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
#define KORL_FUNCTION_korl_resource_resize(name)          void                 name(Korl_Resource_Handle handle, u$ newByteSize)
#define KORL_FUNCTION_korl_resource_destroy(name)         void                 name(Korl_Resource_Handle resourceHandle)
#define KORL_FUNCTION_korl_resource_update(name)          void                 name(Korl_Resource_Handle handle, const void* sourceData, u$ sourceDataBytes, u$ destinationByteOffset)
#define KORL_FUNCTION_korl_resource_getUpdateBuffer(name) void*                name(Korl_Resource_Handle handle, u$ byteOffset, u$* io_bytesRequested_bytesAvailable)
#define KORL_FUNCTION_korl_resource_texture_getSize(name) Korl_Math_V2u32      name(Korl_Resource_Handle resourceHandleTexture)
