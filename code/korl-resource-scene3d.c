#include "korl-resource-scene3d.h"
#include "korl-interface-platform.h"
#include "korl-codec-glb.h"
#include "utility/korl-checkCast.h"
typedef struct _Korl_Resource_Scene3d
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_Codec_Gltf*            gltf;
    Korl_Resource_Handle*       textures;
    u16                         texturesSize;// should be == gltf->textures.size
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
    for(u16 t = 0; t < scene3d->texturesSize; t++)
        korl_resource_destroy(scene3d->textures[t]);
    korl_free(allocator, scene3d->textures);
    korl_free(allocator, scene3d);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_scene3d_transcode)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
    korl_assert(!scene3d->gltf);
    scene3d->gltf = korl_codec_glb_decode(data, dataBytes, scene3d->allocator);
    const Korl_Codec_Gltf_BufferView*const gltfBufferViews = korl_codec_gltf_getBufferViews(scene3d->gltf);
    const Korl_Codec_Gltf_Buffer*const     gltfBuffers     = korl_codec_gltf_getBuffers(scene3d->gltf);
    /* transcode materials */
    const Korl_Codec_Gltf_Texture*const    gltfTextures = korl_codec_gltf_getTextures(scene3d->gltf);
    const Korl_Codec_Gltf_Image*const      gltfImages   = korl_codec_gltf_getImages(scene3d->gltf);
    const Korl_Codec_Gltf_Sampler*const    gltfSamplers = korl_codec_gltf_getSamplers(scene3d->gltf);
    if(scene3d->gltf->textures.size)
        if(scene3d->textures)
            // constrain scene3d re-transcodes to maintain the same # of textures between loads
            korl_assert(scene3d->texturesSize == scene3d->gltf->textures.size);
        else
            scene3d->textures = korl_allocate(scene3d->allocator, scene3d->gltf->textures.size * sizeof(*scene3d->textures));
    scene3d->texturesSize = korl_checkCast_u$_to_u16(scene3d->gltf->textures.size);
    for(u32 t = 0; t < scene3d->gltf->textures.size; t++)
    {
        const Korl_Codec_Gltf_Sampler*const sampler = gltfTextures[t].sampler < 0 ? &KORL_CODEC_GLTF_SAMPLER_DEFAULT : gltfSamplers + gltfTextures[t].sampler;
        if(gltfTextures[t].source < 0)
            korl_log(ERROR, "undefined texture source not supported");// perhaps in the future we can just assign a "default" source image of some kind
        const Korl_Codec_Gltf_Image*const image = gltfImages + gltfTextures[t].source;
        if(image->bufferView < 0)
            korl_log(ERROR, "external image source not supported");
        const Korl_Codec_Gltf_BufferView*const imageBufferView = gltfBufferViews + image->bufferView;
        const Korl_Codec_Gltf_Buffer*const     imageBuffer     = gltfBuffers + imageBufferView->buffer;
        if(imageBuffer->stringUri.size)
            korl_log(ERROR, "external & embedded string buffers not supported");
        korl_assert(imageBuffer->glbByteOffset >= 0);// require that the raw image file data is embedded in the GLB file itself as-is
        /* create a new runtime texture resource */
        KORL_ZERO_STACK(Korl_Resource_Texture_CreateInfo, createInfo);
        createInfo.imageFileMemoryBuffer.data = KORL_C_CAST(u8*, data) + imageBuffer->glbByteOffset + imageBufferView->byteOffset;
        createInfo.imageFileMemoryBuffer.size = imageBufferView->byteLength;
        if(scene3d->textures[t])
            /* destroy the old texture resource */
            // @TODO: this works for _now_, but if we want the ability to pass this resource around & store it elsewhere, we might need the ability to as korl-resource "re-create" a resource; perhaps we can just modify korl_resource_create to take a Resource_Handle param, & perform alternate logic based on this value (if 0, just make a new resource, otherwise we get the previous resource from the pool & unload it)
            korl_resource_destroy(scene3d->textures[t]);
        scene3d->textures[t] = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE), &createInfo);
    }
    /* transcode meshes */
    const Korl_Codec_Gltf_Mesh*const gltfMeshes = korl_codec_gltf_getMeshes(scene3d->gltf);
    for(u32 m = 0; m < scene3d->gltf->meshes.size; m++)
    {
        const Korl_Codec_Gltf_Mesh*const           mesh               = gltfMeshes + m;
        const Korl_Codec_Gltf_Mesh_Primitive*const gltfMeshPrimitives = korl_codec_gltf_mesh_getPrimitives(scene3d->gltf, mesh);
        for(u32 mp = 0; mp < mesh->primitives.size; mp++)
        {
            const Korl_Codec_Gltf_Mesh_Primitive*const meshPrimitive = gltfMeshPrimitives + mp;
            /* extract vertex staging meta & store all vertex data into a runtime buffer resource */
            //@TODO
            /* ??? create a mesh resource? */
            // how do we ensure that we create a resource that has the same Resource_Handle as a user-requested Mesh resource during an earlier time when this resource was not yet loaded?
            // - we could implement some kind of system in korl-resource where we are able to "reserve" resource handles without making a resource
            // - we could just implement special APIs for korl-resource-mesh that allow us to update or re-create the Mesh resource from a new CreateInfo
            //   - korl-resource-scene3d would immediately create a Mesh resource upon user request
            //   - once we get to this point, we would then update the Mesh resource with the proper CreateInfo
            //   - at the end of this function, we can check to see if all of the mesh names that were priviously requested by the user have indeed been transcoded
            //@TODO
        }
    }
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(_korl_resource_scene3d_clearTransientData)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
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
        return korl_gfx_material_defaultUnlit();
    korl_assert(materialIndex < scene3d->gltf->materials.size);
    const Korl_Codec_Gltf_Material*const gltfMaterial = korl_codec_gltf_getMaterials(scene3d->gltf) + materialIndex;
    Korl_Gfx_Material result = korl_gfx_material_defaultUnlit();
    if(gltfMaterial->pbrMetallicRoughness.baseColorTextureIndex >= 0)
        result.maps.resourceHandleTextureBase = scene3d->textures[gltfMaterial->pbrMetallicRoughness.baseColorTextureIndex];
    if(gltfMaterial->KHR_materials_specular.specularColorTextureIndex >= 0)
        result.maps.resourceHandleTextureSpecular = scene3d->textures[gltfMaterial->KHR_materials_specular.specularColorTextureIndex];
    return result;
}
