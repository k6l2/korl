/**
 * The korl-vulkan-memory code module should only be used INTERNALLY by 
 * korl-vulkan* code!  Stay away from this API unless you are doing stuff in 
 * in korl-vulkan* code.
 * ______________________________ API ______________________________
 * _korl_vulkan_deviceMemory_allocator_create
 * _korl_vulkan_deviceMemory_allocator_destroy
 * _korl_vulkan_deviceMemory_allocator_clear
 * _korl_vulkan_deviceMemory_allocator_allocateBuffer
 * _korl_vulkan_deviceMemory_allocator_allocateTexture
 * _korl_vulkan_deviceMemory_allocator_allocateImageBuffer
 * _korl_vulkan_deviceMemory_allocateBuffer
 *     Convenience macro to auto-inject file/line meta data into each allocation.
 * _korl_vulkan_deviceMemory_allocateTexture
 *     Convenience macro to auto-inject file/line meta data into each allocation.
 * _korl_vulkan_deviceMemory_allocateImageBuffer
 *     Convenience macro to auto-inject file/line meta data into each allocation.
 * _korl_vulkan_deviceMemory_allocator_getAllocation : _Korl_Vulkan_DeviceMemory_Alloctation*
 *     Remember that the result address is _transient_!  The user should assume 
 *     that any call to the *_allocate* or *_free APIs will invalidate all 
 *     pointers returned by this function.
 * _korl_vulkan_deviceMemory_allocator_free
 * _korl_vulkan_deviceMemory_allocator_logReport
 * ______________________________ Data Structures ______________________________
 * _Korl_Vulkan_DeviceMemory_AllocatorType
 * _Korl_Vulkan_DeviceMemory_Allocator
 * _Korl_Vulkan_DeviceMemory_AllocationHandle
 *     As usual, a handle value of 0 is considered to be invalid.  
 *     Each handle is composed of the following parts: 
 *     - the index of the Arena that this allocation is contained within : 16 bits
 *     - the unique id assigned to the allocation within that Arena      : 16 bits
 *     - _Korl_Vulkan_DeviceMemory_Allocation_Type                       :  2 bits (for now at least)
 * _Korl_Vulkan_DeviceMemory_Allocation_Type
 * _Korl_Vulkan_DeviceMemory_Alloctation
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include <vulkan/vulkan.h>
typedef u64 _Korl_Vulkan_DeviceMemory_AllocationHandle;
typedef enum _Korl_Vulkan_DeviceMemory_AllocatorType
    { _KORL_VULKAN_DEVICE_MEMORY_ALLOCATOR_TYPE_GENERAL
} _Korl_Vulkan_DeviceMemory_AllocatorType;
typedef struct _Korl_Vulkan_DeviceMemory_Allocator
{
    _Korl_Vulkan_DeviceMemory_AllocatorType type;
    Korl_Memory_AllocatorHandle allocatorHandle;// manages the dynamic array of Arenas, and the dynamic array of Allocations within each Arena
    /** passed as the first parameter to \c _korl_vulkan_findMemoryType */
    u32 memoryTypeBits;
    /** passed as the second parameter to \c _korl_vulkan_findMemoryType */
    VkMemoryPropertyFlags memoryPropertyFlags;
    VkDeviceSize bytesPerArena;
    struct _Korl_Vulkan_DeviceMemory_Arena* stbDaArenas;
} _Korl_Vulkan_DeviceMemory_Allocator;
typedef enum _Korl_Vulkan_DeviceMemory_Allocation_Type
    { _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED
    , _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER
    , _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE
    , _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_IMAGE_BUFFER
    /* IMPORTANT: when adding new types to this enum, we must update the 
        appropriate tagged union in the allocation struct, as well as the code 
        which handles each type in the allocator constructor (and create a 
        corresponding allocation method for the new device object type) 
        @vulkan-device-allocation-type */
} _Korl_Vulkan_DeviceMemory_Allocation_Type;
typedef struct _Korl_Vulkan_DeviceMemory_Alloctation
{
    //KORL-ISSUE-000-000-021: polymorphic-tagged-union
    _Korl_Vulkan_DeviceMemory_Allocation_Type type;
    union 
    {
        struct
        {
            VkBuffer vulkanBuffer;
        } buffer;
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
    VkDeviceSize byteOffset;///@TODO: to reduce memory leaks in the Arena's general allocator (& general confusion), we should just get rid of `byteOffsetAligned` & create a new tiny UNALLOCATED allocation right before this allocation in the Arena's allocation list
    VkDeviceSize byteOffsetAligned;// this is the _actual_ byte offset used to bind the device object to the device memory!
    VkDeviceSize byteSize;
    u32 id;// assigned by the Arena that this allocation is contained within
    const wchar_t* file;// currently, korl-vulkan should not be exposed to any dynamic code modules, so we can safely just store raw file string pointers here
    int line;
} _Korl_Vulkan_DeviceMemory_Alloctation;
#define _korl_vulkan_deviceMemory_allocateBuffer(allocator, bytes, bufferUsageFlags, sharingMode)                                                     _korl_vulkan_deviceMemory_allocator_allocateBuffer(allocator, bytes, bufferUsageFlags, sharingMode, __FILEW__, __LINE__)
#define _korl_vulkan_deviceMemory_allocateTexture(allocator, imageSizeX, imageSizeY, imageUsageFlags)                                                 _korl_vulkan_deviceMemory_allocator_allocateTexture(allocator, imageSizeX, imageSizeY, imageUsageFlags, __FILEW__, __LINE__)
#define _korl_vulkan_deviceMemory_allocateImageBuffer(allocator, imageSizeX, imageSizeY, imageUsageFlags, imageAspectFlags, imageFormat, imageTiling) _korl_vulkan_deviceMemory_allocator_allocateImageBuffer(allocator, imageSizeX, imageSizeY, imageUsageFlags, imageAspectFlags, imageFormat, imageTiling, __FILEW__, __LINE__)
korl_internal _Korl_Vulkan_DeviceMemory_Allocator _korl_vulkan_deviceMemory_allocator_create(Korl_Memory_AllocatorHandle allocatorHandle
                                                                                            ,_Korl_Vulkan_DeviceMemory_AllocatorType type
                                                                                            ,VkMemoryPropertyFlagBits memoryPropertyFlags
                                                                                            ,VkBufferUsageFlags bufferUsageFlags
                                                                                            ,VkImageUsageFlags imageUsageFlags
                                                                                            ,VkDeviceSize bytesPerArena);
korl_internal void _korl_vulkan_deviceMemory_allocator_destroy(_Korl_Vulkan_DeviceMemory_Allocator* allocator);
//KORL-ISSUE-000-000-023: add support for memory allocators to defragment pages
korl_internal void _korl_vulkan_deviceMemory_allocator_clear(_Korl_Vulkan_DeviceMemory_Allocator* allocator);
korl_internal _Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateBuffer(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                           ,VkDeviceSize bytes, VkBufferUsageFlags bufferUsageFlags, VkSharingMode sharingMode
                                                                                                           ,const wchar_t* file, int line);
korl_internal _Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateTexture(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                            ,u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags
                                                                                                            ,const wchar_t* file, int line);
korl_internal _Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateImageBuffer(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                                ,u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags
                                                                                                                ,VkImageAspectFlags imageAspectFlags, VkFormat imageFormat
                                                                                                                ,VkImageTiling imageTiling
                                                                                                                ,const wchar_t* file, int line);
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemory_allocator_getAllocation(_Korl_Vulkan_DeviceMemory_Allocator* allocator, _Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle);
korl_internal void* _korl_vulkan_deviceMemory_allocator_getBufferHostVisibleAddress(_Korl_Vulkan_DeviceMemory_Allocator* allocator, _Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle, const _Korl_Vulkan_DeviceMemory_Alloctation* allocation);
korl_internal void _korl_vulkan_deviceMemory_allocator_free(_Korl_Vulkan_DeviceMemory_Allocator* allocator, _Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle);
korl_internal void _korl_vulkan_deviceMemory_allocator_logReport(_Korl_Vulkan_DeviceMemory_Allocator* allocator);
