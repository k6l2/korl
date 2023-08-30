#include "korl-resource.h"
#include "korl-stb-ds.h"
#include "korl-stb-image.h"
#include "korl-audio.h"
#include "korl-codec-audio.h"
#include "korl-codec-glb.h"
#include "korl-interface-platform.h"
#include "utility/korl-stringPool.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-utility-gfx.h"
#include "utility/korl-pool.h"
#include "korl-resource-shader.h"
#include "korl-resource-gfx-buffer.h"
#include "korl-resource-texture.h"
#include "korl-resource-font.h"
#include "korl-resource-scene3d.h"
#define _LOCAL_STRING_POOL_POINTER (_korl_resource_context->stringPool)
#if 0//@TODO: delete/recycle
typedef struct _Korl_Resource
{
    // _Korl_Resource_MultimediaType type;// we don't actually need the type here, since type is part of the resource handle, and that is the hash key of the database!
    union
    {
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
#endif
typedef struct _Korl_Resource_Descriptor
{
    Korl_StringPool_String                     name;
    Korl_Resource_DescriptorManifest_Callbacks callbacks;
} _Korl_Resource_Descriptor;
typedef enum _Korl_Resource_Item_BackingType
    {_KORL_RESOURCE_ITEM_BACKING_TYPE_ASSET_CACHE
    ,_KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA
} _Korl_Resource_Item_BackingType;
typedef struct _Korl_Resource_Item
{
    u16                             descriptorIndex;
    void*                           descriptorStruct;// stored in runtime memory; this is where transcoded data references are stored, such as Vulkan_DeviceMemory_AllocationHandles, etc.; this data is opaque to korl-resource as the memory requirements for each resource descriptor struct will vary
    _Korl_Resource_Item_BackingType backingType;
    union
    {
        struct
        {
            Korl_StringPool_String    fileName;
            Korl_AssetCache_Get_Flags assetCacheGetFlags;
            bool                      isTranscoded;
        } assetCache;
        struct
        {
            /** NOTE: although we _could_ potentially just use graphics update buffers exclusively 
             * for graphics resources, we still do _need_ to store the resource in CPU memory since 
             * we need a way to fully restore the resource at any time from, for example, a 
             * memoryState load operation */
            void* data;
            bool  transcodingIsUpdated;
            bool  isTransient;// true if the runtime resource was created with the `transient` parameter set to true
            bool  transientDestroy;// raised when we reach a condition where transient runtime resources should be destroyed, such as after loading a korl-memoryState
        } runtime;
    } backingSubType;
} _Korl_Resource_Item;
typedef struct _Korl_Resource_Map
{
    const char*          key;// raw UTF-8 asset file name
    Korl_Resource_Handle value;
} _Korl_Resource_Map;
typedef struct _Korl_Resource_Context
{
    Korl_Memory_AllocatorHandle allocatorHandleRuntime;// all unique data that cannot be easily reobtained/reconstructed from a korl-memoryState is stored here, including this struct itself
    Korl_Memory_AllocatorHandle allocatorHandleTransient;// all cached data that can be retranscoded/reobtained is stored here, such as korl-asset transcodings or audio.resampledData; we do _not_ need to copy this data to korl-memoryState in order for that functionality to work, so we wont!
    _Korl_Resource_Descriptor*  stbDaDescriptors;
    Korl_StringPool*            stringPool;// @korl-string-pool-no-data-segment-storage; used to store: Descriptor names, callback API names
    Korl_Pool                   resourcePool;// stored in runtime memory; pool of _Korl_Resource_Item
    _Korl_Resource_Map*         stbShFileResources;// stored in runtime memory; acceleration data structure allowing the user to more efficiently obtain the same file-backed Resource
    #if 0//@TODO: delete/recycle
    Korl_Audio_Format           audioRendererFormat;// the currently configured audio renderer format, likely to be set by korl-sfx; when this is changed via `korl_resource_setAudioFormat`, all audio resources will be resampled to match this format, and that resampled audio data is what will be mixed into korl-audio; we do this to sacrifice memory for speed, as it should be much better performance to not have to worry about audio resampling algorithms at runtime
    bool                        audioResamplesPending;// set when `audioRendererFormat` changes, or we lazy-load a file resource via `korl_resource_fromFile`; once set, each call to `korl_resource_flushUpdates` will incur iteration over all audio resources to ensure that they are all resampled to `audioRendererFormat`'s specifications; only cleared when `korl_resource_flushUpdates` finds that all audio resources are loaded & resampled
    #endif
} _Korl_Resource_Context;
korl_global_variable _Korl_Resource_Context* _korl_resource_context;
#if 0//@TODO: delete/recycle
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
#endif
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
    mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbDaDescriptors, 16);
    korl_pool_initialize(&context->resourcePool, context->allocatorHandleRuntime, sizeof(_Korl_Resource_Item), 256);
    mcsh_new_arena(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbShFileResources);
    /* register KORL built-in Resource Descriptors */
    korl_resource_shader_register();
    korl_resource_gfxBuffer_register();
    korl_resource_texture_register();
    korl_resource_font_register();
    korl_resource_scene3d_register();
}
korl_internal KORL_POOL_CALLBACK_FOR_EACH(_korl_resource_transcodeFileAssets_forEach)
{
    _Korl_Resource_Context*const context      = _korl_resource_context;
    _Korl_Resource_Item*const    resourceItem = KORL_C_CAST(_Korl_Resource_Item*, item);
    if(resourceItem->backingType != _KORL_RESOURCE_ITEM_BACKING_TYPE_ASSET_CACHE)
        return KORL_POOL_FOR_EACH_CONTINUE;// this is not a file-asset-backed resource
    if(resourceItem->backingSubType.assetCache.isTranscoded)
        return KORL_POOL_FOR_EACH_CONTINUE;// resource is already transcoded; move along
    /* attempt to get the raw file data from korl-assetCache */
    KORL_ZERO_STACK(Korl_AssetCache_AssetData, assetData);
    const Korl_AssetCache_Get_Result assetCacheGetResult = korl_assetCache_get(string_getRawUtf16(&resourceItem->backingSubType.assetCache.fileName), resourceItem->backingSubType.assetCache.assetCacheGetFlags, &assetData);
    if(resourceItem->backingSubType.assetCache.assetCacheGetFlags == KORL_ASSETCACHE_GET_FLAGS_NONE)
        korl_assert(assetCacheGetResult == KORL_ASSETCACHE_GET_RESULT_LOADED);
    /* if we have the assetCache data, we can do the transcoding */
    if(assetCacheGetResult == KORL_ASSETCACHE_GET_RESULT_LOADED)
    {
        korl_assert(resourceItem->descriptorIndex < arrlenu(context->stbDaDescriptors));
        const _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
        korl_assert(descriptor->callbacks.transcode);// all file-asset-backed resources _must_ use a descriptor that has a `transcode` callback
        fnSig_korl_resource_descriptorCallback_transcode*const transcode = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_transcode*, korl_functionDynamo_get(descriptor->callbacks.transcode));
        transcode(resourceItem->descriptorStruct, assetData.data, assetData.dataBytes, context->allocatorHandleTransient);
        resourceItem->backingSubType.assetCache.isTranscoded = true;
    }
    return KORL_POOL_FOR_EACH_CONTINUE;
}
korl_internal void korl_resource_transcodeFileAssets(void)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    korl_pool_forEach(&context->resourcePool, _korl_resource_transcodeFileAssets_forEach, NULL);
}
korl_internal KORL_FUNCTION_korl_resource_getDescriptorStruct(korl_resource_getDescriptorStruct)
{
    _Korl_Resource_Context*const context      = _korl_resource_context;
    _Korl_Resource_Item*const    resourceItem = korl_pool_get(&context->resourcePool, &handle);
    korl_assert(resourceItem);
    return resourceItem->descriptorStruct;
}
korl_internal KORL_FUNCTION_korl_resource_descriptor_add(korl_resource_descriptor_add)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    /* ensure descriptor name is unique */
    {
        const _Korl_Resource_Descriptor*const descriptorsEnd = context->stbDaDescriptors + arrlen(context->stbDaDescriptors);
        for(_Korl_Resource_Descriptor* descriptor = context->stbDaDescriptors; descriptor < descriptorsEnd; descriptor++)
            if(string_equalsAcu8(&descriptor->name, descriptorManifest->utf8DescriptorName))
            {
                korl_log(ERROR, "descriptor name \"%.*hs\" already exists", descriptorManifest->utf8DescriptorName.size, descriptorManifest->utf8DescriptorName.data);
                return;
            }
    }
    /* add descriptor to the database */
    KORL_ZERO_STACK(_Korl_Resource_Descriptor, newDescriptor);
    newDescriptor.name      = string_newAcu8(descriptorManifest->utf8DescriptorName);
    newDescriptor.callbacks = descriptorManifest->callbacks;
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbDaDescriptors, newDescriptor);
    korl_log(INFO, "resource descriptor \"%.*hs\" added", descriptorManifest->utf8DescriptorName.size, descriptorManifest->utf8DescriptorName.data);
}
korl_internal KORL_FUNCTION_korl_resource_fromFile(korl_resource_fromFile)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    /* find the resource descriptor index whose name matches the descriptor name param; the index of the descriptor will be the Resource's "type" */
    const _Korl_Resource_Descriptor*const descriptorsEnd = context->stbDaDescriptors + arrlen(context->stbDaDescriptors);
    _Korl_Resource_Descriptor* descriptor = context->stbDaDescriptors;
    for(; descriptor < descriptorsEnd; descriptor++)
        if(string_equalsAcu8(&descriptor->name, utf8DescriptorName))
            break;
    if(descriptor >= descriptorsEnd)
    {
        korl_log(ERROR, "resource descriptor \"%.*hs\" not found", utf8DescriptorName.size, utf8DescriptorName.data);
        return 0;
    }
    const u16 resourceType = korl_checkCast_i$_to_u16(descriptor - context->stbDaDescriptors);
    /* if the file name is new, we need to add it to the database */
    ptrdiff_t hashMapIndex = mcshgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbShFileResources, utf8FileName.data);
    if(hashMapIndex < 0)
    {
        fnSig_korl_resource_descriptorCallback_descriptorStructCreate*const descriptorStructCreate = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_descriptorStructCreate*, korl_functionDynamo_get(descriptor->callbacks.descriptorStructCreate));
        _Korl_Resource_Item* newResource;
        const Korl_Resource_Handle newResourceHandle = korl_pool_add(&context->resourcePool, resourceType, &newResource);
        newResource->descriptorIndex                              = resourceType;
        newResource->descriptorStruct                             = descriptorStructCreate(context->allocatorHandleRuntime);
        newResource->backingType                                  = _KORL_RESOURCE_ITEM_BACKING_TYPE_ASSET_CACHE;
        newResource->backingSubType.assetCache.fileName           = string_newAcu8(utf8FileName);
        newResource->backingSubType.assetCache.assetCacheGetFlags = assetCacheGetFlags;
        mcshput(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbShFileResources, utf8FileName.data, newResourceHandle);
        hashMapIndex = mcshgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbShFileResources, utf8FileName.data);
    }
    _Korl_Resource_Map*const resourceMapItem = context->stbShFileResources + hashMapIndex;
    return resourceMapItem->value;
}
korl_internal KORL_FUNCTION_korl_resource_create(korl_resource_create)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    /* find the resource descriptor index whose name matches the descriptor name param; the index of the descriptor will be the Resource's "type" */
    const _Korl_Resource_Descriptor*const descriptorsEnd = context->stbDaDescriptors + arrlen(context->stbDaDescriptors);
    _Korl_Resource_Descriptor* descriptor = context->stbDaDescriptors;
    for(; descriptor < descriptorsEnd; descriptor++)
        if(string_equalsAcu8(&descriptor->name, utf8DescriptorName))
            break;
    if(descriptor >= descriptorsEnd)
    {
        korl_log(ERROR, "resource descriptor \"%.*hs\" not found", utf8DescriptorName.size, utf8DescriptorName.data);
        return 0;
    }
    const u16 resourceType = korl_checkCast_i$_to_u16(descriptor - context->stbDaDescriptors);
    fnSig_korl_resource_descriptorCallback_descriptorStructCreate*const descriptorStructCreate = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_descriptorStructCreate*, korl_functionDynamo_get(descriptor->callbacks.descriptorStructCreate));
    fnSig_korl_resource_descriptorCallback_createRuntimeData*const      createRuntimeData      = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_createRuntimeData*     , korl_functionDynamo_get(descriptor->callbacks.createRuntimeData));
    fnSig_korl_resource_descriptorCallback_createRuntimeMedia*const     createRuntimeMedia     = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_createRuntimeMedia*    , korl_functionDynamo_get(descriptor->callbacks.createRuntimeMedia));
    /* add a new resource item & run the create function with the provided create info */
    _Korl_Resource_Item* newResource;
    const Korl_Resource_Handle newResourceHandle = korl_pool_add(&context->resourcePool, resourceType, &newResource);
    newResource->descriptorIndex                    = resourceType;
    newResource->descriptorStruct                   = descriptorStructCreate(context->allocatorHandleRuntime);
    newResource->backingType                        = _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA;
    newResource->backingSubType.runtime.isTransient = transient;
    createRuntimeData(newResource->descriptorStruct, descriptorCreateInfo, context->allocatorHandleRuntime, &newResource->backingSubType.runtime.data);
    createRuntimeMedia(newResource->descriptorStruct);
    return newResourceHandle;
}
korl_internal KORL_FUNCTION_korl_resource_destroy(korl_resource_destroy)
{
    _Korl_Resource_Context*const context      = _korl_resource_context;
    _Korl_Resource_Item*const    resourceItem = korl_pool_get(&context->resourcePool, &resourceHandle);
    korl_assert(resourceItem);
    korl_assert(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA);
    const _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
    fnSig_korl_resource_descriptorCallback_unload*const                  unload                  = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_unload*                 , korl_functionDynamo_get(descriptor->callbacks.unload));
    fnSig_korl_resource_descriptorCallback_descriptorStructDestroy*const descriptorStructDestroy = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_descriptorStructDestroy*, korl_functionDynamo_get(descriptor->callbacks.descriptorStructDestroy));
    unload(resourceItem->descriptorStruct, context->allocatorHandleTransient);
    descriptorStructDestroy(resourceItem->descriptorStruct, context->allocatorHandleRuntime);
    korl_free(context->allocatorHandleRuntime, resourceItem->backingSubType.runtime.data);
    korl_pool_remove(&context->resourcePool, &resourceHandle);
}
korl_internal KORL_FUNCTION_korl_resource_update(korl_resource_update)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    _Korl_Resource_Item*const resourceItem = korl_pool_get(&context->resourcePool, &handle);
    korl_assert(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA);
    _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
    fnSig_korl_resource_descriptorCallback_runtimeBytes*const runtimeBytes = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_runtimeBytes*, korl_functionDynamo_get(descriptor->callbacks.runtimeBytes));
    const u$ resourceBytes = runtimeBytes(resourceItem->descriptorStruct);
    korl_assert(destinationByteOffset + sourceDataBytes <= resourceBytes);
    korl_memory_copy(KORL_C_CAST(u8*, resourceItem->backingSubType.runtime.data) + destinationByteOffset, sourceData, sourceDataBytes);
    resourceItem->backingSubType.runtime.transcodingIsUpdated = false;
}
korl_internal KORL_FUNCTION_korl_resource_getUpdateBuffer(korl_resource_getUpdateBuffer)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    _Korl_Resource_Item*const resourceItem = korl_pool_get(&context->resourcePool, &handle);
    korl_assert(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA);
    _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
    fnSig_korl_resource_descriptorCallback_runtimeBytes*const runtimeBytes = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_runtimeBytes*, korl_functionDynamo_get(descriptor->callbacks.runtimeBytes));
    const u$ resourceBytes = runtimeBytes(resourceItem->descriptorStruct);
    korl_assert(byteOffset < resourceBytes);
    *io_bytesRequested_bytesAvailable = KORL_MATH_MIN(resourceBytes - byteOffset, *io_bytesRequested_bytesAvailable);
    resourceItem->backingSubType.runtime.transcodingIsUpdated = false;
    return KORL_C_CAST(u8*, resourceItem->backingSubType.runtime.data) + byteOffset;
}
korl_internal KORL_FUNCTION_korl_resource_getByteSize(korl_resource_getByteSize)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    _Korl_Resource_Item*const resourceItem = korl_pool_get(&context->resourcePool, &handle);
    korl_assert(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA);
    _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
    fnSig_korl_resource_descriptorCallback_runtimeBytes*const runtimeBytes = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_runtimeBytes*, korl_functionDynamo_get(descriptor->callbacks.runtimeBytes));
    return runtimeBytes(resourceItem->descriptorStruct);
}
korl_internal KORL_FUNCTION_korl_resource_isLoaded(korl_resource_isLoaded)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    _Korl_Resource_Item*const resourceItem = korl_pool_get(&context->resourcePool, &handle);
    if(!resourceItem)
        return false;
    switch(resourceItem->backingType)
    {
    case _KORL_RESOURCE_ITEM_BACKING_TYPE_ASSET_CACHE:
        return resourceItem->backingSubType.assetCache.isTranscoded;
    case _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA:
        return resourceItem->backingSubType.runtime.transcodingIsUpdated;
    }
    korl_log(ERROR, "invalid resource backingType: %i", resourceItem->backingType);
    return false;
}
korl_internal KORL_FUNCTION_korl_resource_resize(korl_resource_resize)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    _Korl_Resource_Item*const resourceItem = korl_pool_get(&context->resourcePool, &handle);
    korl_assert(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA);
    _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
    fnSig_korl_resource_descriptorCallback_runtimeBytes*const runtimeBytes = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_runtimeBytes*, korl_functionDynamo_get(descriptor->callbacks.runtimeBytes));
    if(runtimeBytes(resourceItem->descriptorStruct) == newByteSize)
        return;// silently do nothing if we're not actually resizing
    korl_assert(newByteSize);// for now, ensure that the user is requesting a non-zero # of bytes
    fnSig_korl_resource_descriptorCallback_runtimeResize*const runtimeResize = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_runtimeResize*, korl_functionDynamo_get(descriptor->callbacks.runtimeResize));
    runtimeResize(resourceItem->descriptorStruct, newByteSize, context->allocatorHandleRuntime, &resourceItem->backingSubType.runtime.data);
}
korl_internal KORL_FUNCTION_korl_resource_shift(korl_resource_shift)
{
    if(byteShiftCount == 0)
        return;// silently do nothing else if the user doesn't provide a non-zero byte shift amount
    _Korl_Resource_Context*const context = _korl_resource_context;
    _Korl_Resource_Item*const resourceItem = korl_pool_get(&context->resourcePool, &handle);
    korl_assert(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA);
    _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
    fnSig_korl_resource_descriptorCallback_runtimeBytes*const runtimeBytes = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_runtimeBytes*, korl_functionDynamo_get(descriptor->callbacks.runtimeBytes));
    const u$ resourceBytes = runtimeBytes(resourceItem->descriptorStruct);
    if(byteShiftCount < 0)
    {
        const u$ remainingBytes = korl_checkCast_u$_to_i$(resourceBytes) >= -byteShiftCount 
                                ? resourceBytes + byteShiftCount 
                                : 0;
        if(remainingBytes)
            korl_memory_move(resourceItem->backingSubType.runtime.data, KORL_C_CAST(u8*, resourceItem->backingSubType.runtime.data) + -byteShiftCount, remainingBytes);
        korl_memory_zero(KORL_C_CAST(u8*, resourceItem->backingSubType.runtime.data) + remainingBytes, -byteShiftCount);
    }
    else
    {
        korl_assert(byteShiftCount > 0);
        const u$ remainingBytes = korl_checkCast_u$_to_i$(resourceBytes) >= byteShiftCount 
                                ? resourceBytes - byteShiftCount 
                                : 0;
        if(remainingBytes)
            korl_memory_move(KORL_C_CAST(u8*, resourceItem->backingSubType.runtime.data) + byteShiftCount, resourceItem->backingSubType.runtime.data, remainingBytes);
        korl_memory_zero(resourceItem->backingSubType.runtime.data, byteShiftCount);
    }
    /* shifting data by a non-zero amount => the resource must be flushed */
    resourceItem->backingSubType.runtime.transcodingIsUpdated = false;
}
korl_internal KORL_POOL_CALLBACK_FOR_EACH(_korl_resource_flushUpdates_forEach)
{
    _Korl_Resource_Context*const context      = _korl_resource_context;
    _Korl_Resource_Item*const    resourceItem = KORL_C_CAST(_Korl_Resource_Item*, item);
    if(resourceItem->backingType != _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA)
        return KORL_POOL_FOR_EACH_CONTINUE;
    if(resourceItem->backingSubType.runtime.transcodingIsUpdated)
        return KORL_POOL_FOR_EACH_CONTINUE;
    _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
    if(resourceItem->backingSubType.runtime.isTransient && resourceItem->backingSubType.runtime.transientDestroy)
    {
        /* note that there is no need to call `unload` here, since transient data has already been unloaded */
        fnSig_korl_resource_descriptorCallback_descriptorStructDestroy*const descriptorStructDestroy = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_descriptorStructDestroy*, korl_functionDynamo_get(descriptor->callbacks.descriptorStructDestroy));
        descriptorStructDestroy(resourceItem->descriptorStruct, context->allocatorHandleRuntime);
        korl_free(context->allocatorHandleRuntime, resourceItem->backingSubType.runtime.data);
        return KORL_POOL_FOR_EACH_DELETE_AND_CONTINUE;
    }
    fnSig_korl_resource_descriptorCallback_transcode*const    transcode    = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_transcode*   , korl_functionDynamo_get(descriptor->callbacks.transcode));
    fnSig_korl_resource_descriptorCallback_runtimeBytes*const runtimeBytes = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_runtimeBytes*, korl_functionDynamo_get(descriptor->callbacks.runtimeBytes));
    const u$ resourceBytes = runtimeBytes(resourceItem->descriptorStruct);
    transcode(resourceItem->descriptorStruct, resourceItem->backingSubType.runtime.data, resourceBytes, context->allocatorHandleTransient);
    resourceItem->backingSubType.runtime.transcodingIsUpdated = true;
    return KORL_POOL_FOR_EACH_CONTINUE;
}
korl_internal void korl_resource_flushUpdates(void)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    korl_pool_forEach(&context->resourcePool, _korl_resource_flushUpdates_forEach, NULL);
    #if 0//@TODO: delete/recycle
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
    #endif
}
korl_internal void korl_resource_setAudioFormat(const Korl_Audio_Format* audioFormat)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    #if 0//@TODO: delete/recycle
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
    #endif
}
korl_internal acu8 korl_resource_getAudio(Korl_Resource_Handle handle, Korl_Audio_Format* o_resourceAudioFormat)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    #if 0//@TODO: delete/recycle
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
    #endif
    return (acu8){0};//@TODO
}
korl_internal void korl_resource_defragment(Korl_Memory_AllocatorHandle stackAllocator)
{
    #if 0//@TODO: delete/recycle
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
    #endif
    // KORL-ISSUE-000-000-135: resource: defragment transient resource data
}
korl_internal u32 korl_resource_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer)
{
    const u32 byteOffset = korl_checkCast_u$_to_u32((*pByteBuffer)->size);
    korl_memory_byteBuffer_append(pByteBuffer, (acu8){.size = sizeof(_korl_resource_context), .data = KORL_C_CAST(u8*, &_korl_resource_context)});
    return byteOffset;
}
korl_internal KORL_POOL_CALLBACK_FOR_EACH(_korl_resource_memoryStateRead_forEach)
{
    _Korl_Resource_Context*const    context      = _korl_resource_context;
    _Korl_Resource_Item*const       resourceItem = KORL_C_CAST(_Korl_Resource_Item*, item);
    _Korl_Resource_Descriptor*const descriptor   = context->stbDaDescriptors + resourceItem->descriptorIndex;
    switch(resourceItem->backingType)
    {
    case _KORL_RESOURCE_ITEM_BACKING_TYPE_ASSET_CACHE:{
        /* if the resource is backed by an assetCache file, we can just reset 
            the transcoded flag & clear the descriptorStruct */
        resourceItem->backingSubType.assetCache.isTranscoded = false;
        fnSig_korl_resource_descriptorCallback_clearTransientData*const clearTransientData = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_clearTransientData*, korl_functionDynamo_get(descriptor->callbacks.clearTransientData));
        clearTransientData(resourceItem->descriptorStruct);
        return KORL_POOL_FOR_EACH_CONTINUE;}
    case _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA:{
        if(resourceItem->backingSubType.runtime.isTransient)
        {
            /* if RUNTIME-backed resource is `transient`, don't create runtime 
                media since it's about to be destroyed anyway */
            resourceItem->backingSubType.runtime.transientDestroy = true;
            return KORL_POOL_FOR_EACH_CONTINUE;
        }
        /* for runtime-data-backed resources, all we have to do is re-create 
            multimedia assets & raise a flag to ensure the multimedia assets are 
            updated at the next end-of-frame */
        resourceItem->backingSubType.runtime.transcodingIsUpdated = false;
        fnSig_korl_resource_descriptorCallback_createRuntimeMedia*const createRuntimeMedia = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_createRuntimeMedia*, korl_functionDynamo_get(descriptor->callbacks.createRuntimeMedia));
        createRuntimeMedia(resourceItem->descriptorStruct);
        return KORL_POOL_FOR_EACH_CONTINUE;}
    }
    korl_log(ERROR, "invalid resourceItem backingType: %i", resourceItem->backingType);
    return KORL_POOL_FOR_EACH_CONTINUE;
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
    korl_pool_forEach(&context->resourcePool, _korl_resource_memoryStateRead_forEach, NULL);
    #if 0//@TODO: delete/recycle
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
    }
    #endif
}
korl_internal KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(korl_resource_onAssetHotReload)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    /* convert UTF-16 asset name to UTF-8 
        @TODO: refactor korl-assetCache to not use UTF-16 anymore for convenience, then we can delete this */
    acu8 utf8FileName = korl_string_utf16_to_utf8(context->allocatorHandleRuntime, rawUtf16AssetName);
    /* check to see if this asset file name is mapped to a Resource */
    ptrdiff_t hashMapIndex = mcshgeti(KORL_STB_DS_MC_CAST(context->allocatorHandleRuntime), context->stbShFileResources, utf8FileName.data);
    if(hashMapIndex < 0)
        goto cleanUp;
    /* if this file is backing a resource, we can simply unload the resource; 
        the resource will be re-transcoded at the next call to korl_resource_transcodeFileAssets */
    _Korl_Resource_Map*const        resourceMapItem = context->stbShFileResources + hashMapIndex;
    Korl_Resource_Handle            resourceHandle  = resourceMapItem->value;
    _Korl_Resource_Item*const       resourceItem    = korl_pool_get(&context->resourcePool, &resourceHandle);
    const u16                       descriptorIndex = KORL_POOL_HANDLE_ITEM_TYPE(resourceHandle);
    _Korl_Resource_Descriptor*const descriptor      = context->stbDaDescriptors + descriptorIndex;
    korl_assert(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_ASSET_CACHE);
    fnSig_korl_resource_descriptorCallback_unload*const unload = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_unload*, korl_functionDynamo_get(descriptor->callbacks.unload));
    unload(resourceItem->descriptorStruct, context->allocatorHandleTransient);
    resourceItem->backingSubType.assetCache.isTranscoded = false;
    cleanUp:
        korl_free(context->allocatorHandleRuntime, KORL_C_CAST(void*, utf8FileName.data));
}
#undef _LOCAL_STRING_POOL_POINTER
