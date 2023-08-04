#include "korl-resource-shader.h"
#include "korl-vulkan.h"
typedef struct _Korl_Resource_Shader
{
    Korl_Vulkan_ShaderHandle vulkanShaderHandle;
} _Korl_Resource_Shader;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(korl_resource_shader_unload)
{
    _Korl_Resource_Shader*const shaderResource = KORL_C_CAST(_Korl_Resource_Shader*, resourceDescriptorStruct);
    korl_vulkan_shader_destroy(shaderResource->vulkanShaderHandle);
    shaderResource->vulkanShaderHandle = 0;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(korl_resource_shader_transcode)
{
    _Korl_Resource_Shader*const shaderResource = KORL_C_CAST(_Korl_Resource_Shader*, resourceDescriptorStruct);
    KORL_ZERO_STACK(Korl_Vulkan_CreateInfoShader, createInfoShader);// just keep this data on the stack, since the underlying data is transient anyway, and we don't really need this data to re-create the shader later
    createInfoShader.data      = assetData.data;
    createInfoShader.dataBytes = assetData.dataBytes;
    shaderResource->vulkanShaderHandle = korl_vulkan_shader_create(&createInfoShader, 0);
}
korl_internal void korl_resource_shader_register(void)
{
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName    = KORL_RAW_CONST_UTF8(KORL_RESOURCE_SHADER_DESCRIPTOR_NAME);
    descriptorManifest.resourceBytes         = sizeof(_Korl_Resource_Shader);
    descriptorManifest.callbackUnload        =                      korl_resource_shader_unload;
    descriptorManifest.utf8CallbackUnload    = KORL_RAW_CONST_UTF8("korl_resource_shader_unload");
    descriptorManifest.callbackTranscode     =                      korl_resource_shader_transcode;
    descriptorManifest.utf8CallbackTranscode = KORL_RAW_CONST_UTF8("korl_resource_shader_transcode");
    korl_resource_descriptor_add(&descriptorManifest);
}
korl_internal Korl_Vulkan_ShaderHandle korl_resource_shader_getHandle(Korl_Resource_Handle handleResourceShader)
{
    //@TODO: validate that the korl-resource-item referenced by handleResourceShader is, indeed, a SHADER resource
    _Korl_Resource_Shader*const shaderResource = korl_resource_getDescriptorStruct(handleResourceShader);
    return shaderResource->vulkanShaderHandle;
}
