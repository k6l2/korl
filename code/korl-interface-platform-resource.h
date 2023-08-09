#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-pool.h"
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
    Korl_FunctionDynamo_FunctionHandle descriptorStructCreate;// fnSig_korl_resource_descriptorCallback_descriptorStructCreate
    Korl_FunctionDynamo_FunctionHandle descriptorStructDestroy;// fnSig_korl_resource_descriptorCallback_descriptorStructDestroy
    Korl_FunctionDynamo_FunctionHandle unload;// fnSig_korl_resource_descriptorCallback_unload
    Korl_FunctionDynamo_FunctionHandle transcode;// fnSig_korl_resource_descriptorCallback_transcode
    Korl_FunctionDynamo_FunctionHandle clearTransientData;// fnSig_korl_resource_descriptorCallback_clearTransientData
    Korl_FunctionDynamo_FunctionHandle createRuntimeData;// fnSig_korl_resource_descriptorCallback_createRuntimeData
    Korl_FunctionDynamo_FunctionHandle createRuntimeMedia;// fnSig_korl_resource_descriptorCallback_createRuntimeMedia; only used by resources that support RUNTIME_DATA backing
    Korl_FunctionDynamo_FunctionHandle runtimeBytes;// fnSig_korl_resource_descriptorCallback_createRuntimeMedia; only used by resources that support RUNTIME_DATA backing
    Korl_FunctionDynamo_FunctionHandle runtimeResize;// fnSig_korl_resource_descriptorCallback_createRuntimeMedia; only used by resources that support RUNTIME_DATA backing
} Korl_Resource_DescriptorManifest_Callbacks;
typedef struct Korl_Resource_DescriptorManifest
{
    acu8                                       utf8DescriptorName;
    Korl_Resource_DescriptorManifest_Callbacks callbacks;
} Korl_Resource_DescriptorManifest;
typedef Korl_Pool_Handle Korl_Resource_Handle;
#define KORL_FUNCTION_korl_resource_descriptor_add(name)        void                 name(const Korl_Resource_DescriptorManifest* descriptorManifest)
#define KORL_FUNCTION_korl_resource_fromFile(name)              Korl_Resource_Handle name(acu8 utf8DescriptorName, acu8 utf8FileName, Korl_AssetCache_Get_Flags assetCacheGetFlags)
#define KORL_FUNCTION_korl_resource_create(name)                Korl_Resource_Handle name(acu8 utf8DescriptorName, const void* descriptorCreateInfo)
#define KORL_FUNCTION_korl_resource_resize(name)                void                 name(Korl_Resource_Handle handle, u$ newByteSize)
#define KORL_FUNCTION_korl_resource_destroy(name)               void                 name(Korl_Resource_Handle resourceHandle)
#define KORL_FUNCTION_korl_resource_update(name)                void                 name(Korl_Resource_Handle handle, const void* sourceData, u$ sourceDataBytes, u$ destinationByteOffset)
#define KORL_FUNCTION_korl_resource_getUpdateBuffer(name)       void*                name(Korl_Resource_Handle handle, u$ byteOffset, u$* io_bytesRequested_bytesAvailable)
#define KORL_FUNCTION_korl_resource_texture_getSize(name)       Korl_Math_V2u32      name(Korl_Resource_Handle resourceHandleTexture)
