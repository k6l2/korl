#include "korl-global-defines.h"
#include "korl-windows-global-defines.h"
#include "korl-io.h"
#include "korl-assert.h"
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    /* So all of the code that is commented-out below here is a vain attempt at 
        getting a Windows non-console application to interact with the console 
        that it was invoked by.  As it turns out, it is actually literally 
        impossible to make a Windows application that does this... lol.
        SOURCE: https://stackoverflow.com/a/494000 */
    ///*const BOOL successAttachConsole = */AttachConsole(ATTACH_PARENT_PROCESS);
#if 0
    if(!successAttachConsole)
    {
        //const BOOL successFreeConsole = FreeConsole();
        //korl_assert(successFreeConsole);
        const BOOL successAllocConsole = AllocConsole();
        korl_assert(successAllocConsole);
        /* after allocating a new console, we have to re-open the I/O streams!
            source: https://stackoverflow.com/a/57241985 */
        FILE *fileDummy;
        const errno_t resultReOpenStdIn = 
            freopen_s(&fileDummy, "CONIN$", "r", stdin);
        const errno_t resultReOpenStdErr = 
            freopen_s(&fileDummy, "CONOUT$", "w", stderr);
        const errno_t resultReOpenStdOut = 
            freopen_s(&fileDummy, "CONOUT$", "w", stdout);
        korl_assert(resultReOpenStdIn == 0);
        korl_assert(resultReOpenStdErr == 0);
        korl_assert(resultReOpenStdOut == 0);
        const HANDLE hConOut = 
            CreateFile(_T("CONOUT$"), GENERIC_READ | GENERIC_WRITE, 
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
                FILE_ATTRIBUTE_NORMAL, NULL);
        const HANDLE hConIn = 
            CreateFile(_T("CONIN$"), GENERIC_READ | GENERIC_WRITE, 
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
                FILE_ATTRIBUTE_NORMAL, NULL);
        korl_assert(SetStdHandle(STD_OUTPUT_HANDLE, hConOut));
        korl_assert(SetStdHandle(STD_ERROR_HANDLE, hConOut));
        korl_assert(SetStdHandle(STD_INPUT_HANDLE, hConIn));
    }
#endif//0
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
