/** @TODO: what is the purpose of this resource module??  We're basically doing no actual work in here; 
 * why not just use scene3d resource everywhere & store a mesh index or something in "Drawable" structs that need to draw a mesh?
 * is this resource module just a giant waste of time?...
*/
#include "korl-resource-mesh.h"
#include "korl-interface-platform.h"
#include "korl-vulkan.h"
typedef struct _Korl_Resource_Mesh_Primitive
{
    Korl_Gfx_Material_PrimitiveType primitiveType;
    Korl_Gfx_VertexStagingMeta      vertexStagingMeta;
    Korl_Gfx_Material               material;
} _Korl_Resource_Mesh_Primitive;
typedef struct _Korl_Resource_Mesh
{
    Korl_Memory_AllocatorHandle    allocator;
    Korl_Resource_Handle           vertexBuffer;// shallow resource; someone else manages this, so do not destroy it in here
    _Korl_Resource_Mesh_Primitive* primitives;
    u16                            primitivesSize;
} _Korl_Resource_Mesh;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_mesh_descriptorStructCreate)
{
    _Korl_Resource_Mesh*const mesh = korl_allocate(allocator, sizeof(*mesh));
    mesh->allocator = allocator;
    return mesh;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_mesh_descriptorStructDestroy)
{
    _Korl_Resource_Mesh*const mesh = resourceDescriptorStruct;
    korl_free(allocator, mesh->primitives);
    korl_free(allocator, mesh);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_mesh_unload)
{
    _Korl_Resource_Mesh*const mesh = resourceDescriptorStruct;
    korl_free(mesh->allocator, mesh->primitives);
    mesh->primitives = NULL;
    mesh->primitivesSize = 0;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_mesh_transcode)
{
    _Korl_Resource_Mesh*const mesh = resourceDescriptorStruct;
    // nothing to do here? all our resources are already transcoded
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeData(_korl_resource_mesh_createRuntimeData)
{
    _Korl_Resource_Mesh*const                 mesh       = resourceDescriptorStruct;
    const Korl_Resource_Mesh_CreateInfo*const createInfo = descriptorCreateInfo;
    korl_assert(!"@TODO: what is the point of this resource descriptor again?...");
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeMedia(_korl_resource_mesh_createRuntimeMedia)
{
    _Korl_Resource_Mesh*const mesh = resourceDescriptorStruct;
    // nothing to do here; all our media resources are pre-constructed
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_runtimeBytes(_korl_resource_mesh_runtimeBytes)
{
    const _Korl_Resource_Mesh*const mesh = resourceDescriptorStruct;
    korl_assert(!"not implemented"); return 0;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_runtimeResize(_korl_resource_mesh_runtimeResize)
{
    _Korl_Resource_Mesh*const mesh = resourceDescriptorStruct;
    korl_assert(!"not implemented");
}
korl_internal void korl_resource_mesh_register(void)
{
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_MESH);
    descriptorManifest.callbacks.descriptorStructCreate  = korl_functionDynamo_register(_korl_resource_mesh_descriptorStructCreate);
    descriptorManifest.callbacks.descriptorStructDestroy = korl_functionDynamo_register(_korl_resource_mesh_descriptorStructDestroy);
    descriptorManifest.callbacks.unload                  = korl_functionDynamo_register(_korl_resource_mesh_unload);
    descriptorManifest.callbacks.transcode               = korl_functionDynamo_register(_korl_resource_mesh_transcode);
    descriptorManifest.callbacks.createRuntimeData       = korl_functionDynamo_register(_korl_resource_mesh_createRuntimeData);
    descriptorManifest.callbacks.createRuntimeMedia      = korl_functionDynamo_register(_korl_resource_mesh_createRuntimeMedia);
    descriptorManifest.callbacks.runtimeBytes            = korl_functionDynamo_register(_korl_resource_mesh_runtimeBytes);
    descriptorManifest.callbacks.runtimeResize           = korl_functionDynamo_register(_korl_resource_mesh_runtimeResize);
    korl_resource_descriptor_add(&descriptorManifest);
}
