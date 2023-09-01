#include "korl-resource-gfx-buffer.h"
#include "korl-interface-platform.h"
typedef struct _Korl_Resource_GfxBuffer
{
    Korl_Resource_GfxBuffer_CreateInfo        createInfo;
    Korl_Vulkan_DeviceMemory_AllocationHandle deviceMemoryAllocationHandle;
} _Korl_Resource_GfxBuffer;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_gfxBuffer_descriptorStructCreate)
{
    return korl_allocate(allocatorRuntime, sizeof(_Korl_Resource_GfxBuffer));
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_gfxBuffer_descriptorStructDestroy)
{
    korl_free(allocatorRuntime, resourceDescriptorStruct);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_gfxBuffer_unload)
{
    _Korl_Resource_GfxBuffer*const gfxBuffer = resourceDescriptorStruct;
    korl_vulkan_deviceAsset_destroy(gfxBuffer->deviceMemoryAllocationHandle);
    gfxBuffer->deviceMemoryAllocationHandle = 0;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_gfxBuffer_transcode)
{
    _Korl_Resource_GfxBuffer*const gfxBuffer = resourceDescriptorStruct;
    korl_vulkan_buffer_update(gfxBuffer->deviceMemoryAllocationHandle, data, dataBytes, 0);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeData(_korl_resource_gfxBuffer_createRuntimeData)
{
    _Korl_Resource_GfxBuffer*const                 gfxBuffer  = resourceDescriptorStruct;
    const Korl_Resource_GfxBuffer_CreateInfo*const createInfo = descriptorCreateInfo;
    gfxBuffer->createInfo = *createInfo;
    *o_data               = korl_allocate(allocatorRuntime, createInfo->bytes);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeMedia(_korl_resource_gfxBuffer_createRuntimeMedia)
{
    _Korl_Resource_GfxBuffer*const gfxBuffer = resourceDescriptorStruct;
    gfxBuffer->deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createBuffer(&gfxBuffer->createInfo, gfxBuffer->deviceMemoryAllocationHandle);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_runtimeBytes(_korl_resource_gfxBuffer_runtimeBytes)
{
    const _Korl_Resource_GfxBuffer*const gfxBuffer = resourceDescriptorStruct;
    return gfxBuffer->createInfo.bytes;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_runtimeResize(_korl_resource_gfxBuffer_runtimeResize)
{
    _Korl_Resource_GfxBuffer*const gfxBuffer = resourceDescriptorStruct;
    gfxBuffer->createInfo.bytes = bytes;
    *io_data                    = korl_reallocate(allocatorRuntime, *io_data, bytes);
    korl_vulkan_buffer_resize(&gfxBuffer->deviceMemoryAllocationHandle, bytes);
}
korl_internal void korl_resource_gfxBuffer_register(void)
{
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName                = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_GFX_BUFFER);
    descriptorManifest.callbacks.descriptorStructCreate  = korl_functionDynamo_register(_korl_resource_gfxBuffer_descriptorStructCreate);
    descriptorManifest.callbacks.descriptorStructDestroy = korl_functionDynamo_register(_korl_resource_gfxBuffer_descriptorStructDestroy);
    descriptorManifest.callbacks.transcode               = korl_functionDynamo_register(_korl_resource_gfxBuffer_transcode);
    descriptorManifest.callbacks.unload                  = korl_functionDynamo_register(_korl_resource_gfxBuffer_unload);
    descriptorManifest.callbacks.createRuntimeData       = korl_functionDynamo_register(_korl_resource_gfxBuffer_createRuntimeData);
    descriptorManifest.callbacks.createRuntimeMedia      = korl_functionDynamo_register(_korl_resource_gfxBuffer_createRuntimeMedia);
    descriptorManifest.callbacks.runtimeBytes            = korl_functionDynamo_register(_korl_resource_gfxBuffer_runtimeBytes);
    descriptorManifest.callbacks.runtimeResize           = korl_functionDynamo_register(_korl_resource_gfxBuffer_runtimeResize);
    korl_resource_descriptor_register(&descriptorManifest);
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_resource_gfxBuffer_getVulkanDeviceMemoryAllocationHandle(Korl_Resource_Handle handleResourceGfxBuffer)
{
    if(!handleResourceGfxBuffer)
        return 0;// silently return an empty handle for empty resource handles
    //KORL-ISSUE-000-000-181: resource-*: unsafe; validate resource descriptor; it would be nice if we had a low-cost, easy to implement per-descriptor way of validating the descriptorStruct returned from korl_resource_getDescriptorStruct actually matches the descriptor code calling it; the only way I can think of is forcing the descriptor name string lookup for each of these calls, but sure there is a better way, right?
    _Korl_Resource_GfxBuffer*const gfxBuffer = korl_resource_getDescriptorStruct(handleResourceGfxBuffer);
    return gfxBuffer->deviceMemoryAllocationHandle;
}
