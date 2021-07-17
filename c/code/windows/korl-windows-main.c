#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-io.h"
#include "korl-memory.h"
#include "korl-windows-window.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include "korl-assert.h"
typedef struct Korl_Vulkan_Context Korl_Vulkan_Context;
struct Korl_Vulkan_Context
{
    VkInstance instance;
};
korl_global_variable Korl_Vulkan_Context g_korl_vulkan_context;
korl_internal void korl_vulkan_construct(void)
{
    Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    VkResult vkResult = VK_SUCCESS;
    memset(context, 0, sizeof(*context));
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
        , VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
    KORL_ZERO_STACK(VkApplicationInfo, applicationInfo);
    applicationInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "KORL Application";
    applicationInfo.pEngineName      = "KORL";
    applicationInfo.engineVersion    = 0;
    applicationInfo.apiVersion       = VK_API_VERSION_1_2;
    KORL_ZERO_STACK(VkInstanceCreateInfo, instanceCreateInfo);
    instanceCreateInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo        = &applicationInfo;
    instanceCreateInfo.enabledExtensionCount   = korl_arraySize(enabledExtensions);
    instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions;
    vkResult = 
        vkCreateInstance(
            &instanceCreateInfo, NULL/*pAllocator*/, &context->instance);
    korl_assert(vkResult == VK_SUCCESS);
}
korl_internal void korl_vulkan_destroy(void)
{
    Korl_Vulkan_Context*const context = &g_korl_vulkan_context;
    vkDestroyInstance(context->instance, NULL/*pAllocator*/);
}
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    korl_log(INFO, "start");
    korl_memory_initialize();
    korl_vulkan_construct();
    korl_windows_window_initialize(); 
    korl_windows_window_create(1024, 576);
    korl_windows_window_loop();
    korl_vulkan_destroy();
    korl_log(INFO, "end");
    ExitProcess(KORL_EXIT_SUCCESS);
}
#include "korl-assert.c"
#include "korl-io.c"
#include "korl-windows-utilities.c"
#include "korl-math.c"
#include "korl-memory.c"
#include "korl-windows-window.c"