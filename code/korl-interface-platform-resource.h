#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-pool.h"
#include "utility/korl-utility-math.h"
typedef Korl_Pool_Handle Korl_Resource_Handle;
#define KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(name)  void* name(Korl_Memory_AllocatorHandle allocator)
#define KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(name) void  name(void* resourceDescriptorStruct, Korl_Memory_AllocatorHandle allocator)
#define KORL_FUNCTION_korl_resource_descriptorCallback_unload(name)                  void  name(void* resourceDescriptorStruct)
#define KORL_FUNCTION_korl_resource_descriptorCallback_transcode(name)               void  name(void* resourceDescriptorStruct, const void* data, u$ dataBytes)
#define KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(name)      void  name(void* resourceDescriptorStruct)
#define KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeData(name)       void  name(void* resourceDescriptorStruct, const void* descriptorCreateInfo, Korl_Memory_AllocatorHandle allocator, void** o_data)
#define KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeMedia(name)      void  name(void* resourceDescriptorStruct)
#define KORL_FUNCTION_korl_resource_descriptorCallback_runtimeBytes(name)            u$    name(const void* resourceDescriptorStruct)
#define KORL_FUNCTION_korl_resource_descriptorCallback_runtimeResize(name)           void  name(void* resourceDescriptorStruct, u$ bytes, Korl_Memory_AllocatorHandle allocator, void** io_data)
typedef KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate (fnSig_korl_resource_descriptorCallback_descriptorStructCreate);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(fnSig_korl_resource_descriptorCallback_descriptorStructDestroy);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_unload                 (fnSig_korl_resource_descriptorCallback_unload);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_transcode              (fnSig_korl_resource_descriptorCallback_transcode);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData     (fnSig_korl_resource_descriptorCallback_clearTransientData);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeData      (fnSig_korl_resource_descriptorCallback_createRuntimeData);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeMedia     (fnSig_korl_resource_descriptorCallback_createRuntimeMedia);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_runtimeBytes           (fnSig_korl_resource_descriptorCallback_runtimeBytes);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_runtimeResize          (fnSig_korl_resource_descriptorCallback_runtimeResize);
#if 0//@TODO ?
/** if we want to be able to perform any desired pre-processing to a file-backed resource & cache the resulting file to dramatically speed up future application runs, we might need something like this */
#define KORL_FUNCTION_korl_resource_descriptorCallback_preProcess(name) void name(void)
typedef KORL_FUNCTION_korl_resource_descriptorCallback_preProcess(fnSig_korl_resource_descriptorCallback_preProcess);
#endif
typedef Korl_Pool_Handle Korl_FunctionDynamo_FunctionHandle;// re-declare, to prevent the need to include `korl-interface-platform.h`
typedef struct Korl_Resource_DescriptorManifest_Callbacks
{
    Korl_FunctionDynamo_FunctionHandle descriptorStructCreate; // fnSig_korl_resource_descriptorCallback_descriptorStructCreate
    Korl_FunctionDynamo_FunctionHandle descriptorStructDestroy;// fnSig_korl_resource_descriptorCallback_descriptorStructDestroy
    Korl_FunctionDynamo_FunctionHandle unload;                 // fnSig_korl_resource_descriptorCallback_unload
    Korl_FunctionDynamo_FunctionHandle transcode;              // fnSig_korl_resource_descriptorCallback_transcode
    Korl_FunctionDynamo_FunctionHandle clearTransientData;     // fnSig_korl_resource_descriptorCallback_clearTransientData; only used by resources that support ASSET_CACHE  backing
    Korl_FunctionDynamo_FunctionHandle createRuntimeData;      // fnSig_korl_resource_descriptorCallback_createRuntimeData ; only used by resources that support RUNTIME_DATA backing
    Korl_FunctionDynamo_FunctionHandle createRuntimeMedia;     // fnSig_korl_resource_descriptorCallback_createRuntimeMedia; only used by resources that support RUNTIME_DATA backing
    Korl_FunctionDynamo_FunctionHandle runtimeBytes;           // fnSig_korl_resource_descriptorCallback_runtimeBytes      ; only used by resources that support RUNTIME_DATA backing
    Korl_FunctionDynamo_FunctionHandle runtimeResize;          // fnSig_korl_resource_descriptorCallback_runtimeResize     ; only used by resources that support RUNTIME_DATA backing
} Korl_Resource_DescriptorManifest_Callbacks;
typedef struct Korl_Resource_DescriptorManifest
{
    acu8                                       utf8DescriptorName;
    Korl_Resource_DescriptorManifest_Callbacks callbacks;
} Korl_Resource_DescriptorManifest;
/** to calculate the total spacing between two lines, use the formula: (ascent - decent) + lineGap */
typedef struct Korl_Resource_Font_Metrics
{
    f32 ascent;
    f32 decent;
    f32 lineGap;
    f32 nearestSupportedPixelHeight;// @TODO: this is causing the user-code grief; why can't we just pre-multiply all the other values with the correct scale?
    u8  _nearestSupportedPixelHeightIndex;
} Korl_Resource_Font_Metrics;
typedef struct Korl_Resource_Font_Resources
{
    Korl_Resource_Handle resourceHandleTexture;
    Korl_Resource_Handle resourceHandleSsboGlyphMeshVertices;
} Korl_Resource_Font_Resources;
typedef struct Korl_Resource_Font_TextMetrics
{
    Korl_Math_V2f32 aabbSize;//@TODO: is this properly scaled by nearestSupportedPixelHeight?
    u32             visibleGlyphCount;
} Korl_Resource_Font_TextMetrics;
#define KORL_FUNCTION_korl_resource_descriptor_add(name)                void                                name(const Korl_Resource_DescriptorManifest* descriptorManifest)
#define KORL_FUNCTION_korl_resource_fromFile(name)                      Korl_Resource_Handle                name(acu8 utf8DescriptorName, acu8 utf8FileName, Korl_AssetCache_Get_Flags assetCacheGetFlags)
#define KORL_FUNCTION_korl_resource_create(name)                        Korl_Resource_Handle                name(acu8 utf8DescriptorName, const void* descriptorCreateInfo)
#define KORL_FUNCTION_korl_resource_getDescriptorStruct(name)           void*                               name(Korl_Resource_Handle handle)
#define KORL_FUNCTION_korl_resource_resize(name)                        void                                name(Korl_Resource_Handle handle, u$ newByteSize)
#define KORL_FUNCTION_korl_resource_shift(name)                         void                                name(Korl_Resource_Handle handle, i$ byteShiftCount)
#define KORL_FUNCTION_korl_resource_destroy(name)                       void                                name(Korl_Resource_Handle resourceHandle)
#define KORL_FUNCTION_korl_resource_update(name)                        void                                name(Korl_Resource_Handle handle, const void* sourceData, u$ sourceDataBytes, u$ destinationByteOffset)
#define KORL_FUNCTION_korl_resource_getUpdateBuffer(name)               void*                               name(Korl_Resource_Handle handle, u$ byteOffset, u$* io_bytesRequested_bytesAvailable)
#define KORL_FUNCTION_korl_resource_getByteSize(name)                   u$                                  name(Korl_Resource_Handle handle)
#define KORL_FUNCTION_korl_resource_isLoaded(name)                      bool                                name(Korl_Resource_Handle handle)
#define KORL_FUNCTION_korl_resource_texture_getSize(name)               Korl_Math_V2u32                     name(Korl_Resource_Handle handleResourceTexture)
#define KORL_FUNCTION_korl_resource_texture_getRowByteStride(name)      u32                                 name(Korl_Resource_Handle handleResourceTexture)
#define KORL_FUNCTION_korl_resource_font_getMetrics(name)               Korl_Resource_Font_Metrics          name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight)
#define KORL_FUNCTION_korl_resource_font_getResources(name)             Korl_Resource_Font_Resources        name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight)
#define KORL_FUNCTION_korl_resource_font_getUtf8Metrics(name)           Korl_Resource_Font_TextMetrics      name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu8 utf8Text)
#define KORL_FUNCTION_korl_resource_font_generateUtf8(name)             void                                name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu8 utf8Text, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u16 glyphInstancePositionsByteStride, u32* o_glyphInstanceIndices, u16 glyphInstanceIndicesByteStride)
#define KORL_FUNCTION_korl_resource_font_getUtf16Metrics(name)          Korl_Resource_Font_TextMetrics      name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu16 utf16Text)
#define KORL_FUNCTION_korl_resource_font_generateUtf16(name)            void                                name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu16 utf16Text, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u16 glyphInstancePositionsByteStride, u32* o_glyphInstanceIndices, u16 glyphInstanceIndicesByteStride)
#define KORL_FUNCTION_korl_resource_font_textGraphemePosition(name)     Korl_Math_V2f32                     name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu8 utf8Text, u$ graphemeIndex)
#define KORL_FUNCTION_korl_resource_scene3d_getMaterialCount(name)      u32                                 name(Korl_Resource_Handle handleResourceScene3d)
#define KORL_FUNCTION_korl_resource_scene3d_getMaterial(name)           Korl_Gfx_Material                   name(Korl_Resource_Handle handleResourceScene3d, u32 materialIndex)
#define KORL_FUNCTION_korl_resource_scene3d_getMeshIndex(name)          u32                                 name(Korl_Resource_Handle handleResourceScene3d, acu8 utf8MeshName)
#define KORL_FUNCTION_korl_resource_scene3d_getMeshPrimitiveCount(name) u32                                 name(Korl_Resource_Handle handleResourceScene3d, u32 meshIndex)
#define KORL_FUNCTION_korl_resource_scene3d_getMeshPrimitive(name)      Korl_Resource_Scene3d_MeshPrimitive name(Korl_Resource_Handle handleResourceScene3d, u32 meshIndex, u32 primitiveIndex)
