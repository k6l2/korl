#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-io.h"
#include "korl-assert.h"
#include "korl-windows-utilities.h"
#include "korl-memory.h"
#include "korl-math.h"
LRESULT CALLBACK korl_windowProcedure(
    _In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_DESTROY:{
        PostQuitMessage(KORL_EXIT_SUCCESS);
        } break;
    default: 
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;// the most common user-handled result
}
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    korl_log(INFO, "start");
    korl_memory_initialize();
    /* prepare variables necessary to create a window class */
    const TCHAR windowClassName[] = _T("KorlWindowClass");
    /* note: since this is a global shared handle, it does NOT need to be 
        cleaned up via DestroyCursor later */
    const HCURSOR hCursorArrow = LoadCursor(NULL, IDC_ARROW);
    if(!hCursorArrow) korl_logLastError("LoadCursor failed!");
    /* Note: Windows will automatically delete Window class brushes when they 
        are unregistered, so we don't need to clean this up.  Also, stock 
        objects do not need to be cleaned up, so we gucci. */
    HBRUSH hBrushWindowBackground = 
        KORL_C_CAST(HBRUSH, GetStockObject(BLACK_BRUSH));
    if(!hBrushWindowBackground) korl_logLastError("GetStockObject failed!");
    /* create a window class */
    korl_makeZeroStackVariable(WNDCLASSEX, windowClass);
    windowClass.cbSize        = sizeof(windowClass);
    windowClass.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = korl_windowProcedure;
    windowClass.hCursor       = hCursorArrow;
    windowClass.hbrBackground = hBrushWindowBackground;
    windowClass.lpszClassName = windowClassName;
    const ATOM atomWindowClass = RegisterClassEx(&windowClass);
    if(!atomWindowClass) korl_logLastError("RegisterClassEx failed");
    /* create a window */
    const u32 clientSizeX = 1024;
    const u32 clientSizeY = 576;
    const u32 primaryDisplayPixelSizeX = GetSystemMetrics(SM_CXSCREEN);
    const u32 primaryDisplayPixelSizeY = GetSystemMetrics(SM_CYSCREEN);
    if(primaryDisplayPixelSizeX == 0) korl_log(ERROR, "GetSystemMetrics failed!");
    if(primaryDisplayPixelSizeY == 0) korl_log(ERROR, "GetSystemMetrics failed!");
    /* we will compute the window rect by starting with the desired centered 
        client rect and adjusting it for the non-client region */
    RECT rectCenteredClient;
    rectCenteredClient.left = primaryDisplayPixelSizeX/2 - clientSizeX/2;
    rectCenteredClient.top  = primaryDisplayPixelSizeY/2 - clientSizeY/2;
    rectCenteredClient.right  = rectCenteredClient.left + clientSizeX;
    rectCenteredClient.bottom = rectCenteredClient.top  + clientSizeY;
    const DWORD windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    const BOOL successAdjustClientRect = 
        AdjustWindowRect(&rectCenteredClient, windowStyle, FALSE/*menu?*/);
    if(!successAdjustClientRect) korl_logLastError("AdjustWindowRect failed!");
    const HWND hWnd = 
        CreateWindowEx(
            0/*extended style flags*/, windowClassName, _T("KORL C"), 
            windowStyle, 
            rectCenteredClient.left/*X*/, rectCenteredClient.top/*Y*/, 
            rectCenteredClient.right  - rectCenteredClient.left/*width*/, 
            rectCenteredClient.bottom - rectCenteredClient.top/*height*/, 
            NULL/*hWndParent*/, 
            NULL/*hMenu*/, NULL/*hInstance*/, 
            NULL/*lpParam; passed to WM_CREATE*/);
    if(!hWnd) korl_logLastError("CreateWindowEx failed!");
    /* main window loop */
    bool quit = false;
    while(!quit)
    {
        korl_makeZeroStackVariable(MSG, windowMessage);
        while(
            PeekMessage(
                &windowMessage, NULL/*hWnd; NULL == get all thread messages*/, 
                0/*filterMin*/, 0/*filterMax*/, PM_REMOVE))
        {
            if(windowMessage.message == WM_QUIT) quit = true;
            const BOOL messageTranslated = TranslateMessage(&windowMessage);
            const LRESULT messageResult  = DispatchMessage (&windowMessage);
        }
    }
    korl_log(INFO, "end");
    ExitProcess(KORL_EXIT_SUCCESS);
}
#include "korl-assert.c"
#include "korl-io.c"
#include "korl-windows-utilities.c"
#include "korl-math.c"
#include "korl-memory.c"
