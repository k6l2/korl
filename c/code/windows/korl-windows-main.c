#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-io.h"
#include "korl-memory.h"
#include "korl-windows-window.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include "korl-assert.h"
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    korl_log(INFO, "start");
    korl_memory_initialize();
    /* allocate vulkan */
    KORL_ZERO_STACK(VkInstance, instance);
    {
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
        VkResult vkResult = 
            vkCreateInstance(
                &instanceCreateInfo, NULL/*pAllocator*/, &instance);
        korl_assert(vkResult == VK_SUCCESS);
    }
    korl_windows_window_initialize(); 
    korl_windows_window_create(1024, 576);
    korl_windows_window_loop();
    korl_log(INFO, "end");
    /* nullify vulkan */
    {
        vkDestroyInstance(instance, NULL/*pAllocator*/);
    }
    ExitProcess(KORL_EXIT_SUCCESS);
}
#include "korl-assert.c"
#include "korl-io.c"
#include "korl-windows-utilities.c"
#include "korl-math.c"
#include "korl-memory.c"
#include "korl-windows-window.c"