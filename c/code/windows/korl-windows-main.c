#include "korl-global-defines.h"
#include "korl-windows-global-defines.h"
#include "korl-io.h"
#include "korl-assert.h"
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    korl_log(INFO, "start");
    SYSTEM_INFO systemInfo;
    ZeroMemory(&systemInfo, sizeof(systemInfo));
    GetSystemInfo(&systemInfo);
    korl_log(DEBUG, "dwPageSize=%lu dwAllocationGranularity=%lu PI=%f", 
        systemInfo.dwPageSize, systemInfo.dwAllocationGranularity, 3.14159f);
    korl_log(INFO, "end");
    ExitProcess(KORL_EXIT_SUCCESS);
}
#include "korl-assert.c"
#include "korl-io.c"
#include "korl-windows-utilities.c"
