/**
 * ----- API -----
 * korl_vulkan_safeCast_u$_to_vertexIndex
 * korl_vulkan_construct
 * korl_vulkan_destroy
 * korl_vulkan_createSurface
 *     Not to be confused with \c _korl_vulkan_createSurface (an internal API to 
 *     create an actual Vulkan Surface resource)!  "Surface" here refers to an 
 *     entire rendering region on the screen.  A common use case is to create a 
 *     desktop window, and then create a KORL Vulkan Surface to fill the entire 
 *     region of the window.
 * korl_vulkan_destroySurface
 * korl_vulkan_getSurfaceSize
 * korl_vulkan_clearAllDeviceAllocations
 *     Just to clarify here, calling this function doesn't actually destroy the 
 *     device allocations; it simply allows us to create new allocations which 
 *     can potentially occupy the same place in memory as well as possess the 
 *     same allocation handle.
 * korl_vulkan_setSurfaceClearColor
 * korl_vulkan_frameBegin
 * korl_vulkan_frameEnd
 * korl_vulkan_deferredResize
 *     Call this whenever the window is resized.  This will trigger a resize of the 
 *     swap chain right before the next draw operation in \c korl_vulkan_draw .
 * korl_vulkan_setDrawState
 * korl_vulkan_draw
 * ----- Data Structures -----
 * Korl_Vulkan_DrawVertexData
 * Korl_Vulkan_DrawState
 */
#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
#include "korl-math.h"
#include "korl-checkCast.h"
#include "korl-memory.h"
#include "korl-assetCache.h"
#include "korl-interface-platform-gfx.h"
typedef u64 Korl_Vulkan_DeviceMemory_AllocationHandle;
typedef u64 Korl_Vulkan_ShaderHandle;
typedef enum Korl_Vulkan_DrawVertexData_VertexBufferType
    {KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_UNUSED
    ,KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_RESOURCE
    ,KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_DEVICE_MEMORY_ALLOCATION
} Korl_Vulkan_DrawVertexData_VertexBufferType;
typedef struct Korl_Vulkan_DrawVertexData
{
    Korl_Vulkan_PrimitiveType      primitiveType;
    struct
    {
        Korl_Vulkan_DrawVertexData_VertexBufferType type;
        u$                                          byteOffset;
        union
        {
            Korl_Resource_Handle                      handleResource;
            Korl_Vulkan_DeviceMemory_AllocationHandle handleDeviceMemoryAllocation;
        } subType;
    } vertexBuffer;
    Korl_Vulkan_VertexIndex        indexCount;
    const Korl_Vulkan_VertexIndex* indices;
    u32                            vertexCount;
    u8                             positionDimensions;        // only acceptable values: {2, 3}
    const f32*                     positions;
    u32                            positionsStride;
    const Korl_Math_V2f32*         uvs;
    u32                            uvsStride;
    const Korl_Vulkan_Color4u8*    colors;
    u32                            colorsStride;
    u32                            instanceCount;
    u8                             instancePositionDimensions;// only acceptable values: {2, 3}
    const f32*                     instancePositions;
    u32                            instancePositionsStride;
    const u32*                     instanceUint;
    u32                            instanceUintStride;
} Korl_Vulkan_DrawVertexData;
typedef struct Korl_Vulkan_DrawState_Features
{
    u32 enableBlend     : 1;
    u32 enableDepthTest : 1;//@TODO: separate depth test & depth write
} Korl_Vulkan_DrawState_Features;
typedef struct Korl_Vulkan_DrawState_Blend
{
    Korl_Vulkan_BlendOperation opColor;
    Korl_Vulkan_BlendFactor factorColorSource;
    Korl_Vulkan_BlendFactor factorColorTarget;
    Korl_Vulkan_BlendOperation opAlpha;
    Korl_Vulkan_BlendFactor factorAlphaSource;
    Korl_Vulkan_BlendFactor factorAlphaTarget;
} Korl_Vulkan_DrawState_Blend;
typedef struct Korl_Vulkan_DrawState_SceneProperties
{
    Korl_Math_M4f32 view;
    Korl_Math_M4f32 projection;
    f32             seconds;
} Korl_Vulkan_DrawState_SceneProperties;
typedef struct Korl_Vulkan_DrawState_Model
{
    Korl_Math_M4f32    transform;
    Korl_Math_Aabb2f32 uvAabb;
} Korl_Vulkan_DrawState_Model;
typedef struct Korl_Vulkan_DrawState_Scissor
{
    u32 x;
    u32 y;
    u32 width;
    u32 height;
} Korl_Vulkan_DrawState_Scissor;
typedef Korl_Gfx_Material Korl_Vulkan_DrawState_Material;// @TODO: likely unnecessary abstraction
typedef struct Korl_Vulkan_DrawState_StorageBuffers
{
    Korl_Resource_Handle resourceHandleVertex;
} Korl_Vulkan_DrawState_StorageBuffers;
typedef struct Korl_Vulkan_DrawState_Lighting
{
    u32                   lightsCount;
    const Korl_Gfx_Light* lights;
} Korl_Vulkan_DrawState_Lighting;
typedef struct Korl_Vulkan_DrawState
{
    const Korl_Vulkan_DrawState_Features*        features;
    const Korl_Vulkan_DrawState_Blend*           blend;
    const Korl_Vulkan_DrawState_SceneProperties* sceneProperties;
    const Korl_Vulkan_DrawState_Model*           model;
    const Korl_Vulkan_DrawState_Scissor*         scissor;
    const Korl_Vulkan_DrawState_Material*        material;
    const Korl_Vulkan_DrawState_StorageBuffers*  storageBuffers;
    const Korl_Vulkan_DrawState_Lighting*        lighting;
} Korl_Vulkan_DrawState;
typedef enum Korl_Vulkan_VertexAttribute
    { KORL_VULKAN_VERTEX_ATTRIBUTE_INDEX
    , KORL_VULKAN_VERTEX_ATTRIBUTE_POSITION_3D
    , KORL_VULKAN_VERTEX_ATTRIBUTE_NORMAL_3D
    , KORL_VULKAN_VERTEX_ATTRIBUTE_UV
    , KORL_VULKAN_VERTEX_ATTRIBUTE_INSTANCE_POSITION_2D
    , KORL_VULKAN_VERTEX_ATTRIBUTE_INSTANCE_UINT
    , KORL_VULKAN_VERTEX_ATTRIBUTE_ENUM_COUNT// keep last!
} Korl_Vulkan_VertexAttribute;
typedef struct Korl_Vulkan_VertexAttributeDescriptor
{
    Korl_Vulkan_VertexAttribute vertexAttribute;
    u$                          offset;
    u32                         stride;
} Korl_Vulkan_VertexAttributeDescriptor;
typedef struct Korl_Vulkan_CreateInfoTexture
{
    u32 sizeX;
    u32 sizeY;
} Korl_Vulkan_CreateInfoTexture;
typedef struct Korl_Vulkan_CreateInfoVertexBuffer
{
    u$                                           bytes;
    u$                                           vertexAttributeDescriptorCount;
    const Korl_Vulkan_VertexAttributeDescriptor* vertexAttributeDescriptors;
    bool                                         useAsStorageBuffer;
} Korl_Vulkan_CreateInfoVertexBuffer;
typedef struct Korl_Vulkan_CreateInfoShader
{
    const void* data;
    u$          dataBytes;
} Korl_Vulkan_CreateInfoShader;
korl_internal Korl_Vulkan_VertexIndex                   korl_vulkan_safeCast_u$_to_vertexIndex(u$ x);
korl_internal void                                      korl_vulkan_construct(void);
korl_internal void                                      korl_vulkan_destroy(void);
korl_internal void                                      korl_vulkan_createSurface(void* createSurfaceUserData, u32 sizeX, u32 sizeY);
korl_internal void                                      korl_vulkan_destroySurface(void);
korl_internal Korl_Math_V2u32                           korl_vulkan_getSurfaceSize(void);
korl_internal void                                      korl_vulkan_clearAllDeviceAllocations(void);
korl_internal void                                      korl_vulkan_setSurfaceClearColor(const f32 clearRgb[3]);
korl_internal void                                      korl_vulkan_frameEnd(void);
korl_internal void                                      korl_vulkan_deferredResize(u32 sizeX, u32 sizeY);
korl_internal void                                      korl_vulkan_setDrawState(const Korl_Vulkan_DrawState* state);
korl_internal void                                      korl_vulkan_draw(const Korl_Vulkan_DrawVertexData* vertexData);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createTexture(const Korl_Vulkan_CreateInfoTexture* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createVertexBuffer(const Korl_Vulkan_CreateInfoVertexBuffer* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle);
korl_internal void                                      korl_vulkan_deviceAsset_destroy(Korl_Vulkan_DeviceMemory_AllocationHandle deviceAssetHandle);
korl_internal void                                      korl_vulkan_texture_update(Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle, const Korl_Vulkan_Color4u8* pixelData);
korl_internal Korl_Math_V2u32                           korl_vulkan_texture_getSize(const Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle);
korl_internal void                                      korl_vulkan_vertexBuffer_resize(Korl_Vulkan_DeviceMemory_AllocationHandle* in_out_bufferHandle, u$ bytes);
korl_internal void                                      korl_vulkan_vertexBuffer_update(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle, const void* data, u$ dataBytes, u$ deviceLocalBufferOffset);
korl_internal Korl_Vulkan_ShaderHandle                  korl_vulkan_shader_create(const Korl_Vulkan_CreateInfoShader* createInfo, Korl_Vulkan_ShaderHandle requiredHandle);
korl_internal void                                      korl_vulkan_shader_destroy(Korl_Vulkan_ShaderHandle shaderHandle);
