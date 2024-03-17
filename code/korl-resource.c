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
#include "korl-resource-audio.h"
#define _LOCAL_STRING_POOL_POINTER (_korl_resource_context->stringPool)
typedef struct _Korl_Resource_Descriptor
{
    Korl_StringPool_String                     name;
    Korl_Resource_DescriptorManifest_Callbacks callbacks;
    void*                                      context;
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
            Korl_JobQueue_JobTicket   transcodeJob;
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
} _Korl_Resource_Context;
korl_global_variable _Korl_Resource_Context* _korl_resource_context;
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
    korl_resource_audio_register();
}
korl_internal KORL_JOB_QUEUE_FUNCTION(_korl_resource_transcodeFileAssets_asyncTranscode)
{
    _Korl_Resource_Context*const context      = _korl_resource_context;
    //@TODO: we can't do this; `context->resourcePool` can have resources added to it at any time on the main thread, which will invalidate this memory
    // _Korl_Resource_Item*const    resourceItem = KORL_C_CAST(_Korl_Resource_Item*, data);
}
korl_internal KORL_POOL_CALLBACK_FOR_EACH(_korl_resource_transcodeFileAssets_forEach)
{
    _Korl_Resource_Context*const context      = _korl_resource_context;
    _Korl_Resource_Item*const    resourceItem = KORL_C_CAST(_Korl_Resource_Item*, item);
    if(resourceItem->backingType != _KORL_RESOURCE_ITEM_BACKING_TYPE_ASSET_CACHE // this is not a file-asset-backed resource
    || resourceItem->backingSubType.assetCache.isTranscoded)// resource is already transcoded
        return KORL_POOL_FOR_EACH_CONTINUE;// this is not a file-asset-backed resource
    if(resourceItem->backingSubType.assetCache.transcodeJob)/* if we have an active transcode job, we need to poll it to see if it's done in order to close the ticket; once the ticket is done, we can mark it as being finally transcoded */
    {
        if(korl_jobQueue_jobIsDone(resourceItem->backingSubType.assetCache.transcodeJob))
        {
            //@TODO: free any async job specific memory here?
            resourceItem->backingSubType.assetCache.isTranscoded = true;
        }
    }
    else/* otherwise, we need to load the AssetCache_AssetData, as this is an ASSET_CACHE-backed resource */
    {
        /* attempt to get the raw file data from korl-assetCache */
        KORL_ZERO_STACK(Korl_AssetCache_AssetData, assetData);
        const Korl_AssetCache_Get_Result assetCacheGetResult = korl_assetCache_get(string_getRawUtf16(&resourceItem->backingSubType.assetCache.fileName), resourceItem->backingSubType.assetCache.assetCacheGetFlags, &assetData);
        if(resourceItem->backingSubType.assetCache.assetCacheGetFlags == KORL_ASSETCACHE_GET_FLAGS_NONE)
            korl_assert(assetCacheGetResult == KORL_ASSETCACHE_GET_RESULT_LOADED);
        /* if we have the assetCache data, we can do the transcoding */
        if(assetCacheGetResult == KORL_ASSETCACHE_GET_RESULT_LOADED)
        {
            if(resourceItem->backingSubType.assetCache.assetCacheGetFlags & KORL_ASSETCACHE_GET_FLAG_LAZY)/* if the user wants to lazy-load this resource, we can spawn an async transcode job */
            {
                //@TODO: allocate a transient "asyncTranscode context" that can stay static in memory for the duration of the async job?
                resourceItem->backingSubType.assetCache.transcodeJob = korl_jobQueue_post(_korl_resource_transcodeFileAssets_asyncTranscode, );
            }
            else/* otherwise, we need to transcode this resource immediately */
            {
                korl_assert(resourceItem->descriptorIndex < arrlenu(context->stbDaDescriptors));
                const _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
                korl_assert(descriptor->callbacks.transcode);// all file-asset-backed resources _must_ use a descriptor that has a `transcode` callback
                fnSig_korl_resource_descriptorCallback_transcode*const transcode = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_transcode*, korl_functionDynamo_get(descriptor->callbacks.transcode));
                transcode(resourceItem->descriptorStruct, assetData.data, assetData.dataBytes, context->allocatorHandleTransient);
                resourceItem->backingSubType.assetCache.isTranscoded = true;
            }
        }
    }
    return KORL_POOL_FOR_EACH_CONTINUE;
}
korl_internal void korl_resource_transcodeFileAssets(void)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    korl_pool_forEach(&context->resourcePool, _korl_resource_transcodeFileAssets_forEach, NULL);
}
korl_internal KORL_FUNCTION_korl_resource_getDescriptorContextStruct(korl_resource_getDescriptorContextStruct)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    /* get the Resource_Descriptor */
    const _Korl_Resource_Descriptor*const descriptorsEnd = context->stbDaDescriptors + arrlen(context->stbDaDescriptors);
    _Korl_Resource_Descriptor* descriptor = context->stbDaDescriptors;
    for(; descriptor < descriptorsEnd; descriptor++)
        if(string_equalsAcu8(&descriptor->name, utf8DescriptorName))
            break;
    korl_assert(descriptor < descriptorsEnd);
    /**/
    return descriptor->context;
}
korl_internal KORL_FUNCTION_korl_resource_getDescriptorStruct(korl_resource_getDescriptorStruct)
{
    _Korl_Resource_Context*const context      = _korl_resource_context;
    _Korl_Resource_Item*const    resourceItem = korl_pool_get(&context->resourcePool, &handle);
    korl_assert(resourceItem);
    return resourceItem->descriptorStruct;
}
korl_internal KORL_FUNCTION_korl_resource_descriptor_register(korl_resource_descriptor_register)
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
    if(descriptorManifest->descriptorContext)// if the user wants this descriptor to have a "context" struct, we need to create/initialize it here
    {
        korl_assert(descriptorManifest->descriptorContextBytes);
        newDescriptor.context = korl_allocateDirty(context->allocatorHandleRuntime, descriptorManifest->descriptorContextBytes);
        korl_memory_copy(newDescriptor.context, descriptorManifest->descriptorContext, descriptorManifest->descriptorContextBytes);
    }
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
        //KORL-PERFORMANCE-000-000-053: resource: descriptor structs are going to create a _ton_ of allocations; this is fine for testing purposes, but in a real game (with thousands of resources) this will almost certainly cause a lot of unnecessary overhead when it comes time to perform memory cleanup/analysis operations
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
        if(!(newResource->backingSubType.assetCache.assetCacheGetFlags & KORL_ASSETCACHE_GET_FLAG_LAZY))
            /* if we're no lazy-loading the asset, let's just transcode it right now */
            _korl_resource_transcodeFileAssets_forEach(NULL, newResource);
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
    //KORL-PERFORMANCE-000-000-053: resource: descriptor structs are going to create a _ton_ of allocations; this is fine for testing purposes, but in a real game (with thousands of resources) this will almost certainly cause a lot of unnecessary overhead when it comes time to perform memory cleanup/analysis operations
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
korl_internal KORL_FUNCTION_korl_resource_getRawRuntimeData(korl_resource_getRawRuntimeData)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    _Korl_Resource_Item*const resourceItem = korl_pool_get(&context->resourcePool, &handle);
    korl_assert(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA);
    if(o_bytes)
    {
        _Korl_Resource_Descriptor*const descriptor = context->stbDaDescriptors + resourceItem->descriptorIndex;
        fnSig_korl_resource_descriptorCallback_runtimeBytes*const runtimeBytes = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_runtimeBytes*, korl_functionDynamo_get(descriptor->callbacks.runtimeBytes));
        *o_bytes = runtimeBytes(resourceItem->descriptorStruct);
    }
    return resourceItem->backingSubType.runtime.data;
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
typedef struct _Korl_Resource_ForEach_PoolForEachContext
{
    korl_resource_callback_forEach* resourceForEachCallback;
    void*                           resourceForEachUserData;
    u16                             descriptorIndex;
} _Korl_Resource_ForEach_PoolForEachContext;
korl_internal KORL_POOL_CALLBACK_FOR_EACH(_korl_resource_forEach_poolForEachCallback)
{
    const _Korl_Resource_ForEach_PoolForEachContext*const forEachContext = userData;
    _Korl_Resource_Item*const                             resourceItem   = KORL_C_CAST(_Korl_Resource_Item*, item);
    if(resourceItem->descriptorIndex != forEachContext->descriptorIndex)
        return KORL_POOL_FOR_EACH_CONTINUE;
    korl_assert(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_ASSET_CACHE);// for now, only support iterating over ASSET_CACHE backed resources, for the sake of passing the isTranscoded flag
    const Korl_Resource_ForEach_Result resourceForEachResult = forEachContext->resourceForEachCallback(forEachContext->resourceForEachUserData
                                                                                                      ,resourceItem->descriptorStruct
                                                                                                      ,&resourceItem->backingSubType.assetCache.isTranscoded);
    switch(resourceForEachResult)
    {
    case KORL_RESOURCE_FOR_EACH_RESULT_STOP    : return KORL_POOL_FOR_EACH_DONE;
    case KORL_RESOURCE_FOR_EACH_RESULT_CONTINUE: return KORL_POOL_FOR_EACH_CONTINUE;
    }
    korl_log(ERROR, "invalid resource for-each callback result: %i", resourceForEachResult);
    return KORL_POOL_FOR_EACH_CONTINUE;
}
korl_internal KORL_FUNCTION_korl_resource_forEach(korl_resource_forEach)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    /* get the Resource_Descriptor */
    const _Korl_Resource_Descriptor*const descriptorsEnd = context->stbDaDescriptors + arrlen(context->stbDaDescriptors);
    _Korl_Resource_Descriptor* descriptor = context->stbDaDescriptors;
    for(; descriptor < descriptorsEnd; descriptor++)
        if(string_equalsAcu8(&descriptor->name, utf8DescriptorName))
            break;
    korl_assert(descriptor < descriptorsEnd);
    /**/
    KORL_ZERO_STACK(_Korl_Resource_ForEach_PoolForEachContext, forEachContext)
    forEachContext.resourceForEachCallback = callback;
    forEachContext.resourceForEachUserData = callbackUserData;
    forEachContext.descriptorIndex         = korl_checkCast_i$_to_u16(descriptor - context->stbDaDescriptors);
    korl_pool_forEach(&context->resourcePool, _korl_resource_forEach_poolForEachCallback, &forEachContext);
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
}
typedef struct _Korl_Resource_Defragment_ForEachContext
{
    Korl_Memory_AllocatorHandle   contextAllocator;
    void*                         stbDaMemoryContext;
    Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers;
} _Korl_Resource_Defragment_ForEachContext;
korl_internal KORL_POOL_CALLBACK_FOR_EACH(_korl_resource_defragment_forEach)
{
    _Korl_Resource_Defragment_ForEachContext*const                         forEachContext            = userData;
    _Korl_Resource_Item*const                                              resourceItem              = item;
    _Korl_Resource_Descriptor*const                                        descriptor                = _korl_resource_context->stbDaDescriptors + resourceItem->descriptorIndex;
    fnSig_korl_resource_descriptorCallback_collectDefragmentPointers*const collectDefragmentPointers = KORL_C_CAST(fnSig_korl_resource_descriptorCallback_collectDefragmentPointers*, korl_functionDynamo_get(descriptor->callbacks.collectDefragmentPointers));
    if(forEachContext->contextAllocator == _korl_resource_context->allocatorHandleRuntime)
    {
        /* if we're defragmenting RUNTIME memory, we need to defragment the Resource_Item's descriptorStruct */
        KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD(forEachContext->stbDaMemoryContext, *forEachContext->pStbDaDefragmentPointers, resourceItem->descriptorStruct, _korl_resource_context->resourcePool.items.datas);
        if(resourceItem->backingType == _KORL_RESOURCE_ITEM_BACKING_TYPE_RUNTIME_DATA)
            /* for RUNTIME-backed resources, we also need to defragment the runtime Resource_Item's data block */
            KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD(forEachContext->stbDaMemoryContext, *forEachContext->pStbDaDefragmentPointers, resourceItem->backingSubType.runtime.data, _korl_resource_context->resourcePool.items.datas);
        collectDefragmentPointers(resourceItem->descriptorStruct, forEachContext->stbDaMemoryContext, forEachContext->pStbDaDefragmentPointers, resourceItem->descriptorStruct, forEachContext->contextAllocator, false);
    }
    else
    {
        korl_assert(forEachContext->contextAllocator == _korl_resource_context->allocatorHandleTransient);// there are only two options: {runtime, transient}, and we already handled the RUNTIME memory case
        collectDefragmentPointers(resourceItem->descriptorStruct, forEachContext->stbDaMemoryContext, forEachContext->pStbDaDefragmentPointers, resourceItem->descriptorStruct, forEachContext->contextAllocator, true);
    }
    return KORL_POOL_FOR_EACH_CONTINUE;
}
korl_internal void korl_resource_defragment(Korl_Memory_AllocatorHandle stackAllocator)
{
    if(korl_memory_allocator_isFragmented(_korl_resource_context->allocatorHandleRuntime))
    {
        Korl_Heap_DefragmentPointer* stbDaDefragmentPointers = NULL;
        mcarrsetcap(KORL_STB_DS_MC_CAST(stackAllocator), stbDaDefragmentPointers, 64);
        KORL_MEMORY_STB_DA_DEFRAGMENT(stackAllocator, stbDaDefragmentPointers, _korl_resource_context);
        KORL_MEMORY_STB_DA_DEFRAGMENT_STB_HASHMAP_CHILD(stackAllocator, &stbDaDefragmentPointers, _korl_resource_context->stbShFileResources, _korl_resource_context);
        korl_stringPool_collectDefragmentPointers(_korl_resource_context->stringPool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_resource_context);
        korl_pool_collectDefragmentPointers(&_korl_resource_context->resourcePool, KORL_STB_DS_MC_CAST(stackAllocator), &stbDaDefragmentPointers, _korl_resource_context);
        KORL_MEMORY_STB_DA_DEFRAGMENT_STB_ARRAY_CHILD(stackAllocator, stbDaDefragmentPointers, _korl_resource_context->stbDaDescriptors, _korl_resource_context);
        const _Korl_Resource_Descriptor*const descriptorsEnd = _korl_resource_context->stbDaDescriptors + arrlen(_korl_resource_context->stbDaDescriptors);
        for(_Korl_Resource_Descriptor* descriptor = _korl_resource_context->stbDaDescriptors; descriptor < descriptorsEnd; descriptor++)
            if(descriptor->context)
                KORL_MEMORY_STB_DA_DEFRAGMENT_CHILD(stackAllocator, stbDaDefragmentPointers, descriptor->context, _korl_resource_context->stbDaDescriptors);
        KORL_ZERO_STACK(_Korl_Resource_Defragment_ForEachContext, forEachContext);
        forEachContext.contextAllocator         = _korl_resource_context->allocatorHandleRuntime;
        forEachContext.stbDaMemoryContext       = KORL_STB_DS_MC_CAST(stackAllocator);
        forEachContext.pStbDaDefragmentPointers = &stbDaDefragmentPointers;
        korl_pool_forEach(&_korl_resource_context->resourcePool, _korl_resource_defragment_forEach, &forEachContext);
        korl_memory_allocator_defragment(_korl_resource_context->allocatorHandleRuntime, stbDaDefragmentPointers, arrlenu(stbDaDefragmentPointers), stackAllocator);
    }
    if(korl_memory_allocator_isFragmented(_korl_resource_context->allocatorHandleTransient))
    {
        Korl_Heap_DefragmentPointer* stbDaDefragmentPointers = NULL;
        mcarrsetcap(KORL_STB_DS_MC_CAST(stackAllocator), stbDaDefragmentPointers, 64);
        KORL_ZERO_STACK(_Korl_Resource_Defragment_ForEachContext, forEachContext);
        forEachContext.contextAllocator         = _korl_resource_context->allocatorHandleTransient;
        forEachContext.stbDaMemoryContext       = KORL_STB_DS_MC_CAST(stackAllocator);
        forEachContext.pStbDaDefragmentPointers = &stbDaDefragmentPointers;
        korl_pool_forEach(&_korl_resource_context->resourcePool, _korl_resource_defragment_forEach, &forEachContext);
        korl_memory_allocator_defragment(_korl_resource_context->allocatorHandleTransient, stbDaDefragmentPointers, arrlenu(stbDaDefragmentPointers), stackAllocator);
    }
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
}
korl_internal KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(korl_resource_onAssetHotReload)
{
    _Korl_Resource_Context*const context = _korl_resource_context;
    //KORL-ISSUE-000-000-192: assetCache: refactor korl-assetCache to not use UTF-16 anymore for convenience
    /* convert UTF-16 asset name to UTF-8 */
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
