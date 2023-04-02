#include "korl-resource.h"
#include "korl-stb-ds.h"
#include "korl-stb-image.h"
#include "korl-stringPool.h"
#include "korl-audio.h"
#include "korl-codec-audio.h"
#include "korl-codec-glb.h"
#define _LOCAL_STRING_POOL_POINTER (_korl_resource_context->stringPool)
korl_global_const u$ _KORL_RESOURCE_UNIQUE_ID_MAX = 0x0FFFFFFFFFFFFFFF;
typedef enum _Korl_Resource_Type
    {_KORL_RESOURCE_TYPE_INVALID // this value indicates the entire Resource Handle is not valid
    ,_KORL_RESOURCE_TYPE_FILE    // Resource is derived from a korl-assetCache file
    ,_KORL_RESOURCE_TYPE_RUNTIME // Resource is derived from data that is generated at runtime
} _Korl_Resource_Type;
typedef enum _Korl_Resource_MultimediaType
    {_KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS// this resource maps to a vulkan device-local allocation, or similar type of data
    ,_KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO
} _Korl_Resource_MultimediaType;
typedef struct _Korl_Resource_Handle_Unpacked
{
    _Korl_Resource_Type           type;
    _Korl_Resource_MultimediaType multimediaType;
    u$                            uniqueId;
} _Korl_Resource_Handle_Unpacked;
typedef enum _Korl_Resource_Graphics_Type
    {_KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN
    ,_KORL_RESOURCE_GRAPHICS_TYPE_IMAGE
    ,_KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER
    ,_KORL_RESOURCE_GRAPHICS_TYPE_SHADER
    ,_KORL_RESOURCE_GRAPHICS_TYPE_SCENE3D
} _Korl_Resource_Graphics_Type;
typedef struct _Korl_Resource
{
    // _Korl_Resource_MultimediaType type;// we don't actually need the type here, since type is part of the resource handle, and that is the hash key of the database!
    union
    {
        struct
        {
            _Korl_Resource_Graphics_Type type;
            union
            {
                Korl_Vulkan_DeviceMemory_AllocationHandle deviceMemoryAllocationHandle;
                Korl_Vulkan_ShaderHandle                  shaderHandle;
            } deviceHandle;
            union
            {
                Korl_Vulkan_CreateInfoTexture texture;
                struct
                {
                    Korl_Vulkan_VertexAttributeDescriptor vertexAttributeDescriptors[KORL_VULKAN_VERTEX_ATTRIBUTE_ENUM_COUNT];
                    Korl_Vulkan_CreateInfoVertexBuffer createInfo;
                } vertexBuffer;
            } createInfo;
        } graphics;
        struct
        {
            Korl_Audio_Format format;
            void*             resampledData;// this should be the same audio data as in the `data` member, but resampled to be compatible with the `audioRendererFormat` member of the korl-resource context; NOTE: the # of channels will remain the same as in the `format` member, all other aspects of `format` will be modified to match `audioRendererFormat`; if the aforementioned condition is already met (this audio resource just so happens to have the same format as the context, excluding `channels`), then this member is expected to be NULL, as we can just feed `data` directly to the audio mixer
            u$                resampledDataBytes;
        } audio;
    } subType;
    void* data;// this is a pointer to a memory allocation holding the raw _decoded_ resource; this is the data which should be passed in to the platform module which utilizes this Resource's MultimediaType _unless_ that multimedia type requires further processing to prepare it for the rendering process, such as is the case with audio, as we must resample all audio data to match the format of the audio renderer; examples: an image resource data will point to an RGBA bitmap, an audio resource will point to raw PCM waveform data, in some sample format defined by the `audio.format` member
    u$    dataBytes;
    bool dirty;// IMPORTANT: raising this flag is _not_ sufficient to mark this Resource as dirty, you _must_ also add the handle to this resource to the stbDsDirtyResourceHandles list in the korl-resource context!  If this is true, the multimedia-encoded asset will be updated at the end of the frame, then the flag will be reset
    Korl_StringPool_String    stringFileName;    // only valid for file-backed resources
    Korl_AssetCache_Get_Flags assetCacheGetFlags;// only valid for file-backed resources
    //KORL-FEATURE-000-000-046: resource: right now it is very difficult to trace the owner of a resource; modify API to require FILE:LINE information for Resource allocations
} _Korl_Resource;
typedef struct _Korl_Resource_Map
{
    Korl_Resource_Handle key;
    _Korl_Resource       value;
} _Korl_Resource_Map;
typedef struct _Korl_Resource_Context
{
    Korl_Memory_AllocatorHandle allocatorHandleRuntime;// all unique data that cannot be easily reobtained/reconstructed from a korl-memoryState is stored here, including this struct itself
    Korl_Memory_AllocatorHandle allocatorHandleTransient;// all cached data that can be retranscoded/reobtained is stored here, such as korl-asset transcodings or audio.resampledData; we do _not_ need to copy this data to korl-memoryState in order for that functionality to work, so we wont!
    _Korl_Resource_Map*         stbHmResources;
    Korl_Resource_Handle*       stbDsDirtyResourceHandles;
    u$                          nextUniqueId;// this counter will increment each time we add a _non-file_ resource to the database; file-based resources will have a unique id generated from a hash of the asset file name
    Korl_StringPool*            stringPool;// @korl-string-pool-no-data-segment-storage; used to store the file name strings of file resources, allowing us to hot-reload resources when the underlying korl-asset is hot-reloaded
    Korl_Audio_Format           audioRendererFormat;// the currently configured audio renderer format, likely to be set by korl-sfx; when this is changed via `korl_resource_setAudioFormat`, all audio resources will be resampled to match this format, and that resampled audio data is what will be mixed into korl-audio; we do this to sacrifice memory for speed, as it should be much better performance to not have to worry about audio resampling algorithms at runtime
    bool                        audioResamplesPending;// set when `audioRendererFormat` changes, or we lazy-load a file resource via `korl_resource_fromFile`; once set, each call to `korl_resource_flushUpdates` will incur iteration over all audio resources to ensure that they are all resampled to `audioRendererFormat`'s specifications; only cleared when `korl_resource_flushUpdates` finds that all audio resources are loaded & resampled
} _Korl_Resource_Context;
korl_global_variable _Korl_Resource_Context* _korl_resource_context;
korl_internal _Korl_Resource_Handle_Unpacked _korl_resource_handle_unpack(Korl_Resource_Handle handle)
{
    KORL_ZERO_STACK(_Korl_Resource_Handle_Unpacked, unpackedHandle);
    unpackedHandle.type           = (handle >> 62) & 0b11;
    unpackedHandle.multimediaType = (handle >> 60) & 0b11;
    unpackedHandle.uniqueId       =  handle        & _KORL_RESOURCE_UNIQUE_ID_MAX;
    return unpackedHandle;
}
korl_internal Korl_Resource_Handle _korl_resource_handle_pack(_Korl_Resource_Handle_Unpacked unpackedHandle)
{
    return ((KORL_C_CAST(Korl_Resource_Handle, unpackedHandle.type)           & 0b11) << 62)
         | ((KORL_C_CAST(Korl_Resource_Handle, unpackedHandle.multimediaType) & 0b11) << 60)
         | (                                   unpackedHandle.uniqueId        & _KORL_RESOURCE_UNIQUE_ID_MAX);
}
korl_internal bool _korl_resource_isLoaded(_Korl_Resource*const resource, const _Korl_Resource_Handle_Unpacked unpackedHandle)
{
    if(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS)
        if(resource->subType.graphics.type == _KORL_RESOURCE_GRAPHICS_TYPE_SHADER)
            return 0 != resource->subType.graphics.deviceHandle.shaderHandle;
    return resource->data;
}
korl_internal void _korl_resource_unload(_Korl_Resource*const resource, const _Korl_Resource_Handle_Unpacked unpackedHandle)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    if(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS)
        if(resource->subType.graphics.type == _KORL_RESOURCE_GRAPHICS_TYPE_SHADER)
        {
            korl_vulkan_shader_destroy(resource->subType.graphics.deviceHandle.shaderHandle);
            resource->subType.graphics.deviceHandle.shaderHandle = 0;
            return;
        }
    korl_free(context->allocatorHandleTransient, resource->data);// ASSUMPTION: this function is run _after_ the memory state allocators/allocations are loaded!
    resource->data      = NULL;
    resource->dataBytes = 0;
}
korl_internal _Korl_Resource_Handle_Unpacked _korl_resource_fileNameToUnpackedHandle(acu16 fileName, _Korl_Resource_Graphics_Type* out_graphicsType)
{
    KORL_ZERO_STACK(_Korl_Resource_Handle_Unpacked, unpackedHandle);
    /* automatically determine the type of multimedia this resource is based on the file extension */
    _Korl_Resource_Graphics_Type graphicsType = _KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN;
    {
        korl_shared_const u16* IMAGE_EXTENSIONS[] = {L".png", L".jpg", L".jpeg"};
        for(u32 i = 0; i < korl_arraySize(IMAGE_EXTENSIONS); i++)
        {
            const u$ extensionSize = korl_string_sizeUtf16(IMAGE_EXTENSIONS[i]);
            if(fileName.size < extensionSize)
                continue;
            if(0 == korl_memory_arrayU16Compare(fileName.data + fileName.size - extensionSize, extensionSize, IMAGE_EXTENSIONS[i], extensionSize))
            {
                unpackedHandle.multimediaType = _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS;
                graphicsType                  = _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE;
                goto multimediaTypeFound;
            }
        }
    }
    {
        korl_shared_const u16* SHADER_EXTENSIONS[] = {L".spv"};
        for(u32 i = 0; i < korl_arraySize(SHADER_EXTENSIONS); i++)
        {
            const u$ extensionSize = korl_string_sizeUtf16(SHADER_EXTENSIONS[i]);
            if(fileName.size < extensionSize)
                continue;
            if(0 == korl_memory_arrayU16Compare(fileName.data + fileName.size - extensionSize, extensionSize, SHADER_EXTENSIONS[i], extensionSize))
            {
                unpackedHandle.multimediaType = _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS;
                graphicsType                  = _KORL_RESOURCE_GRAPHICS_TYPE_SHADER;
                goto multimediaTypeFound;
            }
        }
    }
    {
        korl_shared_const u16* SCENE3D_EXTENSIONS[] = {L".glb"};
        for(u32 i = 0; i < korl_arraySize(SCENE3D_EXTENSIONS); i++)
        {
            const u$ extensionSize = korl_string_sizeUtf16(SCENE3D_EXTENSIONS[i]);
            if(fileName.size < extensionSize)
                continue;
            if(0 == korl_memory_arrayU16Compare(fileName.data + fileName.size - extensionSize, extensionSize, SCENE3D_EXTENSIONS[i], extensionSize))
            {
                unpackedHandle.multimediaType = _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS;
                graphicsType                  = _KORL_RESOURCE_GRAPHICS_TYPE_SCENE3D;
                goto multimediaTypeFound;
            }
        }
    }
    {
        korl_shared_const u16* AUDIO_EXTENSIONS[] = {L".wav", L".ogg"};
        for(u32 i = 0; i < korl_arraySize(AUDIO_EXTENSIONS); i++)
        {
            const u$ extensionSize = korl_string_sizeUtf16(AUDIO_EXTENSIONS[i]);
            if(fileName.size < extensionSize)
                continue;
            if(0 == korl_memory_arrayU16Compare(fileName.data + fileName.size - extensionSize, extensionSize, AUDIO_EXTENSIONS[i], extensionSize))
            {
                unpackedHandle.multimediaType = _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO;
                goto multimediaTypeFound;
            }
        }
    }
    goto returnUnpackedHandle;
    multimediaTypeFound:
        /* hash the file name, generate our resource handle */
        unpackedHandle.type     = _KORL_RESOURCE_TYPE_FILE;
        unpackedHandle.uniqueId = korl_memory_acu16_hash(fileName);
        if(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS)
        {
            korl_assert(graphicsType != _KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN);
            *out_graphicsType = graphicsType;
        }
    returnUnpackedHandle:
        return unpackedHandle;
}
korl_internal void _korl_resource_resampleAudio(_Korl_Resource* resource)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    korl_assert(!resource->subType.audio.resampledData);
    if(   resource->subType.audio.format.bytesPerSample == context->audioRendererFormat.bytesPerSample
       && resource->subType.audio.format.sampleFormat   == context->audioRendererFormat.sampleFormat
       && resource->subType.audio.format.frameHz        == context->audioRendererFormat.frameHz)
       return;
    Korl_Audio_Format resampledFormat = context->audioRendererFormat;
    resampledFormat.channels = resource->subType.audio.format.channels;
    const u$  bytesPerFrame    = resource->subType.audio.format.bytesPerSample * resource->subType.audio.format.channels;
    const u$  frames           = resource->dataBytes / bytesPerFrame;
    const f64 seconds          = KORL_C_CAST(f64, frames) / KORL_C_CAST(f64, resource->subType.audio.format.frameHz);
    const u$  newFrames        = resource->subType.audio.format.frameHz == resampledFormat.frameHz
                                 ? frames
                                 : KORL_C_CAST(u$, seconds * resampledFormat.frameHz);
    const u$  newBytesPerFrame = resampledFormat.bytesPerSample * resampledFormat.channels;
    resource->subType.audio.resampledDataBytes = newFrames * newBytesPerFrame;
    resource->subType.audio.resampledData      = korl_allocate(context->allocatorHandleTransient, resource->subType.audio.resampledDataBytes);
    korl_codec_audio_resample(&resource->subType.audio.format, resource->data, resource->dataBytes
                             ,&resampledFormat, resource->subType.audio.resampledData, resource->subType.audio.resampledDataBytes);
}
korl_internal void _korl_resource_fileResourceLoadStep(_Korl_Resource*const resource, const _Korl_Resource_Handle_Unpacked unpackedHandle)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    korl_assert(unpackedHandle.type == _KORL_RESOURCE_TYPE_FILE);
    if(_korl_resource_isLoaded(resource, unpackedHandle))
        return;
    KORL_ZERO_STACK(Korl_AssetCache_AssetData, assetData);
    const Korl_AssetCache_Get_Result assetCacheGetResult = korl_assetCache_get(string_getRawUtf16(&resource->stringFileName), resource->assetCacheGetFlags, &assetData);
    if(resource->assetCacheGetFlags == KORL_ASSETCACHE_GET_FLAGS_NONE)
        korl_assert(assetCacheGetResult == KORL_ASSETCACHE_GET_RESULT_LOADED);
    /* if the asset for this resource was just loaded, create the multimedia resource now */
    if(assetCacheGetResult == KORL_ASSETCACHE_GET_RESULT_LOADED)
    {
        switch(unpackedHandle.multimediaType)
        {
        case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
            switch(resource->subType.graphics.type)
            {
            case _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE:{
                int imageSizeX = 0, imageSizeY = 0, imageChannels = 0;
                ///KORL-PERFORMANCE-000-000-032: resource: stbi API unfortunately doesn't seem to have the ability for the user to provide their own allocator, so we need an extra alloc/copy/free here
                {
                    stbi_uc*const stbiPixels = stbi_load_from_memory(assetData.data, assetData.dataBytes, &imageSizeX, &imageSizeY, &imageChannels, STBI_rgb_alpha);
                    korl_assert(stbiPixels);
                    const u$ pixelBytes = imageSizeX * imageSizeY * imageChannels;
                    resource->data      = korl_allocate(context->allocatorHandleTransient, pixelBytes);
                    resource->dataBytes = pixelBytes;
                    korl_memory_copy(resource->data, stbiPixels, pixelBytes);
                    stbi_image_free(stbiPixels);
                    /* pre-multiply alpha channel into all file images by default */
                    Korl_Vulkan_Color4u8*const colorData = KORL_C_CAST(Korl_Vulkan_Color4u8*, resource->data);
                    for(int i = 0; i < imageSizeX * imageSizeY; i++)
                    {
                        const f32 alpha = KORL_C_CAST(f32, colorData[i].a) / KORL_C_CAST(f32, KORL_U8_MAX);
                        colorData[i].r = korl_math_round_f32_to_u8(alpha * KORL_C_CAST(f32, colorData[i].r));
                        colorData[i].g = korl_math_round_f32_to_u8(alpha * KORL_C_CAST(f32, colorData[i].g));
                        colorData[i].b = korl_math_round_f32_to_u8(alpha * KORL_C_CAST(f32, colorData[i].b));
                    }
                }
                resource->subType.graphics.createInfo.texture.sizeX                  = korl_checkCast_i$_to_u32(imageSizeX);
                resource->subType.graphics.createInfo.texture.sizeY                  = korl_checkCast_i$_to_u32(imageSizeY);
                resource->subType.graphics.deviceHandle.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createTexture(&resource->subType.graphics.createInfo.texture, 0/*0 => generate new handle*/);
                korl_vulkan_texture_update(resource->subType.graphics.deviceHandle.deviceMemoryAllocationHandle, resource->data);
                break;}
            case _KORL_RESOURCE_GRAPHICS_TYPE_SHADER:{
                KORL_ZERO_STACK(Korl_Vulkan_CreateInfoShader, createInfoShader);// just keep this data on the stack, since the underlying data is transient anyway, and we don't really need this data to re-create the shader later
                createInfoShader.data      = assetData.data;
                createInfoShader.dataBytes = assetData.dataBytes;
                resource->subType.graphics.deviceHandle.shaderHandle = korl_vulkan_shader_create(&createInfoShader, 0);
                break;}
            case _KORL_RESOURCE_GRAPHICS_TYPE_SCENE3D:{
                resource->data = korl_codec_glb_decode(assetData.data, assetData.dataBytes, context->allocatorHandleTransient, &resource->dataBytes);
                korl_assert(resource->data);
                break;}
            default:
                korl_log(ERROR, "invalid graphics type %i", resource->subType.graphics.type);
                break;
            }
            break;}
        case _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO:{
            resource->data = korl_codec_audio_decode(assetData.data, assetData.dataBytes, context->allocatorHandleTransient, &resource->dataBytes, &resource->subType.audio.format);
            korl_assert(resource->data);
            break;}
        default:{
            korl_log(ERROR, "invalid multimedia type: %i", unpackedHandle.multimediaType);
            break;}
        }
    }
    if(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO)
        context->audioResamplesPending = true;
}
korl_internal void korl_resource_initialize(void)
{
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_gigabytes(1);
    const Korl_Memory_AllocatorHandle allocator = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-resource", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    _korl_resource_context = korl_allocate(allocator, sizeof(*_korl_resource_context));
    _Korl_Resource_Context*const context = _korl_resource_context;
    context->allocatorHandleRuntime   = allocator;
    context->allocatorHandleTransient = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-resource-transient", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    context->stringPool               = korl_allocate(context->allocatorHandleRuntime, sizeof(*context->stringPool));
    *context->stringPool              = korl_stringPool_create(context->allocatorHandleRuntime);
    mchmdefault(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbDsDirtyResourceHandles, 128);
}
korl_internal KORL_FUNCTION_korl_resource_fromFile(korl_resource_fromFile)
{
    _Korl_Resource_Context*const         context        = _korl_resource_context;
    _Korl_Resource_Graphics_Type         graphicsType   = _KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN;
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_fileNameToUnpackedHandle(fileName, &graphicsType);// sets the type to _KORL_RESOURCE_TYPE_FILE
    /* we should now have all the info needed to create the packed resource handle */
    const Korl_Resource_Handle handle = _korl_resource_handle_pack(unpackedHandle);
    korl_assert(handle);
    /* check if the resource was already added */
    ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    /* if the resource is new, we need to add it to the database */
    if(hashMapIndex < 0)
    {
        mchmput(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
        hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
        korl_assert(hashMapIndex >= 0);
        _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
        resource->stringFileName     = string_newAcu16(fileName);
        resource->assetCacheGetFlags = assetCacheGetFlags;
        if(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS)
            resource->subType.graphics.type = graphicsType;
    }
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    /* regardless of whether or not the resource was just added, we still need to make sure that the asset was loaded for it */
    _korl_resource_fileResourceLoadStep(resource, unpackedHandle);
    /**/
    return handle;
}
korl_internal Korl_Resource_Handle korl_resource_createVertexBuffer(const Korl_Vulkan_CreateInfoVertexBuffer* createInfo)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    korl_assert(context->nextUniqueId <= _KORL_RESOURCE_UNIQUE_ID_MAX);// assuming this ever fires under normal circumstances (_highly_ unlikely), we will need to implement a system to recycle old unused UIDs or something
    /* construct the new resource handle */
    KORL_ZERO_STACK(_Korl_Resource_Handle_Unpacked, unpackedHandle);
    unpackedHandle.type           = _KORL_RESOURCE_TYPE_RUNTIME;
    unpackedHandle.multimediaType = _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS;
    unpackedHandle.uniqueId       = context->nextUniqueId++;
    const Korl_Resource_Handle handle = _korl_resource_handle_pack(unpackedHandle);
    /* add the new resource to the database */
    ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex < 0);
    mchmput(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
    hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    resource->subType.graphics.type = _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER;
    /* allocate the memory for the raw decoded asset data */
    resource->dataBytes = createInfo->bytes;
    resource->data      = korl_allocate(context->allocatorHandleRuntime, createInfo->bytes);
    korl_assert(resource->data);
    /* create the multimedia asset */
    resource->subType.graphics.deviceHandle.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createVertexBuffer(createInfo, 0/*0 => generate new handle*/);
    resource->subType.graphics.createInfo.vertexBuffer.createInfo        = *createInfo;
    // we have to perform a deep-copy of the vertex buffer create info struct, since the vertex attribute descriptors is stored in the create info as a pointer
    korl_assert(createInfo->vertexAttributeDescriptorCount <= korl_arraySize(resource->subType.graphics.createInfo.vertexBuffer.vertexAttributeDescriptors));
    for(u$ v = 0; v < createInfo->vertexAttributeDescriptorCount; v++)
        resource->subType.graphics.createInfo.vertexBuffer.vertexAttributeDescriptors[v] = createInfo->vertexAttributeDescriptors[v];
    return handle;
}
korl_internal Korl_Resource_Handle korl_resource_createTexture(const Korl_Vulkan_CreateInfoTexture* createInfo)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    korl_assert(context->nextUniqueId <= _KORL_RESOURCE_UNIQUE_ID_MAX);// assuming this ever fires under normal circumstances (_highly_ unlikely), we will need to implement a system to recycle old unused UIDs or something
    /* construct the new resource handle */
    KORL_ZERO_STACK(_Korl_Resource_Handle_Unpacked, unpackedHandle);
    unpackedHandle.type           = _KORL_RESOURCE_TYPE_RUNTIME;
    unpackedHandle.multimediaType = _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS;
    unpackedHandle.uniqueId       = context->nextUniqueId++;
    const Korl_Resource_Handle handle = _korl_resource_handle_pack(unpackedHandle);
    /* add the new resource to the database */
    ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex < 0);
    mchmput(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
    hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    resource->subType.graphics.type = _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE;
    /* allocate the memory for the raw decoded asset data */
    resource->dataBytes = createInfo->sizeX * createInfo->sizeY * sizeof(Korl_Vulkan_Color4u8);
    resource->data      = korl_allocate(context->allocatorHandleRuntime, resource->dataBytes);
    korl_assert(resource->data);
    /* create the multimedia asset */
    resource->subType.graphics.deviceHandle.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createTexture(createInfo, 0/*0 => generate new handle*/);
    resource->subType.graphics.createInfo.texture                        = *createInfo;
    return handle;
}
korl_internal void korl_resource_destroy(Korl_Resource_Handle handle)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    if(!handle)
        return;// silently do nothing for NULL handles
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    /* destroy the transcoded multimedia asset */
    switch(unpackedHandle.multimediaType)
    {
    case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
        switch(resource->subType.graphics.type)
        {
        case _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE:
        case _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER:{
            korl_vulkan_deviceAsset_destroy(resource->subType.graphics.deviceHandle.deviceMemoryAllocationHandle);
            break;}
        case _KORL_RESOURCE_GRAPHICS_TYPE_SHADER:{
            korl_vulkan_shader_destroy(resource->subType.graphics.deviceHandle.shaderHandle);
            break;}
        default:
            korl_log(ERROR, "graphics type not implemented: %u", resource->subType.graphics.type);
        }
        break;}
    case _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO:{
        korl_free(context->allocatorHandleTransient, resource->subType.audio.resampledData);
        break;}
    default:{
        korl_log(ERROR, "invalid multimedia type %i", unpackedHandle.multimediaType);
        break;}
    }
    /* if the resource backed by a file, we should destroy the cached file name string */
    string_free(&resource->stringFileName);
    /* destroy the cached decoded raw asset data */
    if(unpackedHandle.type == _KORL_RESOURCE_TYPE_RUNTIME)
        korl_free(context->allocatorHandleRuntime, resource->data);
    else
        korl_free(context->allocatorHandleTransient, resource->data);
    /* remove the resource from the database */
    mchmdel(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
}
korl_internal void korl_resource_update(Korl_Resource_Handle handle, const void* sourceData, u$ sourceDataBytes, u$ destinationByteOffset)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.type == _KORL_RESOURCE_TYPE_RUNTIME);
    korl_assert(destinationByteOffset + sourceDataBytes <= resource->dataBytes);
    korl_memory_copy(KORL_C_CAST(u8*, resource->data) + destinationByteOffset, sourceData, sourceDataBytes);
    if(!resource->dirty)
    {
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbDsDirtyResourceHandles, handle);
        resource->dirty = true;
    }
}
korl_internal u$ korl_resource_getByteSize(Korl_Resource_Handle handle)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    return resource->dataBytes;
}
korl_internal void korl_resource_resize(Korl_Resource_Handle handle, u$ newByteSize)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.type == _KORL_RESOURCE_TYPE_RUNTIME);
    if(resource->dataBytes == newByteSize)
        return;// silently do nothing if we're not actually resizing
    korl_assert(newByteSize);// for now, ensure that the user is requesting a non-zero # of bytes
    switch(unpackedHandle.multimediaType)
    {
    case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
        switch(resource->subType.graphics.type)
        {
        case _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER:{
            korl_vulkan_vertexBuffer_resize(&resource->subType.graphics.deviceHandle.deviceMemoryAllocationHandle, newByteSize);
            resource->subType.graphics.createInfo.vertexBuffer.createInfo.bytes = newByteSize;
            break;}
        default:{
            korl_log(ERROR, "invalid graphics type: %i", resource->subType.graphics.type);
            break;}
        }
        break;}
    default:{
        korl_log(ERROR, "invalid multimedia type: %i", unpackedHandle.multimediaType);
        break;}
    }
    resource->data      = korl_reallocate(context->allocatorHandleRuntime, resource->data, newByteSize);
    resource->dataBytes = newByteSize;
    korl_assert(resource->data);
}
korl_internal void korl_resource_shift(Korl_Resource_Handle handle, i$ byteShiftCount)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.type == _KORL_RESOURCE_TYPE_RUNTIME);
    if(byteShiftCount == 0)
        return;// silently do nothing else if the user doesn't provide a non-zero byte shift amount
    if(byteShiftCount < 0)
    {
        const u$ remainingBytes = korl_checkCast_u$_to_i$(resource->dataBytes) >= (-1*byteShiftCount) 
                                ? resource->dataBytes + byteShiftCount
                                : 0;
        if(remainingBytes)
            korl_memory_move(resource->data, KORL_C_CAST(u8*, resource->data) + -byteShiftCount, remainingBytes);
        korl_memory_zero(KORL_C_CAST(u8*, resource->data) + remainingBytes, -byteShiftCount);
    }
    else
    {
        const u$ remainingBytes = korl_checkCast_u$_to_i$(resource->dataBytes) >= byteShiftCount 
                                ? resource->dataBytes - byteShiftCount 
                                : 0;
        if(remainingBytes)
            korl_memory_move(KORL_C_CAST(u8*, resource->data) + byteShiftCount, resource->data, remainingBytes);
        korl_memory_zero(resource->data, byteShiftCount);
    }
    /* shifting data by a non-zero amount => the resource must be flushed */
    if(!resource->dirty)
    {
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbDsDirtyResourceHandles, handle);
        resource->dirty = true;
    }
}
korl_internal void korl_resource_flushUpdates(void)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    const Korl_Resource_Handle*const dirtyHandlesEnd = context->stbDsDirtyResourceHandles + arrlen(context->stbDsDirtyResourceHandles);
    for(const Korl_Resource_Handle* dirtyHandle = context->stbDsDirtyResourceHandles; dirtyHandle < dirtyHandlesEnd; dirtyHandle++)
    {
        const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, *dirtyHandle);
        if(hashMapIndex < 0)
        {
            //KORL-ISSUE-000-000-128: gui: (minor) WARNING logged on memory state load due to frivolous resource destruction
            korl_log(WARNING, "updated resource handle invalid (update + delete in same frame, etc.): 0x%X", *dirtyHandle);
            continue;
        }
        _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
        korl_assert(resource->dirty);
        const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(*dirtyHandle);
        korl_assert(unpackedHandle.type == _KORL_RESOURCE_TYPE_RUNTIME);
        switch(unpackedHandle.multimediaType)
        {
        case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
            switch(resource->subType.graphics.type)
            {
            case _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE:{
                korl_vulkan_texture_update(resource->subType.graphics.deviceHandle.deviceMemoryAllocationHandle, resource->data);
                break;}
            case _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER:{
                korl_vulkan_vertexBuffer_update(resource->subType.graphics.deviceHandle.deviceMemoryAllocationHandle, resource->data, resource->dataBytes, 0);
                break;}
            default:{
                korl_log(ERROR, "invalid graphics type: %i", resource->subType.graphics.type);
                break;}
            }
            break;}
        default:{
            korl_log(ERROR, "invalid multimedia type: %i", unpackedHandle.multimediaType);
            break;}
        }
        resource->dirty = false;
    }
    mcarrsetlen(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbDsDirtyResourceHandles, 0);
    /* once per frame, if we have any audio resources that are pending generation 
        of cached resampled audio data to match the audio renderer's format, we 
        attempt to resample as many of these as possible; once all audio 
        resources have been resampled, there is no need to do this each frame 
        anymore, so the flag is cleared */
    if(   !korl_memory_isNull(&context->audioRendererFormat, sizeof(context->audioRendererFormat))
       && context->audioResamplesPending)
    {
        u$ resampledResources    = 0;
        u$ pendingAudioResamples = 0;
        const _Korl_Resource_Map*const resourcesEnd = context->stbHmResources + hmlen(context->stbHmResources);
        for(_Korl_Resource_Map* resourceMapIt = context->stbHmResources; resourceMapIt < resourcesEnd; resourceMapIt++)
        {
            const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(resourceMapIt->key);
            if(unpackedHandle.multimediaType != _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO)
                continue;
            _Korl_Resource*const resource = &resourceMapIt->value;
            if(!resource->data)
            {
                if(resource->stringFileName.handle)
                    /* if this is a file-backed resource, attempt to get the lazy-loaded file one more time: */
                    korl_resource_fromFile(string_getRawAcu16(&resource->stringFileName), KORL_ASSETCACHE_GET_FLAG_LAZY);
                /* if we _still_ have no resource data, we have no choice but to consider this audio resource as "pending" */
                if(!resource->data)
                {
                    pendingAudioResamples++;
                    continue;// we can't resample the audio resource if the data has not yet been loaded!
                }
            }
            if(resource->subType.audio.resampledData)
                continue;// we're already resampled
            _korl_resource_resampleAudio(resource);
            resampledResources++;
        }
        context->audioResamplesPending = pendingAudioResamples > 0;
        if(pendingAudioResamples)
            korl_log(VERBOSE, "pendingAudioResamples=%llu", pendingAudioResamples);
        korl_log(VERBOSE, "resampledResources=%llu", resampledResources);
    }
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_resource_getVulkanDeviceMemoryAllocationHandle(Korl_Resource_Handle handle)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    if(!handle)
        return 0;// silently return a NULL device memory allocation handle if the resource handle is NULL
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS);
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    if(unpackedHandle.type == _KORL_RESOURCE_TYPE_FILE)
        _korl_resource_fileResourceLoadStep(resource, unpackedHandle);
    return resource->subType.graphics.deviceHandle.deviceMemoryAllocationHandle;
}
korl_internal void korl_resource_setAudioFormat(const Korl_Audio_Format* audioFormat)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    if(0 == korl_memory_compare(&context->audioRendererFormat, audioFormat, sizeof(*audioFormat)))
        return;
    context->audioRendererFormat   = *audioFormat;
    context->audioResamplesPending = true;
    /* invalidate all resampled audio from previous audio formats */
    const _Korl_Resource_Map*const resourcesEnd = context->stbHmResources + hmlen(context->stbHmResources);
    for(_Korl_Resource_Map* resourceMapIt = context->stbHmResources; resourceMapIt < resourcesEnd; resourceMapIt++)
    {
        const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(resourceMapIt->key);
        if(unpackedHandle.multimediaType != _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO)
            continue;
        _Korl_Resource*const resource = &resourceMapIt->value;
        korl_free(context->allocatorHandleTransient, resource->subType.audio.resampledData);
        resource->subType.audio.resampledData = NULL;
    }
}
korl_internal acu8 korl_resource_getAudio(Korl_Resource_Handle handle, Korl_Audio_Format* o_resourceAudioFormat)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    if(!handle || context->audioResamplesPending)
        return KORL_STRUCT_INITIALIZE_ZERO(acu8);// silently return a NULL device memory allocation handle if the resource handle is NULL _or_ if korl-resource has a pending resampling operation
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO);
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    const _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    *o_resourceAudioFormat = context->audioRendererFormat;
    o_resourceAudioFormat->channels = resource->subType.audio.format.channels;
    if(resource->subType.audio.resampledData)
        /* if this audio had to be resampled, we need to use its resampled data */
        return (acu8){.data=resource->subType.audio.resampledData, .size=resource->subType.audio.resampledDataBytes};
    /* otherwise, we can just directly use the audio data */
    return (acu8){.data=resource->data, .size=resource->dataBytes};
}
korl_internal KORL_FUNCTION_korl_resource_texture_getSize(korl_resource_texture_getSize)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    if(!resourceHandleTexture)
        return KORL_MATH_V2U32_ZERO;
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(resourceHandleTexture);
    korl_assert(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS);
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, resourceHandleTexture);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    korl_assert(resource->subType.graphics.type == _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE);
    if(unpackedHandle.type == _KORL_RESOURCE_TYPE_FILE)
        _korl_resource_fileResourceLoadStep(resource, unpackedHandle);
    return (Korl_Math_V2u32){resource->subType.graphics.createInfo.texture.sizeX
                            ,resource->subType.graphics.createInfo.texture.sizeY};
}
korl_internal Korl_Vulkan_ShaderHandle korl_resource_shader_getHandle(Korl_Resource_Handle handleResourceShader)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    if(!handleResourceShader)
        return 0;
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handleResourceShader);
    korl_assert(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS);
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handleResourceShader);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    korl_assert(resource->subType.graphics.type == _KORL_RESOURCE_GRAPHICS_TYPE_SHADER);
    if(unpackedHandle.type == _KORL_RESOURCE_TYPE_FILE)
        _korl_resource_fileResourceLoadStep(resource, unpackedHandle);
    return resource->subType.graphics.deviceHandle.shaderHandle;
}
korl_internal void korl_resource_defragment(Korl_Memory_AllocatorHandle stackAllocator)
{
    if(korl_memory_allocator_isFragmented(_korl_resource_context->allocatorHandleRuntime))
    {
        Korl_Heap_DefragmentPointer* stbDaDefragmentPointers = NULL;
        mcarrsetcap(KORL_STB_DS_MC_CAST(stackAllocator), stbDaDefragmentPointers, 64);
        KORL_MEMORY_STB_DA_DEFRAGMENT(stackAllocator, stbDaDefragmentPointers, _korl_resource_context);
        KORL_MEMORY_STB_DA_DEFRAGMENT_STB_HASHMAP_CHILD(stackAllocator, &stbDaDefragmentPointers, _korl_resource_context->stbHmResources, _korl_resource_context);
        KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocator, stbDaDefragmentPointers, _korl_resource_context->stbDsDirtyResourceHandles, _korl_resource_context);
        korl_stringPool_collectDefragmentPointers(_korl_resource_context->stringPool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_resource_context);
        for(u$ r = 0; r < hmlenu(_korl_resource_context->stbHmResources); r++)// stb_ds says we can iterate over hash maps the same way as dynamic arrays
        {
            _Korl_Resource_Map*const resourceMapItem = &(_korl_resource_context->stbHmResources[r]);
            const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(resourceMapItem->key);
            if(unpackedHandle.type != _KORL_RESOURCE_TYPE_RUNTIME)
                continue;
            KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD(stackAllocator, stbDaDefragmentPointers, resourceMapItem->value.data, _korl_resource_context->stbHmResources);
        }
        korl_memory_allocator_defragment(_korl_resource_context->allocatorHandleRuntime, stbDaDefragmentPointers, arrlenu(stbDaDefragmentPointers), stackAllocator);
    }
    // KORL-ISSUE-000-000-135: resource: defragment transient resource data
}
korl_internal u32 korl_resource_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer)
{
    const u32 byteOffset = korl_checkCast_u$_to_u32((*pByteBuffer)->size);
    korl_memory_byteBuffer_append(pByteBuffer, (acu8){.data = KORL_C_CAST(u8*, &_korl_resource_context), .size = sizeof(_korl_resource_context)});
    return byteOffset;
}
korl_internal void korl_resource_memoryStateRead(const u8* memoryState)
{
    _korl_resource_context = *KORL_C_CAST(_Korl_Resource_Context**, memoryState);
    _Korl_Resource_Context*const context = _korl_resource_context;
    /* we do _not_ store transient data in a memoryState!; all pointers to 
        memory within the transient allocator are now considered invalid, and we 
        need to wipe all transient memory from the current application session */
    korl_memory_allocator_empty(context->allocatorHandleTransient);
    /* go through each Resource & re-create the transcoded multimedia assets, 
        since we should expect that when a memory state is loaded all multimedia 
        device assets are invalidated! */
    for(u$ r = 0; r < hmlenu(context->stbHmResources); r++)// stb_ds says we can iterate over hash maps the same way as dynamic arrays
    {
        _Korl_Resource_Map*const resourceMapItem = &(context->stbHmResources[r]);
        const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(resourceMapItem->key);
        _Korl_Resource*const resource = &resourceMapItem->value;
        /* regardless of whether or not the resource is backed by a file, we 
            must ensure that all AUDIO resources are resampled to match the 
            audio renderer; ergo, we wipe the transient resampledData and raise 
            the pending resample flag */
        if(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO)
        {
            context->audioResamplesPending = true;
            resource->subType.audio.resampledData = NULL;// NOTE: there is no need to call free on this allocation, as the entire transient allocator has been wiped
        }
        switch(unpackedHandle.type)
        {
        case _KORL_RESOURCE_TYPE_FILE:{
            /* maybe just free the data allocation & reset the resource to the 
                unloaded state?  the external device memory allocations should 
                all be invalid at this point, so we should be able to just 
                nullify this struct */
            resourceMapItem->value.data      = NULL;// NOTE: there is no need to call free on this allocation, as the entire transient allocator has been wiped
            resourceMapItem->value.dataBytes = 0;
            switch(unpackedHandle.multimediaType)
            {
            case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
                /* device memory allocations have been invalidated! */
                korl_memory_zero(&resourceMapItem->value.subType.graphics.deviceHandle, sizeof(resourceMapItem->value.subType.graphics.deviceHandle));
                break;}
            case _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO:{
                break;}
            default:
                korl_log(ERROR, "invalid multimedia type %i", unpackedHandle.multimediaType);
                break;
            }
            break;}
        case _KORL_RESOURCE_TYPE_RUNTIME:{
            /* here we can just re-create each device memory allocation & mark 
                each asset as dirty, and they will get updated at the end of the 
                frame, since we already have all the CPU-encoded asset data from 
                the memory state */
            switch(unpackedHandle.multimediaType)
            {
            case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
                switch(resourceMapItem->value.subType.graphics.type)
                {
                case _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE:{
                    korl_log(VERBOSE, "creating IMAGE Resource 0x%llX=>0x%llX", resourceMapItem->key, resourceMapItem->value.subType.graphics.deviceHandle.deviceMemoryAllocationHandle);
                    korl_vulkan_deviceAsset_createTexture(&resourceMapItem->value.subType.graphics.createInfo.texture, resourceMapItem->value.subType.graphics.deviceHandle.deviceMemoryAllocationHandle);
                    break;}
                case _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER:{
                    korl_log(VERBOSE, "creating VERTEX_BUFFER Resource 0x%llX=>0x%llX", resourceMapItem->key, resourceMapItem->value.subType.graphics.deviceHandle.deviceMemoryAllocationHandle);
                    resourceMapItem->value.subType.graphics.createInfo.vertexBuffer.createInfo.vertexAttributeDescriptors = resourceMapItem->value.subType.graphics.createInfo.vertexBuffer.vertexAttributeDescriptors;// refresh the address of the vertex attribute descriptors, since these hash map items are expected to have transient memory locations
                    korl_vulkan_deviceAsset_createVertexBuffer(&resourceMapItem->value.subType.graphics.createInfo.vertexBuffer.createInfo, resourceMapItem->value.subType.graphics.deviceHandle.deviceMemoryAllocationHandle);
                    break;}
                case _KORL_RESOURCE_GRAPHICS_TYPE_SHADER:{
                    korl_log(ERROR, "RUNTIME GRAPHICS SHADER resources are not supported");// KORL currently requires all shaders to be SPIR-V file assets
                    break;}
                default:
                    korl_log(ERROR, "invalid graphics type %i", resourceMapItem->value.subType.graphics.type);
                    break;
                }
                break;}
            default:
                korl_log(ERROR, "invalid multimedia type %i", unpackedHandle.multimediaType);
                break;
            }
            mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbDsDirtyResourceHandles, resourceMapItem->key);
            resourceMapItem->value.dirty = true;
            break;}
        default:
            korl_log(ERROR, "invalid resource type %i", unpackedHandle.type);
            break;
        }
    }
}
korl_internal KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(korl_resource_onAssetHotReload)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    /* check to see if the asset is loaded in our database */
    _Korl_Resource_Graphics_Type         graphicsType   = _KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN;
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_fileNameToUnpackedHandle(rawUtf16AssetName, &graphicsType);
    const Korl_Resource_Handle           handle         = _korl_resource_handle_pack(unpackedHandle);
    if(!handle)
        return;// if we weren't able to derive a valid resource handle from the provided asset name, then it _should_ be safe to say that we don't have to do anything here
    ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbHmResources, handle);
    if(hashMapIndex < 0)
        return;// if the asset isn't found in the resource database, then we don't have to do anything
    /* perform asset hot-reloading logic; clear the cached resource so that the 
        next time it is obtained by the user (via _fromFile) it is re-transcoded */
    _Korl_Resource*const resource = &(context->stbHmResources[hashMapIndex].value);
    _korl_resource_unload(resource, unpackedHandle);
}
#undef _LOCAL_STRING_POOL_POINTER
