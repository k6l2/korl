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
    Korl_Resource_Handle        vertexBuffer;// single giant gfx-buffer resource containing all index/attribute data for all MeshPrimitives contained in this resource
} _Korl_Resource_Scene3d;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_scene3d_descriptorStructCreate)
{
    _Korl_Resource_Scene3d*const scene3d = korl_allocate(allocator, sizeof(_Korl_Resource_Scene3d));
    scene3d->allocator = allocator;
    KORL_ZERO_STACK(Korl_Resource_GfxBuffer_CreateInfo, createInfoVertexBuffer);
    createInfoVertexBuffer.bytes = 1024;// some arbitrary base size; we expect this to be resized when this resource gets transcoded
    createInfoVertexBuffer.usageFlags = KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_INDEX 
                                      | KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_VERTEX;
    scene3d->vertexBuffer = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_GFX_BUFFER), &createInfoVertexBuffer);
    return scene3d;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_scene3d_descriptorStructDestroy)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
    for(u16 t = 0; t < scene3d->texturesSize; t++)
        korl_resource_destroy(scene3d->textures[t]);
    korl_resource_destroy(scene3d->vertexBuffer);
    korl_free(allocator, scene3d->textures);
    korl_free(allocator, scene3d);
}
korl_internal const void* _korl_resource_scene3d_transcode_getViewedBuffer(_Korl_Resource_Scene3d*const scene3d, const void* glbFileData, i32 bufferViewIndex)
{
    const Korl_Codec_Gltf_BufferView*const gltfBufferViews = korl_codec_gltf_getBufferViews(scene3d->gltf);
    const Korl_Codec_Gltf_Buffer*const     gltfBuffers     = korl_codec_gltf_getBuffers(scene3d->gltf);
    if(bufferViewIndex < 0)
        korl_log(ERROR, "external image source not supported");
    const Korl_Codec_Gltf_BufferView*const bufferView = gltfBufferViews + bufferViewIndex;
    const Korl_Codec_Gltf_Buffer*const     buffer     = gltfBuffers     + bufferView->buffer;
    if(buffer->stringUri.size)
        korl_log(ERROR, "external & embedded string buffers not supported");
    korl_assert(buffer->glbByteOffset >= 0);// require that the raw image file data is embedded in the GLB file itself as-is
    const void*const bufferData       = KORL_C_CAST(u8*, glbFileData) + buffer->glbByteOffset;
    const void*const viewedBufferData = KORL_C_CAST(u8*, bufferData ) + bufferView->byteOffset;
    return viewedBufferData;
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
        const void*const viewedBufferData = _korl_resource_scene3d_transcode_getViewedBuffer(scene3d, data, image->bufferView);
        const Korl_Codec_Gltf_BufferView*const bufferView = gltfBufferViews + image->bufferView;
        /* create a new runtime texture resource */
        KORL_ZERO_STACK(Korl_Resource_Texture_CreateInfo, createInfo);
        createInfo.imageFileMemoryBuffer.data = viewedBufferData;
        createInfo.imageFileMemoryBuffer.size = bufferView->byteLength;
        if(scene3d->textures[t])
            /* destroy the old texture resource */
            // @TODO: this works for _now_, but if we want the ability to pass this resource around & store it elsewhere, we might need the ability to as korl-resource "re-create" a resource; perhaps we can just modify korl_resource_create to take a Resource_Handle param, & perform alternate logic based on this value (if 0, just make a new resource, otherwise we get the previous resource from the pool & unload it)
            korl_resource_destroy(scene3d->textures[t]);
        scene3d->textures[t] = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE), &createInfo);
    }
    /* transcode meshes */
    const Korl_Codec_Gltf_Accessor*const gltfAccessors = korl_codec_gltf_getAccessors(scene3d->gltf);
    /* all mesh primitive data for the entire scene3d resource can reasonably 
        just use the same gfx-buffer with specific offsets for each individual 
        mesh primitive's individual indices/attributes; let's iterate over all 
        mesh primitives & calculate:
        - vertex meta data
        - build a gfx-buffer holding all the index/attribute data */
    u$ vertexBufferBytes     = 1024;
    u$ vertexBufferBytesUsed = 0;// after we extract all the index/attribute data, we will resize the gfx-buffer to match this value exactly
    korl_resource_resize(scene3d->vertexBuffer, vertexBufferBytes);// reset our vertexBuffer to some base size
    const Korl_Codec_Gltf_Mesh*const gltfMeshes = korl_codec_gltf_getMeshes(scene3d->gltf);
    for(u32 m = 0; m < scene3d->gltf->meshes.size; m++)
    {
        const Korl_Codec_Gltf_Mesh*const           mesh                            = gltfMeshes + m;
        const Korl_Codec_Gltf_Mesh_Primitive*const gltfMeshPrimitives              = korl_codec_gltf_mesh_getPrimitives(scene3d->gltf, mesh);
        Korl_Gfx_VertexStagingMeta*                meshPrimitiveVertexStagingMetas = korl_allocate(scene3d->allocator, mesh->primitives.size * sizeof(*meshPrimitiveVertexStagingMetas));
        Korl_Gfx_Material_PrimitiveType*           meshPrimitiveTypes              = korl_allocate(scene3d->allocator, mesh->primitives.size * sizeof(*meshPrimitiveTypes));
        /* extract vertex staging meta & store all vertex data into a runtime buffer resource */
        for(u32 mp = 0; mp < mesh->primitives.size; mp++)
        {
            const Korl_Codec_Gltf_Mesh_Primitive*const meshPrimitive     = gltfMeshPrimitives              + mp;
            Korl_Gfx_VertexStagingMeta*const           vertexStagingMeta = meshPrimitiveVertexStagingMetas + mp;
            Korl_Gfx_Material_PrimitiveType*const      primitiveType     = meshPrimitiveTypes              + mp;
            switch(meshPrimitive->mode)
            {
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_LINES         : *primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINES;          break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_LINE_STRIP    : *primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINE_STRIP;     break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLES     : *primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES;      break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLE_STRIP: *primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_STRIP; break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLE_FAN  : *primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_FAN;   break;
            default: korl_log(ERROR, "unsupported MeshPrimitive mode: %i", meshPrimitive->mode);
            }
            if(meshPrimitive->indices >= 0)
            {
                const Korl_Codec_Gltf_Accessor*const accessor = gltfAccessors + meshPrimitive->indices;
                const void*const viewedBufferData = _korl_resource_scene3d_transcode_getViewedBuffer(scene3d, data, accessor->bufferView);
                const Korl_Codec_Gltf_BufferView*const bufferView = gltfBufferViews + accessor->bufferView;
                if(bufferView->byteStride)
                    // I really can't see a scenario where serializing interleaved mesh primitive attributes is a good idea, so let's just not support them for now
                    korl_log(ERROR, "interleaved attributes not supported");
                /* update our vertex staging meta with accessor & buffer data */
                vertexStagingMeta->indexCount            = accessor->count;
                vertexStagingMeta->indexByteOffsetBuffer = korl_checkCast_u$_to_u32(vertexBufferBytesUsed);
                switch(accessor->componentType)
                {
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U16: vertexStagingMeta->indexType = KORL_GFX_VERTEX_INDEX_TYPE_U16; break;
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U32: vertexStagingMeta->indexType = KORL_GFX_VERTEX_INDEX_TYPE_U32; break;
                default: korl_log(ERROR, "invalid vertex index componentType: %i", accessor->componentType); break;
                }
                /* copy the viewedBufferData to our scene3d's vertex buffer */
                if(vertexBufferBytesUsed + bufferView->byteLength > vertexBufferBytes)
                    korl_resource_resize(scene3d->vertexBuffer, KORL_MATH_MAX(vertexBufferBytesUsed + bufferView->byteLength, 2 * vertexBufferBytes));
                korl_resource_update(scene3d->vertexBuffer, viewedBufferData, bufferView->byteLength, vertexBufferBytesUsed);
                vertexBufferBytesUsed += bufferView->byteLength;
            }
            korl_shared_const Korl_Gfx_VertexAttributeBinding GLTF_ATTRIBUTE_BINDINGS[] = 
                {KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION
                ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_NORMAL
                ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV
                ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_JOINTS_0
                ,KORL_GFX_VERTEX_ATTRIBUTE_BINDING_JOINT_WEIGHTS_0};
            KORL_STATIC_ASSERT(korl_arraySize(GLTF_ATTRIBUTE_BINDINGS) == KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_ENUM_COUNT);
            for(u8 a = 0; a < KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_ENUM_COUNT; a++)
            {
                if(meshPrimitive->attributes[a] < 0)
                    continue;
                const Korl_Codec_Gltf_Accessor*const accessor = gltfAccessors + meshPrimitive->attributes[a];
                const void*const viewedBufferData = _korl_resource_scene3d_transcode_getViewedBuffer(scene3d, data, accessor->bufferView);
                const Korl_Codec_Gltf_BufferView*const bufferView = gltfBufferViews + accessor->bufferView;
                if(bufferView->byteStride)
                    // I really can't see a scenario where serializing interleaved mesh primitive attributes is a good idea, so let's just not support them for now
                    korl_log(ERROR, "interleaved attributes not supported");
                /* update our vertex staging meta with accessor & buffer data */
                if(vertexStagingMeta->vertexCount)
                    korl_assert(accessor->count == vertexStagingMeta->vertexCount);
                else
                    vertexStagingMeta->vertexCount = accessor->count;
                const Korl_Gfx_VertexAttributeBinding binding = GLTF_ATTRIBUTE_BINDINGS[a];
                vertexStagingMeta->vertexAttributeDescriptors[binding].byteOffsetBuffer = korl_checkCast_u$_to_u32(vertexBufferBytesUsed);
                vertexStagingMeta->vertexAttributeDescriptors[binding].byteStride       = korl_codec_gltf_accessor_getStride(accessor, gltfBufferViews);
                vertexStagingMeta->vertexAttributeDescriptors[binding].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
                switch(accessor->componentType)
                {
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U8 : vertexStagingMeta->vertexAttributeDescriptors[binding].elementType = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;  break;
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U32: vertexStagingMeta->vertexAttributeDescriptors[binding].elementType = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32; break;
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_F32: vertexStagingMeta->vertexAttributeDescriptors[binding].elementType = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32; break;
                default:
                    korl_log(ERROR, "unsupported componentType: %u", accessor->componentType);
                }
                switch(accessor->type)
                {
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_SCALAR: vertexStagingMeta->vertexAttributeDescriptors[binding].vectorSize =     1; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC2  : vertexStagingMeta->vertexAttributeDescriptors[binding].vectorSize =     2; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC3  : vertexStagingMeta->vertexAttributeDescriptors[binding].vectorSize =     3; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC4  : vertexStagingMeta->vertexAttributeDescriptors[binding].vectorSize =     4; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT2  : vertexStagingMeta->vertexAttributeDescriptors[binding].vectorSize = 2 * 2; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT3  : vertexStagingMeta->vertexAttributeDescriptors[binding].vectorSize = 3 * 3; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT4  : vertexStagingMeta->vertexAttributeDescriptors[binding].vectorSize = 4 * 4; break;
                }
                /* copy the viewedBufferData to our scene3d's vertex buffer */
                if(vertexBufferBytesUsed + bufferView->byteLength > vertexBufferBytes)
                    korl_resource_resize(scene3d->vertexBuffer, KORL_MATH_MAX(vertexBufferBytesUsed + bufferView->byteLength, 2 * vertexBufferBytes));
                korl_resource_update(scene3d->vertexBuffer, viewedBufferData, bufferView->byteLength, vertexBufferBytesUsed);
                vertexBufferBytesUsed += bufferView->byteLength;
            }
        }
        /* create a mesh resource */
        // how do we ensure that we create a resource that has the same Resource_Handle as a user-requested Mesh resource during an earlier time when this resource was not yet loaded?
        // - we could implement some kind of system in korl-resource where we are able to "reserve" resource handles without making a resource
        // - we could just implement special APIs for korl-resource-mesh that allow us to update or re-create the Mesh resource from a new CreateInfo
        //   - korl-resource-scene3d would immediately create a Mesh resource upon user request
        //   - once we get to this point, we would then update the Mesh resource with the proper CreateInfo
        //   - at the end of this function, we can check to see if all of the mesh names that were priviously requested by the user have indeed been transcoded
        //@TODO
        korl_free(scene3d->allocator, meshPrimitiveVertexStagingMetas);
        korl_free(scene3d->allocator, meshPrimitiveTypes);
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
korl_internal KORL_FUNCTION_korl_resource_scene3d_getMesh(korl_resource_scene3d_getMesh)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    korl_assert(scene3d);// handleResourceScene3d _must_ at least be a valid resource handle for this function to work
    /* if the resource has been transcoded, we can just do a lookup to see if 
        any of our gltf mesh names match the requested mesh name */
    if(scene3d->gltf)
    {
        korl_assert(!"@TODO"); return 0;
    }
    else
    /* otherwise, if we have not yet transcoded this resource, we must create a 
        dummy mesh resource & add this getMesh request to a transient list 
        within the scene3d resource, then when we transcode the scene3d resource 
        we can re-create these dummy mesh resources & verify that all transient 
        mesh request names are contained within the scene3d's transcoded list of 
        mesh names */
    {
        korl_assert(!"@TODO"); return 0;
    }
}
