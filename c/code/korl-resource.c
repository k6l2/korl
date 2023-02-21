#include "korl-resource.h"
#include "korl-stb-ds.h"
#include "korl-stb-image.h"
#include "korl-stringPool.h"
#include "korl-audio.h"
#include "korl-codec-audio.h"
#define _LOCAL_STRING_POOL_POINTER _korl_resource_context.stringPool
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
} _Korl_Resource_Graphics_Type;
typedef struct _Korl_Resource
{
    // _Korl_Resource_MultimediaType type;// we don't actually need the type here, since type is part of the resource handle, and that is the hash key of the database!
    union
    {
        struct
        {
            _Korl_Resource_Graphics_Type type;
            Korl_Vulkan_DeviceMemory_AllocationHandle deviceMemoryAllocationHandle;
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
    Korl_StringPool_String stringFileName;
    //KORL-FEATURE-000-000-046: resource: right now it is very difficult to trace the owner of a resource; modify API to require FILE:LINE information for Resource allocations
} _Korl_Resource;
typedef struct _Korl_Resource_Map
{
    Korl_Resource_Handle key;
    _Korl_Resource       value;
} _Korl_Resource_Map;
typedef struct _Korl_Resource_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    _Korl_Resource_Map*         stbHmResources;
    Korl_Resource_Handle*       stbDsDirtyResourceHandles;
    u$                          nextUniqueId;// this counter will increment each time we add a _non-file_ resource to the database; file-based resources will have a unique id generated from a hash of the asset file name
    Korl_StringPool*            stringPool;// @korl-string-pool-no-data-segment-storage; used to store the file name strings of file resources, allowing us to hot-reload resources when the underlying korl-asset is hot-reloaded
    Korl_Audio_Format           audioRendererFormat;// the currently configured audio renderer format, likely to be set by korl-sfx; when this is changed via `korl_resource_setAudioFormat`, all audio resources will be resampled to match this format, and that resampled audio data is what will be mixed into korl-audio; we do this to sacrifice memory for speed, as it should be much better performance to not have to worry about audio resampling algorithms at runtime
    bool                        audioResamplesPending;// set when `audioRendererFormat` changes, or we lazy-load a file resource via `korl_resource_fromFile`; once set, each call to `korl_resource_flushUpdates` will incur iteration over all audio resources to ensure that they are all resampled to `audioRendererFormat`'s specifications; only cleared when `korl_resource_flushUpdates` finds that all audio resources are loaded & resampled
} _Korl_Resource_Context;
korl_global_variable _Korl_Resource_Context _korl_resource_context;
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
korl_internal _Korl_Resource_Handle_Unpacked _korl_resource_fileNameToUnpackedHandle(acu16 fileName, _Korl_Resource_Graphics_Type* out_graphicsType)
{
    KORL_ZERO_STACK(_Korl_Resource_Handle_Unpacked, unpackedHandle);
    /* automatically determine the type of multimedia this resource is based on the file extension */
    bool multimediaTypeFound = false;
    _Korl_Resource_Graphics_Type graphicsType = _KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN;
    if(!multimediaTypeFound)
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
                multimediaTypeFound           = true;
                break;
            }
        }
    }
    if(!multimediaTypeFound)
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
                multimediaTypeFound           = true;
                break;
            }
        }
    }
    if(!multimediaTypeFound)
        goto returnUnpackedHandle;
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
    korl_assert(!resource->subType.audio.resampledData);
    if(   resource->subType.audio.format.bytesPerSample == _korl_resource_context.audioRendererFormat.bytesPerSample
       && resource->subType.audio.format.sampleFormat   == _korl_resource_context.audioRendererFormat.sampleFormat
       && resource->subType.audio.format.frameHz        == _korl_resource_context.audioRendererFormat.frameHz)
       return;
    Korl_Audio_Format resampledFormat = _korl_resource_context.audioRendererFormat;
    resampledFormat.channels = resource->subType.audio.format.channels;
    const u$  bytesPerFrame    = resource->subType.audio.format.bytesPerSample * resource->subType.audio.format.channels;
    const u$  frames           = resource->dataBytes / bytesPerFrame;
    const f64 seconds          = KORL_C_CAST(f64, frames) / KORL_C_CAST(f64, resource->subType.audio.format.frameHz);
    const u$  newFrames        = resource->subType.audio.format.frameHz == resampledFormat.frameHz
                                 ? frames
                                 : KORL_C_CAST(u$, seconds * resampledFormat.frameHz);
    const u$  newBytesPerFrame = resampledFormat.bytesPerSample * resampledFormat.channels;
    resource->subType.audio.resampledDataBytes = newFrames * newBytesPerFrame;
    resource->subType.audio.resampledData      = korl_allocate(_korl_resource_context.allocatorHandle, resource->subType.audio.resampledDataBytes);
    korl_codec_audio_resample(&resource->subType.audio.format, resource->data, resource->dataBytes
                             ,&resampledFormat, resource->subType.audio.resampledData, resource->subType.audio.resampledDataBytes);
}
korl_internal void korl_resource_initialize(void)
{
    korl_memory_zero(&_korl_resource_context, sizeof(_korl_resource_context));
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_gigabytes(1);
    _korl_resource_context.allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, L"korl-resource", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    _korl_resource_context.stringPool      = korl_allocate(_korl_resource_context.allocatorHandle, sizeof(*_korl_resource_context.stringPool));
    mchmdefault(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbDsDirtyResourceHandles, 128);
    *_korl_resource_context.stringPool = korl_stringPool_create(_korl_resource_context.allocatorHandle);
}
korl_internal KORL_FUNCTION_korl_resource_fromFile(korl_resource_fromFile)
{
    _Korl_Resource_Graphics_Type         graphicsType   = _KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN;
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_fileNameToUnpackedHandle(fileName, &graphicsType);
    /* we should now have all the info needed to create the packed resource handle */
    const Korl_Resource_Handle handle = _korl_resource_handle_pack(unpackedHandle);
    korl_assert(handle);
    /* check if the resource was already added */
    ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    /* if the resource is new, we need to add it to the database */
    if(hashMapIndex < 0)
    {
        mchmput(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
        hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
        korl_assert(hashMapIndex >= 0);
        _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
        resource->stringFileName = string_newAcu16(fileName);
    }
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    /* regardless of whether or not the resource was just added, we still need to make sure that the asset was loaded for it */
    if(!resource->data)
    {
        KORL_ZERO_STACK(Korl_AssetCache_AssetData, assetData);
        const Korl_AssetCache_Get_Result assetCacheGetResult = korl_assetCache_get(fileName.data, assetCacheGetFlags, &assetData);
        if(assetCacheGetFlags == KORL_ASSETCACHE_GET_FLAGS_NONE)
            korl_assert(assetCacheGetResult == KORL_ASSETCACHE_GET_RESULT_LOADED);
        /* if the asset for this resource was just loaded, create the multimedia resource now */
        if(assetCacheGetResult == KORL_ASSETCACHE_GET_RESULT_LOADED)
        {
            switch(unpackedHandle.multimediaType)
            {
            case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
                resource->subType.graphics.type = graphicsType;
                switch(resource->subType.graphics.type)
                {
                case _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE:{
                    int imageSizeX = 0, imageSizeY = 0, imageChannels = 0;
                    ///KORL-PERFORMANCE-000-000-032: resource: stbi API unfortunately doesn't seem to have the ability for the user to provide their own allocator, so we need an extra alloc/copy/free here
                    {
                        stbi_uc*const stbiPixels = stbi_load_from_memory(assetData.data, assetData.dataBytes, &imageSizeX, &imageSizeY, &imageChannels, STBI_rgb_alpha);
                        korl_assert(stbiPixels);
                        const u$ pixelBytes = imageSizeX*imageSizeY*imageChannels;
                        resource->data      = korl_allocate(_korl_resource_context.allocatorHandle, pixelBytes);
                        resource->dataBytes = pixelBytes;
                        korl_memory_copy(resource->data, stbiPixels, pixelBytes);
                        stbi_image_free(stbiPixels);
                    }
                    resource->subType.graphics.createInfo.texture.sizeX     = korl_checkCast_i$_to_u32(imageSizeX);
                    resource->subType.graphics.createInfo.texture.sizeY     = korl_checkCast_i$_to_u32(imageSizeY);
                    resource->subType.graphics.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createTexture(&resource->subType.graphics.createInfo.texture, 0/*0 => generate new handle*/);
                    korl_vulkan_texture_update(resource->subType.graphics.deviceMemoryAllocationHandle, resource->data);
                    break;}
                default:
                    korl_log(ERROR, "invalid graphics type %i", resource->subType.graphics.type);
                    break;
                }
                break;}
            case _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO:{
                resource->data = korl_codec_audio_decode(assetData.data, assetData.dataBytes, _korl_resource_context.allocatorHandle, &resource->dataBytes, &resource->subType.audio.format);
                korl_assert(resource->data);
                break;}
            default:{
                korl_log(ERROR, "invalid multimedia type: %i", unpackedHandle.multimediaType);
                break;}
            }
        }
        if(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO)
            _korl_resource_context.audioResamplesPending = true;
    }
    /**/
    return handle;
}
korl_internal Korl_Resource_Handle korl_resource_createVertexBuffer(const Korl_Vulkan_CreateInfoVertexBuffer* createInfo)
{
    korl_assert(_korl_resource_context.nextUniqueId <= _KORL_RESOURCE_UNIQUE_ID_MAX);// assuming this ever fires under normal circumstances (_highly_ unlikely), we will need to implement a system to recycle old unused UIDs or something
    /* construct the new resource handle */
    KORL_ZERO_STACK(_Korl_Resource_Handle_Unpacked, unpackedHandle);
    unpackedHandle.type           = _KORL_RESOURCE_TYPE_RUNTIME;
    unpackedHandle.multimediaType = _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS;
    unpackedHandle.uniqueId       = _korl_resource_context.nextUniqueId++;
    const Korl_Resource_Handle handle = _korl_resource_handle_pack(unpackedHandle);
    /* add the new resource to the database */
    ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex < 0);
    mchmput(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
    hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    resource->subType.graphics.type = _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER;
    /* allocate the memory for the raw decoded asset data */
    resource->dataBytes = createInfo->bytes;
    resource->data      = korl_allocate(_korl_resource_context.allocatorHandle, createInfo->bytes);
    korl_assert(resource->data);
    /* create the multimedia asset */
    resource->subType.graphics.deviceMemoryAllocationHandle       = korl_vulkan_deviceAsset_createVertexBuffer(createInfo, 0/*0 => generate new handle*/);
    resource->subType.graphics.createInfo.vertexBuffer.createInfo = *createInfo;
    // we have to perform a deep-copy of the vertex buffer create info struct, since the vertex attribute descriptors is stored in the create info as a pointer
    korl_assert(createInfo->vertexAttributeDescriptorCount <= korl_arraySize(resource->subType.graphics.createInfo.vertexBuffer.vertexAttributeDescriptors));
    for(u$ v = 0; v < createInfo->vertexAttributeDescriptorCount; v++)
        resource->subType.graphics.createInfo.vertexBuffer.vertexAttributeDescriptors[v] = createInfo->vertexAttributeDescriptors[v];
    return handle;
}
korl_internal Korl_Resource_Handle korl_resource_createTexture(const Korl_Vulkan_CreateInfoTexture* createInfo)
{
    korl_assert(_korl_resource_context.nextUniqueId <= _KORL_RESOURCE_UNIQUE_ID_MAX);// assuming this ever fires under normal circumstances (_highly_ unlikely), we will need to implement a system to recycle old unused UIDs or something
    /* construct the new resource handle */
    KORL_ZERO_STACK(_Korl_Resource_Handle_Unpacked, unpackedHandle);
    unpackedHandle.type           = _KORL_RESOURCE_TYPE_RUNTIME;
    unpackedHandle.multimediaType = _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS;
    unpackedHandle.uniqueId       = _korl_resource_context.nextUniqueId++;
    const Korl_Resource_Handle handle = _korl_resource_handle_pack(unpackedHandle);
    /* add the new resource to the database */
    ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex < 0);
    mchmput(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
    hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    resource->subType.graphics.type = _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE;
    /* allocate the memory for the raw decoded asset data */
    resource->dataBytes = createInfo->sizeX * createInfo->sizeY * sizeof(Korl_Vulkan_Color4u8);
    resource->data      = korl_allocate(_korl_resource_context.allocatorHandle, resource->dataBytes);
    korl_assert(resource->data);
    /* create the multimedia asset */
    resource->subType.graphics.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createTexture(createInfo, 0/*0 => generate new handle*/);
    resource->subType.graphics.createInfo.texture           = *createInfo;
    return handle;
}
korl_internal void korl_resource_destroy(Korl_Resource_Handle handle)
{
    if(!handle)
        return;// silently do nothing for NULL handles
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    const _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    /* destroy the transcoded multimedia asset */
    switch(unpackedHandle.multimediaType)
    {
    case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
        korl_vulkan_deviceAsset_destroy(resource->subType.graphics.deviceMemoryAllocationHandle);
        break;}
    case _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO:{
        korl_free(_korl_resource_context.allocatorHandle, resource->subType.audio.resampledData);
        break;}
    default:{
        korl_log(ERROR, "invalid multimedia type %i", unpackedHandle.multimediaType);
        break;}
    }
    /* if the resource backed by a file, we should destroy the cached file name string */
    string_free(resource->stringFileName);
    /* destroy the cached decoded raw asset data */
    korl_free(_korl_resource_context.allocatorHandle, resource->data);
    /* remove the resource from the database */
    mchmdel(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
}
korl_internal void korl_resource_update(Korl_Resource_Handle handle, const void* sourceData, u$ sourceDataBytes, u$ destinationByteOffset)
{
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.type == _KORL_RESOURCE_TYPE_RUNTIME);
    korl_assert(destinationByteOffset + sourceDataBytes <= resource->dataBytes);
    korl_memory_copy(KORL_C_CAST(u8*, resource->data) + destinationByteOffset, sourceData, sourceDataBytes);
    if(!resource->dirty)
    {
        mcarrpush(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbDsDirtyResourceHandles, handle);
        resource->dirty = true;
    }
}
korl_internal u$ korl_resource_getByteSize(Korl_Resource_Handle handle)
{
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    return resource->dataBytes;
}
korl_internal void korl_resource_resize(Korl_Resource_Handle handle, u$ newByteSize)
{
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
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
            korl_vulkan_vertexBuffer_resize(&resource->subType.graphics.deviceMemoryAllocationHandle, newByteSize);
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
    resource->data      = korl_reallocate(_korl_resource_context.allocatorHandle, resource->data, newByteSize);
    resource->dataBytes = newByteSize;
    korl_assert(resource->data);
}
korl_internal void korl_resource_shift(Korl_Resource_Handle handle, i$ byteShiftCount)
{
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
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
        mcarrpush(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbDsDirtyResourceHandles, handle);
        resource->dirty = true;
    }
}
korl_internal void korl_resource_flushUpdates(void)
{
    const Korl_Resource_Handle*const dirtyHandlesEnd = _korl_resource_context.stbDsDirtyResourceHandles + arrlen(_korl_resource_context.stbDsDirtyResourceHandles);
    for(const Korl_Resource_Handle* dirtyHandle = _korl_resource_context.stbDsDirtyResourceHandles; dirtyHandle < dirtyHandlesEnd; dirtyHandle++)
    {
        const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, *dirtyHandle);
        if(hashMapIndex < 0)
        {
            // @TODO: investigate why this is happening when we load a memory state
            korl_log(WARNING, "updated resource handle invalid (update + delete in same frame, etc.): 0x%X", *dirtyHandle);
            continue;
        }
        _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
        korl_assert(resource->dirty);
        const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(*dirtyHandle);
        korl_assert(unpackedHandle.type == _KORL_RESOURCE_TYPE_RUNTIME);
        switch(unpackedHandle.multimediaType)
        {
        case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
            switch(resource->subType.graphics.type)
            {
            case _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE:{
                korl_vulkan_texture_update(resource->subType.graphics.deviceMemoryAllocationHandle, resource->data);
                break;}
            case _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER:{
                korl_vulkan_vertexBuffer_update(resource->subType.graphics.deviceMemoryAllocationHandle, resource->data, resource->dataBytes, 0);
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
    mcarrsetlen(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbDsDirtyResourceHandles, 0);
    /* once per frame, if we have any audio resources that are pending generation 
        of cached resampled audio data to match the audio renderer's format, we 
        attempt to resample as many of these as possible; once all audio 
        resources have been resampled, there is no need to do this each frame 
        anymore, so the flag is cleared */
    if(   !korl_memory_isNull(&_korl_resource_context.audioRendererFormat, sizeof(_korl_resource_context.audioRendererFormat))
       && _korl_resource_context.audioResamplesPending)
    {
        u$ resampledResources    = 0;
        u$ pendingAudioResamples = 0;
        const _Korl_Resource_Map*const resourcesEnd = _korl_resource_context.stbHmResources + hmlen(_korl_resource_context.stbHmResources);
        for(_Korl_Resource_Map* resourceMapIt = _korl_resource_context.stbHmResources; resourceMapIt < resourcesEnd; resourceMapIt++)
        {
            const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(resourceMapIt->key);
            if(unpackedHandle.type != _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO)
                continue;
            _Korl_Resource*const resource = &resourceMapIt->value;
            if(!resource->data)
            {
                if(resource->stringFileName.handle)
                    /* if this is a file-backed resource, attempt to get the lazy-loaded file one more time: */
                    korl_resource_fromFile(string_getRawAcu16(resource->stringFileName), KORL_ASSETCACHE_GET_FLAG_LAZY);
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
        _korl_resource_context.audioResamplesPending = pendingAudioResamples > 0;
        if(pendingAudioResamples)
            korl_log(VERBOSE, "pendingAudioResamples=%llu", pendingAudioResamples);
        korl_log(VERBOSE, "resampledResources=%llu", resampledResources);
    }
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_resource_getVulkanDeviceMemoryAllocationHandle(Korl_Resource_Handle handle)
{
    if(!handle)
        return 0;// silently return a NULL device memory allocation handle if the resource handle is NULL
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS);
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    const _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    return resource->subType.graphics.deviceMemoryAllocationHandle;
}
korl_internal void korl_resource_setAudioFormat(const Korl_Audio_Format* audioFormat)
{
    if(0 == korl_memory_compare(&_korl_resource_context.audioRendererFormat, audioFormat, sizeof(*audioFormat)))
        return;
    _korl_resource_context.audioRendererFormat   = *audioFormat;
    _korl_resource_context.audioResamplesPending = true;
    /* invalidate all resampled audio from previous audio formats */
    const _Korl_Resource_Map*const resourcesEnd = _korl_resource_context.stbHmResources + hmlen(_korl_resource_context.stbHmResources);
    for(_Korl_Resource_Map* resourceMapIt = _korl_resource_context.stbHmResources; resourceMapIt < resourcesEnd; resourceMapIt++)
    {
        const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(resourceMapIt->key);
        if(unpackedHandle.multimediaType != _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO)
            continue;
        _Korl_Resource*const resource = &resourceMapIt->value;
        korl_free(_korl_resource_context.allocatorHandle, resource->subType.audio.resampledData);
        resource->subType.audio.resampledData = NULL;
    }
}
korl_internal acu8 korl_resource_getAudio(Korl_Resource_Handle handle, Korl_Audio_Format* o_resourceAudioFormat)
{
    if(!handle || _korl_resource_context.audioResamplesPending)
        return KORL_STRUCT_INITIALIZE_ZERO(acu8);// silently return a NULL device memory allocation handle if the resource handle is NULL _or_ if korl-resource has a pending resampling operation
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_AUDIO);
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    const _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    *o_resourceAudioFormat = _korl_resource_context.audioRendererFormat;
    o_resourceAudioFormat->channels = resource->subType.audio.format.channels;
    if(resource->subType.audio.resampledData)
        /* if this audio had to be resampled, we need to use its resampled data */
        return (acu8){.data=resource->subType.audio.resampledData, .size=resource->subType.audio.resampledDataBytes};
    /* otherwise, we can just directly use the audio data */
    return (acu8){.data=resource->data, .size=resource->dataBytes};
}
korl_internal void korl_resource_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer)
{
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_resource_context, sizeof(_korl_resource_context));
}
korl_internal bool korl_resource_saveStateRead(HANDLE hFile)
{
    //KORL-ISSUE-000-000-081: savestate: weak/bad assumption; we currently rely on the fact that korl memory allocator handles remain the same between sessions
    if(!ReadFile(hFile, &_korl_resource_context, sizeof(_korl_resource_context), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    /* go through each Resource & re-create the transcoded multimedia assets, 
        since we should expect that when a memory state is loaded all multimedia 
        device assets are invalidated! */
    for(u$ r = 0; r < hmlenu(_korl_resource_context.stbHmResources); r++)// stb_ds says we can iterate over hash maps the same way as dynamic arrays
    {
        _Korl_Resource_Map*const resourceMapItem = &(_korl_resource_context.stbHmResources[r]);
        const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(resourceMapItem->key);
        switch(unpackedHandle.type)
        {
        case _KORL_RESOURCE_TYPE_FILE:{
            /* maybe just free the data allocation & reset the resource to the 
                unloaded state?  the external device memory allocations should 
                all be invalid at this point, so we should be able to just 
                nullify this struct */
            korl_free(_korl_resource_context.allocatorHandle, resourceMapItem->value.data);// ASSUMPTION: this function is run _after_ the memory state allocators/allocations are loaded!
            resourceMapItem->value.data      = NULL;
            resourceMapItem->value.dataBytes = 0;
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
                    korl_vulkan_deviceAsset_createTexture(&resourceMapItem->value.subType.graphics.createInfo.texture, resourceMapItem->value.subType.graphics.deviceMemoryAllocationHandle);
                    break;}
                case _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER:{
                    resourceMapItem->value.subType.graphics.createInfo.vertexBuffer.createInfo.vertexAttributeDescriptors = resourceMapItem->value.subType.graphics.createInfo.vertexBuffer.vertexAttributeDescriptors;// refresh the address of the vertex attribute descriptors, since these hash map items are expected to have transient memory locations
                    korl_vulkan_deviceAsset_createVertexBuffer(&resourceMapItem->value.subType.graphics.createInfo.vertexBuffer.createInfo, resourceMapItem->value.subType.graphics.deviceMemoryAllocationHandle);
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
            mcarrpush(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbDsDirtyResourceHandles, resourceMapItem->key);
            resourceMapItem->value.dirty = true;
            break;}
        default:
            korl_log(ERROR, "invalid resource type %i", unpackedHandle.type);
            break;
        }
    }
    return true;
}
korl_internal KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(korl_resource_onAssetHotReload)
{
    /* check to see if the asset is loaded in our database */
    _Korl_Resource_Graphics_Type graphicsType           = _KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN;
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_fileNameToUnpackedHandle(rawUtf16AssetName, &graphicsType);
    const Korl_Resource_Handle handle                   = _korl_resource_handle_pack(unpackedHandle);
    if(!handle)
        return;// if we weren't able to derive a valid resource handle from the provided asset name, then it _should_ be safe to say that we don't have to do anything here
    ptrdiff_t hashMapIndex = mchmgeti(KORL_STB_DS_MC_CAST(_korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    if(hashMapIndex < 0)
        return;// if the asset isn't found in the resource database, then we don't have to do anything
    /* perform asset hot-reloading logic; clear the cached resource so that the 
        next time it is obtained by the user (via _fromFile) it is re-transcoded */
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    korl_free(_korl_resource_context.allocatorHandle, resource->data);// ASSUMPTION: this function is run _after_ the memory state allocators/allocations are loaded!
    resource->data      = NULL;
    resource->dataBytes = 0;
}
#undef _LOCAL_STRING_POOL_POINTER
