#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-commandLine.h"
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
    bool commandLineEndProgram;
    bool useLogOutputDebugger;
    bool useLogOutputConsole;
    bool useLogFileBig;
    bool logFileDisable;
    const Korl_CommandLine_ArgumentDescriptor descriptors[] = 
        { {&useLogOutputDebugger, KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL, L"--log-debugger"    , L"-ld" , L"Send log output to the debugger, if we are attached to one."}
        , {&useLogOutputConsole , KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL, L"--log-console"     , L"-lc" , L"Send log output to a console.  If the calling process has a console, it will be used.  Otherwise, a new console will be created."}
        , {&useLogFileBig       , KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL, L"--log-file-big"    , L"-lfb", L"Disable the internal mechanism to limit the size of the log file to some maximum value.  Without this, log files will be cut if capacity is reached; only the beginning/end of the logs will be written to the file."}
        , {&logFileDisable      , KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL, L"--log-file-disable", L"-lfd", L"Disable logging to a file."} };
    korl_assert_initialize();
    korl_memory_initialize();
    commandLineEndProgram = korl_commandLine_parse(descriptors, korl_arraySize(descriptors));
    korl_log_initialize(useLogOutputDebugger | commandLineEndProgram, 
                        useLogOutputConsole  | commandLineEndProgram, 
                        useLogFileBig, commandLineEndProgram);
    if(commandLineEndProgram)
    {
        korl_commandLine_logUsage(descriptors, korl_arraySize(descriptors));
        return KORL_EXIT_SUCCESS;
    }
    //KORL-FEATURE-000-000-000: hook into Windows exception handler for crash reporting
    korl_log(INFO, "korl_windows_main START --------------------------------------------------------");
    korl_time_initialize();
    const PlatformTimeStamp timeStampInitializeStart = korl_timeStamp();
    korl_file_initialize();
    korl_log_initiateFile(!logFileDisable);
    korl_stb_image_initialize();
    korl_stb_truetype_initialize();
    korl_assetCache_initialize();
    korl_vulkan_construct();
    korl_gfx_initialize();
    korl_gui_initialize();
    korl_windows_window_initialize();
    korl_windows_window_create(1024, 576);
    korl_log(INFO, "KORL initialization completed in %f seconds!", korl_time_secondsSinceTimeStamp(timeStampInitializeStart));
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
#include "korl-gui.c"
#include "korl-windows-gui.c"
#include "korl-time.c"
