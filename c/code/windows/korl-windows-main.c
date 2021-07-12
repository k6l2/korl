#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-io.h"
#include "korl-memory.h"
#include "korl-windows-window.h"
#include <vulkan/vulkan.h>
#include "korl-assert.h"
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    korl_log(INFO, "start");
    korl_memory_initialize();
    /* test vulkan */
    {
        KORL_ZERO_STACK(uint32_t, vkExtensionCount);
        VkResult vkResult = 
            vkEnumerateInstanceExtensionProperties(
                NULL, &vkExtensionCount, NULL);
        korl_assert(vkResult == VK_SUCCESS);
        korl_log(INFO, "vkExtensionCount=%u", vkExtensionCount);
    }
    korl_windows_window_initialize(); 
    korl_windows_window_create(1024, 576);
    korl_windows_window_loop();
    korl_log(INFO, "end");
    ExitProcess(KORL_EXIT_SUCCESS);
}
#include "korl-assert.c"
#include "korl-io.c"
#include "korl-windows-utilities.c"
#include "korl-math.c"
#include "korl-memory.c"
#include "korl-windows-window.c"