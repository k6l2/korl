/**
 * This header is used internally by vulkan code modules.
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-memoryPool.h"
#include "korl-vulkan-memory.h"
#include "korl-math.h"
#include "korl-vulkan.h"
#include "korl-stringPool.h"
#define _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE 4
typedef enum _Korl_Vulkan_DescriptorSetIndex
    /* ideally, these descriptor sets indices are defined in the order of least- 
        to most-frequently-changed for performance reasons; see 
        "Pipeline Layout Compatibility" in the Vulkan spec for more details */
    { _KORL_VULKAN_DESCRIPTOR_SET_INDEX_UBO_VP_TRANSFORMS
    , _KORL_VULKAN_DESCRIPTOR_SET_INDEX_VERTEX_SSBO
    , _KORL_VULKAN_DESCRIPTOR_SET_INDEX_FRAGMENT_SAMPLERS
    , _KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT// Keep last!
} _Korl_Vulkan_DescriptorSetIndex;
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
        itself: */
    VkPrimitiveTopology primitiveTopology;
    u8 positionDimensions;        // only acceptable values: {2, 3}
    u8 instancePositionDimensions;// only acceptable values: {2, 3}
    u32 positionsStride;       // 0 => absence of this attribute
    u32 uvsStride;             // 0 => absence of this attribute
    u32 colorsStride;          // 0 => absence of this attribute
    u32 instancePositionStride;// 0 => absence of this attribute
    u32 instanceUintStride;    // 0 => absence of this attribute
    bool useTexture;
    Korl_Vulkan_DrawState_Features features;
    Korl_Vulkan_DrawState_Blend blend;
} _Korl_Vulkan_Pipeline;
typedef struct _Korl_Vulkan_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
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
    //KORL-ISSUE-000-000-014: move everything below this point into \c _Korl_Vulkan_SurfaceContext ???
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
    //KORL-ISSUE-000-000-015: add shader module collection
    VkShaderModule shaderVertex2d;
    VkShaderModule shaderVertex3d;
    VkShaderModule shaderVertex3dColor;
    VkShaderModule shaderVertex3dUv;
    VkShaderModule shaderVertexText;
    VkShaderModule shaderFragmentColor;
    VkShaderModule shaderFragmentColorTexture;
    _Korl_Vulkan_Pipeline* stbDaPipelines;
    /* pipeline layouts (uniform data) are (potentially) shared between pipelines */
    VkPipelineLayout pipelineLayout;
    /** layouts for the descriptor data which are shared between all KORL Vulkan Surfaces
     * (UBO, view projection, texture samplers, etc...) */
    VkDescriptorSetLayout descriptorSetLayouts[_KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT];
    /* render passes are (potentially) shared between pipelines */
    VkRenderPass renderPass;
    /** Primarily used to store device asset names; not sure if this will be 
     * used for anything else in the future... */
    Korl_StringPool stringPool;
#if 0///@TODO: delete/recycle
    Korl_Vulkan_TextureHandle textureHandleDefaultTexture;
#endif
} _Korl_Vulkan_Context;
/** 
 * Make sure to ensure memory alignment of these data members according to GLSL 
 * data alignment spec after this struct definition!  See Vulkan Specification 
 * `15.6.4. Offset and Stride Assignment` for details.
 */
typedef struct _Korl_Vulkan_SwapChainImageUniformTransforms
{
    Korl_Math_M4f32 m4f32Projection;
    Korl_Math_M4f32 m4f32View;
    //KORL-PERFORMANCE-000-000-010: pre-calculate the ViewProjection matrix
} _Korl_Vulkan_SwapChainImageUniformTransforms;
/* Ensure _Korl_Vulkan_SwapChainImageUniformTransforms member alignment here: */
_STATIC_ASSERT((offsetof(_Korl_Vulkan_SwapChainImageUniformTransforms, m4f32Projection) & 16) == 0);
_STATIC_ASSERT((offsetof(_Korl_Vulkan_SwapChainImageUniformTransforms, m4f32View      ) & 16) == 0);
typedef struct _Korl_Vulkan_DrawPushConstants
{
    Korl_Math_M4f32 m4f32Model;
} _Korl_Vulkan_DrawPushConstants;
typedef struct _Korl_Vulkan_DescriptorPool
{
    VkDescriptorPool vkDescriptorPool;
    u$ setsAllocated;
} _Korl_Vulkan_DescriptorPool;
typedef struct _Korl_Vulkan_SwapChainImageContext
{
    VkImageView   imageView;
    VkFramebuffer frameBuffer;
    VkCommandPool commandPool;// the command buffers in this pool should all be considered _transient_; the command pool will be cleared at the start of each frame
    VkCommandBuffer commandBufferGraphics;
    VkCommandBuffer commandBufferTransfer;
    /// @TODO: it may or may not be better to handle descriptor pools similar to stbDaStagingBuffers (at the SurfaceContext level), 
    ///        and just fill each pool until it is full, then move onto the next pool, allocating more pools as-needed with frames sharing pools; 
    ///        theoretically this should lead to pools being reset less often, which intuitively seems like better performance to me...
    _Korl_Vulkan_DescriptorPool* stbDaDescriptorPools;// these will all get reset at the beginning of each frame
} _Korl_Vulkan_SwapChainImageContext;
/** 
 * the contents of this struct are expected to be nullified at the end of each 
 * call to \c frameBegin() 
 */
typedef struct _Korl_Vulkan_SurfaceContextDrawState
{
    /** 
     * This is an index into the array of pipelines currently in the 
     * _Korl_Vulkan_Context. This can essentially be our batch "render state".  
     * If this value is >= \c arrlenu(context->stbDaPipelines) , that 
     * means there is no valid render state. 
     */
    u$ currentPipeline;
    _Korl_Vulkan_Pipeline pipelineConfigurationCache;
    /** ----- dynamic uniform state (push constants, etc...) ----- */
    _Korl_Vulkan_DrawPushConstants pushConstants;
    VkRect2D scissor;
    /** ----- descriptor state ----- */
    _Korl_Vulkan_SwapChainImageUniformTransforms uboTransforms;
    Korl_Vulkan_DeviceAssetHandle texture;
    Korl_Vulkan_DeviceAssetHandle vertexStorageBuffer;
#if 0///@TODO: delete/recycle
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
    /* descriptor set state cache; used to prevent the descriptor set from 
        flushing unless we are _actually_ changing the state of the descriptor 
        set, since this is likely to be an expensive operation */
    Korl_Math_M4f32 m4f32Projection;
    Korl_Math_M4f32 m4f32View;
    Korl_Math_M4f32 m4f32Model;
    VkImageView textureImageView;
    VkSampler   textureSampler;
#endif
} _Korl_Vulkan_SurfaceContextDrawState;
/**
 * Each buffer acts as a linear allocator
 */
typedef struct _Korl_Vulkan_Buffer
{
    _Korl_Vulkan_DeviceMemory_AllocationHandle allocation;
    // total bytes occupied by this buffer should be contained within the allocation
    VkDeviceSize bytesUsed;
    /** Care must be taken with this value so that it doesn't overflow.  By 
     * incrementing this value each frame, we can know how many frames ago this 
     * buffer was used, and we can use this value to determine whether or not 
     * the GPU can possibly be still using the data contained within it, since 
     * the swap chain size is a small finite number, and WIP frames are 
     * processed in order. */
    u8 framesSinceLastUsed;
} _Korl_Vulkan_Buffer;
typedef struct _Korl_Vulkan_DeviceAsset
{
    _Korl_Vulkan_DeviceMemory_AllocationHandle deviceAllocation;
    bool nullify;// Used to defer ultimate destruction of device assets until we can deduce that the _must_ no longer be in use (using framesSinceLastUsed)
    u8 framesSinceLastUsed;
    u8 salt;
    union
    {
        struct
        {
            /** A \c stride value of \c 0 here indicates that this buffer does not 
             * contain the vertex attribute at that respective index of this array */
            struct
            {
                u$ offset;
                u32 stride;
            } attributeDescriptors[KORL_VULKAN_VERTEX_ATTRIBUTE_ENUM_COUNT];
        } buffer;
    } subType;
} _Korl_Vulkan_DeviceAsset;
typedef struct _Korl_Vulkan_DeviceAssetDatabase
{
    void* memoryContext;
    _Korl_Vulkan_DeviceMemory_Allocator* deviceMemoryAllocator;
    _Korl_Vulkan_DeviceAsset* stbDaAssets;
    u8 nextSalt;
} _Korl_Vulkan_DeviceAssetDatabase;
typedef struct _Korl_Vulkan_QueuedTextureUpload
{
    VkBuffer        bufferTransferFrom;
    VkDeviceSize    bufferStagingOffset;
    Korl_Math_V2u32 imageSize;
    VkImage         imageTransferTo;
} _Korl_Vulkan_QueuedTextureUpload;
/**
 * It makes sense for this data structure to be separate from the 
 * \c Korl_Vulkan_Context , as this state needs to be created on a per-window 
 * basis, so there will potentially be a collection of these which all need to 
 * be created, destroyed, & managed separately.
 */
typedef struct _Korl_Vulkan_SurfaceContext
{
    VkSurfaceKHR surface;
    Korl_Math_V3f32 frameBeginClearColor;
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
    VkImage                            swapChainImages       [_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    // VkCommandBuffer                    swapChainCommandBuffers[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];///@TODO: delete/recycle
    _Korl_Vulkan_SwapChainImageContext swapChainImageContexts[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    unsigned wipFrameCurrent;// this # will increase each frame, then get modded by swapChainImagesSize
    unsigned wipFrameCount;  // the # of frames that are potentially WIP; this # will start at 0, then quickly grow until it == swapChainImagesSize, allowing us to know which frame fence to wait on (if at all) to acquire the next image
    struct
    {
        VkSemaphore semaphoreTransfersDone;// tells the primary graphics command buffer submission that memory transfers are complete for this frame
        VkSemaphore semaphoreImageAvailable;
        VkSemaphore semaphoreRenderDone;
        VkFence     fenceFrameComplete;
    } wipFrames[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    bool deferredResize;
    u32 deferredResizeX, deferredResizeY;
    /** expected to be nullified at the end of each call to \c frameBegin() */
    _Korl_Vulkan_SurfaceContextDrawState drawState;
    _Korl_Vulkan_SurfaceContextDrawState drawStateLast;// assigned at the end of every draw call; used to determine if we need to allocate/bind a new/different descriptorSet/pipeline
#if 0///@TODO: delete/recycle
    bool hasStencilComponent;//KORL-ISSUE-000-000-018: unused
#endif
    /** this allocator will maintain device-local objects required during the 
     * render process, such as depth buffers, etc. */
    _Korl_Vulkan_DeviceMemory_Allocator deviceMemoryRenderResources;
    _Korl_Vulkan_DeviceMemory_AllocationHandle depthStencilImageBuffer;
    /** Used for allocation of host-visible staging buffers */
    _Korl_Vulkan_DeviceMemory_Allocator deviceMemoryHostVisible;
    _Korl_Vulkan_Buffer* stbDaStagingBuffers;
    u16 stagingBufferIndexLastUsed;// used to more evenly distribute the usage of staging buffers instead of more heavily utilizing some far more than others
    /** Used for allocation of device-local assets, such as textures, mesh 
     * manifolds, SSBOs, etc... */
    _Korl_Vulkan_DeviceMemory_Allocator deviceMemoryDeviceLocal;
    _Korl_Vulkan_DeviceAssetDatabase deviceAssetDatabase;
    Korl_Vulkan_DeviceAssetHandle defaultTexture;
    ///@TODO: we could theoretically refactor korl-vulkan such that \c korl_vulkan_frameBegin is no longer a thing (or at least is an internal API which is only ever called once during initialization, & once during frameEnd to "begin" the next frame)
    ///       and if we are able to do this, there would be absolutely no need for \c stbDaQueuedTextureUploads (it could be removed entirely)
    ///       in addition, this would allow us to completely remove the API requirement that certain functions must be called during frameBegin/End, 
    ///       which would also allow us to remove \c frameStackCounter
    _Korl_Vulkan_QueuedTextureUpload* stbDaQueuedTextureUploads;// this collection will be cleared each time frameEnd is called during a valid frame, where it will be used to compose memory transfer commands for the current frame's commandBufferTransfer
} _Korl_Vulkan_SurfaceContext;
korl_global_variable _Korl_Vulkan_Context g_korl_vulkan_context;
/** 
 * for now we'll just have one global surface context, since the KORL 
 * application will only use one window 
 * */
korl_global_variable _Korl_Vulkan_SurfaceContext g_korl_vulkan_surfaceContext;
