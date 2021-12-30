#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-io.h"
#include "korl-memory.h"
#include "korl-windows-window.h"
#include "korl-assert.h"
#include "korl-vulkan.h"
#include "korl-file.h"
#include "korl-assetCache.h"
#include "korl-stb-image.h"
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    korl_log(INFO, "start ---------------------------------------------------");
    korl_memory_initialize();
    korl_stb_image_initialize();// depends on memory module
    korl_file_initialize();// depends on memory module
    korl_assetCache_initialize();// depends on {memory, file, stb_image} modules
    korl_vulkan_construct();
    korl_windows_window_initialize();
    korl_windows_window_create(1024, 576);
    korl_windows_window_loop();
    korl_vulkan_destroy();
    korl_log(INFO, "end -----------------------------------------------------");
    ExitProcess(KORL_EXIT_SUCCESS);
}
#include "korl-assert.c"
#include "korl-io.c"
#include "korl-windows-utilities.c"
#include "korl-math.c"
#include "korl-memory.c"
#include "korl-windows-window.c"
#include "korl-vulkan.c"
#include "korl-vulkan-memory.c"
#include "korl-windows-vulkan.c"
#include "korl-gfx.c"
#include "korl-checkCast.c"
#include "korl-file.c"
#include "korl-assetCache.c"
#include "korl-stb-image.c"
