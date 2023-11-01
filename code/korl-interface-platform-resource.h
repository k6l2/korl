#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-pool.h"
#include "utility/korl-utility-math.h"
typedef struct Korl_Algorithm_GraphDirected_SortedElement Korl_Algorithm_GraphDirected_SortedElement;// forward declare so we don't have to include utility/korl-algorithm.h
typedef Korl_Pool_Handle Korl_Resource_Handle;
#define KORL_FUNCTION_korl_resource_descriptorCallback_collectDefragmentPointers(name) void  name(void* resourceDescriptorStruct, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent, Korl_Memory_AllocatorHandle korlResourceAllocator, bool korlResourceAllocatorIsTransient)
#define KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(name)    void* name(Korl_Memory_AllocatorHandle allocatorRuntime)
#define KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(name)   void  name(void* resourceDescriptorStruct, Korl_Memory_AllocatorHandle allocatorRuntime)
#define KORL_FUNCTION_korl_resource_descriptorCallback_unload(name)                    void  name(void* resourceDescriptorStruct, Korl_Memory_AllocatorHandle allocatorTransient)
#define KORL_FUNCTION_korl_resource_descriptorCallback_transcode(name)                 void  name(void* resourceDescriptorStruct, const void* data, u$ dataBytes, Korl_Memory_AllocatorHandle allocatorTransient)
#define KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(name)        void  name(void* resourceDescriptorStruct)
#define KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeData(name)         void  name(void* resourceDescriptorStruct, const void* descriptorCreateInfo, Korl_Memory_AllocatorHandle allocatorRuntime, void** o_data)
#define KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeMedia(name)        void  name(void* resourceDescriptorStruct)
#define KORL_FUNCTION_korl_resource_descriptorCallback_runtimeBytes(name)              u$    name(const void* resourceDescriptorStruct)
#define KORL_FUNCTION_korl_resource_descriptorCallback_runtimeResize(name)             void  name(void* resourceDescriptorStruct, u$ bytes, Korl_Memory_AllocatorHandle allocatorRuntime, void** io_data)
typedef KORL_FUNCTION_korl_resource_descriptorCallback_collectDefragmentPointers(fnSig_korl_resource_descriptorCallback_collectDefragmentPointers);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate   (fnSig_korl_resource_descriptorCallback_descriptorStructCreate);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy  (fnSig_korl_resource_descriptorCallback_descriptorStructDestroy);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_unload                   (fnSig_korl_resource_descriptorCallback_unload);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_transcode                (fnSig_korl_resource_descriptorCallback_transcode);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData       (fnSig_korl_resource_descriptorCallback_clearTransientData);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeData        (fnSig_korl_resource_descriptorCallback_createRuntimeData);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeMedia       (fnSig_korl_resource_descriptorCallback_createRuntimeMedia);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_runtimeBytes             (fnSig_korl_resource_descriptorCallback_runtimeBytes);
typedef KORL_FUNCTION_korl_resource_descriptorCallback_runtimeResize            (fnSig_korl_resource_descriptorCallback_runtimeResize);
#if 0//KORL-FEATURE-000-000-062: resource: add ability for resource descriptors to pre-process file assets & write a cached pre-processed file to disc, then just use the pre-processed file instead of the original
/** if we want to be able to perform any desired pre-processing to a file-backed resource & cache the resulting file to dramatically speed up future application runs, we might need something like this */
#define KORL_FUNCTION_korl_resource_descriptorCallback_preProcess(name) void name(void)
typedef KORL_FUNCTION_korl_resource_descriptorCallback_preProcess(fnSig_korl_resource_descriptorCallback_preProcess);
#endif
typedef Korl_Pool_Handle Korl_FunctionDynamo_FunctionHandle;// re-declare, to prevent the need to include `korl-interface-platform.h`
typedef struct Korl_Resource_DescriptorManifest_Callbacks
{
    Korl_FunctionDynamo_FunctionHandle collectDefragmentPointers;// fnSig_korl_resource_descriptorCallback_collectDefragmentPointers
    Korl_FunctionDynamo_FunctionHandle descriptorStructCreate;   // fnSig_korl_resource_descriptorCallback_descriptorStructCreate
    Korl_FunctionDynamo_FunctionHandle descriptorStructDestroy;  // fnSig_korl_resource_descriptorCallback_descriptorStructDestroy
    Korl_FunctionDynamo_FunctionHandle unload;                   // fnSig_korl_resource_descriptorCallback_unload
    Korl_FunctionDynamo_FunctionHandle transcode;                // fnSig_korl_resource_descriptorCallback_transcode
    Korl_FunctionDynamo_FunctionHandle clearTransientData;       // fnSig_korl_resource_descriptorCallback_clearTransientData; only used by resources that support ASSET_CACHE  backing
    Korl_FunctionDynamo_FunctionHandle createRuntimeData;        // fnSig_korl_resource_descriptorCallback_createRuntimeData ; only used by resources that support RUNTIME_DATA backing
    Korl_FunctionDynamo_FunctionHandle createRuntimeMedia;       // fnSig_korl_resource_descriptorCallback_createRuntimeMedia; only used by resources that support RUNTIME_DATA backing
    Korl_FunctionDynamo_FunctionHandle runtimeBytes;             // fnSig_korl_resource_descriptorCallback_runtimeBytes      ; only used by resources that support RUNTIME_DATA backing
    Korl_FunctionDynamo_FunctionHandle runtimeResize;            // fnSig_korl_resource_descriptorCallback_runtimeResize     ; only used by resources that support RUNTIME_DATA backing
} Korl_Resource_DescriptorManifest_Callbacks;
typedef struct Korl_Resource_DescriptorManifest
{
    acu8                                       utf8DescriptorName;
    const void*                                descriptorContext;// optional; if != NULL, this allows a descriptor to declare & initialize a "context struct" which all resources of this descriptor type can utilize
    u$                                         descriptorContextBytes;// _must_ be != 0 if `descriptorContext` != NULL
    Korl_Resource_DescriptorManifest_Callbacks callbacks;
} Korl_Resource_DescriptorManifest;
/** to calculate the total spacing between two lines, use the formula: (ascent - decent) + lineGap */
typedef struct Korl_Resource_Font_Metrics
{
    f32 ascent; // pre-scaled by (textPixelHeight / nearestSupportedPixelHeight)
    f32 decent; // pre-scaled by (textPixelHeight / nearestSupportedPixelHeight)
    f32 lineGap;// pre-scaled by (textPixelHeight / nearestSupportedPixelHeight)
    f32 nearestSupportedPixelHeight;// NOTE: since all text metrics are pre-scaled by (textPixelHeight / nearestSupportedPixelHeight), the only thing the user needs this for is applying that scale factor to the model=>world transform of any text meshes generated from the same parameters as these metrics
    u8  _nearestSupportedPixelHeightIndex;
} Korl_Resource_Font_Metrics;
typedef struct Korl_Resource_Font_Resources
{
    Korl_Resource_Handle resourceHandleTexture;
    Korl_Resource_Handle resourceHandleSsboGlyphMeshVertices;
} Korl_Resource_Font_Resources;
typedef struct Korl_Resource_Font_TextMetrics
{
    Korl_Math_V2f32 aabbSize;// NOTE: this, like all other text metrics, is already pre-scaled by (textPixelHeight / nearestSupportedPixelHeight)
    u32             visibleGlyphCount;
} Korl_Resource_Font_TextMetrics;
typedef enum Korl_Resource_ForEach_Result
    {KORL_RESOURCE_FOR_EACH_RESULT_STOP
    ,KORL_RESOURCE_FOR_EACH_RESULT_CONTINUE
} Korl_Resource_ForEach_Result;
#define KORL_RESOURCE_FOR_EACH_CALLBACK(name) Korl_Resource_ForEach_Result name(void* userData, void* resourceDescriptorStruct, bool* isTranscoded)
typedef KORL_RESOURCE_FOR_EACH_CALLBACK(korl_resource_callback_forEach);
#define KORL_FUNCTION_korl_resource_descriptor_register(name)                     void                                              name(const Korl_Resource_DescriptorManifest* descriptorManifest)
#define KORL_FUNCTION_korl_resource_fromFile(name)                                Korl_Resource_Handle                              name(acu8 utf8DescriptorName, acu8 utf8FileName, Korl_AssetCache_Get_Flags assetCacheGetFlags)
#define KORL_FUNCTION_korl_resource_create(name)                                  Korl_Resource_Handle                              name(acu8 utf8DescriptorName, const void* descriptorCreateInfo, bool transient)
#define KORL_FUNCTION_korl_resource_getDescriptorContextStruct(name)              void*                                             name(acu8 utf8DescriptorName)
#define KORL_FUNCTION_korl_resource_getDescriptorStruct(name)                     void*                                             name(Korl_Resource_Handle handle)
#define KORL_FUNCTION_korl_resource_resize(name)                                  void                                              name(Korl_Resource_Handle handle, u$ newByteSize)
#define KORL_FUNCTION_korl_resource_shift(name)                                   void                                              name(Korl_Resource_Handle handle, i$ byteShiftCount)
#define KORL_FUNCTION_korl_resource_destroy(name)                                 void                                              name(Korl_Resource_Handle resourceHandle)
#define KORL_FUNCTION_korl_resource_update(name)                                  void                                              name(Korl_Resource_Handle handle, const void* sourceData, u$ sourceDataBytes, u$ destinationByteOffset)
#define KORL_FUNCTION_korl_resource_getUpdateBuffer(name)                         void*                                             name(Korl_Resource_Handle handle, u$ byteOffset, u$* io_bytesRequested_bytesAvailable)
//@TODO: consider deprecating this API, in favor of just using KORL_FUNCTION_korl_resource_getRawRuntimeData, since that API is a super-set of getByteSize
#define KORL_FUNCTION_korl_resource_getByteSize(name)                             u$                                                name(Korl_Resource_Handle handle)
#define KORL_FUNCTION_korl_resource_getRawRuntimeData(name)                       const void*                                       name(Korl_Resource_Handle handle, u$* o_bytes)
#define KORL_FUNCTION_korl_resource_isLoaded(name)                                bool                                              name(Korl_Resource_Handle handle)
#define KORL_FUNCTION_korl_resource_forEach(name)                                 void                                              name(acu8 utf8DescriptorName, korl_resource_callback_forEach* callback, void* callbackUserData)
#define KORL_FUNCTION_korl_resource_texture_getSize(name)                         Korl_Math_V2u32                                   name(Korl_Resource_Handle handleResourceTexture)
#define KORL_FUNCTION_korl_resource_texture_getRowByteStride(name)                u32                                               name(Korl_Resource_Handle handleResourceTexture)
#define KORL_FUNCTION_korl_resource_font_getMetrics(name)                         Korl_Resource_Font_Metrics                        name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight)
#define KORL_FUNCTION_korl_resource_font_getResources(name)                       Korl_Resource_Font_Resources                      name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight)
#define KORL_FUNCTION_korl_resource_font_getUtf8Metrics(name)                     Korl_Resource_Font_TextMetrics                    name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu8 utf8Text)
#define KORL_FUNCTION_korl_resource_font_generateUtf8(name)                       void                                              name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu8 utf8Text, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u16 glyphInstancePositionsByteStride, u32* o_glyphInstanceIndices, u16 glyphInstanceIndicesByteStride)
#define KORL_FUNCTION_korl_resource_font_getUtf16Metrics(name)                    Korl_Resource_Font_TextMetrics                    name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu16 utf16Text)
#define KORL_FUNCTION_korl_resource_font_generateUtf16(name)                      void                                              name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu16 utf16Text, Korl_Math_V2f32 instancePositionOffset, Korl_Math_V2f32* o_glyphInstancePositions, u16 glyphInstancePositionsByteStride, u32* o_glyphInstanceIndices, u16 glyphInstanceIndicesByteStride)
#define KORL_FUNCTION_korl_resource_font_textGraphemePosition(name)               Korl_Math_V2f32                                   name(Korl_Resource_Handle handleResourceFont, f32 textPixelHeight, acu8 utf8Text, u$ graphemeIndex)
#define KORL_FUNCTION_korl_resource_scene3d_setDefaultResource(name)              void                                              name(Korl_Resource_Scene3d_DefaultResource defaultResourceId, Korl_Resource_Handle resourceHandle)
#define KORL_FUNCTION_korl_resource_scene3d_getMaterialCount(name)                u32                                               name(Korl_Resource_Handle handleResourceScene3d)
#define KORL_FUNCTION_korl_resource_scene3d_getMaterial(name)                     Korl_Gfx_Material                                 name(Korl_Resource_Handle handleResourceScene3d, u32 materialIndex)
#define KORL_FUNCTION_korl_resource_scene3d_getMeshIndex(name)                    u32                                               name(Korl_Resource_Handle handleResourceScene3d, acu8 utf8MeshName)
#define KORL_FUNCTION_korl_resource_scene3d_getMeshName(name)                     acu8                                              name(Korl_Resource_Handle handleResourceScene3d, u32 meshIndex)
#define KORL_FUNCTION_korl_resource_scene3d_getMeshPrimitiveCount(name)           u32                                               name(Korl_Resource_Handle handleResourceScene3d, u32 meshIndex)
#define KORL_FUNCTION_korl_resource_scene3d_getMeshPrimitive(name)                Korl_Resource_Scene3d_MeshPrimitive               name(Korl_Resource_Handle handleResourceScene3d, u32 meshIndex, u32 primitiveIndex)
#define KORL_FUNCTION_korl_resource_scene3d_getMeshTriangles(name)                Korl_Math_TriangleMesh                            name(Korl_Resource_Handle handleResourceScene3d, u32 meshIndex, Korl_Memory_AllocatorHandle allocator)
#define KORL_FUNCTION_korl_resource_scene3d_newSkin(name)                         Korl_Resource_Scene3d_Skin*                       name(Korl_Resource_Handle handleResourceScene3d, u32 meshIndex, Korl_Memory_AllocatorHandle allocator)
#define KORL_FUNCTION_korl_resource_scene3d_skin_getBoneParentIndices(name)       const i32*                                        name(Korl_Resource_Handle handleResourceScene3d, u32 skinIndex)
#define KORL_FUNCTION_korl_resource_scene3d_skin_getBoneTopologicalOrder(name)    const Korl_Algorithm_GraphDirected_SortedElement* name(Korl_Resource_Handle handleResourceScene3d, u32 skinIndex)
#define KORL_FUNCTION_korl_resource_scene3d_skin_getBoneInverseBindMatrices(name) const Korl_Math_M4f32*                            name(Korl_Resource_Handle handleResourceScene3d, u32 skinIndex)
#define KORL_FUNCTION_korl_resource_scene3d_skin_applyAnimation(name)             void                                              name(Korl_Resource_Handle handleResourceScene3d, Korl_Resource_Scene3d_Skin* targetSkin, u32 animationIndex, f32 secondsRelativeToAnimationStart)
#define KORL_FUNCTION_korl_resource_scene3d_getAnimationIndex(name)               u32                                               name(Korl_Resource_Handle handleResourceScene3d, acu8 utf8AnimationName)
#define KORL_FUNCTION_korl_resource_scene3d_animation_seconds(name)               f32                                               name(Korl_Resource_Handle handleResourceScene3d, u32 animationIndex)
