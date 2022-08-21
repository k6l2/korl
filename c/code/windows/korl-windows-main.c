#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-commandLine.h"
#include "korl-log.h"
#include "korl-memory.h"
#include "korl-windows-window.h"
#include "korl-vulkan.h"
#include "korl-file.h"
#include "korl-assetCache.h"
#include "korl-stb-image.h"
#include "korl-stb-truetype.h"
#include "korl-stb-ds.h"
#include "korl-gfx.h"
#include "korl-gui.h"
#include "korl-time.h"
#include "korl-crash.h"
#include "korl-windows-gamepad.h"
#if 0//KORL-ISSUE-000-000-036: (low priority) configure STB & other code to not use CRT
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
#else
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#endif
{
    bool displayHelp;
    bool useLogOutputDebugger;
    bool useLogOutputConsole;
    bool useLogFileBig;
    bool logFileDisable;
    const Korl_CommandLine_ArgumentDescriptor descriptors[] = 
        { {&displayHelp         , KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL, L"--help"            , L"-h"  , L"Display all possible command line arguments, then exit the program."}
        , {&useLogOutputDebugger, KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL, L"--log-debugger"    , L"-ld" , L"Send log output to the debugger, if we are attached to one."}
        , {&useLogOutputConsole , KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL, L"--log-console"     , L"-lc" , L"Send log output to a console.  If the calling process has a console, it will be used.  Otherwise, a new console will be created."}
        , {&useLogFileBig       , KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL, L"--log-file-big"    , L"-lfb", L"Disable the internal mechanism to limit the size of the log file to some maximum value.  Without this, log files will be cut if capacity is reached; only the beginning/end of the logs will be written to the file, and only the first half of the logs will be written to the file real-time (the second half of the log file will be written when the program ends)."}
        , {&logFileDisable      , KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL, L"--log-file-disable", L"-lfd", L"Disable logging to a file."} };
    korl_memory_initialize();
    korl_log_initialize();
    korl_log(INFO, "korl_windows_main START ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄");
    korl_time_initialize();
    korl_time_probeStart(KORL_initialization);
    korl_time_probeStart(init_module_file);  korl_file_initialize();                                           korl_time_probeStop(init_module_file);
    korl_time_probeStart(init_module_crash); korl_crash_initialize();                                          korl_time_probeStop(init_module_crash);
    korl_time_probeStart(parseCommandLine);  korl_commandLine_parse(descriptors, korl_arraySize(descriptors)); korl_time_probeStop(parseCommandLine);
    korl_time_probeStart(log_configure);     korl_log_configure(useLogOutputDebugger | displayHelp, 
                                                                useLogOutputConsole  | displayHelp, 
                                                                useLogFileBig, displayHelp);                       korl_time_probeStop(log_configure);
    if(displayHelp)
    {
        korl_commandLine_logUsage(descriptors, korl_arraySize(descriptors));
        goto shutdownSuccess;
    }
    korl_time_probeStart(init_module_gamepad);      korl_windows_gamepad_initialize();      korl_time_probeStop(init_module_gamepad);
    korl_time_probeStart(init_module_window);       korl_windows_window_initialize();       korl_time_probeStop(init_module_window);
    korl_time_probeStart(create_window);            korl_windows_window_create(1024, 576);  korl_time_probeStop(create_window);
    korl_time_probeStart(init_module_log);          korl_log_initiateFile(!logFileDisable); korl_time_probeStop(init_module_log);
    korl_time_probeStart(init_module_stb_image);    korl_stb_image_initialize();            korl_time_probeStop(init_module_stb_image);
    korl_time_probeStart(init_module_stb_truetype); korl_stb_truetype_initialize();         korl_time_probeStop(init_module_stb_truetype);
    korl_time_probeStart(init_module_stb_ds);       korl_stb_ds_initialize();               korl_time_probeStop(init_module_stb_ds);
    korl_time_probeStart(init_module_assetCache);   korl_assetCache_initialize();           korl_time_probeStop(init_module_assetCache);
    korl_time_probeStart(init_module_vulkan);       korl_vulkan_construct();                korl_time_probeStop(init_module_vulkan);
    korl_time_probeStart(init_module_gfx);          korl_gfx_initialize();                  korl_time_probeStop(init_module_gfx);
    korl_time_probeStart(init_module_gui);          korl_gui_initialize();                  korl_time_probeStop(init_module_gui);
    korl_time_probeStop(KORL_initialization);
    korl_windows_window_loop();
    korl_vulkan_destroy();
    korl_log(INFO, "korl_windows_main END ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄🏁");
shutdownSuccess:
    korl_time_shutDown();
    korl_log_shutDown();
#if 0//KORL-ISSUE-000-000-036: (low priority) configure STB & other code to not use CRT
    ExitProcess(KORL_EXIT_SUCCESS);
#else
    return KORL_EXIT_SUCCESS;
#endif
}
#include "korl-log.c"
#include "korl-commandLine.c"
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
#include "korl-stb-ds.c"
#include "korl-gui.c"
#include "korl-windows-gui.c"
#include "korl-time.c"
#include "korl-crash.c"
#include "korl-stringPool.c"
#include "korl-windows-gamepad.c"
#include "korl-windows-mouse.c"
