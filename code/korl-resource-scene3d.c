#include "korl-resource-scene3d.h"
#include "korl-interface-platform.h"
#include "korl-codec-glb.h"
typedef struct _Korl_Resource_Scene3d
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_Codec_Gltf*            gltf;
} _Korl_Resource_Scene3d;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_scene3d_descriptorStructCreate)
{
    _Korl_Resource_Scene3d*const scene3d = korl_allocate(allocator, sizeof(_Korl_Resource_Scene3d));
    scene3d->allocator = allocator;
    return scene3d;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_scene3d_descriptorStructDestroy)
{
    korl_free(allocator, resourceDescriptorStruct);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_scene3d_transcode)
{
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(_korl_resource_scene3d_clearTransientData)
{
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_scene3d_unload)
{
    _korl_resource_scene3d_clearTransientData(resourceDescriptorStruct);
}
korl_internal void korl_resource_scene3d_register(void)
{
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SCENE3D);
    descriptorManifest.callbacks.descriptorStructCreate  = korl_functionDynamo_register(_korl_resource_scene3d_descriptorStructCreate);
    descriptorManifest.callbacks.descriptorStructDestroy = korl_functionDynamo_register(_korl_resource_scene3d_descriptorStructDestroy);
    descriptorManifest.callbacks.unload                  = korl_functionDynamo_register(_korl_resource_scene3d_unload);
    descriptorManifest.callbacks.transcode               = korl_functionDynamo_register(_korl_resource_scene3d_transcode);
    descriptorManifest.callbacks.clearTransientData      = korl_functionDynamo_register(_korl_resource_scene3d_clearTransientData);
    korl_resource_descriptor_add(&descriptorManifest);
}
