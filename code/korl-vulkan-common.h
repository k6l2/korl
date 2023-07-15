/**
 * This header is used internally by vulkan code modules.
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-vulkan.h"
#include "korl-vulkan-memory.h"
#include "utility/korl-utility-math.h"
#include "utility/korl-stringPool.h"
#include "korl-memoryPool.h"
#include "korl-interface-platform-gfx.h"
#define _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE 0
#define _KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE 4
/** These enum values correspond directly to the `layout(set = x)` value for 
 * uniform blocks in GLSL */
typedef enum _Korl_Vulkan_DescriptorSetIndex
    /* ideally, these descriptor sets indices are defined in the order of least- 
        to most-frequently-changed for performance reasons; see 
        "Pipeline Layout Compatibility" in the Vulkan spec for more details */
    {_KORL_VULKAN_DESCRIPTOR_SET_INDEX_SCENE
    ,_KORL_VULKAN_DESCRIPTOR_SET_INDEX_LIGHTS
    ,_KORL_VULKAN_DESCRIPTOR_SET_INDEX_STORAGE// used for things like glyph mesh lookup tables, maybe animation transforms, etc.; mostly stuff in vertex shader
    ,_KORL_VULKAN_DESCRIPTOR_SET_INDEX_MATERIAL// color maps, textures, etc.; mostly stuff used in fragment shader
    ,_KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT// Keep last!
} _Korl_Vulkan_DescriptorSetIndex;
KORL_STATIC_ASSERT(_KORL_VULKAN_DESCRIPTOR_SET_INDEX_ENUM_COUNT <= 4);// Vulkan Spec 42.1; maxBoundDescriptorSets minimum is 4; any higher than this is hardware-dependent
typedef enum _Korl_Vulkan_DescriptorSetBinding
    {_KORL_VULKAN_DESCRIPTOR_SET_BINDING_SCENE_PROPERTIES_UBO = 0
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_SCENE_TRANSFORMS_COUNT// keep last in the `SCENE_TRANSFORMS` section
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_LIGHTS_SSBO = 0
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_LIGHTS_COUNT// keep last in the `LIGHTS` section
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_VERTEX_STORAGE_SSBO = 0
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_VERTEX_STORAGE_COUNT// keep last in the `VERTEX_STORAGE` section
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_UBO = 0
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_BASE
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_SPECULAR
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_EMISSIVE
    ,_KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_COUNT// keep last in the `MATERIAL` section
} _Korl_Vulkan_DescriptorSetBinding;
#define _KORL_VULKAN_DESCRIPTOR_BINDING_TOTAL (  _KORL_VULKAN_DESCRIPTOR_SET_BINDING_SCENE_TRANSFORMS_COUNT\
                                               + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_LIGHTS_COUNT\
                                               + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_VERTEX_STORAGE_COUNT\
                                               + _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_COUNT)
korl_global_const VkDescriptorSetLayoutBinding _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_SCENE_TRANSFORMS[] = 
    {{.binding         = _KORL_VULKAN_DESCRIPTOR_SET_BINDING_SCENE_PROPERTIES_UBO
     ,.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
     ,.descriptorCount = 1
     ,.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}/*_Korl_Vulkan_Uniform_SceneProperties*/};
korl_global_const VkDescriptorSetLayoutBinding _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_LIGHTS[] = 
    {{.binding         = _KORL_VULKAN_DESCRIPTOR_SET_BINDING_LIGHTS_SSBO
     ,.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
     ,.descriptorCount = 1
     ,.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT}/*_Korl_Vulkan_SurfaceContextDrawState::lights, transcoded as _Korl_Vulkan_ShaderBuffer_Lights*/};
korl_global_const VkDescriptorSetLayoutBinding _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_STORAGE[] = 
    {{.binding         = _KORL_VULKAN_DESCRIPTOR_SET_BINDING_VERTEX_STORAGE_SSBO
     ,.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
     ,.descriptorCount = 1
     ,.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT}/*_Korl_Vulkan_SurfaceContextDrawState::vertexStorageBuffer*/};
korl_global_const VkDescriptorSetLayoutBinding _KORL_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDINGS_MATERIAL[] = 
    {{.binding         = _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_UBO
     ,.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
     ,.descriptorCount = 1
     ,.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT}/*Korl_Gfx_Material_Properties*/
    ,{.binding         = _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_BASE
     ,.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
     ,.descriptorCount = 1
     ,.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT}/*_Korl_Vulkan_SurfaceContextDrawState::materialMaps::base*/
    ,{.binding         = _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_SPECULAR
     ,.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
     ,.descriptorCount = 1
     ,.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT}/*_Korl_Vulkan_SurfaceContextDrawState::materialMaps::specular*/
    ,{.binding         = _KORL_VULKAN_DESCRIPTOR_SET_BINDING_MATERIAL_TEXTURE_EMISSIVE
     ,.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
     ,.descriptorCount = 1
     ,.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT}/*_Korl_Vulkan_SurfaceContextDrawState::materialMaps::emissive*/};
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
    Korl_Gfx_Material_Modes materialModes;
    VkShaderModule          shaderVertex;
    VkShaderModule          shaderFragment;
    struct
    {
        //note: `binding` & `input` values are implicit; those are just the index into this array!
        VkFormat          format;// VK_FORMAT_UNDEFINED => attribute binding unused by this pipeline
        u32               byteOffset;
        u32               byteStride;// aka per-(vertex|instance) byte offset
        VkVertexInputRate inputRate;
    } vertexAttributes[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT];
} _Korl_Vulkan_Pipeline;
typedef struct _Korl_Vulkan_Shader
{
    // potentially add some unique identification data here (like salt, etc.) to reduce the chance of the user attempting to use an invalid handle
    VkShaderModule shaderModule;// VK_NULL_HANDLE => this Shader is unoccupied
} _Korl_Vulkan_Shader;
typedef struct _Korl_Vulkan_ShaderTrash
{
    _Korl_Vulkan_Shader shader;
    u8                  framesSinceQueued;// once this value >= SurfaceContext::swapChainImagesSize, we can safely delete the shader module
} _Korl_Vulkan_ShaderTrash;
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
    // KORL-ISSUE-000-000-147: vulkan: delete all these shader modules; move the shader management task out to korl-gfx
    VkShaderModule shaderVertex2d;
    VkShaderModule shaderVertex2dColor;
    VkShaderModule shaderVertex2dUv;
    VkShaderModule shaderVertex3d;
    VkShaderModule shaderVertex3dColor;
    VkShaderModule shaderVertex3dUv;
    VkShaderModule shaderVertexText;
    VkShaderModule shaderFragmentColor;
    VkShaderModule shaderFragmentColorTexture;
    _Korl_Vulkan_Shader*      stbDaShaders;
    _Korl_Vulkan_ShaderTrash* stbDaShaderTrash;
    _Korl_Vulkan_Pipeline*    stbDaPipelines;
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
} _Korl_Vulkan_Context;
typedef struct _Korl_Vulkan_DescriptorPool
{
    VkDescriptorPool vkDescriptorPool;
    u$               setsAllocated;
} _Korl_Vulkan_DescriptorPool;
typedef struct _Korl_Vulkan_SwapChainImageContext
{
    VkImageView                  imageView;
    VkFramebuffer                frameBuffer;
    // KORL-PERFORMANCE-000-000-037: vulkan: it may or may not be better to handle descriptor pools similar to stbDaStagingBuffers (at the SurfaceContext level), and just fill each pool until it is full, then move onto the next pool, allocating more pools as-needed with frames sharing pools; theoretically this should lead to pools being reset less often, which intuitively seems like better performance to me...
    _Korl_Vulkan_DescriptorPool* stbDaDescriptorPools;// these will all get reset at the beginning of each frame
    #if KORL_DEBUG && _KORL_VULKAN_DEBUG_DEVICE_ASSET_IN_USE
        u$* stbDaInUseDeviceAssetIndices;
    #endif
} _Korl_Vulkan_SwapChainImageContext;
/** 
 * Make sure to ensure memory alignment of these data members according to GLSL 
 * data alignment spec after this struct definition!  See Vulkan Specification 
 * `15.6.4. Offset and Stride Assignment` for details.
 */
typedef struct _Korl_Vulkan_Uniform_SceneProperties
{
    Korl_Math_M4f32 m4f32Projection;
    Korl_Math_M4f32 m4f32View;
    f32             seconds;
    f32             _padding_0[3];
    //KORL-PERFORMANCE-000-000-010: pre-calculate the ViewProjection matrix
} _Korl_Vulkan_Uniform_SceneProperties;
/* Ensure _Korl_Vulkan_Uniform_SceneProperties member alignment here: */
KORL_STATIC_ASSERT((offsetof(_Korl_Vulkan_Uniform_SceneProperties, m4f32Projection) % 16) == 0);
KORL_STATIC_ASSERT((offsetof(_Korl_Vulkan_Uniform_SceneProperties, m4f32View      ) % 16) == 0);
typedef struct _Korl_Vulkan_DrawPushConstants
{
    struct
    {
        Korl_Math_M4f32 m4f32Model;
    } vertex;
    struct
    {
        //KORL-ISSUE-000-000-149: vulkan: HACK; uvAabb fragment shader push constant is hacky & very implementation-specific; I would much rather allow the user to be able to configure their own push constants or something
        Korl_Math_V4f32 uvAabb;// {x,y} => min, {z,w} => max; default = {0,0,1,1}
    } fragment;
} _Korl_Vulkan_DrawPushConstants;
KORL_STATIC_ASSERT(sizeof(_Korl_Vulkan_DrawPushConstants) <= 128);// vulkan spec 42.1 table 53; minimum limit for VkPhysicalDeviceLimits::maxPushConstantsSize
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
    u$                       currentPipeline;
    _Korl_Vulkan_Pipeline    pipelineConfigurationCache;
    Korl_Vulkan_ShaderHandle transientShaderHandleVertex;
    Korl_Vulkan_ShaderHandle transientShaderHandleFragment;
    /** ----- dynamic uniform state (push constants, etc...) ----- */
    _Korl_Vulkan_DrawPushConstants pushConstants;
    VkRect2D                       scissor;
    /** ----- descriptor state ----- */
    _Korl_Vulkan_Uniform_SceneProperties uboSceneProperties;
    Korl_Gfx_Material_Properties         uboMaterialProperties;
    KORL_MEMORY_POOL_DECLARE(Korl_Gfx_Light, lights, KORL_VULKAN_MAX_LIGHTS);
    struct
    {
        Korl_Vulkan_DeviceMemory_AllocationHandle base;
        Korl_Vulkan_DeviceMemory_AllocationHandle specular;
        Korl_Vulkan_DeviceMemory_AllocationHandle emissive;
    } materialMaps;
    Korl_Vulkan_DeviceMemory_AllocationHandle vertexStorageBuffer;
} _Korl_Vulkan_SurfaceContextDrawState;
/**
 * Each buffer acts as a linear allocator
 */
typedef struct _Korl_Vulkan_Buffer
{
    Korl_Vulkan_DeviceMemory_AllocationHandle allocation;
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
typedef struct _Korl_Vulkan_QueuedFreeDeviceLocalAllocation
{
    Korl_Vulkan_DeviceMemory_AllocationHandle allocationHandle;
    u8 framesSinceQueued;// once this # reaches the SurfaceContext's swapChainImagesSize, we know that this device memory _must_ no longer be in use, and so we can free it
} _Korl_Vulkan_QueuedFreeDeviceLocalAllocation;
typedef struct _Korl_Vulkan_DeviceLocalAllocationShallowDequeueBatch
{
    u$ dequeueCount;
    u8 framesSinceQueued;// once this # reaches the SurfaceContext's swapChainImagesSize, we know that this device memory _must_ no longer be in use, and so we can free it
} _Korl_Vulkan_DeviceLocalAllocationShallowDequeueBatch;
typedef struct _Korl_Vulkan_QueuedTextureUpload
{
    VkBuffer        bufferTransferFrom;
    VkDeviceSize    bufferStagingOffset;
    Korl_Math_V2u32 imageSize;
    VkImage         imageTransferTo;
} _Korl_Vulkan_QueuedTextureUpload;
typedef struct _Korl_Vulkan_QueuedBufferTransfer
{
    VkBuffer bufferSource;
    VkBuffer bufferTarget;
    VkBufferCopy copyRegion;
} _Korl_Vulkan_QueuedBufferTransfer;
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
    /** the index of the swap chain we are working with for the current frame */
    u32 frameSwapChainImageIndex;
    VkSurfaceFormatKHR swapChainSurfaceFormat;
    VkExtent2D         swapChainImageExtent;
    VkSwapchainKHR     swapChain;
    u32                                swapChainImagesSize;
    VkImage                            swapChainImages       [_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    _Korl_Vulkan_SwapChainImageContext swapChainImageContexts[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    unsigned wipFrameCurrent;// this # will increase each frame, then get modded by swapChainImagesSize
    unsigned wipFrameCount;  // the # of frames that are potentially WIP; this # will start at 0, then quickly grow until it == swapChainImagesSize, allowing us to know which frame fence to wait on (if at all) to acquire the next image
    struct
    {
        VkSemaphore semaphoreTransfersDone;// tells the primary graphics command buffer submission that memory transfers are complete for this frame
        VkSemaphore semaphoreImageAvailable;
        VkSemaphore semaphoreRenderDone;
        VkFence     fenceFrameComplete;
        VkCommandBuffer commandBufferTransfer;
        VkCommandBuffer commandBufferGraphics;
    } wipFrames[_KORL_VULKAN_SURFACECONTEXT_MAX_SWAPCHAIN_SIZE];
    bool deferredResize;
    u32 deferredResizeX, deferredResizeY;
    /** expected to be nullified at the end of each call to \c frameBegin() */
    _Korl_Vulkan_SurfaceContextDrawState drawState;
    _Korl_Vulkan_SurfaceContextDrawState drawStateLast;// assigned at the end of every draw call; used to determine if we need to allocate/bind a new/different descriptorSet/pipeline
    bool hasStencilComponent;//KORL-ISSUE-000-000-018: unused
    /** this allocator will maintain device-local objects required during the 
     * render process, such as depth buffers, etc. */
    _Korl_Vulkan_DeviceMemory_Allocator deviceMemoryRenderResources;
    Korl_Vulkan_DeviceMemory_AllocationHandle depthStencilImageBuffer;
    /** Used for allocation of host-visible staging buffers */
    _Korl_Vulkan_DeviceMemory_Allocator deviceMemoryHostVisible;
    _Korl_Vulkan_Buffer* stbDaStagingBuffers;
    u16 stagingBufferIndexLastUsed;// used to more evenly distribute the usage of staging buffers instead of more heavily utilizing some far more than others
    /** Used for allocation of device-local assets, such as textures, mesh 
     * manifolds, SSBOs, etc... */
    _Korl_Vulkan_DeviceMemory_Allocator deviceMemoryDeviceLocal;
    KORL_MEMORY_POOL_DECLARE(_Korl_Vulkan_DeviceLocalAllocationShallowDequeueBatch, deviceLocalAllocationShallowDequeueBatches, 8);// used to remember how many device-local allocations we performed a shallow free on, so that we can completely free them when we know they are definitely no longer in use
    _Korl_Vulkan_QueuedFreeDeviceLocalAllocation* stbDaDeviceLocalFreeQueue;// used to defer the destruction of device-local assets until we know that they are no longer in use
    Korl_Vulkan_DeviceMemory_AllocationHandle defaultTexture;
    VkCommandPool   commandPool;
} _Korl_Vulkan_SurfaceContext;
korl_global_variable _Korl_Vulkan_Context g_korl_vulkan_context;
/** 
 * for now we'll just have one global surface context, since the KORL 
 * application will only use one window 
 * */
korl_global_variable _Korl_Vulkan_SurfaceContext g_korl_vulkan_surfaceContext;
