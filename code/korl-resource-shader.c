#include "korl-resource-shader.h"
#include "korl-vulkan.h"
typedef struct _Korl_Resource_Shader
{
    Korl_Vulkan_ShaderHandle vulkanShaderHandle;
} _Korl_Resource_Shader;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_shader_descriptorStructCreate)
{
    return korl_allocate(allocatorRuntime, sizeof(_Korl_Resource_Shader));
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_shader_descriptorStructDestroy)
{
    korl_free(allocatorRuntime, resourceDescriptorStruct);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(_korl_resource_shader_clearTransientData)
{
    _Korl_Resource_Shader*const shaderResource = KORL_C_CAST(_Korl_Resource_Shader*, resourceDescriptorStruct);
    /* no need to destroy the vulkan shader, as all korl-vulkan device objects should already be destroyed */
    shaderResource->vulkanShaderHandle = 0;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_shader_unload)
{
    _Korl_Resource_Shader*const shaderResource = KORL_C_CAST(_Korl_Resource_Shader*, resourceDescriptorStruct);
    korl_vulkan_shader_destroy(shaderResource->vulkanShaderHandle);
    shaderResource->vulkanShaderHandle = 0;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_shader_transcode)
{
    _Korl_Resource_Shader*const shaderResource = KORL_C_CAST(_Korl_Resource_Shader*, resourceDescriptorStruct);
    KORL_ZERO_STACK(Korl_Vulkan_CreateInfoShader, createInfoShader);// just keep this data on the stack, since the underlying data is transient anyway, and we don't really need this data to re-create the shader later
    createInfoShader.data      = data;
    createInfoShader.dataBytes = dataBytes;
    shaderResource->vulkanShaderHandle = korl_vulkan_shader_create(&createInfoShader, shaderResource->vulkanShaderHandle);
}
korl_internal void korl_resource_shader_register(void)
{
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER);
    descriptorManifest.callbacks.descriptorStructCreate  = korl_functionDynamo_register(_korl_resource_shader_descriptorStructCreate);
    descriptorManifest.callbacks.descriptorStructDestroy = korl_functionDynamo_register(_korl_resource_shader_descriptorStructDestroy);
    descriptorManifest.callbacks.clearTransientData      = korl_functionDynamo_register(_korl_resource_shader_clearTransientData);
    descriptorManifest.callbacks.unload                  = korl_functionDynamo_register(_korl_resource_shader_unload);
    descriptorManifest.callbacks.transcode               = korl_functionDynamo_register(_korl_resource_shader_transcode);
    korl_resource_descriptor_register(&descriptorManifest);
}
korl_internal Korl_Vulkan_ShaderHandle korl_resource_shader_getHandle(Korl_Resource_Handle handleResourceShader)
{
    if(!handleResourceShader)
        return 0;// silently return an empty handle for empty resource handles
    //KORL-ISSUE-000-000-181: resource-*: unsafe; validate resource descriptor; it would be nice if we had a low-cost, easy to implement per-descriptor way of validating the descriptorStruct returned from korl_resource_getDescriptorStruct actually matches the descriptor code calling it; the only way I can think of is forcing the descriptor name string lookup for each of these calls, but sure there is a better way, right?
    _Korl_Resource_Shader*const shaderResource = korl_resource_getDescriptorStruct(handleResourceShader);
    return shaderResource->vulkanShaderHandle;
}
