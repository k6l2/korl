#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-log.h"
#include "korl-memory.h"
#include "korl-windows-window.h"
#include "korl-assert.h"
#include "korl-vulkan.h"
#include "korl-file.h"
#include "korl-assetCache.h"
#include "korl-stb-image.h"
#include "korl-stb-truetype.h"
#include "korl-gfx.h"
#include "korl-gui.h"
#include "korl-time.h"
#if 0//KORL-ISSUE-000-000-036: (low priority) configure STB & other code to not use CRT
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
#else
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#endif
{
    korl_assert_initialize();
    korl_memory_initialize();
    /* process arguments passed to the program */
    bool useLogOutputDebugger = false;
    bool useLogOutputConsole  = false;
    bool useLogFileBig        = false;
    bool logFileEnabled       = true;
    {
        wchar_t* cStringCommandLine = GetCommandLine();// memory managed by Windows
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(cStringCommandLine, &argc);
        korl_assert(argv);
        for(int a = 0; a < argc; a++)
            if(     0 == korl_memory_stringCompare(argv[a], L"--log-debugger"))
                useLogOutputDebugger = true;
            else if(0 == korl_memory_stringCompare(argv[a], L"--log-console"))
                useLogOutputConsole = true;
            else if(0 == korl_memory_stringCompare(argv[a], L"--log-file-big"))
                useLogFileBig = true;
            else if(0 == korl_memory_stringCompare(argv[a], L"--log-file-disable"))
                logFileEnabled = false;
        korl_assert(LocalFree(argv) == NULL);
    }
    /**/
    korl_log_initialize(useLogOutputDebugger, useLogOutputConsole, useLogFileBig);
    //KORL-FEATURE-000-000-000: hook into Windows exception handler for crash reporting
    korl_log(INFO, "korl_windows_main START --------------------------------------------------------");
    korl_time_initialize();
    korl_file_initialize();
    if(logFileEnabled)
        korl_log_initiateFile();
    korl_stb_image_initialize();
    korl_stb_truetype_initialize();
    korl_assetCache_initialize();
    korl_vulkan_construct();
    korl_gfx_initialize();
    korl_gui_initialize();
    korl_windows_window_initialize();
    korl_windows_window_create(1024, 576);
    korl_windows_window_loop();
    korl_vulkan_destroy();
    korl_log(INFO, "korl_windows_main END ----------------------------------------------------------");
    korl_log_shutDown();
#if 0//KORL-ISSUE-000-000-036: (low priority) configure STB & other code to not use CRT
    ExitProcess(KORL_EXIT_SUCCESS);
#else
    return KORL_EXIT_SUCCESS;
#endif
}
#include "korl-assert.c"
#include "korl-log.c"
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
#include "korl-stb-truetype.c"
#include "korl-gui.c"
#include "korl-windows-gui.c"
#include "korl-time.c"
