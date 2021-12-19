/**
 * This header is used internally by vulkan code modules.
 */
#pragma once
#include "korl-globalDefines.h"
#include <vulkan/vulkan.h>
#define _KORL_VULKAN_MAX_ASSET_NAME_LENGTH 64
#define _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_STAGING 1024
#define _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTEX_INDICES_DEVICE 10*1024
#define _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_STAGING 1024
#define _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_VERTICES_DEVICE 10*1024
typedef enum _Korl_Vulkan_DeviceMemory_Allocation_Type
{
    _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_UNALLOCATED, 
    _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_VERTEX_BUFFER, 
    _KORL_VULKAN_DEVICEMEMORY_ALLOCATION_TYPE_TEXTURE
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
        struct Texture
        {
            VkImage image;
            VkImageView imageView;
            VkSampler sampler;
        } texture;
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
typedef struct _Korl_Vulkan_QueueFamilyMetaData
{
    /* unify the unique queue family index variables with an array so we can 
        easily iterate over them */
    union
    {
        struct
        {
            u32 graphics;
            u32 present;
        } indexQueues;
        u32 indices[2];
    } indexQueueUnion;
    bool hasIndexQueueGraphics;
    bool hasIndexQueuePresent;
} _Korl_Vulkan_QueueFamilyMetaData;
/** 
 * Using the contents of this struct, we should be able to fully create a 
 * conformant KORL pipeline.  This can essentially be used as the current 
 * "render state" of the vertex batching functionality as well.
 */
typedef struct _Korl_Vulkan_Pipeline
{
    /* actually store the pipeline handle in here */
    VkPipeline pipeline;
    /* ---------------------------------------------------------------------- */
    /* pipeline meta data which should be able to fully describe the pipeline 
        itself */
    VkPrimitiveTopology primitiveTopology;
    ///@todo: blend equations
    enum _Korl_Vulkan_Pipeline_OptionalVertexAttributeFlags
    {
        _KORL_VULKAN_PIPELINE_OPTIONALVERTEXATTRIBUTE_FLAG_COLOR = 1 << 0, 
        _KORL_VULKAN_PIPELINE_OPTIONALVERTEXATTRIBUTE_FLAG_UV    = 1 << 1
    } flagsOptionalVertexAttributes;
    /* ---------------------------------------------------------------------- */
    /* render state that has nothing to do with the pipeline itself */
    bool useIndexBuffer;
} _Korl_Vulkan_Pipeline;
typedef struct _Korl_Vulkan_DeviceAsset
{
    wchar_t name[_KORL_VULKAN_MAX_ASSET_NAME_LENGTH];
    _Korl_Vulkan_DeviceMemory_Alloctation* deviceAllocation;
} _Korl_Vulkan_DeviceAsset;
typedef struct _Korl_Vulkan_Context
{
    /**
     * This member is a placeholder and should be replaced by an object instead 
     * of a pointer, and the code which uses this member should be refactored 
     * appropriately when I decide to use a custom host memory allocator.
     */
    VkAllocationCallbacks* allocator;
    VkInstance instance;
    /* instance extension function pointers */
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR      vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
#if KORL_DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
    VkDebugUtilsMessengerEXT debugMessenger;
#endif// KORL_DEBUG
    /**
     * @todo: move everything below this point into \c _Korl_Vulkan_SurfaceContext ???
     */
    /* for now we're just going to have a single window with Vulkan rendering, 
        so we only have to make one device, ergo we only need one global 
        instance of these variables */
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    /* we save this data member because once we query for this meta data in the 
        device creation routines, we basically never have to query for it again 
        (unless we need to create a device again for some reason), and this data 
        is needed for swap chain creation, which is very likely to happen 
        multiple times during program execution */
    _Korl_Vulkan_QueueFamilyMetaData queueFamilyMetaData;
    VkQueue queueGraphics;
    VkQueue queuePresent;
    VkCommandPool commandPoolGraphics;
    /** Used to issue commands to transfer memory to device-local storage */
    VkCommandPool commandPoolTransfer;
    /**
     * @todo: store a collection of shader modules which can be referenced by 
     * external API, and store built-in shaders in known positions
     */
    VkShaderModule shaderBatchVertColor;
    VkShaderModule shaderBatchVertTexture;
    VkShaderModule shaderBatchFragColor;
    VkShaderModule shaderBatchFragTexture;
    /** @robustness: use KORL_MEMORY_POOL_* API */
    u32 pipelinesCount;
    _Korl_Vulkan_Pipeline pipelines[1024];
    /* pipeline layouts (uniform data) are (potentially) shared between 
        pipelines */
    VkPipelineLayout pipelineLayout;
    /** the layout for the descriptor data used in batch rendering which is 
     * shared between all KORL Vulkan Surfaces
     * (UBO, view projection, and currently texture asset) */
    VkDescriptorSetLayout batchDescriptorSetLayout;
    /* render passes are (potentially) shared between pipelines */
    VkRenderPass renderPass;
    /* we need a place to store assets which exist & persist between multiple 
        swap chain images */
    _Korl_Vulkan_DeviceMemoryLinear deviceMemoryLinearAssetsStaging;
    _Korl_Vulkan_DeviceMemoryLinear deviceMemoryLinearAssets;
    /* storage for assets that are exist on the device 
        (textures, shaders, buffers, etc) */
    KORL_MEMORY_POOL_DECLARE(_Korl_Vulkan_DeviceAsset, deviceAssets, 1024);
} _Korl_Vulkan_Context;
/** 
 * Make sure to ensure memory alignment of these data members according to GLSL 
 * data alignment spec after this struct definition!  See Vulkan Specification 
 * `15.6.4. Offset and Stride Assignment` for details.
 */
typedef struct _Korl_Vulkan_SwapChainImageBatchUbo
{
    Korl_Math_M4f32 m4f32Projection;
    Korl_Math_M4f32 m4f32View;
    /** @todo: pre-calculate the ViewProjection matrix */
} _Korl_Vulkan_SwapChainImageBatchUbo;
/* Ensure _Korl_Vulkan_SwapChainImageBatchUbo member alignment here: */
_STATIC_ASSERT((offsetof(_Korl_Vulkan_SwapChainImageBatchUbo, m4f32View) & 16) == 0);
typedef struct _Korl_Vulkan_SwapChainImageContext
{
    VkImageView imageView;
    VkFramebuffer frameBuffer;
    VkFence fence;
    _Korl_Vulkan_DeviceMemoryLinear deviceMemoryLinearHostVisible;
    _Korl_Vulkan_DeviceMemoryLinear deviceMemoryLinearDeviceLocal;
    _Korl_Vulkan_DeviceMemory_Alloctation* bufferStagingBatchIndices;
    _Korl_Vulkan_DeviceMemory_Alloctation* bufferStagingBatchPositions;
    _Korl_Vulkan_DeviceMemory_Alloctation* bufferStagingBatchColors;
    _Korl_Vulkan_DeviceMemory_Alloctation* bufferStagingBatchUvs;
    _Korl_Vulkan_DeviceMemory_Alloctation* bufferStagingUbo;
    _Korl_Vulkan_DeviceMemory_Alloctation* bufferDeviceBatchIndices;
    _Korl_Vulkan_DeviceMemory_Alloctation* bufferDeviceBatchPositions;
    _Korl_Vulkan_DeviceMemory_Alloctation* bufferDeviceBatchColors;
    _Korl_Vulkan_DeviceMemory_Alloctation* bufferDeviceBatchUvs;
} _Korl_Vulkan_SwapChainImageContext;
/** 
 * the contents of this struct are expected to be nullified at the end of each 
 * call to \c frameBegin() 
 */
typedef struct _Korl_Vulkan_SurfaceContextBatchState
{
    /** 
     * This is an index into the array of pipelines currently in the 
     * _Korl_Vulkan_Context. This can essentially be our batch "render state".  
     * If this value is equal to \c korl_arraySize(context->pipelines) , that 
     * means there is no valid render state. 
     */
    u32  currentPipeline;
    u32  vertexIndexCountStaging;
    u32  vertexIndexCountDevice;
    u32  vertexCountStaging;
    u32  vertexCountDevice;
    /* While the above metrics represent the TOTAL number of vertex 
        attributes/indices in batch buffers, the following metrics represent the 
        number of vertex attributes/indices in the current pipeline batch.  The 
        batch buffers will probably hold multiple pipeline batches. */
    u32  pipelineVertexIndexCount;
    u32  pipelineVertexCount;
    u32  descriptorSetIndexCurrent;
    bool descriptorSetIsUsed;// true if ANY geometry has been staged using the current working batch descriptor set
} _Korl_Vulkan_SurfaceContextBatchState;
#define _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE 4
#define _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES 2
#define _KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS 1024
/**
 * It makes sense for this data structure to be separate from the 
 * \c Korl_Vulkan_Context , as this state needs to be created on a per-window 
 * basis, so there will potentially be a collection of these which all need to 
 * be created, destroyed, & managed separately.
 */
typedef struct _Korl_Vulkan_SurfaceContext
{
    VkSurfaceKHR surface;
    /** 
     * Used to ensure that the user calls \c korl_vulkan_frameBegin before, and 
     * the same number of times as, \c korl_vulkan_frameEnd .  This value == 1 
     * if the last frame delimitter called was \c korl_vulkan_frameBegin , and 
     * == 0 if the last frame delimitter called was \c korl_vulkan_frameEnd .
     */
    u8 frameStackCounter;
    /** the index of the swap chain we are working with for the current frame */
    u32 frameSwapChainImageIndex;
    VkSurfaceFormatKHR swapChainSurfaceFormat;
    VkExtent2D         swapChainImageExtent;
    VkSwapchainKHR     swapChain;
    u32                                swapChainImagesSize;
    VkImage                            swapChainImages        [_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    VkCommandBuffer                    swapChainCommandBuffers[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    _Korl_Vulkan_SwapChainImageContext swapChainImageContexts [_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    /** 
     * Currently only used to manage batch rendering shared UBO descriptors, but 
     * probably might be expanded to manage even more.
     */
    VkDescriptorPool batchDescriptorPool;
    /** Used for batch rendering descriptor sets, such as shared UBO data, etc. */
    VkDescriptorSet batchDescriptorSets[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE][_KORL_VULKAN_SURFACECONTEXT_MAX_BATCH_DESCRIPTOR_SETS];
    unsigned wipFrameCurrent;
    struct
    {
        VkSemaphore semaphoreImageAvailable;
        VkSemaphore semaphoreRenderDone;
        VkFence     fence;
    } wipFrames[_KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES];
    bool deferredResize;
    u32 deferredResizeX, deferredResizeY;
    /** expected to be nullified at the end of each call to \c frameBegin() */
    _Korl_Vulkan_SurfaceContextBatchState batchState;
} _Korl_Vulkan_SurfaceContext;
korl_global_variable _Korl_Vulkan_Context g_korl_vulkan_context;
/** 
 * for now we'll just have one global surface context, since the KORL 
 * application will only use one window 
 * */
korl_global_variable _Korl_Vulkan_SurfaceContext g_korl_vulkan_surfaceContext;
