#include "korl-resource-scene3d.h"
#include "korl-interface-platform.h"
#include "korl-codec-glb.h"
#include "utility/korl-checkCast.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-utility-algorithm.h"
#include "utility/korl-utility-stb-ds.h"
typedef struct _Korl_Resource_Scene3d_Context
{
    Korl_Resource_Handle defaultResources[KORL_RESOURCE_SCENE3D_DEFAULTRESOURCE_ENUM_COUNT];
} _Korl_Resource_Scene3d_Context;
typedef struct _Korl_Resource_Scene3d_Animation_SampleSet
{
    // NOTE: this SampleSet's `keyFramesSeconds` array & `sample` array should have the exact same size
    u32 keyFramesSecondsStartIndex;
    u32 sampleStartIndex;
    // the SampleSet's interpolation method is contained within this SampleSet's respective gltfSampler->interpolation
} _Korl_Resource_Scene3d_Animation_SampleSet;
typedef struct _Korl_Resource_Scene3d_Animation
{
    f32                                         keyFrameSecondsStart;
    f32                                         keyFrameSecondsEnd;
    _Korl_Resource_Scene3d_Animation_SampleSet* sampleSets;// array size == gltfAnimation->samplers.size
    f32*                                        keyFramesSeconds;// array size == SUM(unique(gltfAnimation->samplers[i].input).count)
    /** NOTE: we likely want to store our own f32 Samples like this because gltf 
     * allows Animation Sampler Accessors to interpret the raw data buffer with 
     * various component encodings; when the Animation is being transcoded, we 
     * will just transcode the Sampler output data into raw floats, preventing 
     * needless overhead associated with converting these values at runtime; it 
     * _should_ also make the code needed to compute animations simpler, since 
     * we only have to worry about one component primitive type */
    f32*                                        samples;
} _Korl_Resource_Scene3d_Animation;
typedef struct _Korl_Resource_Scene3d_Skin
{
    /** all these array sizes should == this object's respective gltfSkin->joints.size */
    Korl_Math_M4f32* boneInverseBindMatrices;
    u32*             boneTopologicalOrder;// indices of each bone in the boneInverseBindMatrices array, as well as all Korl_Resource_Scene3d_Skin bone members, and all joints in the gltf.skin, in topological order (root nodes first, followed by children)
    i32*             boneParentIndices;// < 0 => bone has no parent
    i32*             nodeIndex_to_boneIndex;// < 0 => gltfNode is not related to any bones of this skin; acceleration structure; useful for topological sort, performing animations on gltfNodes instead of bones
} _Korl_Resource_Scene3d_Skin;
typedef struct _Korl_Resource_Scene3d_Mesh
{
    u16 meshPrimitivesOffset;// acceleration data for Korl_Resource_Scene3d_MeshPrimitive lookups; the index of the first MeshPrimitive associated with a given gltf->mesh in the meshPrimitives array
    i32 skinIndex;           // < 0 => no skin is compatible with this mesh
} _Korl_Resource_Scene3d_Mesh;
typedef struct _Korl_Resource_Scene3d
{
    /** _all_ of the dynamic allocations we will manage will be stored in 
     *  transient memory; it all can & will be reconstructed in the event that 
     * we are loading from a korl-memoryState; 
     * NOTE: I am choosing to manage a 
     * single transient dynamic memory allocation for all of this data, and 
     * reference individual portions of it with various pointers throughout this 
     * struct; 
     * NOTE: I am choosing to store pointers instead of memory offsets 
     * in order to make it easier to debug this data */
    Korl_Codec_Gltf*                     gltf;
    // _all_ of the dynamic memory pointers below here are simply pointing to offsets into the `gltf` allocation
    Korl_Resource_Handle*                textures;
    Korl_Resource_Handle                 vertexBuffer;      // single giant gfx-buffer resource containing all index/attribute data for all MeshPrimitives contained in this resource
    _Korl_Resource_Scene3d_Mesh*         meshes;            // meta data (acceleration, mappings, etc.) for each gltf->mesh
    Korl_Resource_Scene3d_MeshPrimitive* meshPrimitives;    // array containing _all_ mesh primitives for all meshes
    u16                                  meshPrimitivesSize;// should be == SUM(gltf->meshes[i].primitives.size)
    _Korl_Resource_Scene3d_Skin*         skins;             // pre-baked data needed to instantiate a Korl_Resource_Scene3d_Skin
    _Korl_Resource_Scene3d_Animation*    animations;
} _Korl_Resource_Scene3d;
korl_internal KORL_HEAP_ON_ALLOCATION_MOVED_CALLBACK(_korl_resource_scene3d_onTransientMemoryMovedCallback)
{
    /* Scene3d is holding a bunch of memory addresses pointing to data within 
        the `scene3d->gltf` allocation, which just moved & invalidated all of them */
    _Korl_Resource_Scene3d*const scene3d = allocationAddressParent;
    //NOTE: no need to move scene3d->gltf, as that is exactly what just got moved
    scene3d->textures       = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->textures)       + byteOffsetFromOldAddress);
    scene3d->meshes         = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->meshes)         + byteOffsetFromOldAddress);
    scene3d->meshPrimitives = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->meshPrimitives) + byteOffsetFromOldAddress);
    scene3d->skins          = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->skins)          + byteOffsetFromOldAddress);
    scene3d->animations     = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->animations)     + byteOffsetFromOldAddress);
    for(u32 s = 0; s < scene3d->gltf->skins.size; s++)
    {
        scene3d->skins[s].boneInverseBindMatrices = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->skins[s].boneInverseBindMatrices) + byteOffsetFromOldAddress);
        scene3d->skins[s].boneTopologicalOrder    = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->skins[s].boneTopologicalOrder)    + byteOffsetFromOldAddress);
        scene3d->skins[s].boneParentIndices       = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->skins[s].boneParentIndices)       + byteOffsetFromOldAddress);
        scene3d->skins[s].nodeIndex_to_boneIndex  = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->skins[s].nodeIndex_to_boneIndex)  + byteOffsetFromOldAddress);
    }
    for(u32 a = 0; a < scene3d->gltf->animations.size; a++)
    {
        scene3d->animations[a].samples          = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->animations[a].samples)          + byteOffsetFromOldAddress);
        scene3d->animations[a].sampleSets       = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->animations[a].sampleSets)       + byteOffsetFromOldAddress);
        scene3d->animations[a].keyFramesSeconds = KORL_C_CAST(void*, KORL_C_CAST(i$, scene3d->animations[a].keyFramesSeconds) + byteOffsetFromOldAddress);
    }
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_collectDefragmentPointers(_korl_resource_scene3d_collectDefragmentPointers)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
    if(korlResourceAllocatorIsTransient)
    {
        if(scene3d->gltf)
            mcarrpush(stbDaMemoryContext, *pStbDaDefragmentPointers
                     ,(KORL_STRUCT_INITIALIZE(Korl_Heap_DefragmentPointer){KORL_C_CAST(void**, &(scene3d->gltf))
                     ,0/*userAddressByteOffset*/, parent, _korl_resource_scene3d_onTransientMemoryMovedCallback, NULL}));
    }
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_scene3d_descriptorStructCreate)
{
    return korl_allocate(allocatorRuntime, sizeof(_Korl_Resource_Scene3d));
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_scene3d_descriptorStructDestroy)
{
    /* all our dynamic data is stored in the transient allocator */
    korl_free(allocatorRuntime, resourceDescriptorStruct);
}
korl_internal Korl_Gfx_Material _korl_resource_scene3d_getMaterial(_Korl_Resource_Scene3d*const scene3d, u32 materialIndex, const Korl_Codec_Gltf_Mesh_Primitive*const meshPrimitive)
{
    korl_assert(materialIndex < scene3d->gltf->materials.size);
    const Korl_Codec_Gltf_Material*const gltfMaterial      = korl_codec_gltf_getMaterials(scene3d->gltf) + materialIndex;
    _Korl_Resource_Scene3d_Context*const descriptorContext = korl_resource_getDescriptorContextStruct(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SCENE3D));
    Korl_Gfx_Material result = korl_gfx_material_defaultUnlit();
    switch(gltfMaterial->alphaMode)
    {
    case KORL_CODEC_GLTF_MATERIAL_ALPHA_MODE_OPAQUE: 
    case KORL_CODEC_GLTF_MATERIAL_ALPHA_MODE_MASK  : result.modes.flags |= KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE; break;
    case KORL_CODEC_GLTF_MATERIAL_ALPHA_MODE_BLEND : result.modes.flags |= KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_BLEND; break;
    }
    result.modes.cullMode = gltfMaterial->doubleSided ? KORL_GFX_MATERIAL_CULL_MODE_NONE : KORL_GFX_MATERIAL_CULL_MODE_BACK;
    if(gltfMaterial->extras.rawUtf8KorlShaderVertex.size)
        result.shaders.resourceHandleShaderVertex = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER)
                                                                          ,korl_codec_gltf_getUtf8(scene3d->gltf, gltfMaterial->extras.rawUtf8KorlShaderVertex), KORL_ASSETCACHE_GET_FLAG_LAZY);
    else if(!gltfMaterial->extras.korlIsUnlit)
        if(meshPrimitive && meshPrimitive->attributes[KORL_CODEC_GLTF_MESH_PRIMITIVE_ATTRIBUTE_JOINTS_0] >= 0)
            result.shaders.resourceHandleShaderVertex = descriptorContext->defaultResources[KORL_RESOURCE_SCENE3D_DEFAULTRESOURCE_SHADER_VERTEX_LIT_SKINNED];
        else
            result.shaders.resourceHandleShaderVertex = descriptorContext->defaultResources[KORL_RESOURCE_SCENE3D_DEFAULTRESOURCE_SHADER_VERTEX_LIT];
    if(gltfMaterial->extras.rawUtf8KorlShaderFragment.size)
        result.shaders.resourceHandleShaderFragment = korl_resource_fromFile(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SHADER)
                                                                            ,korl_codec_gltf_getUtf8(scene3d->gltf, gltfMaterial->extras.rawUtf8KorlShaderFragment), KORL_ASSETCACHE_GET_FLAG_LAZY);
    else if(!gltfMaterial->extras.korlIsUnlit)
        result.shaders.resourceHandleShaderFragment = descriptorContext->defaultResources[KORL_RESOURCE_SCENE3D_DEFAULTRESOURCE_SHADER_FRAGMENT_LIT];
    if(gltfMaterial->pbrMetallicRoughness.baseColorTextureIndex >= 0)
        result.maps.resourceHandleTextureBase = scene3d->textures[gltfMaterial->pbrMetallicRoughness.baseColorTextureIndex];
    else
        result.maps.resourceHandleTextureBase = descriptorContext->defaultResources[KORL_RESOURCE_SCENE3D_DEFAULTRESOURCE_MATERIAL_MAP_BASE];
    if(gltfMaterial->KHR_materials_specular.specularColorTextureIndex >= 0)
        result.maps.resourceHandleTextureSpecular = scene3d->textures[gltfMaterial->KHR_materials_specular.specularColorTextureIndex];
    else
        result.maps.resourceHandleTextureSpecular = descriptorContext->defaultResources[KORL_RESOURCE_SCENE3D_DEFAULTRESOURCE_MATERIAL_MAP_SPECULAR];
    result.fragmentShaderUniform.factorColorSpecular = gltfMaterial->KHR_materials_specular.factors;
    result.fragmentShaderUniform.shininess           = 32;// not sure if we can actually store this in gltf; see: https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_specular/README.md
    return result;
}
korl_internal const void* _korl_resource_scene3d_transcode_getViewedBuffer(_Korl_Resource_Scene3d*const scene3d, const void* glbFileData, i32 bufferViewIndex)
{
    korl_assert(bufferViewIndex >= 0);
    const Korl_Codec_Gltf_BufferView*const gltfBufferViews = korl_codec_gltf_getBufferViews(scene3d->gltf);
    const Korl_Codec_Gltf_Buffer*const     gltfBuffers     = korl_codec_gltf_getBuffers(scene3d->gltf);
    const Korl_Codec_Gltf_BufferView*const bufferView      = gltfBufferViews + bufferViewIndex;
    const Korl_Codec_Gltf_Buffer*const     buffer          = gltfBuffers     + bufferView->buffer;
    if(buffer->stringUri.size)
        korl_log(ERROR, "external & embedded string buffers not supported");
    korl_assert(buffer->glbByteOffset >= 0);// require that the raw data is embedded in the GLB file itself as-is
    const void*const bufferData       = KORL_C_CAST(u8*, glbFileData) + buffer->glbByteOffset;
    const void*const viewedBufferData = KORL_C_CAST(u8*, bufferData ) + bufferView->byteOffset;
    return viewedBufferData;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_scene3d_transcode)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
    korl_assert(!scene3d->gltf);
    scene3d->gltf = korl_codec_glb_decode(data, dataBytes, allocatorTransient);
    /* calculate how much total transient memory we require, then do a single 
        reallocation on `scene3d->gltf` here to satisfy this requirement; we 
        will then gradually assign pointers to offsets within this memory as we 
        progress through this function; we do it this way (instead of 
        reallocating scene3d->gltf each time we need new memory) to prevent the 
        need to re-validate all previously-obtained pointers, which seems like 
        it would be _way_ more work */
    scene3d->meshPrimitivesSize = 0;
    for(u32 m = 0; m < scene3d->gltf->meshes.size; m++)
        scene3d->meshPrimitivesSize += korl_checkCast_u$_to_u16(korl_codec_gltf_getMeshes(scene3d->gltf)[m].primitives.size);
    u$ skinBytesRequired = 0;
    for(u32 s = 0; s < scene3d->gltf->skins.size; s++)
    {
        const Korl_Codec_Gltf_Skin*const  gltfSkin = korl_codec_gltf_getSkins(scene3d->gltf) + s;
        _Korl_Resource_Scene3d_Skin*const skin     = NULL;
        skinBytesRequired += gltfSkin->joints.size * (  sizeof(*skin->boneInverseBindMatrices)
                                                      + sizeof(*skin->boneTopologicalOrder)
                                                      + sizeof(*skin->boneParentIndices))
                           + scene3d->gltf->nodes.size * sizeof(*skin->nodeIndex_to_boneIndex);
    }
    u$ animationBytesRequired = 0;
    for(u32 a = 0; a < scene3d->gltf->animations.size; a++)
    {
        const Korl_Codec_Gltf_Animation*const         gltfAnimation    = korl_codec_gltf_getAnimations(scene3d->gltf) + a;
        const Korl_Codec_Gltf_Animation_Sampler*const gltfAnimSamplers = korl_codec_gltf_getAnimationSamplers(scene3d->gltf, gltfAnimation);
        _Korl_Resource_Scene3d_Animation*const        animation        = NULL;
        animationBytesRequired += gltfAnimation->samplers.size * sizeof(*animation->sampleSets);
        for(u32 s = 0; s < gltfAnimation->samplers.size; s++)
        {
            const Korl_Codec_Gltf_Animation_Sampler*const gltfAnimSampler = gltfAnimSamplers + s;
            bool previousSharedInputSampleSet = false;
            for(u32 s2 = 0; s2 < s && !previousSharedInputSampleSet; s2++)
            {
                const Korl_Codec_Gltf_Animation_Sampler*const gltfAnimSampler2 = gltfAnimSamplers + s2;
                if(gltfAnimSampler2->input == gltfAnimSampler->input)
                    previousSharedInputSampleSet = true;
            }
            if(!previousSharedInputSampleSet)
            {
                const Korl_Codec_Gltf_Accessor*const inputAccessor = korl_codec_gltf_getAccessors(scene3d->gltf) + gltfAnimSampler->input;
                animationBytesRequired += inputAccessor->count * sizeof(*animation->keyFramesSeconds);
            }
            const Korl_Codec_Gltf_Accessor*const outputAccessor               = korl_codec_gltf_getAccessors(scene3d->gltf) + gltfAnimSampler->output;
            const u8                             outputAccessorComponentCount = korl_codec_gltf_accessor_getComponentCount(outputAccessor);
            animationBytesRequired += outputAccessor->count * outputAccessorComponentCount * sizeof(*animation->samples);
        }
    }
    const u$ additionalBytesRequired = scene3d->gltf->textures.size   * sizeof(*scene3d->textures)
                                     + scene3d->gltf->meshes.size     * sizeof(*scene3d->meshes)
                                     + scene3d->meshPrimitivesSize    * sizeof(*scene3d->meshPrimitives)
                                     + scene3d->gltf->skins.size      * sizeof(*scene3d->skins)
                                     + skinBytesRequired
                                     + scene3d->gltf->animations.size * sizeof(*scene3d->animations)
                                     + animationBytesRequired;
    /* perform a single reallocation on scene3d->gltf to make enough room to store all our transcoded data in a single allocation */
    scene3d->gltf = korl_reallocate(allocatorTransient, scene3d->gltf, scene3d->gltf->bytes + additionalBytesRequired);
    u8* nextTranscodedDataByte = KORL_C_CAST(u8*, scene3d->gltf) + scene3d->gltf->bytes;// we'll use this moving address to linearly allocate space for transcoded data
    /* at this point, our transcoded allocation data buffer is locked into memory, 
        so we can start calculating const address offsets & know with confidence 
        that those memory locations will _not_ be invalidated during execution */
    const Korl_Codec_Gltf_BufferView*const gltfBufferViews = korl_codec_gltf_getBufferViews(scene3d->gltf);
    const Korl_Codec_Gltf_Buffer*const     gltfBuffers     = korl_codec_gltf_getBuffers(scene3d->gltf);
    /* transcode materials */
    const Korl_Codec_Gltf_Texture*const    gltfTextures = korl_codec_gltf_getTextures(scene3d->gltf);
    const Korl_Codec_Gltf_Image*const      gltfImages   = korl_codec_gltf_getImages(scene3d->gltf);
    const Korl_Codec_Gltf_Sampler*const    gltfSamplers = korl_codec_gltf_getSamplers(scene3d->gltf);
    scene3d->textures = KORL_C_CAST(void*, nextTranscodedDataByte);
    nextTranscodedDataByte += scene3d->gltf->textures.size * sizeof(*scene3d->textures);
    for(u32 t = 0; t < scene3d->gltf->textures.size; t++)
    {
        const Korl_Codec_Gltf_Sampler*const sampler = gltfTextures[t].sampler < 0 ? &KORL_CODEC_GLTF_SAMPLER_DEFAULT : gltfSamplers + gltfTextures[t].sampler;
        if(gltfTextures[t].source < 0)
            korl_log(ERROR, "undefined texture source not supported");// perhaps in the future we can just assign a "default" source image of some kind
        const Korl_Codec_Gltf_Image*const image = gltfImages + gltfTextures[t].source;
        if(image->bufferView < 0)
            korl_log(ERROR, "external image sources (URIs) not supported");// if this ever gets implemented, it will probably only be used to get resource handles of image file assets
        const void*const viewedBufferData = _korl_resource_scene3d_transcode_getViewedBuffer(scene3d, data, image->bufferView);
        const Korl_Codec_Gltf_BufferView*const bufferView = gltfBufferViews + image->bufferView;
        /* create a new runtime texture resource */
        KORL_ZERO_STACK(Korl_Resource_Texture_CreateInfo, createInfo);
        createInfo.imageFileMemoryBuffer.data = viewedBufferData;
        createInfo.imageFileMemoryBuffer.size = bufferView->byteLength;
        if(scene3d->textures[t])
            /* destroy the old texture resource */
            // NOTE: this works for _now_, but if we want the ability to pass this resource around & store it elsewhere, we might need the ability to as korl-resource "re-create" a resource; perhaps we can just modify korl_resource_create to take a Resource_Handle param, & perform alternate logic based on this value (if 0, just make a new resource, otherwise we get the previous resource from the pool & unload it)
            korl_resource_destroy(scene3d->textures[t]);
        scene3d->textures[t] = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE), &createInfo, true);
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
    {/* create a new transient vertex buffer to store all the mesh vertex data */
        korl_assert(!scene3d->vertexBuffer);
        KORL_ZERO_STACK(Korl_Resource_GfxBuffer_CreateInfo, createInfoVertexBuffer);
        createInfoVertexBuffer.bytes = vertexBufferBytes;// some arbitrary base size; we expect this to be resized when this resource gets transcoded
        createInfoVertexBuffer.usageFlags = KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_INDEX 
                                          | KORL_RESOURCE_GFX_BUFFER_USAGE_FLAG_VERTEX;
        scene3d->vertexBuffer = korl_resource_create(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_GFX_BUFFER), &createInfoVertexBuffer, true);
    }
    const Korl_Codec_Gltf_Mesh*const gltfMeshes = korl_codec_gltf_getMeshes(scene3d->gltf);
    const Korl_Codec_Gltf_Node*const gltfNodes  = korl_codec_gltf_getNodes(scene3d->gltf);
    scene3d->meshes = KORL_C_CAST(void*, nextTranscodedDataByte);
    nextTranscodedDataByte += scene3d->gltf->meshes.size * sizeof(*scene3d->meshes);
    u16 currentMeshPrimitivesIndex = 0;
    for(u32 m = 0; m < scene3d->gltf->meshes.size; m++)
    {
        scene3d->meshes[m].meshPrimitivesOffset = currentMeshPrimitivesIndex;
        currentMeshPrimitivesIndex += korl_checkCast_u$_to_u16(gltfMeshes[m].primitives.size);
    }
    korl_assert(currentMeshPrimitivesIndex == scene3d->meshPrimitivesSize);// sanity check that we got the same # as above
    scene3d->meshPrimitives = KORL_C_CAST(void*, nextTranscodedDataByte);
    nextTranscodedDataByte += scene3d->meshPrimitivesSize * sizeof(*scene3d->meshPrimitives);
    for(u32 m = 0; m < scene3d->gltf->meshes.size; m++)
    {
        const Korl_Codec_Gltf_Mesh*const           mesh               = gltfMeshes + m;
        const Korl_Codec_Gltf_Mesh_Primitive*const gltfMeshPrimitives = korl_codec_gltf_mesh_getPrimitives(scene3d->gltf, mesh);
        Korl_Resource_Scene3d_MeshPrimitive*const  meshPrimitives     = scene3d->meshPrimitives + scene3d->meshes[m].meshPrimitivesOffset;
        /* extract vertex staging meta & store all vertex data into a runtime buffer resource */
        for(u32 mp = 0; mp < mesh->primitives.size; mp++)
        {
            const Korl_Codec_Gltf_Mesh_Primitive*const meshPrimitive      = gltfMeshPrimitives + mp;
            Korl_Resource_Scene3d_MeshPrimitive*const  sceneMeshPrimitive = meshPrimitives     + mp;
            sceneMeshPrimitive->vertexBufferByteOffset = vertexBufferBytesUsed;// vertex attributes have buffer byte offset limitations (see maxVertexInputAttributeOffset of Vulkan spec), so we should offset all bindings by some starting amount to minimize this distance
            sceneMeshPrimitive->vertexBuffer           = scene3d->vertexBuffer;// for now, all MeshPrimitives share the same giant vertex buffer
            switch(meshPrimitive->mode)
            {
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_POINTS        : korl_log(ERROR, "unsupported MeshPrimitive mode: %i", meshPrimitive->mode);          break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_LINES         : sceneMeshPrimitive->primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINES;          break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_LINE_LOOP     : korl_log(ERROR, "unsupported MeshPrimitive mode: %i", meshPrimitive->mode);          break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_LINE_STRIP    : sceneMeshPrimitive->primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINE_STRIP;     break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLES     : sceneMeshPrimitive->primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES;      break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLE_STRIP: sceneMeshPrimitive->primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_STRIP; break;
            case KORL_CODEC_GLTF_MESH_PRIMITIVE_MODE_TRIANGLE_FAN  : sceneMeshPrimitive->primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_FAN;   break;
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
                sceneMeshPrimitive->vertexStagingMeta.indexCount            = accessor->count;
                sceneMeshPrimitive->vertexStagingMeta.indexByteOffsetBuffer = korl_checkCast_u$_to_u32(vertexBufferBytesUsed - sceneMeshPrimitive->vertexBufferByteOffset);
                switch(accessor->componentType)
                {
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U16: sceneMeshPrimitive->vertexStagingMeta.indexType = KORL_GFX_VERTEX_INDEX_TYPE_U16; break;
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U32: sceneMeshPrimitive->vertexStagingMeta.indexType = KORL_GFX_VERTEX_INDEX_TYPE_U32; break;
                default: korl_log(ERROR, "invalid vertex index componentType: %i", accessor->componentType); break;
                }
                /* calculate bytes required for memory alignment */
                //KORL-ISSUE-000-000-199: scene3d: HACK; hard-coded memory alignment of vertex attribute data; it would be better if there was an API to query the platform for alignment required for each vertex attribute; related to KORL-ISSUE-000-000-178
                const u32 alignedBytes = korl_checkCast_u$_to_u32(korl_math_nextHighestDivision(bufferView->byteLength, 4)) * 4;
                /* copy the viewedBufferData to our scene3d's vertex buffer */
                if(vertexBufferBytesUsed + alignedBytes > vertexBufferBytes)
                    korl_resource_resize(scene3d->vertexBuffer, KORL_MATH_MAX(vertexBufferBytesUsed + alignedBytes, 2 * vertexBufferBytes));
                korl_resource_update(scene3d->vertexBuffer, viewedBufferData, bufferView->byteLength, vertexBufferBytesUsed);
                vertexBufferBytesUsed += alignedBytes;
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
                if(sceneMeshPrimitive->vertexStagingMeta.vertexCount)
                    korl_assert(accessor->count == sceneMeshPrimitive->vertexStagingMeta.vertexCount);
                else
                    sceneMeshPrimitive->vertexStagingMeta.vertexCount = accessor->count;
                const Korl_Gfx_VertexAttributeBinding binding = GLTF_ATTRIBUTE_BINDINGS[a];
                sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].byteOffsetBuffer = korl_checkCast_u$_to_u32(vertexBufferBytesUsed - sceneMeshPrimitive->vertexBufferByteOffset);
                sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].byteStride       = korl_codec_gltf_accessor_getStride(accessor, gltfBufferViews);
                sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
                switch(accessor->componentType)
                {
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U8 : sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].elementType = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;  break;
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U32: sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].elementType = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32; break;
                case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_F32: sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].elementType = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32; break;
                default:
                    korl_log(ERROR, "unsupported componentType: %u", accessor->componentType);
                }
                switch(accessor->type)
                {
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_SCALAR: sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize =     1; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC2  : sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize =     2; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC3  : sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize =     3; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC4  : sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize =     4; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT2  : sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize = 2 * 2; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT3  : sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize = 3 * 3; break;
                case KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT4  : sceneMeshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[binding].vectorSize = 4 * 4; break;
                }
                /* calculate bytes required for memory alignment */
                //KORL-ISSUE-000-000-199: scene3d: HACK; hard-coded memory alignment of vertex attribute data; it would be better if there was an API to query the platform for alignment required for each vertex attribute; related to KORL-ISSUE-000-000-178
                const u32 alignedBytes = korl_checkCast_u$_to_u32(korl_math_nextHighestDivision(bufferView->byteLength, 4)) * 4;
                /* copy the viewedBufferData to our scene3d's vertex buffer */
                if(vertexBufferBytesUsed + alignedBytes > vertexBufferBytes)
                    korl_resource_resize(scene3d->vertexBuffer, KORL_MATH_MAX(vertexBufferBytesUsed + alignedBytes, 2 * vertexBufferBytes));
                korl_resource_update(scene3d->vertexBuffer, viewedBufferData, bufferView->byteLength, vertexBufferBytesUsed);
                vertexBufferBytesUsed += alignedBytes;
            }
            if(meshPrimitive->material < 0)
                sceneMeshPrimitive->material = korl_gfx_material_defaultUnlit();
            else
                sceneMeshPrimitive->material = _korl_resource_scene3d_getMaterial(scene3d, meshPrimitive->material, meshPrimitive);
        }
        /* enumerate Nodes to identify whether or not there is a skin compatible with this mesh */
        scene3d->meshes[m].skinIndex = -1;
        for(u32 n = 0; n < scene3d->gltf->nodes.size; n++)
        {
            const Korl_Codec_Gltf_Node*const node = gltfNodes + n;
            if(node->mesh == korl_checkCast_u$_to_i32(m) && node->skin >= 0)
            {
                scene3d->meshes[m].skinIndex = node->skin;
                break;// for now, only one skin can be associated with a mesh
            }
        }
    }
    /* transcode skins */
    const Korl_Codec_Gltf_Skin*const gltfSkins = korl_codec_gltf_getSkins(scene3d->gltf);
    scene3d->skins = KORL_C_CAST(void*, nextTranscodedDataByte);
    nextTranscodedDataByte += scene3d->gltf->skins.size * sizeof(*scene3d->skins);
    for(u32 s = 0; s < scene3d->gltf->skins.size; s++)
    {
        const Korl_Codec_Gltf_Skin*const  gltfSkin = gltfSkins      + s;
        _Korl_Resource_Scene3d_Skin*const skin     = scene3d->skins + s;
        skin->boneInverseBindMatrices = KORL_C_CAST(void*, nextTranscodedDataByte); nextTranscodedDataByte +=     gltfSkin->joints.size * sizeof(*skin->boneInverseBindMatrices);
        skin->boneTopologicalOrder    = KORL_C_CAST(void*, nextTranscodedDataByte); nextTranscodedDataByte +=     gltfSkin->joints.size * sizeof(*skin->boneTopologicalOrder);
        skin->boneParentIndices       = KORL_C_CAST(void*, nextTranscodedDataByte); nextTranscodedDataByte +=     gltfSkin->joints.size * sizeof(*skin->boneParentIndices);
        skin->nodeIndex_to_boneIndex  = KORL_C_CAST(void*, nextTranscodedDataByte); nextTranscodedDataByte += scene3d->gltf->nodes.size * sizeof(*skin->nodeIndex_to_boneIndex);
        /* transcode nodeIndex=>boneIndex lookup table */
        const u32*const skinJointIndices = korl_codec_gltf_skin_getJointIndices(scene3d->gltf, gltfSkin);
        for(u32 n = 0; n < scene3d->gltf->nodes.size; n++)
        {
            skin->nodeIndex_to_boneIndex[n] = -1;
            for(u32 j = 0; j < gltfSkin->joints.size && skin->nodeIndex_to_boneIndex[n] == -1; j++)
                if(skinJointIndices[j] == n)
                    skin->nodeIndex_to_boneIndex[n] = j;
        }
        /* transcode inverse bind matrices */
        if(gltfSkin->inverseBindMatrices >= 0)
        {
            const Korl_Codec_Gltf_Accessor*const accessor = gltfAccessors + gltfSkin->inverseBindMatrices;
            korl_assert(   accessor->type          == KORL_CODEC_GLTF_ACCESSOR_TYPE_MAT4 
                        && accessor->componentType == KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_F32);// gltf spec. 3.7.3.1
            const Korl_Math_M4f32*const            viewedBufferData = _korl_resource_scene3d_transcode_getViewedBuffer(scene3d, data, accessor->bufferView);
            const Korl_Codec_Gltf_BufferView*const bufferView       = gltfBufferViews + accessor->bufferView;
            /* gltf matrices are column-major in memory, so we need to transpose them all for korl-math */
            for(u32 m = 0; m < accessor->count; m++)
                skin->boneInverseBindMatrices[m] = korl_math_m4f32_transpose(viewedBufferData + m);
        }
        else
            korl_log(ERROR, "implicit inverseBindMatrices not implemented");
        /* transcode topological bone order & the parent index array */
        KORL_ZERO_STACK(Korl_Algorithm_GraphDirected_CreateInfo, graphCreateInfo);
        graphCreateInfo.allocator = allocatorTransient;
        graphCreateInfo.nodesSize = gltfSkin->joints.size;
        Korl_Algorithm_GraphDirected graphDirected = korl_algorithm_graphDirected_create(&graphCreateInfo);
        for(u32 j = 0; j < gltfSkin->joints.size; j++)
            skin->boneParentIndices[j] = -1;// initialize all bones to have no parent
        for(u32 j = 0; j < gltfSkin->joints.size; j++)
        {
            const Korl_Codec_Gltf_Node*const jointNode = korl_codec_gltf_skin_getJointNode(scene3d->gltf, gltfSkin, j);
            for(u32 jc = 0; jc < jointNode->children.size; jc++)
            {
                const Korl_Codec_Gltf_Node*const jointNodeChild  = korl_codec_gltf_node_getChild(scene3d->gltf, jointNode, jc);
                const u32                        childNodeIndex  = korl_checkCast_i$_to_u32(jointNodeChild - gltfNodes);
                if(skin->nodeIndex_to_boneIndex[childNodeIndex] < 0)
                    continue;// if the child of this Node is not in the skin, we can't possibly add it to the graph
                const u32 childJointIndex = korl_checkCast_i$_to_u32(skin->nodeIndex_to_boneIndex[childNodeIndex]);
                korl_algorithm_graphDirected_addEdge(&graphDirected, j, childJointIndex);
                skin->boneParentIndices[childJointIndex] = j;
            }
        }
        if(!korl_algorithm_graphDirected_sortTopological(&graphDirected, skin->boneTopologicalOrder))
            korl_log(ERROR, "skin topological sort failed; invalid bone DAG (graph has cycles?)");
        korl_algorithm_graphDirected_destroy(&graphDirected);
    }
    /* transcode animations; note that, as with skins, most of the data we 
        actually need at runtime is stored inside scene3d->gltf; the important 
        data we need to extract is the buffered Sampler data 
        (inputs/keyframeTimes & outputs/keyframeSamples) */
    const Korl_Codec_Gltf_Animation*const gltfAnimations = korl_codec_gltf_getAnimations(scene3d->gltf);
    scene3d->animations = KORL_C_CAST(void*, nextTranscodedDataByte);
    nextTranscodedDataByte += scene3d->gltf->animations.size * sizeof(*scene3d->animations);
    for(u32 a = 0; a < scene3d->gltf->animations.size; a++)
    {
        const Korl_Codec_Gltf_Animation*const         gltfAnimation    = gltfAnimations + a;
        const Korl_Codec_Gltf_Animation_Sampler*const gltfAnimSamplers = korl_codec_gltf_getAnimationSamplers(scene3d->gltf, gltfAnimation);
        _Korl_Resource_Scene3d_Animation*const        animation        = scene3d->animations + a;
        /* compute how many bytes we need for this animation's dynamic memory components */
        u$ bytesRequiredSampleInputs  = 0;
        u$ bytesRequiredSampleOutputs = 0;
        for(u32 s = 0; s < gltfAnimation->samplers.size; s++)
        {
            const Korl_Codec_Gltf_Animation_Sampler*const gltfAnimSampler = gltfAnimSamplers + s;
            bool previousSharedInputSampleSet = false;
            for(u32 s2 = 0; s2 < s && !previousSharedInputSampleSet; s2++)
            {
                const Korl_Codec_Gltf_Animation_Sampler*const gltfAnimSampler2 = gltfAnimSamplers + s2;
                if(gltfAnimSampler2->input == gltfAnimSampler->input)
                    previousSharedInputSampleSet = true;
            }
            if(!previousSharedInputSampleSet)
            {
                const Korl_Codec_Gltf_Accessor*const inputAccessor = korl_codec_gltf_getAccessors(scene3d->gltf) + gltfAnimSampler->input;
                bytesRequiredSampleInputs += inputAccessor->count * sizeof(*animation->keyFramesSeconds);
            }
            const Korl_Codec_Gltf_Accessor*const outputAccessor               = korl_codec_gltf_getAccessors(scene3d->gltf) + gltfAnimSampler->output;
            const u8                             outputAccessorComponentCount = korl_codec_gltf_accessor_getComponentCount(outputAccessor);
            bytesRequiredSampleOutputs += outputAccessor->count * outputAccessorComponentCount * sizeof(*animation->samples);
        }
        /* allocate this animation's dynamic memory */
        animation->sampleSets       = KORL_C_CAST(void*, nextTranscodedDataByte);
        nextTranscodedDataByte     += gltfAnimation->samplers.size * sizeof(*animation->sampleSets);
        animation->keyFramesSeconds = KORL_C_CAST(void*, nextTranscodedDataByte);
        nextTranscodedDataByte     += bytesRequiredSampleInputs;
        animation->samples          = KORL_C_CAST(void*, nextTranscodedDataByte);
        nextTranscodedDataByte     += bytesRequiredSampleOutputs;
        /* transcode key frame I/O data */
        animation->keyFrameSecondsStart = KORL_F32_MAX;
        animation->keyFrameSecondsEnd   = 0;
        u32 currentKeyFrameSecond = 0;
        u32 currentSample         = 0;
        for(u32 s = 0; s < gltfAnimation->samplers.size; s++)
        {
            const Korl_Codec_Gltf_Animation_Sampler*const    gltfAnimSampler = gltfAnimSamplers      + s;
            _Korl_Resource_Scene3d_Animation_SampleSet*const sampleSet       = animation->sampleSets + s;
            /* transcode key frame seconds */
            /* check if any previous sampler's use the same sampler inputs; 
                if so, we can just use the same start index */
            const _Korl_Resource_Scene3d_Animation_SampleSet* previousSharedInputSampleSet = NULL;
            for(u32 s2 = 0; s2 < s && !previousSharedInputSampleSet; s2++)
            {
                const Korl_Codec_Gltf_Animation_Sampler*const gltfAnimSampler2 = gltfAnimSamplers + s2;
                if(   gltfAnimSampler2->input == gltfAnimSampler->input
                   && !previousSharedInputSampleSet)
                    previousSharedInputSampleSet = animation->sampleSets + s2;
            }
            if(previousSharedInputSampleSet)
                sampleSet->keyFramesSecondsStartIndex = previousSharedInputSampleSet->keyFramesSecondsStartIndex;
            else
            {
                const Korl_Codec_Gltf_Accessor*const inputAccessor = gltfAccessors + gltfAnimSampler->input;
                sampleSet->keyFramesSecondsStartIndex = currentKeyFrameSecond;
                currentKeyFrameSecond                += inputAccessor->count;
                const void*const viewedBuffer         = _korl_resource_scene3d_transcode_getViewedBuffer(scene3d, data, inputAccessor->bufferView);
                korl_memory_copy(animation->keyFramesSeconds + sampleSet->keyFramesSecondsStartIndex, viewedBuffer, inputAccessor->count * sizeof(*animation->keyFramesSeconds));
                /* input Accessors for Animation_Sampler inputs _must_ have a defined "min" & "max"; 
                    we can use this data to calculate the animation's start/end times */
                korl_assert(inputAccessor->min.size == 1);
                korl_assert(inputAccessor->max.size == 1);
                const f32*const inputAccesssorMin = korl_codec_gltf_accessor_getMin(scene3d->gltf, inputAccessor);
                const f32*const inputAccesssorMax = korl_codec_gltf_accessor_getMax(scene3d->gltf, inputAccessor);
                KORL_MATH_ASSIGN_CLAMP_MAX(animation->keyFrameSecondsStart, inputAccesssorMin[0]);
                KORL_MATH_ASSIGN_CLAMP_MIN(animation->keyFrameSecondsEnd  , inputAccesssorMax[0]);
                korl_assert(animation->keyFrameSecondsStart <= animation->keyFrameSecondsEnd);
            }
            /* transcode key frame samples */
            const Korl_Codec_Gltf_Accessor*const   outputAccessor               = gltfAccessors + gltfAnimSampler->output;
            const u8                               outputAccessorComponentCount = korl_codec_gltf_accessor_getComponentCount(outputAccessor);
            const void*const                       viewedBuffer                 = _korl_resource_scene3d_transcode_getViewedBuffer(scene3d, data, outputAccessor->bufferView);
            const Korl_Codec_Gltf_BufferView*const outputBufferView             = gltfBufferViews + outputAccessor->bufferView;
            korl_assert(outputBufferView->byteStride == 0);// gltf 2.0 spec. 3.6.2.1.: Accessors not used for vertex attributes _must_ be tightly packed
            sampleSet->sampleStartIndex  = currentSample;
            currentSample               += outputAccessor->count * outputAccessorComponentCount;
            if(outputAccessor->componentType == KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_F32)
                /* if the sample outputs are f32s, we can just do a simple memcpy for all sample outputs */
                korl_memory_copy(animation->samples + sampleSet->sampleStartIndex, viewedBuffer, outputAccessor->count * outputAccessorComponentCount * sizeof(f32));
            else
            {/* otherwise, we have to iterate over each sample output f32 component & perform a integer=>float conversion (as described by gltf 2.0 spec. 3.11) */
                for(u32 c = 0; c < outputAccessor->count * outputAccessorComponentCount; c++)
                {
                    switch(outputAccessor->type)
                    {
                    case KORL_CODEC_GLTF_ACCESSOR_TYPE_SCALAR:
                    case KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC4:{
                        switch(outputAccessor->componentType)
                        {/* int=>float conversion formulas from gltf 2.0 spec 3.11 */
                        case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_I8 :
                            animation->samples[sampleSet->sampleStartIndex + c] = KORL_MATH_MAX(  KORL_C_CAST(f32, KORL_C_CAST(const i8*, viewedBuffer)[c]) 
                                                                                                / (KORL_C_CAST(f32, KORL_U8_MAX) / 2.f)
                                                                                               ,-1.f);
                            break;
                        case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U8 :
                            animation->samples[sampleSet->sampleStartIndex + c] = KORL_C_CAST(const u8*, viewedBuffer)[c] / KORL_C_CAST(f32, KORL_U8_MAX);
                            break;
                        case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_I16:
                            animation->samples[sampleSet->sampleStartIndex + c] = KORL_MATH_MAX(  KORL_C_CAST(f32, KORL_C_CAST(const i16*, viewedBuffer)[c]) 
                                                                                                / (KORL_C_CAST(f32, KORL_U16_MAX) / 2.f)
                                                                                               ,-1.f);
                            break;
                        case KORL_CODEC_GLTF_ACCESSOR_COMPONENT_TYPE_U16:
                            animation->samples[sampleSet->sampleStartIndex + c] = KORL_C_CAST(const u16*, viewedBuffer)[c] / KORL_C_CAST(f32, KORL_U16_MAX);
                            break;
                        default: korl_log(ERROR, "invalid Accessor componentType: %i", outputAccessor->componentType);
                        }
                        break;}
                    default: korl_log(ERROR, "invalid Accessor type: %i", outputAccessor->type);
                    }
                }
            }
        }
    }
    /* final sanity check: ensure that we distributed all the transient memory we bulk-allocated for transcoded data */
    korl_assert(nextTranscodedDataByte == KORL_C_CAST(u8*, scene3d->gltf) + scene3d->gltf->bytes + additionalBytesRequired);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(_korl_resource_scene3d_clearTransientData)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
    /* no need to explicitly free/destroy transient data, since this function is 
        only called when transient data has been wiped anyway; we just need to 
        reset the state of the descriptor struct */
    korl_memory_zero(scene3d, sizeof(*scene3d));
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_scene3d_unload)
{
    _Korl_Resource_Scene3d*const scene3d = resourceDescriptorStruct;
    korl_resource_destroy(scene3d->vertexBuffer);
    for(u32 t = 0; t < scene3d->gltf->textures.size; t++)
        korl_resource_destroy(scene3d->textures[t]);
    korl_free(allocatorTransient, scene3d->gltf);
    korl_memory_zero(scene3d, sizeof(*scene3d));
}
korl_internal void korl_resource_scene3d_register(void)
{
    KORL_ZERO_STACK(_Korl_Resource_Scene3d_Context, context);
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName                  = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SCENE3D);
    descriptorManifest.descriptorContext                   = &context;
    descriptorManifest.descriptorContextBytes              = sizeof(context);
    descriptorManifest.callbacks.collectDefragmentPointers = korl_functionDynamo_register(_korl_resource_scene3d_collectDefragmentPointers);
    descriptorManifest.callbacks.descriptorStructCreate    = korl_functionDynamo_register(_korl_resource_scene3d_descriptorStructCreate);
    descriptorManifest.callbacks.descriptorStructDestroy   = korl_functionDynamo_register(_korl_resource_scene3d_descriptorStructDestroy);
    descriptorManifest.callbacks.unload                    = korl_functionDynamo_register(_korl_resource_scene3d_unload);
    descriptorManifest.callbacks.transcode                 = korl_functionDynamo_register(_korl_resource_scene3d_transcode);
    descriptorManifest.callbacks.clearTransientData        = korl_functionDynamo_register(_korl_resource_scene3d_clearTransientData);
    korl_resource_descriptor_register(&descriptorManifest);
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_setDefaultResource(korl_resource_scene3d_setDefaultResource)
{
    korl_assert(defaultResourceId < KORL_RESOURCE_SCENE3D_DEFAULTRESOURCE_ENUM_COUNT);
    _Korl_Resource_Scene3d_Context*const descriptorContext = korl_resource_getDescriptorContextStruct(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_SCENE3D));
    descriptorContext->defaultResources[defaultResourceId] = resourceHandle;
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
    return _korl_resource_scene3d_getMaterial(scene3d, materialIndex, NULL);
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_getMeshIndex(korl_resource_scene3d_getMeshIndex)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return KORL_U32_MAX;
    const Korl_Codec_Gltf_Mesh*const gltfMeshes = korl_codec_gltf_getMeshes(scene3d->gltf);
    for(u32 m = 0; m < scene3d->gltf->meshes.size; m++)
        if(korl_string_equalsAcu8(korl_codec_gltf_mesh_getName(scene3d->gltf, gltfMeshes + m), utf8MeshName))
            return m;
    korl_log(WARNING, "mesh name \"%.*hs\" not found", utf8MeshName.size, utf8MeshName.data);
    return KORL_U32_MAX;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_getMeshName(korl_resource_scene3d_getMeshName)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return KORL_RAW_CONST_UTF8("SCENE3D UNLOADED");
    korl_assert(meshIndex < scene3d->gltf->meshes.size);
    const Korl_Codec_Gltf_Mesh*const gltfMeshes = korl_codec_gltf_getMeshes(scene3d->gltf);
    return korl_codec_gltf_mesh_getName(scene3d->gltf, gltfMeshes + meshIndex);
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_getMeshPrimitiveCount(korl_resource_scene3d_getMeshPrimitiveCount)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return 0;
    korl_assert(meshIndex < scene3d->gltf->meshes.size);
    return korl_codec_gltf_getMeshes(scene3d->gltf)[meshIndex].primitives.size;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_getMeshPrimitive(korl_resource_scene3d_getMeshPrimitive)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return KORL_STRUCT_INITIALIZE_ZERO(Korl_Resource_Scene3d_MeshPrimitive);
    korl_assert(meshIndex < scene3d->gltf->meshes.size);
    korl_assert(primitiveIndex < korl_codec_gltf_getMeshes(scene3d->gltf)[meshIndex].primitives.size);
    const u32 sceneMeshPrimitiveIndex = scene3d->meshes[meshIndex].meshPrimitivesOffset + primitiveIndex;
    korl_assert(sceneMeshPrimitiveIndex < scene3d->meshPrimitivesSize);
    return scene3d->meshPrimitives[sceneMeshPrimitiveIndex];
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_getMeshTriangles(korl_resource_scene3d_getMeshTriangles)
{
    KORL_ZERO_STACK(Korl_Math_TriangleMesh, result);
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return result;
    korl_assert(meshIndex < scene3d->gltf->meshes.size);
    const Korl_Codec_Gltf_Mesh*const           gltfMesh           = korl_codec_gltf_getMeshes(scene3d->gltf) + meshIndex;
    const _Korl_Resource_Scene3d_Mesh*const    mesh               = scene3d->meshes + meshIndex;
    const Korl_Codec_Gltf_Mesh_Primitive*const gltfMeshPrimitives = korl_codec_gltf_mesh_getPrimitives(scene3d->gltf, gltfMesh);
    result = korl_math_triangleMesh_create(allocator);
    for(u32 mp = 0; mp < gltfMesh->primitives.size; mp++)
    {
        const Korl_Codec_Gltf_Mesh_Primitive*const      gltfMeshPrimitive = gltfMeshPrimitives + mp;
        const Korl_Resource_Scene3d_MeshPrimitive*const meshPrimitive     = scene3d->meshPrimitives + mesh->meshPrimitivesOffset + mp;
        korl_assert(meshPrimitive->primitiveType == KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES);// for now, we only support TRIANGLES
        /* obtain a pointer to the raw data contained within scene3d->vertexBuffer 
            so that we can transcode raw mesh vertex data */
        u$                vertexBufferBytes = 0;
        const void* const vertexBuffer      = korl_resource_getRawRuntimeData(scene3d->vertexBuffer, &vertexBufferBytes);
        /* determine what vertex data we have to transcode into the TriangleMesh */
        const void* rawIndices = NULL;
        u32*        indices    = NULL;
        if(meshPrimitive->vertexStagingMeta.indexType != KORL_GFX_VERTEX_INDEX_TYPE_INVALID)
        {
            /* if this MeshPrimitive has vertex indices, we should use them; 
                we have to transcode indices into a u32 array, since that is the 
                only type of vertex index that TriangleMesh API currently supports */
            rawIndices = KORL_C_CAST(const u8*, vertexBuffer) 
                       + meshPrimitive->vertexBufferByteOffset 
                       + meshPrimitive->vertexStagingMeta.indexByteOffsetBuffer;
            switch(meshPrimitive->vertexStagingMeta.indexType)
            {
            case KORL_GFX_VERTEX_INDEX_TYPE_INVALID: korl_log(ERROR, ""); break;// this error should never happen, since this condition was already checked above
            case KORL_GFX_VERTEX_INDEX_TYPE_U32    : indices = KORL_C_CAST(u32*, rawIndices); break;// if the indices are already u32, we can just use them :)
            case KORL_GFX_VERTEX_INDEX_TYPE_U16    : // transcode u16 indices into u32, using a temporary buffer
                indices = korl_allocateDirty(allocator, meshPrimitive->vertexStagingMeta.indexCount * sizeof(u32));
                for(u32 i = 0; i < meshPrimitive->vertexStagingMeta.indexCount; i++)
                    indices[i] = KORL_C_CAST(const u16*, rawIndices)[i];
                break;
            }
        }
        korl_assert(meshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID);// all MeshPrimitives _must_ have a position attribute
        korl_assert(meshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType == KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32);// for now, we only support f32 position components
        korl_assert(meshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize == 3);// for now, we only support 3D position data
        const Korl_Math_V3f32*const positions = KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(const u8*, vertexBuffer) 
                                                                              + meshPrimitive->vertexBufferByteOffset 
                                                                              + meshPrimitive->vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
        /* transcode the TriangleMesh data for this MeshPrimitive */
        if(indices)
            korl_math_triangleMesh_addIndexed(&result, positions, meshPrimitive->vertexStagingMeta.vertexCount, indices, meshPrimitive->vertexStagingMeta.indexCount);
        else
            korl_math_triangleMesh_add(&result, positions, meshPrimitive->vertexStagingMeta.vertexCount);
        /* cleanup */
        if(indices && indices != rawIndices)
            korl_free(allocator, indices);
    }
    return result;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_newSkin(korl_resource_scene3d_newSkin)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return NULL;
    korl_assert(meshIndex < scene3d->gltf->meshes.size);
    const _Korl_Resource_Scene3d_Mesh*const mesh = scene3d->meshes + meshIndex;
    korl_assert(mesh->skinIndex >= 0);
    const _Korl_Resource_Scene3d_Skin*const _skin    = scene3d->skins + mesh->skinIndex;
    const Korl_Codec_Gltf_Skin*const        gltfSkin = korl_codec_gltf_getSkins(scene3d->gltf) + mesh->skinIndex;
    Korl_Resource_Scene3d_Skin*const        skin     = korl_allocateDirty(allocator, sizeof(*skin) + gltfSkin->joints.size * sizeof(*skin->bones));
    korl_memory_zero(skin, sizeof(*skin));
    skin->skinIndex  = mesh->skinIndex;
    skin->allocator  = allocator;
    skin->bonesSize = gltfSkin->joints.size;
    /* before giving the user the Skin instance, we should initialize the bones */
    for(u32 b = 0; b < gltfSkin->joints.size; b++)
    {
        const Korl_Codec_Gltf_Node*const gltfBoneNode = korl_codec_gltf_skin_getJointNode(scene3d->gltf, gltfSkin, b);
        skin->bones[b]._position       = gltfBoneNode->translation;
        skin->bones[b]._versor         = gltfBoneNode->rotation;
        skin->bones[b]._scale          = gltfBoneNode->scale;
        skin->bones[b]._m4f32IsUpdated = false;
    }
    return skin;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_skin_getBoneParentIndices(korl_resource_scene3d_skin_getBoneParentIndices)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return NULL;
    korl_assert(skinIndex < scene3d->gltf->skins.size);
    const _Korl_Resource_Scene3d_Skin*const _skin = scene3d->skins + skinIndex;
    return _skin->boneParentIndices;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_skin_getBoneTopologicalOrder(korl_resource_scene3d_skin_getBoneTopologicalOrder)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return NULL;
    korl_assert(skinIndex < scene3d->gltf->skins.size);
    const _Korl_Resource_Scene3d_Skin*const _skin = scene3d->skins + skinIndex;
    return _skin->boneTopologicalOrder;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_skin_getBoneInverseBindMatrices(korl_resource_scene3d_skin_getBoneInverseBindMatrices)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return NULL;
    korl_assert(skinIndex < scene3d->gltf->skins.size);
    const _Korl_Resource_Scene3d_Skin*const _skin = scene3d->skins + skinIndex;
    return _skin->boneInverseBindMatrices;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_getAnimationIndex(korl_resource_scene3d_getAnimationIndex)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return KORL_U32_MAX;
    const Korl_Codec_Gltf_Animation*const gltfAnimations = korl_codec_gltf_getAnimations(scene3d->gltf);
    for(u32 a = 0; a < scene3d->gltf->animations.size; a++)
        if(korl_string_equalsAcu8(korl_codec_gltf_getUtf8(scene3d->gltf, gltfAnimations[a].rawUtf8Name), utf8AnimationName))
            return a;
    korl_log(WARNING, "animation name \"%.*hs\" not found", utf8AnimationName.size, utf8AnimationName.data);
    return KORL_U32_MAX;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_animation_seconds(korl_resource_scene3d_animation_seconds)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return 0;
    if(animationIndex >= scene3d->gltf->animations.size)
        return 0;
    const _Korl_Resource_Scene3d_Animation*const animation = scene3d->animations + animationIndex;
    return animation->keyFrameSecondsEnd - animation->keyFrameSecondsStart;
}
korl_internal KORL_FUNCTION_korl_resource_scene3d_skin_applyAnimation(korl_resource_scene3d_skin_applyAnimation)
{
    _Korl_Resource_Scene3d*const scene3d = korl_resource_getDescriptorStruct(handleResourceScene3d);
    if(!scene3d || !scene3d->gltf)
        return;
    if(animationIndex >= scene3d->gltf->animations.size)
        return;
    korl_assert(targetSkin->skinIndex < scene3d->gltf->skins.size);
    const _Korl_Resource_Scene3d_Skin*const       skin                  = scene3d->skins + targetSkin->skinIndex;
    const Korl_Codec_Gltf_Animation*const         gltfAnimation         = korl_codec_gltf_getAnimations(scene3d->gltf) + animationIndex;
    const _Korl_Resource_Scene3d_Animation*const  animation             = scene3d->animations                          + animationIndex;
    const Korl_Codec_Gltf_Animation_Channel*const gltfAnimationChannels = korl_codec_gltf_getAnimationChannels(scene3d->gltf, gltfAnimation);
    const Korl_Codec_Gltf_Animation_Sampler*const gltfAnimationSamplers = korl_codec_gltf_getAnimationSamplers(scene3d->gltf, gltfAnimation);
    const Korl_Codec_Gltf_Accessor*const          gltfAccessors         = korl_codec_gltf_getAccessors(scene3d->gltf);
    /* gltf 2.0 spec. 3.11: "Skinned animation is achieved by animating the joints in the skin’s joint hierarchy." 
        calculate the absolute animation time using the Animation's start/end times & secondsRelativeToAnimationStart; 
        iterate over each Animation_Channel; 
            use the lookup table we baked in _Korl_Resource_Scene3d_Skin to convert the target->node from nodeIndex=>boneIndex; 
            from the SampleSet of the current Channel, calculate the two keyframe indices that surround the absolute animation time; 
            perform the Sampler.interpolation logic on the targetSkin->bone[boneIndex]'s Channel.targetPath according to gltf 2.0 spec. appendix C; 
        note that we shouldn't have to worry about bone hierarchies here, as we are only animating the local bone transforms; 
        model-space targetSkin->bone transforms will be calculated when we actually want to render the skinned mesh; */
    const f32 animationSeconds = KORL_MATH_CLAMP(animation->keyFrameSecondsStart + secondsRelativeToAnimationStart, animation->keyFrameSecondsStart, animation->keyFrameSecondsEnd);
    for(u32 ac = 0; ac < gltfAnimation->channels.size; ac++)
    {
        const Korl_Codec_Gltf_Animation_Channel*const gltfAnimationChannel = gltfAnimationChannels + ac;
        if(gltfAnimationChannel->target.node < 0)
            continue;// gltf 2.0 spec. 3.11: "When node isn’t defined, channel SHOULD be ignored."
        /* obtain the target bone of the skin */
        const i32 targetBoneIndex = skin->nodeIndex_to_boneIndex[gltfAnimationChannel->target.node];
        korl_assert(targetBoneIndex >= 0);// sanity check; the skin _must_ utilize this Animation Channel's target node
        korl_assert(KORL_C_CAST(u32, targetBoneIndex) < targetSkin->bonesSize);// sanity check; make sure we don't go out-of-bounds
        Korl_Math_Transform3d*const targetBoneTransform = targetSkin->bones + targetBoneIndex;
        /* calculate which keyframes surround animationSeconds */
        const _Korl_Resource_Scene3d_Animation_SampleSet*const sampleSet         = animation->sampleSets       + gltfAnimationChannel->sampler;
        const Korl_Codec_Gltf_Animation_Sampler*const          gltfSampler       = gltfAnimationSamplers       + gltfAnimationChannel->sampler;
        const Korl_Codec_Gltf_Accessor*const                   gltfAccessorInput = gltfAccessors               + gltfSampler->input;
        const f32*const                                        keyFramesSeconds  = animation->keyFramesSeconds + sampleSet->keyFramesSecondsStartIndex;
        u32 k = 0;// keyframe index; the index of the keyframe immediately _before_ animationSeconds
        // PERFORMANCE: keyFramesSeconds should be in ascending order; use binary search, or some acceleration data structure to find keyframe index to achieve sub-linear times, if necessary
        for(; k < gltfAccessorInput->count; k++)
            if(   (keyFramesSeconds[k] < animationSeconds || korl_math_isNearlyEqual(keyFramesSeconds[k], animationSeconds))
               && (k >= gltfAccessorInput->count - 1      || keyFramesSeconds[k + 1] > animationSeconds))
                break;
        korl_assert(k < gltfAccessorInput->count);// ensure that we did, in fact, find a keyframe
        /* perform interpolation on the target path; implementation should be derived from gltf 2.0 spec. appendix C */
        const Korl_Codec_Gltf_Accessor*const gltfAccessorOutput = gltfAccessors      + gltfSampler->output;
        const f32*const                      rawSamples         = animation->samples + sampleSet->sampleStartIndex;
        const Korl_Math_V3f32*const          rawSamplesV3f32    = KORL_C_CAST(Korl_Math_V3f32*, rawSamples);
        const Korl_Math_V4f32*const          rawSamplesV4f32    = KORL_C_CAST(Korl_Math_V4f32*, rawSamples);
        switch(gltfSampler->interpolation)
        {
        case KORL_CODEC_GLTF_ANIMATION_SAMPLER_INTERPOLATION_LINEAR:
            const u32 kNext               = KORL_MATH_CLAMP(k + 1, 0, gltfAccessorInput->count - 1);
            const f32 keyframeCurrent     = animationSeconds - keyFramesSeconds[k];
            const f32 keyframeDuration    = keyFramesSeconds[kNext] - keyFramesSeconds[k];
            const f32 interpolationFactor = korl_math_isNearlyZero(keyframeDuration) ? 0 : keyframeCurrent / keyframeDuration;
            korl_assert(   (interpolationFactor > 0 || korl_math_isNearlyZero(interpolationFactor))
                        && (interpolationFactor < 1 || korl_math_isNearlyEqual(interpolationFactor, 1)));
            switch(gltfAnimationChannel->target.path)
            {
            case KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_TRANSLATION:
                korl_assert(gltfAccessorOutput->type == KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC3);
                korl_math_transform3d_setPosition(targetBoneTransform, korl_math_v3f32_interpolateLinear(rawSamplesV3f32[k], rawSamplesV3f32[kNext], interpolationFactor));
                break;
            case KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_ROTATION:
                korl_assert(gltfAccessorOutput->type == KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC4);
                //KORL-ISSUE-000-000-186: scene3d: (_minor_) gltf 2.0 spec. appendix C.3 & C.4 wants us to apply Spherical Linear Interpolation here instead of just Linear Interpolation; LERP is only a reasonable approximation when the angle between quaternions is close to zero, but I believe Casey Muratori actually looked into this and found that in the vast majority of cases LERP provides more than enough in terms of accuracy (and seems to make the math for blending multiple animations _much_ easier!): https://caseymuratori.com/blog_0002
                korl_math_transform3d_setVersor(targetBoneTransform, korl_math_quaternion_normal(KORL_STRUCT_INITIALIZE(Korl_Math_Quaternion){.v4 = korl_math_v4f32_interpolateLinear(rawSamplesV4f32[k], rawSamplesV4f32[kNext], interpolationFactor)}));
                break;
            case KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_SCALE:
                korl_assert(gltfAccessorOutput->type == KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC3);
                korl_math_transform3d_setScale(targetBoneTransform, korl_math_v3f32_interpolateLinear(rawSamplesV3f32[k], rawSamplesV3f32[kNext], interpolationFactor));
                break;
            case KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_WEIGHTS:
                korl_log(ERROR, "not implemented");
                break;
            default: korl_log(ERROR, "invalid animation channel target path: %i", gltfAnimationChannel->target.path);
            }
            break;
        case KORL_CODEC_GLTF_ANIMATION_SAMPLER_INTERPOLATION_STEP:
            switch(gltfAnimationChannel->target.path)
            {
            case KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_TRANSLATION:
                korl_assert(gltfAccessorOutput->type == KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC3);
                korl_math_transform3d_setPosition(targetBoneTransform, rawSamplesV3f32[k]);
                break;
            case KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_ROTATION:
                korl_assert(gltfAccessorOutput->type == KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC4);
                korl_math_transform3d_setVersor(targetBoneTransform, KORL_STRUCT_INITIALIZE(Korl_Math_Quaternion){.v4 = rawSamplesV4f32[k]});
                break;
            case KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_SCALE:
                korl_assert(gltfAccessorOutput->type == KORL_CODEC_GLTF_ACCESSOR_TYPE_VEC3);
                korl_math_transform3d_setScale(targetBoneTransform, rawSamplesV3f32[k]);
                break;
            case KORL_CODEC_GLTF_ANIMATION_CHANNEL_TARGET_PATH_WEIGHTS:
                korl_log(ERROR, "not implemented");
                break;
            default: korl_log(ERROR, "invalid animation channel target path: %i", gltfAnimationChannel->target.path);
            }
            break;
        case KORL_CODEC_GLTF_ANIMATION_SAMPLER_INTERPOLATION_CUBIC_SPLINE:
            korl_log(ERROR, "not implemented");
            break;
        default: korl_log(ERROR, "invalid sampler interpolation: %i", gltfSampler->interpolation);
        }
    }
}
