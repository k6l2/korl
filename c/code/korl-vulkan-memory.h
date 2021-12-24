/**
 * The korl-vulkan-memory code module should only be used INTERNALLY by 
 * korl-vulkan* code!  Stay away from this API unless you are doing stuff in 
 * in korl-vulkan* code.
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-memory.h"
#include <vulkan/vulkan.h>
typedef enum _Korl_Vulkan_DeviceMemory_Allocation_Type
{
    _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED, 
    _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER, 
    _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE, 
    _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_IMAGE_BUFFER
    /* IMPORTANT: when adding new types to this enum, we must update the 
        appropriate tagged union in the allocation struct, as well as the code 
        which handles each type in the allocator constructor (and create a 
        corresponding allocation method for the new device object type) 
        @vulkan-device-allocation-type */
} _Korl_Vulkan_DeviceMemory_Allocation_Type;
typedef struct _Korl_Vulkan_DeviceMemory_Alloctation
{
    /* @polymorphic-tagged-union
        We essentially need "virtual constructor/destructor" logic for this 
        data type, since this logic is specific to the device object type (we 
        need to use different Vulkan API for these tasks).
        It's worth noting that I have found this to be a common enough design 
        pattern that at one point I created a simple C++ parser which 
        automatically generated dispatch routines for virtual functions (kcpp?), 
        which theoretically might make development of these constructs easier.  
        Although, I am not yet _entirely_ convinced that this is the case just 
        yet.  Generated v-tables are not a panacea, and still don't properly 
        solve issues associated with function pointer tables, such as hot code 
        reloading, so such a feature might need a bit more effort to make it 
        easy-to-use like C++ virtuals, but also not complete shit under the hood 
        that we can't fix (like C++ virtuals).  */
    _Korl_Vulkan_DeviceMemory_Allocation_Type type;
    union 
    {
        VkBuffer buffer;
        struct
        {
            VkImage image;
            VkImageView imageView;
            VkSampler sampler;
        } texture;
        struct
        {
            VkImage image;
            VkImageView imageView;
        } imageBuffer;
        /* @vulkan-device-allocation-type */
    } deviceObject;
    VkDeviceSize byteOffset;
    VkDeviceSize byteSize;
} _Korl_Vulkan_DeviceMemory_Alloctation;
/**
 * This is the "dumbest" possible allocator - we literally just allocate a big 
 * chunk of memory, and then bind (allocate) new device objects one right after 
 * another contiguously.  We don't do any defragmentation, and we don't even 
 * have the ability to "free" objects, really (except when the entire allocator 
 * needs to be destroyed).  But, for the purposes of prototyping, this will do 
 * just fine for now.  
 * @todo: if we ever start having more "shippable" asset pipelines, we will need 
 *        a more intelligent allocator which allows things like:
 *        - dynamic growth
 *        - "freeing" allocations
 *        - defragmentation
 */
typedef struct _Korl_Vulkan_DeviceMemoryLinear
{
    VkDeviceMemory deviceMemory;
    /** passed as the first parameter to \c _korl_vulkan_findMemoryType */
    u32 memoryTypeBits;
    /** passed as the second parameter to \c _korl_vulkan_findMemoryType */
    VkMemoryPropertyFlags memoryPropertyFlags;
    VkDeviceSize byteSize;
    VkDeviceSize bytesAllocated;
    KORL_MEMORY_POOL_DECLARE(_Korl_Vulkan_DeviceMemory_Alloctation, allocations, 64);
} _Korl_Vulkan_DeviceMemoryLinear;
/**
 * \param bufferUsageFlags If this allocator is going to be used to allocate any 
 * VkBuffer objects, then this value MUST be populated with the set of all 
 * possible flags that any allocated buffer might use.  If no flags are 
 * specified, then the allocator cannot be guaranteed to allow VkBuffer 
 * allocations.
 * \param imageUsageFlags Similar to \c bufferUsageFlags , except for VkImage.
 */
korl_internal void _korl_vulkan_deviceMemoryLinear_create(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, 
    VkMemoryPropertyFlagBits memoryPropertyFlags, 
    VkBufferUsageFlags bufferUsageFlags, 
    VkImageUsageFlags imageUsageFlags, 
    VkDeviceSize bytes);
korl_internal void _korl_vulkan_deviceMemoryLinear_destroy(_Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear);
korl_internal void _korl_vulkan_deviceMemoryLinear_clear(_Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear);
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemoryLinear_allocateBuffer(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, VkDeviceSize bytes, 
    VkBufferUsageFlags bufferUsageFlags, VkSharingMode sharingMode);
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemoryLinear_allocateTexture(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, 
    u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags);
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemoryLinear_allocateImageBuffer(
    _Korl_Vulkan_DeviceMemoryLinear*const deviceMemoryLinear, 
    u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags, 
    VkImageAspectFlags imageAspectFlags, VkFormat imageFormat, 
    VkImageTiling imageTiling);
