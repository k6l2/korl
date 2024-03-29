#include "korl-resource-texture.h"
#include "korl-stb-image.h"
#include "korl-interface-platform.h"
typedef struct _Korl_Resource_Texture
{
    Korl_Resource_Texture_CreateInfo          createInfo;
    Korl_Vulkan_DeviceMemory_AllocationHandle deviceMemoryAllocationHandle;
} _Korl_Resource_Texture;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_collectDefragmentPointers(_korl_resource_texture_collectDefragmentPointers)
{
    // nothing to do here; we are not managing any dynamic memory in the descriptor struct
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_texture_descriptorStructCreate)
{
    return korl_allocate(allocatorRuntime, sizeof(_Korl_Resource_Texture));
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_texture_descriptorStructDestroy)
{
    korl_free(allocatorRuntime, resourceDescriptorStruct);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(_korl_resource_texture_clearTransientData)
{
    _Korl_Resource_Texture*const texture = resourceDescriptorStruct;
    // we do _not_ call `korl_vulkan_deviceAsset_destroy` here, since vulkan deviceAssets are transient, so they should already be destroyed
    // all we have to do is wipe our device memory allocation handle:
    texture->deviceMemoryAllocationHandle = 0;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_texture_unload)
{
    _Korl_Resource_Texture*const texture = resourceDescriptorStruct;
    korl_vulkan_deviceAsset_destroy(texture->deviceMemoryAllocationHandle);
    texture->deviceMemoryAllocationHandle = 0;
}
korl_internal void _korl_resource_texture_stbDecode(_Korl_Resource_Texture*const texture, const void* data, u$ dataBytes)
{
    int imageSizeX = 0, imageSizeY = 0, imageChannels = 0;
    //KORL-PERFORMANCE-000-000-032: resource: stbi API unfortunately doesn't seem to have the ability for the user to provide their own allocator, so we need an extra alloc/copy/free here
    //KORL-ISSUE-000-000-190: vulkan: query vulkan for a supported texture format; not all image formats will be supported on all device implementations, and we do want to only use the smallest amount of image data possible, so best thing we can do here is (1) obtain the image file details without decoding the image (see stb-image for API to do this), (2) query vulkan for a format which is large enough to support this format (using vkGetPhysicalDeviceImageFormatProperties), then (3) pass this format's channel count into stb-image; if no STBI_* corresponding format is supported, we should just (1) pass STBI_default, (2) query vulkan for _any_ supported image format, then (3) transcode the output of stb-image to the format from step 2; if no format from step 2 is found, we can declare the platform to be unsupported or something, I guess
    stbi_uc*const stbiPixels = stbi_load_from_memory(data, korl_checkCast_u$_to_i32(dataBytes), &imageSizeX, &imageSizeY, &imageChannels, STBI_rgb_alpha);
    korl_assert(stbiPixels);
    texture->createInfo.size        = KORL_STRUCT_INITIALIZE(Korl_Math_V2u32){imageSizeX, imageSizeY};
    //KORL-ISSUE-000-000-191: resource-texture: support user-specified color-space configuration; we're assuming all texture assets are loaded as SRGB
    texture->createInfo.formatImage = KORL_RESOURCE_TEXTURE_FORMAT_R8G8B8A8_SRGB;
    texture->deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createTexture(&texture->createInfo, texture->deviceMemoryAllocationHandle);
    Korl_Gfx_Color4u8*const colorData = KORL_C_CAST(Korl_Gfx_Color4u8*, stbiPixels);
    /* pre-multiply alpha channel into all file images by default */
    for(int i = 0; i < imageSizeX * imageSizeY; i++)
    {
        const f32 alpha = KORL_C_CAST(f32, colorData[i].a) / KORL_C_CAST(f32, KORL_U8_MAX);
        colorData[i].r = korl_math_round_f32_to_u8(alpha * KORL_C_CAST(f32, colorData[i].r));
        colorData[i].g = korl_math_round_f32_to_u8(alpha * KORL_C_CAST(f32, colorData[i].g));
        colorData[i].b = korl_math_round_f32_to_u8(alpha * KORL_C_CAST(f32, colorData[i].b));
    }
    /**/
    korl_vulkan_texture_update(texture->deviceMemoryAllocationHandle, colorData);
    stbi_image_free(stbiPixels);// stbiPixels data is now entirely within graphics device staging memory, so we can discard it immediately
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_texture_transcode)
{
    _Korl_Resource_Texture*const texture = resourceDescriptorStruct;
    if(texture->deviceMemoryAllocationHandle)
    {
        if(texture->createInfo.imageFileMemoryBuffer.size)
        {
            /* if the user provided an image file memory buffer when the resource was created, 
                there is nothing to update, since we don't currently support changing such texture resources */
        }
        else
            /* if the device memory allocation already exists, this must be a RUNTIME resource, so the data is just a pixel array to upload to the graphics device */
            korl_vulkan_texture_update(texture->deviceMemoryAllocationHandle, data);
    }
    else/* otherwise, this must be a file-asset-backed resource, so we need to decode the file data into pixel data first */
        _korl_resource_texture_stbDecode(texture, data, dataBytes);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeData(_korl_resource_texture_createRuntimeData)
{
    _Korl_Resource_Texture*const texture = resourceDescriptorStruct;
    const Korl_Resource_Texture_CreateInfo*const createInfo = descriptorCreateInfo;
    texture->createInfo = *createInfo;
    if(texture->createInfo.imageFileMemoryBuffer.size == 0)
    {
        /* only create runtime data if the user did _not_ provide an image file memory buffer */
        korl_assert(createInfo->formatImage != KORL_RESOURCE_TEXTURE_FORMAT_UNDEFINED);
        *o_data = korl_allocate(allocatorRuntime, createInfo->size.x * createInfo->size.y * KORL_RESOURCE_TEXTURE_FORMAT_BYTE_STRIDES[texture->createInfo.formatImage]);
    }
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_createRuntimeMedia(_korl_resource_texture_createRuntimeMedia)
{
    _Korl_Resource_Texture*const texture = resourceDescriptorStruct;
    if(texture->createInfo.imageFileMemoryBuffer.size)
        /* if the user provided an image file memory buffer, we can just decode it right away */
        _korl_resource_texture_stbDecode(texture, texture->createInfo.imageFileMemoryBuffer.data, texture->createInfo.imageFileMemoryBuffer.size);
    else
        texture->deviceMemoryAllocationHandle = korl_vulkan_deviceAsset_createTexture(&texture->createInfo, texture->deviceMemoryAllocationHandle);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_runtimeBytes(_korl_resource_texture_runtimeBytes)
{
    const _Korl_Resource_Texture*const texture = resourceDescriptorStruct;
    return texture->createInfo.size.x * texture->createInfo.size.y * KORL_RESOURCE_TEXTURE_FORMAT_BYTE_STRIDES[texture->createInfo.formatImage];
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_runtimeResize(_korl_resource_texture_runtimeResize)
{
    _Korl_Resource_Texture*const texture = resourceDescriptorStruct;
    korl_assert(!"not implemented");
}
korl_internal void korl_resource_texture_register(void)
{
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName                  = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_TEXTURE);
    descriptorManifest.callbacks.collectDefragmentPointers = korl_functionDynamo_register(_korl_resource_texture_collectDefragmentPointers);
    descriptorManifest.callbacks.descriptorStructCreate    = korl_functionDynamo_register(_korl_resource_texture_descriptorStructCreate);
    descriptorManifest.callbacks.descriptorStructDestroy   = korl_functionDynamo_register(_korl_resource_texture_descriptorStructDestroy);
    descriptorManifest.callbacks.clearTransientData        = korl_functionDynamo_register(_korl_resource_texture_clearTransientData);
    descriptorManifest.callbacks.unload                    = korl_functionDynamo_register(_korl_resource_texture_unload);
    descriptorManifest.callbacks.transcode                 = korl_functionDynamo_register(_korl_resource_texture_transcode);
    descriptorManifest.callbacks.createRuntimeData         = korl_functionDynamo_register(_korl_resource_texture_createRuntimeData);
    descriptorManifest.callbacks.createRuntimeMedia        = korl_functionDynamo_register(_korl_resource_texture_createRuntimeMedia);
    descriptorManifest.callbacks.runtimeBytes              = korl_functionDynamo_register(_korl_resource_texture_runtimeBytes);
    descriptorManifest.callbacks.runtimeResize             = korl_functionDynamo_register(_korl_resource_texture_runtimeResize);
    korl_resource_descriptor_register(&descriptorManifest);
}
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_resource_texture_getVulkanDeviceMemoryAllocationHandle(Korl_Resource_Handle handleResourceTexture)
{
    if(!handleResourceTexture)
        return 0;// silently return an empty device memory allocation for empty resource handles
    //KORL-ISSUE-000-000-181: resource-*: unsafe; validate resource descriptor; it would be nice if we had a low-cost, easy to implement per-descriptor way of validating the descriptorStruct returned from korl_resource_getDescriptorStruct actually matches the descriptor code calling it; the only way I can think of is forcing the descriptor name string lookup for each of these calls, but sure there is a better way, right?
    _Korl_Resource_Texture*const texture = korl_resource_getDescriptorStruct(handleResourceTexture);
    return texture->deviceMemoryAllocationHandle;
}
korl_internal KORL_FUNCTION_korl_resource_texture_getSize(korl_resource_texture_getSize)
{
    if(!handleResourceTexture)
        return KORL_MATH_V2U32_ZERO;
    //KORL-ISSUE-000-000-181: resource-*: unsafe; validate resource descriptor; it would be nice if we had a low-cost, easy to implement per-descriptor way of validating the descriptorStruct returned from korl_resource_getDescriptorStruct actually matches the descriptor code calling it; the only way I can think of is forcing the descriptor name string lookup for each of these calls, but sure there is a better way, right?
    _Korl_Resource_Texture*const texture = korl_resource_getDescriptorStruct(handleResourceTexture);
    return texture->createInfo.size;
}
korl_internal KORL_FUNCTION_korl_resource_texture_getRowByteStride(korl_resource_texture_getRowByteStride)
{
    if(!handleResourceTexture)
        return 0;
    //KORL-ISSUE-000-000-181: resource-*: unsafe; validate resource descriptor; it would be nice if we had a low-cost, easy to implement per-descriptor way of validating the descriptorStruct returned from korl_resource_getDescriptorStruct actually matches the descriptor code calling it; the only way I can think of is forcing the descriptor name string lookup for each of these calls, but sure there is a better way, right?
    _Korl_Resource_Texture*const texture = korl_resource_getDescriptorStruct(handleResourceTexture);
    return texture->createInfo.size.x * KORL_RESOURCE_TEXTURE_FORMAT_BYTE_STRIDES[texture->createInfo.formatImage];
}
