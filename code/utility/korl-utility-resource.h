/** utilities for KORL built-in resource descriptors */
#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-gfx.h"
#include "utility/korl-utility-math.h"
/** KORL built-in resource descriptor names ***********************************/
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_SHADER     = "korl-rd-shader";
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_GFX_BUFFER = "korl-rd-gfx-buffer";
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE    = "korl-rd-texture";
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_FONT       = "korl-rd-font";// composed of GFX_BUFFER & TEXTURE resources
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_SCENE3D    = "korl-rd-scene3d";
korl_global_const char*const KORL_RESOURCE_DESCRIPTOR_NAME_MESH       = "korl-rd-mesh";
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
typedef enum Korl_Resource_Texture_Format
    {KORL_RESOURCE_TEXTURE_FORMAT_UNDEFINED
    ,KORL_RESOURCE_TEXTURE_FORMAT_R8_UNORM
    ,KORL_RESOURCE_TEXTURE_FORMAT_R8_SRGB
    ,KORL_RESOURCE_TEXTURE_FORMAT_R8G8_UNORM
    ,KORL_RESOURCE_TEXTURE_FORMAT_R8G8_SRGB
    ,KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8_UNORM
    ,KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8_SRGB
    ,KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8A8_UNORM
    ,KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8A8_SRGB
    ,KORL_RESOURCE_TEXTURE_FORMAT_ENUM_COUNT
} Korl_Resource_Texture_Format;
korl_global_const u8 KORL_RESOURCE_TEXTURE_FORMAT_COMPONENTS[] = 
    {0//KORL_RESOURCE_TEXTURE_FORMAT_UNDEFINED
    ,1//KORL_RESOURCE_TEXTURE_FORMAT_R8_UNORM
    ,1//KORL_RESOURCE_TEXTURE_FORMAT_R8_SRGB
    ,2//KORL_RESOURCE_TEXTURE_FORMAT_R8G8_UNORM
    ,2//KORL_RESOURCE_TEXTURE_FORMAT_R8G8_SRGB
    ,3//KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8_UNORM
    ,3//KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8_SRGB
    ,4//KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8A8_UNORM
    ,4//KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8A8_SRGB
    };
korl_global_const u8 KORL_RESOURCE_TEXTURE_FORMAT_BYTE_STRIDES[] = 
    {0//KORL_RESOURCE_TEXTURE_FORMAT_UNDEFINED
    ,1//KORL_RESOURCE_TEXTURE_FORMAT_R8_UNORM
    ,1//KORL_RESOURCE_TEXTURE_FORMAT_R8_SRGB
    ,2//KORL_RESOURCE_TEXTURE_FORMAT_R8G8_UNORM
    ,2//KORL_RESOURCE_TEXTURE_FORMAT_R8G8_SRGB
    ,3//KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8_UNORM
    ,3//KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8_SRGB
    ,4//KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8A8_UNORM
    ,4//KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8A8_SRGB
    };
KORL_STATIC_ASSERT(sizeof(KORL_RESOURCE_TEXTURE_FORMAT_COMPONENTS) == KORL_RESOURCE_TEXTURE_FORMAT_ENUM_COUNT);
KORL_STATIC_ASSERT(sizeof(KORL_RESOURCE_TEXTURE_FORMAT_COMPONENTS) == sizeof(KORL_RESOURCE_TEXTURE_FORMAT_BYTE_STRIDES));
typedef struct Korl_Resource_Texture_CreateInfo
{
    Korl_Math_V2u32              size;
    Korl_Resource_Texture_Format formatImage;
    acu8                         imageFileMemoryBuffer;
} Korl_Resource_Texture_CreateInfo;
/** MISC **********************************************************************/
/** The collection of all components required to draw any given mesh primitive, 
 * excluding model or pose transforms. */
typedef struct Korl_Resource_Scene3d_MeshPrimitive
{
    Korl_Gfx_VertexStagingMeta      vertexStagingMeta;
    Korl_Resource_Handle            vertexBuffer;
    u$                              vertexBufferByteOffset;
    Korl_Gfx_Material_PrimitiveType primitiveType;
    Korl_Gfx_Material               material;
} Korl_Resource_Scene3d_MeshPrimitive;
/** Skins are created using \c korl_resource_scene3d_newSkin ; the user is 
 * responsible for calling \c korl_resource_scene3d_skin_destroy when they are 
 * done to prevent memory leaks */
typedef struct Korl_Resource_Scene3d_Skin
{
    Korl_Memory_AllocatorHandle allocator; // we need to store a skin instance somewhere in dynamic memory because we don't know how many bones a skin requires
    u32                         skinIndex; // used for obtaining the pre-baked topologically sorted boneIndex array
    u32                         bonesCount;
    Korl_Math_Transform3d*      bones;     // the actual transforms which can be modified/animated at any time by the user; before being used to calculate vertex world-space positions, we must multiply these by their respective inverseBindMatrices which are baked in the scene3d resource
} Korl_Resource_Scene3d_Skin;
korl_internal void korl_resource_scene3d_skin_destroy(Korl_Resource_Scene3d_Skin* context);
