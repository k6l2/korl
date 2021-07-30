#include "korl-io.h"
#include "korl-assert.h"
#include "korl-vulkan-common.h"
#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
#endif// defined(_WIN32)
korl_global_const char* G_KORL_VULKAN_ENABLED_LAYERS[] = 
    { "VK_LAYER_KHRONOS_validation" };
korl_global_const char* G_KORL_VULKAN_DEVICE_EXTENSIONS[] = 
    { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#define KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, proc)                      \
    {                                                                          \
        context->vk##proc =                                                    \
            (PFN_vk##proc)vkGetInstanceProcAddr(context->instance, "vk"#proc); \
        korl_assert(context->vk##proc);                                        \
    }
#if KORL_DEBUG
korl_internal VkBool32 VKAPI_CALL _korl_vulkan_debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData)
{
    // pUserData is currently unused, and we don't pass one so it should be NULL
    korl_assert(!pUserData);
    char* messageTypeString = "GVP";
    if(!(messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT))
        messageTypeString[0] = ' ';
    if(!(messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT))
        messageTypeString[1] = ' ';
    if(!(messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT))
        messageTypeString[2] = ' ';
    if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        korl_log(ERROR  , "{%s} %s", messageTypeString, pCallbackData->pMessage);
    else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        korl_log(WARNING, "{%s} %s", messageTypeString, pCallbackData->pMessage);
    else
        korl_log(INFO   , "{%s} %s", messageTypeString, pCallbackData->pMessage);
    /* according to documentation, we MUST always return VK_FALSE! */
    return VK_FALSE;
}
#endif// KORL_DEBUG
/** 
 * \return true if \c deviceNew is a "better" device than \c deviceOld
 */
korl_internal bool _korl_vulkan_isBetterDevice(
    const VkPhysicalDevice deviceOld, 
    const VkPhysicalDevice deviceNew, 
    const _Korl_Vulkan_QueueFamilyMetaData*const queueFamilyMetaData, 
    const _Korl_Vulkan_DeviceSurfaceMetaData*const deviceSurfaceMetaData, 
    const VkPhysicalDeviceProperties*const propertiesOld, 
    const VkPhysicalDeviceProperties*const propertiesNew/*, 
    const VkPhysicalDeviceFeatures*const featuresOld, 
    const VkPhysicalDeviceFeatures*const featuresNew*/)
{
    korl_assert(deviceNew != VK_NULL_HANDLE);
    VkResult vkResult = VK_SUCCESS;
    /* don't even consider devices that don't support all the extensions we 
        need */
    VkExtensionProperties extensionProperties[256];
    KORL_ZERO_STACK(u32, extensionPropertiesSize);
    vkResult = 
        vkEnumerateDeviceExtensionProperties(
            deviceNew, NULL/*pLayerName*/, &extensionPropertiesSize, 
            NULL/*pProperties*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(extensionPropertiesSize <= korl_arraySize(extensionProperties));
    extensionPropertiesSize = korl_arraySize(extensionProperties);
    vkResult = 
        vkEnumerateDeviceExtensionProperties(
            deviceNew, NULL/*pLayerName*/, &extensionPropertiesSize, 
            extensionProperties);
    korl_assert(vkResult == VK_SUCCESS);
    bool deviceNewSupportsExtensions = true;
    for(size_t e = 0; e < korl_arraySize(G_KORL_VULKAN_DEVICE_EXTENSIONS); e++)
    {
        bool extensionFound = false;
        for(u32 de = 0; de < extensionPropertiesSize; de++)
            if(strcmp(extensionProperties[de].extensionName, 
                      G_KORL_VULKAN_DEVICE_EXTENSIONS[e]) == 0)
            {
                extensionFound = true;
                break;
            }
        if(!extensionFound)
        {
            deviceNewSupportsExtensions = false;
            break;
        }
    }
    if(!deviceNewSupportsExtensions)
        return false;
    /* don't consider devices that don't have the ability to create a swap chain 
        for the surface we provided earlier */
    if(    deviceSurfaceMetaData->formatsSize      == 0 
        || deviceSurfaceMetaData->presentModesSize == 0)
        return false;
    /* don't even consider using devices that can't render graphics */
    if(!queueFamilyMetaData->hasIndexQueueGraphics)
        return false;
    /* if the old device hasn't been selected, just use the new device */
    if(deviceOld == VK_NULL_HANDLE)
        return true;
    /* always attempt to use a discrete GPU first */
    if(    propertiesNew->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && propertiesOld->deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return true;
    return false;
}
korl_internal _Korl_Vulkan_DeviceSurfaceMetaData _korl_vulkan_deviceSurfaceMetaData(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    KORL_ZERO_STACK(_Korl_Vulkan_DeviceSurfaceMetaData, result);
    VkResult vkResult = VK_SUCCESS;
    /* query for device-surface capabilities */
    vkResult = 
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice, surface, &result.capabilities);
    korl_assert(vkResult == VK_SUCCESS);
    /* query for device-surface formats */
    vkResult = 
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &result.formatsSize, 
            NULL/*pSurfaceFormats*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(result.formatsSize <= korl_arraySize(result.formats));
    result.formatsSize = korl_arraySize(result.formats);
    vkResult = 
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &result.formatsSize, result.formats);
    korl_assert(vkResult == VK_SUCCESS);
    /* query for device-surface present modes */
    vkResult = 
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surface, &result.presentModesSize, 
            NULL/*pPresentModes*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(result.presentModesSize <= korl_arraySize(result.presentModes));
    result.presentModesSize = korl_arraySize(result.presentModes);
    vkResult = 
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surface, &result.presentModesSize, 
            result.presentModes);
    korl_assert(vkResult == VK_SUCCESS);
    return result;
}
korl_internal _Korl_Vulkan_QueueFamilyMetaData _korl_vulkan_queueFamilyMetaData(
    VkPhysicalDevice physicalDevice, 
    const u32 queueFamilyCount, 
    const VkQueueFamilyProperties*const queueFamilies, 
    VkSurfaceKHR surface)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    VkResult vkResult = VK_SUCCESS;
    KORL_ZERO_STACK(_Korl_Vulkan_QueueFamilyMetaData, result);
    for(u32 q = 0; q < queueFamilyCount; q++)
    {
        if(!result.hasIndexQueueGraphics 
            && (queueFamilies[q].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            result.hasIndexQueueGraphics = true;
            result.indexQueueUnion.indexQueues.graphics = q;
        }
        if(!result.hasIndexQueuePresent)
        {
            KORL_ZERO_STACK(VkBool32, surfaceSupport);
            vkResult = 
                context->vkGetPhysicalDeviceSurfaceSupportKHR(
                    physicalDevice, q, surface, &surfaceSupport);
            korl_assert(vkResult == VK_SUCCESS);
            if(surfaceSupport)
            {
                result.hasIndexQueuePresent = true;
                result.indexQueueUnion.indexQueues.present = q;
            }
        }
    }
    return result;
}
korl_internal void korl_vulkan_construct(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    VkResult vkResult = VK_SUCCESS;
    /* nullify the context memory before doing anything for safety */
    memset(context, 0, sizeof(*context));
    /* get a list of VkLayerProperties so we can check of validation layer 
        support if needed */
    KORL_ZERO_STACK(u32, layerCount);
    vkResult = 
        vkEnumerateInstanceLayerProperties(
            &layerCount, 
            NULL/*pProperties; NULL->return # of properties in param 1*/);
    korl_assert(vkResult == VK_SUCCESS);
    VkLayerProperties layerProperties[128];
    korl_assert(layerCount <= korl_arraySize(layerProperties));
    layerCount = korl_arraySize(layerProperties);
    vkResult = 
        vkEnumerateInstanceLayerProperties(&layerCount, layerProperties);
    korl_assert(vkResult == VK_SUCCESS);
    korl_log(INFO, "Provided layers supported by this platform "
        "{\"name\"[specVersion, implementationVersion]}:");
    for(u32 l = 0; l < layerCount; l++)
    {
        korl_log(INFO, "layer[%u]=\"%s\"[%u, %u]", l, 
            layerProperties[l].layerName, layerProperties[l].specVersion, 
            layerProperties[l].implementationVersion);
    }
    /* select layers & check to see if they are supported */
    for(u32 el = 0; el < korl_arraySize(G_KORL_VULKAN_ENABLED_LAYERS); el++)
    {
        bool enabledLayerSupported = false;
        for(u32 l = 0; l < layerCount; l++)
            if(strcmp(G_KORL_VULKAN_ENABLED_LAYERS[el], 
                      layerProperties[l].layerName))
            {
                enabledLayerSupported = true;
                break;
            }
        korl_assert(enabledLayerSupported);
    }
    /* log the list of instance extensions supported by the platform */
    KORL_ZERO_STACK(u32, extensionCount);
    VkExtensionProperties extensionProperties[128];
    vkResult = 
        vkEnumerateInstanceExtensionProperties(
            NULL/*pLayerName*/, &extensionCount, 
            NULL/*pProperties; NULL->return # of properties in param 2*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(extensionCount <= korl_arraySize(extensionProperties));
    extensionCount = korl_arraySize(extensionProperties);
    vkResult = 
        vkEnumerateInstanceExtensionProperties(
            NULL/*pLayerName*/, &extensionCount, extensionProperties);
    korl_assert(vkResult == VK_SUCCESS);
    korl_log(INFO, "Provided extensions supported by this platform "
        "{\"name\"[specVersion]}:");
    for(u32 e = 0; e < extensionCount; e++)
        korl_log(INFO, "extension[%u]=\"%s\"[%u]", e, 
            extensionProperties[e].extensionName, 
            extensionProperties[e].specVersion);
    /* create the VkInstance */
    /* @robustness: cross-check this list of extension names with the 
        extensionProperties queried above to check if they are all provided by 
        the platform */
    const char* enabledExtensions[] = 
        { VK_KHR_SURFACE_EXTENSION_NAME
#if defined(_WIN32)
        , VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif// defined(_WIN32)
#if KORL_DEBUG
        , VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif// KORL_DEBUG
        };
    KORL_ZERO_STACK(VkApplicationInfo, applicationInfo);
    applicationInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "KORL Application";
    applicationInfo.pEngineName      = "KORL";
    applicationInfo.engineVersion    = 0;
    applicationInfo.apiVersion       = VK_API_VERSION_1_2;
    KORL_ZERO_STACK(VkInstanceCreateInfo, createInfoInstance);
    createInfoInstance.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfoInstance.pApplicationInfo        = &applicationInfo;
    createInfoInstance.enabledExtensionCount   = korl_arraySize(enabledExtensions);
    createInfoInstance.ppEnabledExtensionNames = enabledExtensions;
#if KORL_DEBUG
    createInfoInstance.enabledLayerCount       = korl_arraySize(G_KORL_VULKAN_ENABLED_LAYERS);
    createInfoInstance.ppEnabledLayerNames     = G_KORL_VULKAN_ENABLED_LAYERS;
    KORL_ZERO_STACK(VkDebugUtilsMessengerCreateInfoEXT, createInfoDebugUtilsMessenger);
    createInfoDebugUtilsMessenger.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfoDebugUtilsMessenger.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfoDebugUtilsMessenger.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfoDebugUtilsMessenger.pfnUserCallback = _korl_vulkan_debugUtilsMessengerCallback;
    /* append the debug utils messenger create info to the instance create info 
        so we can get debug info messages in create/destroy instance API */
    createInfoDebugUtilsMessenger.pNext = createInfoInstance.pNext;
    createInfoInstance.pNext = &createInfoDebugUtilsMessenger;
#endif// KORL_DEBUG
    vkResult = 
        vkCreateInstance(
            &createInfoInstance, context->allocator, &context->instance);
    korl_assert(vkResult == VK_SUCCESS);
    /* get instance function pointers */
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfaceSupportKHR);
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfaceFormatsKHR);
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, GetPhysicalDeviceSurfacePresentModesKHR);
#if KORL_DEBUG
    /* get debug extension function pointers */
    // note: there's no need to store this in the context since we're never 
    //  going to call it again //
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = 
        KORL_C_CAST(PFN_vkCreateDebugUtilsMessengerEXT, 
            vkGetInstanceProcAddr(
                context->instance, "vkCreateDebugUtilsMessengerEXT"));
    korl_assert(vkCreateDebugUtilsMessengerEXT);
    KORL_VULCAN_GET_INSTANCE_PROC_ADDR(context, DestroyDebugUtilsMessengerEXT);
    /* attach a VkDebugUtilsMessengerEXT to our instance */
    vkResult = 
        vkCreateDebugUtilsMessengerEXT(
            context->instance, &createInfoDebugUtilsMessenger, 
            context->allocator, &context->debugMessenger);
    korl_assert(vkResult == VK_SUCCESS);
#endif// KORL_DEBUG
}
korl_internal void _korl_vulkan_destroySwapChainDependencies(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    vkDestroyPipeline(context->device, context->pipeline, context->allocator);
    vkDestroyPipelineLayout(context->device, context->pipelineLayout, context->allocator);
    vkDestroyRenderPass(context->device, context->renderPass, context->allocator);
}
korl_internal void korl_vulkan_destroy(void)
{
    _korl_vulkan_destroySwapChainDependencies();
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    vkDestroyShaderModule(context->device, context->shaderTriangleVert, context->allocator);
    vkDestroyShaderModule(context->device, context->shaderTriangleFrag, context->allocator);
    vkDestroyCommandPool(context->device, context->commandPool, context->allocator);
    vkDestroyDevice(context->device, context->allocator);
#if KORL_DEBUG
    context->vkDestroyDebugUtilsMessengerEXT(
        context->instance, context->debugMessenger, context->allocator);
#endif// KORL_DEBUG
    vkDestroyInstance(context->instance, context->allocator);
    /* nullify the context after cleaning up properly for safety */
    memset(context, 0, sizeof(*context));
}
korl_internal void korl_vulkan_createDevice(VkSurfaceKHR surface)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    VkResult vkResult = VK_SUCCESS;
    /* enumerate & choose a physical device */
    KORL_ZERO_STACK(u32, physicalDeviceCount);
    vkResult = 
        vkEnumeratePhysicalDevices(
            context->instance, &physicalDeviceCount, 
            NULL/*pPhysicalDevices; NULL->return the count in param 2*/);
    korl_assert(vkResult == VK_SUCCESS);
    // we need at least one physical device compatible with Vulkan
    korl_assert(physicalDeviceCount > 0);
    VkPhysicalDevice physicalDevices[16];
    korl_assert(physicalDeviceCount <= korl_arraySize(physicalDevices));
    physicalDeviceCount = korl_arraySize(physicalDevices);
    vkResult = 
        vkEnumeratePhysicalDevices(
            context->instance, &physicalDeviceCount, physicalDevices);
    korl_assert(vkResult == VK_SUCCESS);
    KORL_ZERO_STACK(VkPhysicalDeviceProperties, devicePropertiesBest);
    KORL_ZERO_STACK(VkPhysicalDeviceFeatures, deviceFeaturesBest);
    KORL_ZERO_STACK(u32, queueFamilyCountBest);
    for(u32 d = 0; d < physicalDeviceCount; d++)
    {
        KORL_ZERO_STACK(u32, queueFamilyCount);
        VkQueueFamilyProperties queueFamilies[32];
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevices[d], &queueFamilyCount, 
            NULL/*pQueueFamilyProperties; NULL->return the count via param 2*/);
        korl_assert(queueFamilyCount <= korl_arraySize(queueFamilies));
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevices[d], &queueFamilyCount, queueFamilies);
        _Korl_Vulkan_QueueFamilyMetaData queueFamilyMetaData = 
            _korl_vulkan_queueFamilyMetaData(
                physicalDevices[d], queueFamilyCount, queueFamilies, surface);
        KORL_ZERO_STACK(VkPhysicalDeviceProperties, deviceProperties);
        KORL_ZERO_STACK(VkPhysicalDeviceFeatures, deviceFeatures);
        vkGetPhysicalDeviceProperties(physicalDevices[d], &deviceProperties);
        vkGetPhysicalDeviceFeatures(physicalDevices[d], &deviceFeatures);
        _Korl_Vulkan_DeviceSurfaceMetaData deviceSurfaceMetaData = 
            _korl_vulkan_deviceSurfaceMetaData(physicalDevices[d], surface);
        if(_korl_vulkan_isBetterDevice(
                context->physicalDevice, physicalDevices[d], 
                &queueFamilyMetaData, &deviceSurfaceMetaData, 
                &devicePropertiesBest, &deviceProperties/*, 
                &deviceFeaturesBest, &deviceFeatures*/))
        {
            context->physicalDevice = physicalDevices[d];
            devicePropertiesBest = deviceProperties;
            deviceFeaturesBest = deviceFeatures;
            context->queueFamilyMetaData = queueFamilyMetaData;
            context->deviceSurfaceMetaData = deviceSurfaceMetaData;
        }
    }
    korl_assert(context->physicalDevice != VK_NULL_HANDLE);
    korl_log(INFO, "chosen physical device: \"%s\"", 
        devicePropertiesBest.deviceName);
    /* determine how many queue families we need, which determines how many 
        VkDeviceQueueCreateInfo structs we will need to create the logical 
        device with our specifications */
    VkDeviceQueueCreateInfo createInfoDeviceQueueFamilies[8];
    KORL_ZERO_STACK(u32, createInfoDeviceQueueFamiliesSize);
    // we're only going to make one queue for each queue family, so this array 
    //  size is just going to be size == 1 for now
    korl_shared_const f32 QUEUE_PRIORITIES[] = {1.f};
    for(u32 f = 0; 
    f < korl_arraySize(context->queueFamilyMetaData.indexQueueUnion.indices); 
    f++)
    {
        KORL_ZERO_STACK(bool, queueFamilyCreateInfoFound);
        for(u32 c = 0; c < createInfoDeviceQueueFamiliesSize; c++)
        {
            if(createInfoDeviceQueueFamilies[c].queueFamilyIndex == 
                context->queueFamilyMetaData.indexQueueUnion.indices[f])
            {
                queueFamilyCreateInfoFound = true;
                break;
            }
        }
        if(queueFamilyCreateInfoFound)
            continue;
        korl_assert(createInfoDeviceQueueFamiliesSize < 
            korl_arraySize(createInfoDeviceQueueFamilies));
        VkDeviceQueueCreateInfo*const createInfo = 
            &createInfoDeviceQueueFamilies[createInfoDeviceQueueFamiliesSize];
        memset(createInfo, 0, sizeof(*createInfo));
        createInfo->sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        createInfo->queueFamilyIndex = context->queueFamilyMetaData.indexQueueUnion.indices[f];
        createInfo->queueCount       = korl_arraySize(QUEUE_PRIORITIES);
        createInfo->pQueuePriorities = QUEUE_PRIORITIES;
        createInfoDeviceQueueFamiliesSize++;
    }
    /* create logical device */
    KORL_ZERO_STACK(VkPhysicalDeviceFeatures, physicalDeviceFeatures);
    // leave all device features turned off for now~
    KORL_ZERO_STACK(VkDeviceCreateInfo, createInfoDevice);
    createInfoDevice.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfoDevice.queueCreateInfoCount    = createInfoDeviceQueueFamiliesSize;
    createInfoDevice.pQueueCreateInfos       = createInfoDeviceQueueFamilies;
    createInfoDevice.pEnabledFeatures        = &physicalDeviceFeatures;
    createInfoDevice.enabledExtensionCount   = korl_arraySize(G_KORL_VULKAN_DEVICE_EXTENSIONS);
    createInfoDevice.ppEnabledExtensionNames = G_KORL_VULKAN_DEVICE_EXTENSIONS;
#if KORL_DEBUG
    createInfoDevice.enabledLayerCount       = korl_arraySize(G_KORL_VULKAN_ENABLED_LAYERS);
    createInfoDevice.ppEnabledLayerNames     = G_KORL_VULKAN_ENABLED_LAYERS;
#endif// KORL_DEBUG
    vkResult = 
        vkCreateDevice(
            context->physicalDevice, &createInfoDevice, 
            context->allocator, &context->device);
    korl_assert(vkResult == VK_SUCCESS);
    /* obtain opaque handles to the queues that we need */
    vkGetDeviceQueue(
        context->device, 
        context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics, 
        0/*queueIndex*/, &context->queueGraphics);
    vkGetDeviceQueue(
        context->device, 
        context->queueFamilyMetaData.indexQueueUnion.indexQueues.present, 
        0/*queueIndex*/, &context->queuePresent);
    /* create graphics command pool */
    KORL_ZERO_STACK(VkCommandPoolCreateInfo, createInfoCommandPool);
    createInfoCommandPool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfoCommandPool.queueFamilyIndex = context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics;
    vkResult = 
        vkCreateCommandPool(
            context->device, &createInfoCommandPool, context->allocator, 
            &context->commandPool);
}
korl_internal void korl_vulkan_destroySurface(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = 
        &g_korl_windows_vulkan_surfaceContext;
    for(size_t f = 0; f < _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES; f++)
    {
        vkDestroySemaphore(
            context->device, 
            surfaceContext->wipFramesSemaphoreImageAvailable[f], 
            context->allocator);
        vkDestroySemaphore(
            context->device, surfaceContext->wipFramesSemaphoreRenderDone[f], 
            context->allocator);
        vkDestroyFence(
            context->device, surfaceContext->wipFramesFence[f], 
            context->allocator);
    }
    vkDestroySurfaceKHR(
        context->instance, surfaceContext->surface, context->allocator);
    memset(surfaceContext, 0, sizeof(*surfaceContext));
}
/** This API will create the parts of the swap chain which can be destroyed & 
 * re-created in the event that the swap chain needs to be resized. */
korl_internal void _korl_vulkan_createSwapChainTransient(u32 sizeX, u32 sizeY)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = 
        &g_korl_windows_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    /* given what we know about the device & the surface, select the best 
        settings for the swap chain */
    korl_assert(context->deviceSurfaceMetaData.formatsSize > 0);
    surfaceContext->swapChainSurfaceFormat = context->deviceSurfaceMetaData.formats[0];
    for(u32 f = 0; f < context->deviceSurfaceMetaData.formatsSize; f++)
        if(    context->deviceSurfaceMetaData.formats[f].format == VK_FORMAT_B8G8R8A8_SRGB
            && context->deviceSurfaceMetaData.formats[f].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surfaceContext->swapChainSurfaceFormat = context->deviceSurfaceMetaData.formats[f];
            break;
        }
    // the only present mode REQUIRED by the spec to be supported
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    surfaceContext->swapChainImageExtent = context->deviceSurfaceMetaData.capabilities.currentExtent;
    if(    surfaceContext->swapChainImageExtent.width  == 0xFFFFFFFF 
        && surfaceContext->swapChainImageExtent.height == 0xFFFFFFFF)
    {
        surfaceContext->swapChainImageExtent.width = KORL_MATH_CLAMP(sizeX, 
            context->deviceSurfaceMetaData.capabilities.minImageExtent.width, 
            context->deviceSurfaceMetaData.capabilities.maxImageExtent.width);
        surfaceContext->swapChainImageExtent.height = KORL_MATH_CLAMP(sizeY, 
            context->deviceSurfaceMetaData.capabilities.minImageExtent.height, 
            context->deviceSurfaceMetaData.capabilities.maxImageExtent.height);
    }
    u32 minImageCount = 
        context->deviceSurfaceMetaData.capabilities.minImageCount + 1;
    if(    context->deviceSurfaceMetaData.capabilities.maxImageCount > 0 
        && context->deviceSurfaceMetaData.capabilities.maxImageCount < minImageCount)
        minImageCount = context->deviceSurfaceMetaData.capabilities.maxImageCount;
    /* create the swap chain */
    KORL_ZERO_STACK(VkSwapchainCreateInfoKHR, createInfoSwapChain);
    createInfoSwapChain.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfoSwapChain.surface          = surfaceContext->surface;
    createInfoSwapChain.minImageCount    = minImageCount;
    createInfoSwapChain.imageFormat      = surfaceContext->swapChainSurfaceFormat.format;
    createInfoSwapChain.imageColorSpace  = surfaceContext->swapChainSurfaceFormat.colorSpace;
    createInfoSwapChain.imageExtent      = surfaceContext->swapChainImageExtent;
    createInfoSwapChain.imageArrayLayers = 1;//non-stereoscopic
    createInfoSwapChain.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfoSwapChain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfoSwapChain.preTransform     = context->deviceSurfaceMetaData.capabilities.currentTransform;
    createInfoSwapChain.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfoSwapChain.presentMode      = presentMode;
    createInfoSwapChain.clipped          = VK_TRUE;
    createInfoSwapChain.oldSwapchain     = VK_NULL_HANDLE;
    if(context->queueFamilyMetaData.indexQueueUnion.indexQueues.graphics != 
        context->queueFamilyMetaData.indexQueueUnion.indexQueues.present)
    {
        createInfoSwapChain.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfoSwapChain.queueFamilyIndexCount = 2;
        createInfoSwapChain.pQueueFamilyIndices   = context->queueFamilyMetaData.indexQueueUnion.indices;
    }
    vkResult = 
        vkCreateSwapchainKHR(
            context->device, &createInfoSwapChain, 
            context->allocator, &surfaceContext->swapChain);
    korl_assert(vkResult == VK_SUCCESS);
    /* create render pass */
    KORL_ZERO_STACK(VkAttachmentDescription, colorAttachment);
    colorAttachment.format         = surfaceContext->swapChainSurfaceFormat.format;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    KORL_ZERO_STACK(VkAttachmentReference, attachmentReference);
    attachmentReference.attachment = 0;
    attachmentReference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    KORL_ZERO_STACK(VkSubpassDescription, subPass);
    subPass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPass.colorAttachmentCount = 1;
    subPass.pColorAttachments    = &attachmentReference;
    KORL_ZERO_STACK(VkSubpassDependency, subpassDependency);
    subpassDependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass    = 0;
    subpassDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    KORL_ZERO_STACK(VkRenderPassCreateInfo, createInfoRenderPass);
    createInfoRenderPass.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfoRenderPass.attachmentCount = 1;
    createInfoRenderPass.pAttachments    = &colorAttachment;
    createInfoRenderPass.subpassCount    = 1;
    createInfoRenderPass.pSubpasses      = &subPass;
    createInfoRenderPass.dependencyCount = 1;
    createInfoRenderPass.pDependencies   = &subpassDependency;
    vkResult = 
        vkCreateRenderPass(
            context->device, &createInfoRenderPass, context->allocator, 
            &context->renderPass);
    korl_assert(vkResult == VK_SUCCESS);
    /* get swap chain images */
    vkResult = 
        vkGetSwapchainImagesKHR(
            context->device, surfaceContext->swapChain, 
            &surfaceContext->swapChainImagesSize, NULL/*pSwapchainImages*/);
    korl_assert(vkResult == VK_SUCCESS);
    korl_assert(surfaceContext->swapChainImagesSize <= 
        korl_arraySize(surfaceContext->swapChainImages));
    surfaceContext->swapChainImagesSize = 
        korl_arraySize(surfaceContext->swapChainImages);
    vkResult = 
        vkGetSwapchainImagesKHR(
            context->device, surfaceContext->swapChain, 
            &surfaceContext->swapChainImagesSize, 
            surfaceContext->swapChainImages);
    korl_assert(vkResult == VK_SUCCESS);
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        /* create image views for all the swap chain images */
        KORL_ZERO_STACK(VkImageViewCreateInfo, createInfoImageView);
        createInfoImageView.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfoImageView.image                           = surfaceContext->swapChainImages[i];
        createInfoImageView.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        createInfoImageView.format                          = surfaceContext->swapChainSurfaceFormat.format;
        createInfoImageView.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfoImageView.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfoImageView.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfoImageView.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfoImageView.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfoImageView.subresourceRange.layerCount     = 1;
        createInfoImageView.subresourceRange.baseArrayLayer = 0;
        createInfoImageView.subresourceRange.levelCount     = 1;
        createInfoImageView.subresourceRange.baseMipLevel   = 0;
        vkResult = 
            vkCreateImageView(
                context->device, &createInfoImageView, context->allocator, 
                &surfaceContext->swapChainImageViews[i]);
        korl_assert(vkResult == VK_SUCCESS);
        /* create a frame buffer for all the swap chain image views */
        VkImageView frameBufferAttachments[] = 
            { surfaceContext->swapChainImageViews[i] };
        KORL_ZERO_STACK(VkFramebufferCreateInfo, createInfoFrameBuffer);
        createInfoFrameBuffer.sType      = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfoFrameBuffer.renderPass = context->renderPass;
        createInfoFrameBuffer.attachmentCount = korl_arraySize(frameBufferAttachments);
        createInfoFrameBuffer.pAttachments    = frameBufferAttachments;
        createInfoFrameBuffer.width           = surfaceContext->swapChainImageExtent.width;
        createInfoFrameBuffer.height          = surfaceContext->swapChainImageExtent.height;
        createInfoFrameBuffer.layers          = 1;
        vkResult = 
            vkCreateFramebuffer(
                context->device, &createInfoFrameBuffer, context->allocator, 
                &surfaceContext->swapChainFrameBuffers[i]);
        korl_assert(vkResult == VK_SUCCESS);
        /* initialize the swap chain fence references to nothing */
        surfaceContext->swapChainFences[i] = VK_NULL_HANDLE;
    }
    /* allocate a command buffer for each of the swap chain frame buffers */
    KORL_ZERO_STACK(VkCommandBufferAllocateInfo, allocateInfoCommandBuffers);
    allocateInfoCommandBuffers.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfoCommandBuffers.commandPool        = context->commandPool;
    allocateInfoCommandBuffers.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfoCommandBuffers.commandBufferCount = surfaceContext->swapChainImagesSize;
    vkResult = 
        vkAllocateCommandBuffers(
            context->device, &allocateInfoCommandBuffers, 
            surfaceContext->swapChainCommandBuffers);
    korl_assert(vkResult == VK_SUCCESS);
}
korl_internal void korl_vulkan_createSwapChain(u32 sizeX, u32 sizeY)
{
    _korl_vulkan_createSwapChainTransient(sizeX, sizeY);
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = 
        &g_korl_windows_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    /* create the semaphores we will use to sync rendering operations of the 
        swap chain */
    KORL_ZERO_STACK(VkSemaphoreCreateInfo, createInfoSemaphore);
    createInfoSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    KORL_ZERO_STACK(VkFenceCreateInfo, createInfoFence);
    createInfoFence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfoFence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(size_t f = 0; f < _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES; f++)
    {
        vkResult = 
            vkCreateSemaphore(
                context->device, &createInfoSemaphore, context->allocator, 
                &surfaceContext->wipFramesSemaphoreImageAvailable[f]);
        korl_assert(vkResult == VK_SUCCESS);
        vkResult = 
            vkCreateSemaphore(
                context->device, &createInfoSemaphore, context->allocator, 
                &surfaceContext->wipFramesSemaphoreRenderDone[f]);
        korl_assert(vkResult == VK_SUCCESS);
        vkResult = 
            vkCreateFence(
                context->device, &createInfoFence, context->allocator, 
                &surfaceContext->wipFramesFence[f]);
        korl_assert(vkResult == VK_SUCCESS);
    }
}
korl_internal void korl_vulkan_destroySwapChain(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = 
        &g_korl_windows_vulkan_surfaceContext;
    VkResult vkResult = vkDeviceWaitIdle(context->device);
    korl_assert(vkResult == VK_SUCCESS);
    vkFreeCommandBuffers(
        context->device, context->commandPool, 
        surfaceContext->swapChainImagesSize, 
        surfaceContext->swapChainCommandBuffers);
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        vkDestroyFramebuffer(
            context->device, surfaceContext->swapChainFrameBuffers[i], 
            context->allocator);
        vkDestroyImageView(
            context->device, surfaceContext->swapChainImageViews[i], 
            context->allocator);
    }
    vkDestroySwapchainKHR(
        context->device, surfaceContext->swapChain, context->allocator);
}
korl_internal void korl_vulkan_loadShaders(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    VkResult vkResult = VK_SUCCESS;
    /* @hack: just load shader files into memory right here */
    const Korl_File_Result spirvTriangleVertex   = korl_readEntireFile(L"triangle.vert.spv");
    const Korl_File_Result spirvTriangleFragment = korl_readEntireFile(L"triangle.frag.spv");
    /* create shader modules */
    KORL_ZERO_STACK(VkShaderModuleCreateInfo, createInfoShaderVert);
    createInfoShaderVert.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfoShaderVert.codeSize = spirvTriangleVertex.dataSize;
    createInfoShaderVert.pCode    = spirvTriangleVertex.data;
    vkResult = 
        vkCreateShaderModule(
            context->device, &createInfoShaderVert, context->allocator, 
            &context->shaderTriangleVert);
    korl_assert(vkResult == VK_SUCCESS);
    KORL_ZERO_STACK(VkShaderModuleCreateInfo, createInfoShaderFrag);
    createInfoShaderFrag.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfoShaderFrag.codeSize = spirvTriangleFragment.dataSize;
    createInfoShaderFrag.pCode    = spirvTriangleFragment.data;
    vkResult = 
        vkCreateShaderModule(
            context->device, &createInfoShaderFrag, context->allocator, 
            &context->shaderTriangleFrag);
    korl_assert(vkResult == VK_SUCCESS);
}
korl_internal void korl_vulkan_createPipeline(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = 
        &g_korl_windows_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    /* set fixed functions */
    KORL_ZERO_STACK(VkPipelineVertexInputStateCreateInfo, createInfoVertexInput);
    createInfoVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    // @hack: we don't set any other members here since we're hard-coding the 
    //        vertex data in the vertex shader!
    KORL_ZERO_STACK(VkPipelineInputAssemblyStateCreateInfo, createInfoInputAssembly);
    createInfoInputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    createInfoInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewPort;
    viewPort.x        = 0.f;
    viewPort.y        = 0.f;
    viewPort.width    = KORL_C_CAST(float, surfaceContext->swapChainImageExtent.width);
    viewPort.height   = KORL_C_CAST(float, surfaceContext->swapChainImageExtent.height);
    viewPort.minDepth = 0.f;
    viewPort.maxDepth = 1.f;
    VkRect2D scissor;
    scissor.extent   = surfaceContext->swapChainImageExtent;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    KORL_ZERO_STACK(VkPipelineViewportStateCreateInfo, createInfoViewport);
    createInfoViewport.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    createInfoViewport.viewportCount = 1;
    createInfoViewport.pViewports    = &viewPort;
    createInfoViewport.scissorCount  = 1;
    createInfoViewport.pScissors     = &scissor;
    KORL_ZERO_STACK(VkPipelineRasterizationStateCreateInfo, createInfoRasterizer);
    createInfoRasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    createInfoRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    createInfoRasterizer.lineWidth   = 1.f;
    createInfoRasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
    createInfoRasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;//right-handed triangles!
    KORL_ZERO_STACK(VkPipelineMultisampleStateCreateInfo, createInfoMultisample);
    createInfoMultisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    createInfoMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    createInfoMultisample.minSampleShading     = 1.f;
    // we have only one framebuffer, so we only need one attachment:
    // enable alpha blending
    KORL_ZERO_STACK(VkPipelineColorBlendAttachmentState, colorBlendAttachment);
    colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    KORL_ZERO_STACK(VkPipelineColorBlendStateCreateInfo, createInfoColorBlend);
    createInfoColorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    createInfoColorBlend.attachmentCount = 1;
    createInfoColorBlend.pAttachments    = &colorBlendAttachment;
    VkDynamicState dynamicStates[] = 
        { VK_DYNAMIC_STATE_VIEWPORT
        , VK_DYNAMIC_STATE_LINE_WIDTH };
    KORL_ZERO_STACK(VkPipelineDynamicStateCreateInfo, createInfoDynamicState);
    createInfoDynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    createInfoDynamicState.dynamicStateCount = korl_arraySize(dynamicStates);
    createInfoDynamicState.pDynamicStates    = dynamicStates;
    /* create pipeline layout */
    KORL_ZERO_STACK(VkPipelineLayoutCreateInfo, createInfoPipelineLayout);
    createInfoPipelineLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vkResult = 
        vkCreatePipelineLayout(
            context->device, &createInfoPipelineLayout, context->allocator, 
            &context->pipelineLayout);
    korl_assert(vkResult == VK_SUCCESS);
    /* create pipeline */
    KORL_ZERO_STACK_ARRAY(
        VkPipelineShaderStageCreateInfo, createInfoShaderStages, 2);
    createInfoShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfoShaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    createInfoShaderStages[0].module = context->shaderTriangleVert;
    createInfoShaderStages[0].pName  = "main";
    createInfoShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfoShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    createInfoShaderStages[1].module = context->shaderTriangleFrag;
    createInfoShaderStages[1].pName  = "main";
    KORL_ZERO_STACK(VkGraphicsPipelineCreateInfo, createInfoPipeline);
    createInfoPipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfoPipeline.stageCount = korl_arraySize(createInfoShaderStages);
    createInfoPipeline.pStages    = createInfoShaderStages;
    createInfoPipeline.pVertexInputState = &createInfoVertexInput;
    createInfoPipeline.pInputAssemblyState = &createInfoInputAssembly;
    createInfoPipeline.pViewportState      = &createInfoViewport;
    createInfoPipeline.pRasterizationState = &createInfoRasterizer;
    createInfoPipeline.pMultisampleState   = &createInfoMultisample;
    createInfoPipeline.pColorBlendState    = &createInfoColorBlend;
    createInfoPipeline.layout              = context->pipelineLayout;
    createInfoPipeline.renderPass          = context->renderPass;
    createInfoPipeline.subpass             = 0;
    vkResult = 
        vkCreateGraphicsPipelines(
            context->device, VK_NULL_HANDLE/*pipeline cache*/, 
            1, &createInfoPipeline, context->allocator, &context->pipeline);
}
korl_internal void korl_vulkan_recordAllCommandBuffers(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = 
        &g_korl_windows_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    for(u32 i = 0; i < surfaceContext->swapChainImagesSize; i++)
    {
        KORL_ZERO_STACK(VkCommandBufferBeginInfo, beginInfoCommandBuffer);
        beginInfoCommandBuffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkResult = 
            vkBeginCommandBuffer(
                surfaceContext->swapChainCommandBuffers[i], 
                &beginInfoCommandBuffer);
        korl_assert(vkResult == VK_SUCCESS);
        /* define the color we are going to clear the color attachment with when 
            the render pass begins */
        KORL_ZERO_STACK(VkClearValue, clearValue);
        clearValue.color.float32[0] = 0.05f;
        clearValue.color.float32[2] = 0.05f;
        clearValue.color.float32[3] = 1.f;
        KORL_ZERO_STACK(VkRenderPassBeginInfo, beginInfoRenderPass);
        beginInfoRenderPass.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfoRenderPass.renderPass        = context->renderPass;
        beginInfoRenderPass.framebuffer       = surfaceContext->swapChainFrameBuffers[i];
        beginInfoRenderPass.renderArea.extent = surfaceContext->swapChainImageExtent;
        beginInfoRenderPass.clearValueCount   = 1;
        beginInfoRenderPass.pClearValues      = &clearValue;
        vkCmdBeginRenderPass(
            surfaceContext->swapChainCommandBuffers[i], &beginInfoRenderPass, 
            VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(
            surfaceContext->swapChainCommandBuffers[i], 
            VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipeline);
        vkCmdDraw(
            surfaceContext->swapChainCommandBuffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(surfaceContext->swapChainCommandBuffers[i]);
        vkResult = 
            vkEndCommandBuffer(surfaceContext->swapChainCommandBuffers[i]);
        korl_assert(vkResult == VK_SUCCESS);
    }
}
korl_internal void korl_vulkan_draw(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = 
        &g_korl_windows_vulkan_surfaceContext;
    VkResult vkResult = VK_SUCCESS;
    if(surfaceContext->deferredResize)
    {
        /* destroy swap chain & all resources which depend on it */
        korl_vulkan_destroySwapChain();
        _korl_vulkan_destroySwapChainDependencies();
        /* re-create the swap chain & all resources which depend on it */
        _korl_vulkan_createSwapChainTransient(
            surfaceContext->deferredResizeX, surfaceContext->deferredResizeY);
        korl_vulkan_createPipeline();
        korl_vulkan_recordAllCommandBuffers();
        /* clear the deferred resize flag for the next frame */
        surfaceContext->deferredResize = false;
    }
    /* wait on the fence for the current WIP frame */
    vkResult = 
        vkWaitForFences(
            context->device, 1, 
            &surfaceContext->wipFramesFence[surfaceContext->wipFrameCurrent], 
            VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/);
    korl_assert(vkResult == VK_SUCCESS);
    /* acquire the next image from the swap chain */
    KORL_ZERO_STACK(u32, nextImageIndex);
    vkResult = 
        vkAcquireNextImageKHR(
            context->device, surfaceContext->swapChain, 
            UINT64_MAX/*timeout; UINT64_MAX -> disable*/, 
            surfaceContext->wipFramesSemaphoreImageAvailable[surfaceContext->wipFrameCurrent], 
            VK_NULL_HANDLE/*fence*/, &nextImageIndex);
    if(vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
        return;
    korl_assert(vkResult == VK_SUCCESS);
    if(surfaceContext->swapChainFences[nextImageIndex] != VK_NULL_HANDLE)
    {
        vkResult = 
            vkWaitForFences(
                context->device, 1, 
                &surfaceContext->swapChainFences[nextImageIndex], 
                VK_TRUE/*waitAll*/, UINT64_MAX/*timeout; max -> disable*/);
        korl_assert(vkResult == VK_SUCCESS);
    }
    surfaceContext->swapChainFences[nextImageIndex] = 
        surfaceContext->wipFramesFence[surfaceContext->wipFrameCurrent];
    /* submit graphics commands to the graphics queue */
    VkSemaphore submitGraphicsWaitSemaphores[] = 
        { surfaceContext->wipFramesSemaphoreImageAvailable[surfaceContext->wipFrameCurrent] };
    VkPipelineStageFlags submitGraphicsWaitStages[] = 
        { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore submitGraphicsSignalSemaphores[] = 
        { surfaceContext->wipFramesSemaphoreRenderDone[surfaceContext->wipFrameCurrent] };
    KORL_ZERO_STACK(VkSubmitInfo, submitInfoGraphics);
    submitInfoGraphics.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfoGraphics.waitSemaphoreCount   = korl_arraySize(submitGraphicsWaitSemaphores);
    submitInfoGraphics.pWaitSemaphores      = submitGraphicsWaitSemaphores;
    submitInfoGraphics.pWaitDstStageMask    = submitGraphicsWaitStages;
    submitInfoGraphics.commandBufferCount   = 1;
    submitInfoGraphics.pCommandBuffers      = &surfaceContext->swapChainCommandBuffers[nextImageIndex];
    submitInfoGraphics.signalSemaphoreCount = korl_arraySize(submitGraphicsSignalSemaphores);
    submitInfoGraphics.pSignalSemaphores    = submitGraphicsSignalSemaphores;
    // close the fence in preparation to submit commands to the graphics queue
    vkResult = 
        vkResetFences(
            context->device, 1, 
            &surfaceContext->wipFramesFence[surfaceContext->wipFrameCurrent]);
    korl_assert(vkResult == VK_SUCCESS);
    vkResult = 
        vkQueueSubmit(
            context->queueGraphics, 1, &submitInfoGraphics, 
            surfaceContext->wipFramesFence[surfaceContext->wipFrameCurrent]);
    korl_assert(vkResult == VK_SUCCESS);
    /* present the swap chain */
    VkSwapchainKHR presentInfoSwapChains[] = { surfaceContext->swapChain };
    KORL_ZERO_STACK(VkPresentInfoKHR, presentInfo);
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = submitGraphicsSignalSemaphores;
    presentInfo.swapchainCount     = korl_arraySize(presentInfoSwapChains);
    presentInfo.pSwapchains        = presentInfoSwapChains;
    presentInfo.pImageIndices      = &nextImageIndex;
    vkResult = vkQueuePresentKHR(context->queuePresent, &presentInfo);
    if(vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
        return;
    korl_assert(vkResult == VK_SUCCESS);
    /* advance to the next WIP frame index */
    surfaceContext->wipFrameCurrent = 
        (surfaceContext->wipFrameCurrent + 1) % 
        _KORL_VULKAN_SURFACECONTEXT_MAX_WIP_FRAMES;
}
korl_internal void korl_vulkan_deferredResize(u32 sizeX, u32 sizeY)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = 
        &g_korl_windows_vulkan_surfaceContext;
    surfaceContext->deferredResize = true;
    surfaceContext->deferredResizeX = sizeX;
    surfaceContext->deferredResizeY = sizeY;
}
