#include "korl-resource.h"
#include "korl-vulkan.h"
#include "korl-stb-ds.h"
#include "korl-stb-image.h"
korl_global_const u$ _KORL_RESOURCE_UNIQUE_ID_MAX = 0x0FFFFFFFFFFFFFFF;
typedef enum _Korl_Resource_Type
    {_KORL_RESOURCE_TYPE_INVALID // this value indicates the entire Resource Handle is not valid
    ,_KORL_RESOURCE_TYPE_FILE    // Resource is derived from a korl-assetCache file
    ,_KORL_RESOURCE_TYPE_RUNTIME // Resource is derived from data that is generated at runtime
} _Korl_Resource_Type;
typedef enum _Korl_Resource_MultimediaType
    {_KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS// this resource maps to a vulkan device-local allocation, or similar type of data
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
                Korl_Vulkan_CreateInfoVertexBuffer vertexBuffer;
            } createInfo;
        } graphics;
    } subType;
    void* data;// this is a pointer to a memory allocation holding the raw _decoded_ resource; this is the data which should be passed in to the platform module which utilizes this Resource's MultimediaType; example: an image resource data will point to an RGBA bitmap
    u$    dataBytes;
    bool dirty;// if this is true, the multimedia-encoded asset will be updated at the end of the frame, then the flag will be reset
} _Korl_Resource;
typedef struct _Korl_Resource_Map
{
    Korl_Resource_Handle key;
    _Korl_Resource       value;
} _Korl_Resource_Map;
typedef struct _Korl_Resource_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    _Korl_Resource_Map* stbHmResources;
    Korl_Resource_Handle* stbDsDirtyResourceHandles;
    u$ nextUniqueId;// this counter will increment each time we add a _non-file_ resource to the database; file-based resources will have a unique id generated from a hash of the asset file name
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
korl_internal void korl_resource_initialize(void)
{
    korl_memory_zero(&_korl_resource_context, sizeof(_korl_resource_context));
    _korl_resource_context.allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_gigabytes(1), L"korl-resource", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, NULL/*auto-select start address*/);
    mchmdefault(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
    mcarrsetcap(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbDsDirtyResourceHandles, 128);
}
korl_internal KORL_PLATFORM_RESOURCE_FROM_FILE(korl_resource_fromFile)
{
    /* hash the file name, generate our resource handle */
    KORL_ZERO_STACK(_Korl_Resource_Handle_Unpacked, unpackedHandle);
    unpackedHandle.type     = _KORL_RESOURCE_TYPE_FILE;
    unpackedHandle.uniqueId = korl_memory_acu16_hash(fileName);
    /* automatically determine the type of multimedia this resource is based on the file extension */
    korl_shared_const u16* IMAGE_EXTENSIONS[] = {L".png", L".jpg", L".jpeg"};
    _Korl_Resource_Graphics_Type graphicsType = _KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN;
    for(u32 i = 0; i < korl_arraySize(IMAGE_EXTENSIONS); i++)
    {
        const u$ extensionSize = korl_memory_stringSize(IMAGE_EXTENSIONS[i]);
        if(fileName.size < extensionSize)
            continue;
        if(0 == korl_memory_arrayU16Compare(fileName.data + fileName.size - extensionSize, extensionSize, IMAGE_EXTENSIONS[i], extensionSize))
        {
            unpackedHandle.multimediaType = _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS;
            graphicsType                  = _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE;
            break;
        }
    }
    if(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS)
        korl_assert(graphicsType != _KORL_RESOURCE_GRAPHICS_TYPE_UNKNOWN);
    ///@TODO: assert that multimediaType is valid?
    /* we should now have all the info needed to create the packed resource handle */
    const Korl_Resource_Handle handle = _korl_resource_handle_pack(unpackedHandle);
    /* check if the resource was already added */
    ptrdiff_t hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    /* if the resource is new, we need to add it to the database */
    if(hashMapIndex < 0)
    {
        mchmput(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
        hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
        korl_assert(hashMapIndex >= 0);
        if(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS)
            _korl_resource_context.stbHmResources[hashMapIndex].value.subType.graphics.type = graphicsType;
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
                switch(resource->subType.graphics.type)
                {
                case _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE:{
                    int imageSizeX = 0, imageSizeY = 0, imageChannels = 0;
                    /* @TODO: performance; stbi API unfortunately doesn't seem to have the ability for the user to provide their own allocator, so we need an extra alloc/copy/free here */
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
                    resource->subType.graphics.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createTexture(&resource->subType.graphics.createInfo.texture);
                    korl_vulkan_texture_update(resource->subType.graphics.deviceMemoryAllocationHandle, resource->data);
                    break;}
                default:
                    korl_log(ERROR, "invalid graphics type %i", resource->subType.graphics.type);
                    break;
                }
                break;}
            default:{
                korl_log(ERROR, "invalid multimedia type: %i", unpackedHandle.multimediaType);
                break;}
            }
        }
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
    ptrdiff_t hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex < 0);
    mchmput(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
    hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    resource->subType.graphics.type = _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER;
    /* allocate the memory for the raw decoded asset data */
    resource->dataBytes = createInfo->bytes;
    resource->data      = korl_allocate(_korl_resource_context.allocatorHandle, createInfo->bytes);
    korl_assert(resource->data);
    /* create the multimedia asset */
    resource->subType.graphics.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createVertexBuffer(createInfo);
    resource->subType.graphics.createInfo.vertexBuffer      = *createInfo;
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
    ptrdiff_t hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex < 0);
    mchmput(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Resource));
    hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    resource->subType.graphics.type = _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE;
    /* allocate the memory for the raw decoded asset data */
    resource->dataBytes = createInfo->sizeX * createInfo->sizeY * sizeof(Korl_Vulkan_Color4u8);
    resource->data      = korl_allocate(_korl_resource_context.allocatorHandle, resource->dataBytes);
    korl_assert(resource->data);
    /* create the multimedia asset */
    resource->subType.graphics.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createTexture(createInfo);
    resource->subType.graphics.createInfo.texture           = *createInfo;
    return handle;
}
korl_internal void korl_resource_destroy(Korl_Resource_Handle handle)
{
    if(!handle)
        return;// silently do nothing for NULL handles
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    const _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    /* destroy the transcoded multimedia asset */
    switch(unpackedHandle.multimediaType)
    {
    case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
        korl_vulkan_deviceAsset_destroy(resource->subType.graphics.deviceMemoryAllocationHandle);
        break;}
    default:{
        korl_log(ERROR, "invalid multimedia type %i", unpackedHandle.multimediaType);
        break;}
    }
    /* destroy the cached decoded raw asset data */
    korl_free(_korl_resource_context.allocatorHandle, resource->data);
    /* remove the resource from the database */
    mchmdel(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
}
korl_internal void korl_resource_update(Korl_Resource_Handle handle, const void* data, u$ dataBytes)
{
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.type == _KORL_RESOURCE_TYPE_RUNTIME);
    korl_assert(dataBytes <= resource->dataBytes);
    korl_memory_copy(resource->data, data, dataBytes);
    if(!resource->dirty)
    {
        mcarrpush(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbDsDirtyResourceHandles, handle);
        resource->dirty = true;
    }
}
korl_internal void korl_resource_resize(Korl_Resource_Handle handle, u$ newByteSize)
{
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.type == _KORL_RESOURCE_TYPE_RUNTIME);
    if(resource->dataBytes != newByteSize && !resource->dirty)
    {
        mcarrpush(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbDsDirtyResourceHandles, handle);
        resource->dirty = true;
    }
    resource->data      = korl_reallocate(_korl_resource_context.allocatorHandle, resource->data, newByteSize);
    resource->dataBytes = newByteSize;
    korl_assert(resource->data);
}
korl_internal void korl_resource_flushUpdates(void)
{
    const Korl_Resource_Handle*const dirtyHandlesEnd = _korl_resource_context.stbDsDirtyResourceHandles + arrlen(_korl_resource_context.stbDsDirtyResourceHandles);
    for(const Korl_Resource_Handle* dirtyHandle = _korl_resource_context.stbDsDirtyResourceHandles; dirtyHandle < dirtyHandlesEnd; dirtyHandle++)
    {
        const ptrdiff_t hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, *dirtyHandle);
        if(hashMapIndex < 0)
        {
            korl_log(WARNING, "updated resource handle invalid (update + delete in same frame, etc.): 0x%X", *dirtyHandle);
            continue;
        }
        _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
        korl_assert(resource->dirty);
        const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(*dirtyHandle);
        switch(unpackedHandle.multimediaType)
        {
        case _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS:{
            switch(resource->subType.graphics.type)
            {
            case _KORL_RESOURCE_GRAPHICS_TYPE_IMAGE:{
                korl_vulkan_texture_update(resource->subType.graphics.deviceMemoryAllocationHandle, resource->data);
                break;}
            case _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER:{
                if(resource->subType.graphics.createInfo.vertexBuffer.bytes != resource->dataBytes)
                {
                    korl_vulkan_vertexBuffer_resize(&resource->subType.graphics.deviceMemoryAllocationHandle, resource->dataBytes);
                    resource->subType.graphics.createInfo.vertexBuffer.bytes = resource->dataBytes;
                }
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
    mcarrsetlen(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbDsDirtyResourceHandles, 0);
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_resource_getVulkanDeviceMemoryAllocationHandle(Korl_Resource_Handle handle)
{
    if(!handle)
        return 0;// silently return a NULL device memory allocation handle if the resource handle is NULL
    const _Korl_Resource_Handle_Unpacked unpackedHandle = _korl_resource_handle_unpack(handle);
    korl_assert(unpackedHandle.multimediaType == _KORL_RESOURCE_MULTIMEDIA_TYPE_GRAPHICS);
    const ptrdiff_t hashMapIndex = mchmgeti(KORL_C_CAST(void*, _korl_resource_context.allocatorHandle), _korl_resource_context.stbHmResources, handle);
    korl_assert(hashMapIndex >= 0);
    const _Korl_Resource*const resource = &(_korl_resource_context.stbHmResources[hashMapIndex].value);
    return resource->subType.graphics.deviceMemoryAllocationHandle;
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
    for(u$ r = 0; r < arrlenu(_korl_resource_context.stbHmResources); r++)// stb_ds says we can iterate over hash maps the same way as dynamic arrays
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
            korl_memory_zero(&resourceMapItem->value, sizeof(resourceMapItem->value));
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
                    resourceMapItem->value.subType.graphics.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createTexture(&resourceMapItem->value.subType.graphics.createInfo.texture);
                    break;}
                case _KORL_RESOURCE_GRAPHICS_TYPE_VERTEX_BUFFER:{
                    resourceMapItem->value.subType.graphics.deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createVertexBuffer(&resourceMapItem->value.subType.graphics.createInfo.vertexBuffer);
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
            resourceMapItem->value.dirty = true;
            break;}
        default:
            korl_log(ERROR, "invalid resource type %i", unpackedHandle.type);
            break;
        }
    }
    return true;
}
