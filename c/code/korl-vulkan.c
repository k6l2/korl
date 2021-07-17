#include "korl-io.h"
#include "korl-assert.h"
#include <vulkan/vulkan.h>
#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
#endif// defined(_WIN32)
typedef struct _Korl_Vulkan_Context _Korl_Vulkan_Context;
struct _Korl_Vulkan_Context
{
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
#if KORL_DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
    VkDebugUtilsMessengerEXT debugMessenger;
#endif// KORL_DEBUG
};
korl_global_variable _Korl_Vulkan_Context g_korl_vulkan_context;
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
    const VkPhysicalDeviceProperties*const propertiesOld, 
    const VkPhysicalDeviceProperties*const propertiesNew/*, 
    const VkPhysicalDeviceFeatures*const featuresOld, 
    const VkPhysicalDeviceFeatures*const featuresNew*/)
{
    korl_assert(deviceNew != VK_NULL_HANDLE);
    KORL_ZERO_STACK(u32, queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        deviceNew, &queueFamilyCount, 
        NULL/*pQueueFamilyProperties; NULL->return the count via param 2*/);
    VkQueueFamilyProperties queueFamilies[64];
    korl_assert(queueFamilyCount <= korl_arraySize(queueFamilies));
    vkGetPhysicalDeviceQueueFamilyProperties(
        deviceNew, &queueFamilyCount, queueFamilies);
    bool hasGraphicsQueue = false;
    for(u32 q = 0; q < queueFamilyCount; q++)
        if(queueFamilies[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            hasGraphicsQueue = true;
    /* don't even consider using devices that can't render graphics */
    if(!hasGraphicsQueue)
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
    const char* enabledLayers[] = 
        { "VK_LAYER_KHRONOS_validation" };
    for(u32 el = 0; el < korl_arraySize(enabledLayers); el++)
    {
        bool enabledLayerSupported = false;
        for(u32 l = 0; l < layerCount; l++)
            if(strcmp(enabledLayers[el], layerProperties[l].layerName))
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
    createInfoInstance.enabledLayerCount       = korl_arraySize(enabledLayers);
    createInfoInstance.ppEnabledLayerNames     = enabledLayers;
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
            &createInfoInstance, NULL/*pAllocator*/, &context->instance);
    korl_assert(vkResult == VK_SUCCESS);
#if KORL_DEBUG
    /* get debug extension function pointers */
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = 
        KORL_C_CAST(PFN_vkCreateDebugUtilsMessengerEXT, 
            vkGetInstanceProcAddr(
                context->instance, "vkCreateDebugUtilsMessengerEXT"));
    korl_assert(vkCreateDebugUtilsMessengerEXT);
    context->vkDestroyDebugUtilsMessengerEXT = 
        KORL_C_CAST(PFN_vkDestroyDebugUtilsMessengerEXT, 
            vkGetInstanceProcAddr(
                context->instance, "vkDestroyDebugUtilsMessengerEXT"));
    korl_assert(context->vkDestroyDebugUtilsMessengerEXT);
    /* attach a VkDebugUtilsMessengerEXT to our instance */
    vkResult = 
        vkCreateDebugUtilsMessengerEXT(
            context->instance, &createInfoDebugUtilsMessenger, 
            NULL/*pAllocator*/, &context->debugMessenger);
    korl_assert(vkResult == VK_SUCCESS);
#endif// KORL_DEBUG
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
    for(u32 d = 0; d < physicalDeviceCount; d++)
    {
        KORL_ZERO_STACK(VkPhysicalDeviceProperties, deviceProperties);
        KORL_ZERO_STACK(VkPhysicalDeviceFeatures, deviceFeatures);
        vkGetPhysicalDeviceProperties(physicalDevices[d], &deviceProperties);
        vkGetPhysicalDeviceFeatures(physicalDevices[d], &deviceFeatures);
        if(_korl_vulkan_isBetterDevice(
                context->physicalDevice, physicalDevices[d], 
                &devicePropertiesBest, &deviceProperties/*, 
                &deviceFeaturesBest, &deviceFeatures*/))
        {
            context->physicalDevice = physicalDevices[d];
            devicePropertiesBest = deviceProperties;
            deviceFeaturesBest = deviceFeatures;
        }
    }
    korl_assert(context->physicalDevice != VK_NULL_HANDLE);
    korl_log(INFO, "chosen physical device: \"%s\"", 
        devicePropertiesBest.deviceName);
}
korl_internal void korl_vulkan_destroy(void)
{
    _Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
#if KORL_DEBUG
    context->vkDestroyDebugUtilsMessengerEXT(
        context->instance, context->debugMessenger, NULL/*pAllocator*/);
#endif// KORL_DEBUG
    vkDestroyInstance(context->instance, NULL/*pAllocator*/);
    /* nullify the context after cleaning up properly for safety */
    memset(context, 0, sizeof(*context));
}
