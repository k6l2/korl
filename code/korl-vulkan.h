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
 * korl_vulkan_allocateStaging
 *     When the user wants to generate their own vertex data each frame on the 
 *     CPU, there's no point in managing their own memory buffers to accomplish 
 *     this.  Instead, they can call this function to allocate CPU-visible 
 *     Vulkan buffer memory, write whatever vertex data they want directly to 
 *     the returned memory address, then draw the data however they want via a 
 *     call to \c korl_vulkan_drawStagingAllocation .
 * korl_vulkan_drawStagingAllocation
 * ----- Data Structures -----
 * Korl_Vulkan_DrawVertexData
 * Korl_Vulkan_DrawState
 */
#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
#include "utility/korl-utility-math.h"
#include "utility/korl-checkCast.h"
#include "korl-memory.h"
#include "korl-assetCache.h"
#include "korl-interface-platform-gfx.h"
#define KORL_VULKAN_MAX_LIGHTS 8
typedef u64 Korl_Vulkan_DeviceMemory_AllocationHandle;
typedef u64 Korl_Vulkan_ShaderHandle;
typedef enum Korl_Vulkan_DrawVertexData_VertexBufferType
    {KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_UNUSED
    ,KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_RESOURCE
    ,KORL_VULKAN_DRAW_VERTEX_DATA_VERTEX_BUFFER_TYPE_DEVICE_MEMORY_ALLOCATION
} Korl_Vulkan_DrawVertexData_VertexBufferType;
typedef struct Korl_Vulkan_DrawMode
{
    Korl_Vulkan_PrimitiveType primitiveType;
    Korl_Gfx_PolygonMode      polygonMode;
    Korl_Gfx_CullMode         cullMode;
} Korl_Vulkan_DrawMode;
typedef struct Korl_Vulkan_DrawVertexData//@TODO: delete/deprecate
{
    // @TODO: I'm torn on this; do we keep placing these types of properties inside DrawVertexData, or do we move them into DrawState ? 🤔🤔🤔🤔🤔
    // @TODO: copy-pasta of Korl_Vulkan_DrawMode!
    Korl_Vulkan_PrimitiveType      primitiveType;
    Korl_Gfx_PolygonMode           polygonMode;
    Korl_Gfx_CullMode              cullMode;
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
    u32 enableDepthTest : 1;//KORL-ISSUE-000-000-155: vulkan/gfx: separate depth test & depth write operations in draw state
} Korl_Vulkan_DrawState_Features;
typedef struct Korl_Vulkan_DrawState_Blend
{
    struct
    {
        Korl_Vulkan_BlendOperation operation;
        Korl_Vulkan_BlendFactor    factorSource;
        Korl_Vulkan_BlendFactor    factorTarget;
    } color, alpha;
} Korl_Vulkan_DrawState_Blend;
typedef struct Korl_Vulkan_DrawState_SceneProperties
{
    Korl_Math_M4f32 view, projection;
    f32             seconds;
} Korl_Vulkan_DrawState_SceneProperties;
typedef struct Korl_Vulkan_DrawState_Model
{
    Korl_Math_M4f32    transform;
    Korl_Math_Aabb2f32 uvAabb;
} Korl_Vulkan_DrawState_Model;
typedef struct Korl_Vulkan_DrawState_Scissor
{
    u32 x, y;
    u32 width, height;
} Korl_Vulkan_DrawState_Scissor;
typedef Korl_Gfx_Material Korl_Vulkan_DrawState_Material;//KORL-ISSUE-000-000-160: vulkan: Korl_Vulkan_DrawState_Material; likely unnecessary abstraction
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
typedef enum Korl_Vulkan_VertexIndexType
    {KORL_VULKAN_VERTEX_INDEX_TYPE_INVALID
    ,KORL_VULKAN_VERTEX_INDEX_TYPE_U16
    ,KORL_VULKAN_VERTEX_INDEX_TYPE_U32
} Korl_Vulkan_VertexIndexType;
typedef enum Korl_Vulkan_VertexAttributeBinding
    {KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_POSITION
    ,KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_COLOR
    ,KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UV
    ,KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_NORMAL
    ,KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_UINT
    ,KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT// keep last!
} Korl_Vulkan_VertexAttributeBinding;
typedef enum Korl_Vulkan_VertexAttributeElementType
    {KORL_VULKAN_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
    ,KORL_VULKAN_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32
    ,KORL_VULKAN_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8
    ,KORL_VULKAN_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32
} Korl_Vulkan_VertexAttributeElementType;
typedef enum Korl_Vulkan_VertexAttributeInputRate
    {KORL_VULKAN_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX
    ,KORL_VULKAN_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE
} Korl_Vulkan_VertexAttributeInputRate;
typedef struct Korl_Vulkan_VertexAttributeDescriptor2//@TODO: rename to Korl_Vulkan_VertexAttributeDescriptor
{
    u32                                    byteOffsetBuffer;// the byte offset within the buffer containing this attribute which contains the first attribute value
    u32                                    byteStride;// aka byteOffsetPerVertex
    Korl_Vulkan_VertexAttributeInputRate   inputRate;
    Korl_Vulkan_VertexAttributeElementType elementType;
    u8                                     vectorSize;
} Korl_Vulkan_VertexAttributeDescriptor2;
typedef struct Korl_Vulkan_VertexStagingMeta
{
    // indexByteStride is implicitly _always_ == sizeof(indexType), so we don't need such a member
    u32                                    indexByteOffsetBuffer;
    u32                                    indexCount;
    Korl_Vulkan_VertexIndexType            indexType;
    u32                                    vertexCount;
    Korl_Vulkan_VertexAttributeDescriptor2 vertexAttributeDescriptors[KORL_VULKAN_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT];
} Korl_Vulkan_VertexStagingMeta;
typedef struct Korl_Vulkan_StagingAllocation
{
    void*        buffer;
    VkDeviceSize bytes;
    VkBuffer     deviceBuffer;
    VkDeviceSize deviceBufferOffset;
} Korl_Vulkan_StagingAllocation;
typedef struct Korl_Vulkan_CreateInfoTexture
{
    u32 sizeX, sizeY;// @TODO: V2u32
} Korl_Vulkan_CreateInfoTexture;
enum Korl_Vulkan_BufferUsageFlags
    {KORL_VULKAN_BUFFER_USAGE_FLAG_INDEX   = 1 << 0
    ,KORL_VULKAN_BUFFER_USAGE_FLAG_VERTEX  = 1 << 1
    ,KORL_VULKAN_BUFFER_USAGE_FLAG_STORAGE = 1 << 2
};
typedef struct Korl_Vulkan_CreateInfoVertexBuffer//@TODO: rename to Korl_Vulkan_CreateInfoBuffer
{
    u$  bytes;
    u32 usageFlags;// see: Korl_Vulkan_BufferUsageFlags
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
korl_internal void                                      korl_vulkan_draw(const Korl_Vulkan_DrawVertexData* vertexData);//@TODO: delete/deprecate
korl_internal Korl_Vulkan_StagingAllocation             korl_vulkan_stagingAllocate(const Korl_Vulkan_VertexStagingMeta* stagingMeta);
//@TODO: stagingReallocate?
korl_internal void                                      korl_vulkan_drawStagingAllocation(const Korl_Vulkan_StagingAllocation* stagingAllocation, const Korl_Vulkan_VertexStagingMeta* stagingMeta, const Korl_Vulkan_DrawMode* drawMode);
korl_internal void                                      korl_vulkan_drawVertexBuffer(Korl_Vulkan_DeviceMemory_AllocationHandle vertexBuffer, const Korl_Vulkan_VertexStagingMeta* stagingMeta, const Korl_Vulkan_DrawMode* drawMode);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createTexture(const Korl_Vulkan_CreateInfoTexture* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle);
//@TODO: rename to korl_vulkan_deviceAsset_createBuffer
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_vulkan_deviceAsset_createVertexBuffer(const Korl_Vulkan_CreateInfoVertexBuffer* createInfo, Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle);
korl_internal void                                      korl_vulkan_deviceAsset_destroy(Korl_Vulkan_DeviceMemory_AllocationHandle deviceAssetHandle);
korl_internal void                                      korl_vulkan_texture_update(Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle, const Korl_Vulkan_Color4u8* pixelData);
korl_internal Korl_Math_V2u32                           korl_vulkan_texture_getSize(const Korl_Vulkan_DeviceMemory_AllocationHandle textureHandle);
korl_internal void                                      korl_vulkan_vertexBuffer_resize(Korl_Vulkan_DeviceMemory_AllocationHandle* in_out_bufferHandle, u$ bytes);
korl_internal void                                      korl_vulkan_vertexBuffer_update(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle, const void* data, u$ dataBytes, u$ deviceLocalBufferOffset);
korl_internal void*                                     korl_vulkan_vertexBuffer_getStagingBuffer(Korl_Vulkan_DeviceMemory_AllocationHandle bufferHandle, u$ bytes, u$ bufferByteOffset);
korl_internal Korl_Vulkan_ShaderHandle                  korl_vulkan_shader_create(const Korl_Vulkan_CreateInfoShader* createInfo, Korl_Vulkan_ShaderHandle requiredHandle);
korl_internal void                                      korl_vulkan_shader_destroy(Korl_Vulkan_ShaderHandle shaderHandle);
