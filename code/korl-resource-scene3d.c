#include "korl-resource-scene3d.h"
#include "korl-interface-platform.h"
#include "korl-codec-glb.h"
typedef struct _Korl_Resource_Scene3d
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_Codec_Gltf*            gltf;
    Korl_Resource_Handle*       textures;// array size == gltf.textures.size
} _Korl_Resource_Scene3d;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_scene3d_descriptorStructCreate)
{
    _Korl_Resource_Scene3d*const scene3d = korl_allocate(allocator, sizeof(_Korl_Resource_Scene3d));
    scene3d->allocator = allocator;
    return scene3d;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_scene3d_descriptorStructDestroy)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
    korl_free(allocator, scene3d);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_scene3d_transcode)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
    korl_assert(!scene3d->gltf);
    scene3d->gltf = korl_codec_glb_decode(data, dataBytes, scene3d->allocator);
    const Korl_Codec_Gltf_Texture*const    gltfTextures    = korl_codec_gltf_getTextures(scene3d->gltf);
    const Korl_Codec_Gltf_Image*const      gltfImages      = korl_codec_gltf_getImages(scene3d->gltf);
    const Korl_Codec_Gltf_Sampler*const    gltfSamplers    = korl_codec_gltf_getSamplers(scene3d->gltf);
    const Korl_Codec_Gltf_BufferView*const gltfBufferViews = korl_codec_gltf_getBufferViews(scene3d->gltf);
    const Korl_Codec_Gltf_Buffer*const     gltfBuffers     = korl_codec_gltf_getBuffers(scene3d->gltf);
    if(scene3d->gltf->textures.size)
        scene3d->textures = korl_allocate(scene3d->allocator, scene3d->gltf->textures.size * sizeof(*scene3d->textures));
    for(u32 i = 0; i < scene3d->gltf->textures.size; i++)
    {
        const Korl_Codec_Gltf_Sampler*const sampler = gltfTextures[i].sampler < 0 ? &KORL_CODEC_GLTF_SAMPLER_DEFAULT : gltfSamplers + gltfTextures[i].sampler;
        if(gltfTextures[i].source < 0)
            korl_log(ERROR, "undefined texture source not supported");// perhaps in the future we can just assign a "default" source image of some kind
        const Korl_Codec_Gltf_Image*const image = gltfImages + gltfTextures[i].source;
        if(image->bufferView < 0)
            korl_log(ERROR, "external image source not supported");
        const Korl_Codec_Gltf_BufferView*const imageBufferView = gltfBufferViews + image->bufferView;
        const Korl_Codec_Gltf_Buffer*const     imageBuffer     = gltfBuffers + imageBufferView->buffer;
        if(imageBuffer->stringUri.size)
            korl_log(ERROR, "external & embedded string buffers not supported");
        korl_assert(imageBuffer->glbByteOffset >= 0);// require that the raw image file data is embedded in the GLB file itself as-is
        KORL_ZERO_STACK(Korl_Resource_Texture_CreateInfo, createInfo);
        createInfo.imageFileMemoryBuffer.data = KORL_C_CAST(u8*, data) + imageBuffer->glbByteOffset + imageBufferView->byteOffset;
        createInfo.imageFileMemoryBuffer.size = imageBufferView->byteLength;
        scene3d->textures[i] = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE), &createInfo);
    }
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(_korl_resource_scene3d_clearTransientData)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
    for(u32 i = 0; i < scene3d->gltf->textures.size; i++)
        korl_resource_destroy(scene3d->textures[i]);
    korl_free(scene3d->allocator, scene3d->textures);
    scene3d->textures = NULL;
    korl_free(scene3d->allocator, scene3d->gltf);
    scene3d->gltf = NULL;
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
korl_internal KORL_FUNCTION_korl_resource_scene3d_getMaterialCount(korl_resource_scene3d_getMaterialCount)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return 0;
    return scene3d->gltf->materials.size;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_getMaterial(korl_resource_scene3d_getMaterial)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return korl_gfx_material_defaultUnlit(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_INVALID, KORL_GFX_MATERIAL_MODE_FLAGS_NONE, korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    korl_assert(materialIndex < scene3d->gltf->materials.size);
    const Korl_Codec_Gltf_Material*const gltfMaterial = korl_codec_gltf_getMaterials(scene3d->gltf) + materialIndex;
    Korl_Gfx_Material result = korl_gfx_material_defaultUnlit(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_INVALID, KORL_GFX_MATERIAL_MODE_FLAGS_NONE, korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    if(gltfMaterial->pbrMetallicRoughness.baseColorTextureIndex >= 0)
        result.maps.resourceHandleTextureBase = scene3d->textures[gltfMaterial->pbrMetallicRoughness.baseColorTextureIndex];
    if(gltfMaterial->KHR_materials_specular.specularColorTextureIndex >= 0)
        result.maps.resourceHandleTextureSpecular = scene3d->textures[gltfMaterial->KHR_materials_specular.specularColorTextureIndex];
    return result;
}
