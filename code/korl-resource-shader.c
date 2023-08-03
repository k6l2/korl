#include "korl-resource-shader.h"
#include "korl-vulkan.h"
typedef struct _Korl_Resource_Shader
{
    Korl_Vulkan_ShaderHandle vulkanShaderHandle;
} _Korl_Resource_Shader;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(korl_resource_shader_transcode)
{
    _Korl_Resource_Shader*const shaderResource = KORL_C_CAST(_Korl_Resource_Shader*, resource);
    KORL_ZERO_STACK(Korl_Vulkan_CreateInfoShader, createInfoShader);// just keep this data on the stack, since the underlying data is transient anyway, and we don't really need this data to re-create the shader later
    createInfoShader.data      = assetData.data;
    createInfoShader.dataBytes = assetData.dataBytes;
    shaderResource->vulkanShaderHandle = korl_vulkan_shader_create(&createInfoShader, 0);
}
korl_internal void korl_resource_shader_register(void)
{
    korl_resource_descriptor_add(KORL_RAW_CONST_UTF8(KORL_RESOURCE_SHADER_DESCRIPTOR_NAME), sizeof(_Korl_Resource_Shader), korl_resource_shader_transcode);
}
