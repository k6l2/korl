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
#include "korl-vulkan.h"// we need to put the device memory allocation handle in here since it is exposed to code that uses korl-vulkan; also contains vertex attribute enum count
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include <vulkan/vulkan.h>
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
    struct _Korl_Vulkan_DeviceMemory_Alloctation* stbDaShallowFreedAllocations;// these are allocations we have "freed", which allows us to create more allocations which can occupy the same device memory locations, but the Vulkan device objects have _not_ been destroyed yet
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
            VkBufferUsageFlags bufferUsageFlags;
            /** A \c stride value of \c 0 here indicates that this buffer does not 
             * contain the vertex attribute at that respective index of this array */
            struct
            {
                u$  offset;
                u32 stride;
            } attributeDescriptors[KORL_VULKAN_VERTEX_ATTRIBUTE_ENUM_COUNT];
        } buffer;
        struct
        {
            VkImage image;
            VkImageView imageView;
            VkSampler sampler;
            u32 sizeX;
            u32 sizeY;
        } texture;
        struct
        {
            VkImage image;
            VkImageView imageView;
        } imageBuffer;
        /* @vulkan-device-allocation-type */
    } subType;
    VkDeviceSize byteOffset;// this value will _always_ be aligned according to the memory requirements of the device object contained in this allocation
    VkDeviceSize bytesOccupied;// Derived from the Vulkan memory requirements of this allocation; it's entirely possible for an allocation to occupy more bytes than it utilizes!  This is mostly only useful for the internal allocation strategy.
    VkDeviceSize bytesUsed;// The size originally requested when the user called allocate.  The user is likely only going to care about this value for the purposes of manipulation of the data stored in this allocation.
    const wchar_t* file;// currently, korl-vulkan should not be exposed to any dynamic code modules, so we can safely just store raw file string pointers here
    int line;
    bool freeQueued;
} _Korl_Vulkan_DeviceMemory_Alloctation;
#define _korl_vulkan_deviceMemory_allocateBuffer(allocator, bytes, bufferUsageFlags, sharingMode, requiredHandle, out_allocation)                                                     _korl_vulkan_deviceMemory_allocator_allocateBuffer(allocator, bytes, bufferUsageFlags, sharingMode, requiredHandle, out_allocation, __FILEW__, __LINE__)
#define _korl_vulkan_deviceMemory_allocateTexture(allocator, imageSizeX, imageSizeY, imageUsageFlags, requiredHandle, out_allocation)                                                 _korl_vulkan_deviceMemory_allocator_allocateTexture(allocator, imageSizeX, imageSizeY, imageUsageFlags, requiredHandle, out_allocation, __FILEW__, __LINE__)
#define _korl_vulkan_deviceMemory_allocateImageBuffer(allocator, imageSizeX, imageSizeY, imageUsageFlags, imageAspectFlags, imageFormat, imageTiling, requiredHandle, out_allocation) _korl_vulkan_deviceMemory_allocator_allocateImageBuffer(allocator, imageSizeX, imageSizeY, imageUsageFlags, imageAspectFlags, imageFormat, imageTiling, requiredHandle, out_allocation, __FILEW__, __LINE__)
korl_internal _Korl_Vulkan_DeviceMemory_Allocator _korl_vulkan_deviceMemory_allocator_create(Korl_Memory_AllocatorHandle allocatorHandle
                                                                                            ,_Korl_Vulkan_DeviceMemory_AllocatorType type
                                                                                            ,VkMemoryPropertyFlagBits memoryPropertyFlags
                                                                                            ,VkBufferUsageFlags bufferUsageFlags
                                                                                            ,VkImageUsageFlags imageUsageFlags
                                                                                            ,VkDeviceSize bytesPerArena);
korl_internal void _korl_vulkan_deviceMemory_allocator_destroy(_Korl_Vulkan_DeviceMemory_Allocator* allocator);
//KORL-ISSUE-000-000-023: add support for memory allocators to defragment pages
korl_internal void _korl_vulkan_deviceMemory_allocator_clear(_Korl_Vulkan_DeviceMemory_Allocator* allocator);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateBuffer(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                          ,VkDeviceSize bytes, VkBufferUsageFlags bufferUsageFlags, VkSharingMode sharingMode
                                                                                                          ,Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle
                                                                                                          ,_Korl_Vulkan_DeviceMemory_Alloctation** out_allocation
                                                                                                          ,const wchar_t* file, int line);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateTexture(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                           ,u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags
                                                                                                           ,Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle
                                                                                                           ,_Korl_Vulkan_DeviceMemory_Alloctation** out_allocation
                                                                                                           ,const wchar_t* file, int line);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle _korl_vulkan_deviceMemory_allocator_allocateImageBuffer(_Korl_Vulkan_DeviceMemory_Allocator* allocator
                                                                                                               ,u32 imageSizeX, u32 imageSizeY, VkImageUsageFlags imageUsageFlags
                                                                                                               ,VkImageAspectFlags imageAspectFlags, VkFormat imageFormat
                                                                                                               ,VkImageTiling imageTiling
                                                                                                               ,Korl_Vulkan_DeviceMemory_AllocationHandle requiredHandle
                                                                                                               ,_Korl_Vulkan_DeviceMemory_Alloctation** out_allocation
                                                                                                               ,const wchar_t* file, int line);
korl_internal _Korl_Vulkan_DeviceMemory_Alloctation* _korl_vulkan_deviceMemory_allocator_getAllocation(_Korl_Vulkan_DeviceMemory_Allocator* allocator, Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle);
korl_internal void* _korl_vulkan_deviceMemory_allocator_getBufferHostVisibleAddress(_Korl_Vulkan_DeviceMemory_Allocator* allocator, Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle);
korl_internal void _korl_vulkan_deviceMemory_allocator_free(_Korl_Vulkan_DeviceMemory_Allocator* allocator, Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle);
/** Unlike \c _free , this API will only make it such that another allocation 
 * can occupy the exact same provided allocation handle.  This will effectively 
 * allow the user to immediately create new allocations that overwrite the data 
 * which old allocations' device objects are bound to.  Vulkan is perfectly okay 
 * with this, & I have observed that validation layers will not complain at all 
 * when this happens.  This call _must_ be followed by at least one call to 
 * \c _freeShallowDequeue where the sum total of the \c dequeueCount parameters 
 * of each of those calls is equal to the total # of invocations of this 
 * function in order to fully destroy the underlying vulkan device objects.  
 * Failure to do so should (fortunately) cause Vulkan validation errors when 
 * \c _destroy is called on the provided \c allocator .  
 * There are very few reasons why you would want to use these functions; the 
 * reason I am creating them is to achieve a full re-population of korl-vulkan 
 * device assets when loading a KORL memory state, such that all allocation 
 * handles within the memory state that is loaded can remain valid.  */
korl_internal void _korl_vulkan_deviceMemory_allocator_freeShallowQueueDestroyDeviceObjects(_Korl_Vulkan_DeviceMemory_Allocator* allocator, Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle);
korl_internal void _korl_vulkan_deviceMemory_allocator_freeShallowDequeue(_Korl_Vulkan_DeviceMemory_Allocator* allocator, u$ dequeueCount);
korl_internal void _korl_vulkan_deviceMemory_allocator_logReport(_Korl_Vulkan_DeviceMemory_Allocator* allocator);
#define _KORL_VULKAN_DEVICEMEMORY_ALLOCATOR_FOR_EACH_CALLBACK(name) void name(void* userData\
                                                                             ,_Korl_Vulkan_DeviceMemory_Allocator* allocator\
                                                                             ,_Korl_Vulkan_DeviceMemory_Alloctation* allocation\
                                                                             ,Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle)
typedef _KORL_VULKAN_DEVICEMEMORY_ALLOCATOR_FOR_EACH_CALLBACK(fnSig_korl_vulkan_deviceMemory_allocator_forEachCallback);
korl_internal void _korl_vulkan_deviceMemory_allocator_forEach(_Korl_Vulkan_DeviceMemory_Allocator* allocator\
                                                              ,fnSig_korl_vulkan_deviceMemory_allocator_forEachCallback* callback, void* callbackUserData);
