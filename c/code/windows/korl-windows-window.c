#include "korl-windows-window.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-io.h"
#include "korl-vulkan.h"
#include "korl-windows-vulkan.h"
korl_global_const TCHAR g_korl_windows_window_className[] = _T("KorlWindowClass");
LRESULT CALLBACK _korl_windows_window_windowProcedure(
    _In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_CREATE:{
        const CREATESTRUCT*const createStruct = 
            KORL_C_CAST(CREATESTRUCT*, lParam);
        /* obtain the client rect of the window */
        RECT clientRect;
        const BOOL resultGetClientRect = GetClientRect(hWnd, &clientRect);
        if(!resultGetClientRect)
            korl_logLastError("GetClientRect failed!");
        /* create vulkan surface for this window */
        KORL_ZERO_STACK(Korl_Windows_Vulkan_SurfaceUserData, surfaceUserData);
        surfaceUserData.hInstance = createStruct->hInstance;
        surfaceUserData.hWnd      = hWnd;
        korl_vulkan_createDevice(korl_vulkan_createSurface(&surfaceUserData));
        korl_vulkan_loadShaders();
        korl_vulkan_createSwapChain(
            clientRect.right - clientRect.left, 
            clientRect.bottom - clientRect.top);
        korl_vulkan_createPipeline();
        korl_vulkan_recordAllCommandBuffers();
        }break;
    case WM_DESTROY:{
        korl_vulkan_destroySwapChain();
        korl_vulkan_destroySurface();
        PostQuitMessage(KORL_EXIT_SUCCESS);
        } break;
    /* @todo: WM_PAINT, ValidateRect(hWnd, NULL); ??? */
    default: 
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;// the most common user-handled result
}
korl_internal void korl_windows_window_initialize(void)
{
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
    KORL_ZERO_STACK(WNDCLASSEX, windowClass);
    windowClass.cbSize        = sizeof(windowClass);
    windowClass.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = _korl_windows_window_windowProcedure;
    windowClass.hCursor       = hCursorArrow;
    windowClass.hbrBackground = hBrushWindowBackground;
    windowClass.lpszClassName = g_korl_windows_window_className;
    const ATOM atomWindowClass = RegisterClassEx(&windowClass);
    if(!atomWindowClass) korl_logLastError("RegisterClassEx failed");
}
korl_internal void korl_windows_window_create(u32 sizeX, u32 sizeY)
{
    /* get a handle to the file used to create the calling process */
    const HMODULE hInstance = GetModuleHandle(NULL/*lpModuleName*/);
    korl_assert(hInstance);
    /* create a window */
    const u32 primaryDisplayPixelSizeX = GetSystemMetrics(SM_CXSCREEN);
    const u32 primaryDisplayPixelSizeY = GetSystemMetrics(SM_CYSCREEN);
    if(primaryDisplayPixelSizeX == 0) korl_log(ERROR, "GetSystemMetrics failed!");
    if(primaryDisplayPixelSizeY == 0) korl_log(ERROR, "GetSystemMetrics failed!");
    /* we will compute the window rect by starting with the desired centered 
        client rect and adjusting it for the non-client region */
    RECT rectCenteredClient;
    rectCenteredClient.left = primaryDisplayPixelSizeX/2 - sizeX/2;
    rectCenteredClient.top  = primaryDisplayPixelSizeY/2 - sizeY/2;
    rectCenteredClient.right  = rectCenteredClient.left + sizeX;
    rectCenteredClient.bottom = rectCenteredClient.top  + sizeY;
    const DWORD windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    const BOOL successAdjustClientRect = 
        AdjustWindowRect(&rectCenteredClient, windowStyle, FALSE/*menu?*/);
    if(!successAdjustClientRect) korl_logLastError("AdjustWindowRect failed!");
    const HWND hWnd = 
        CreateWindowEx(
            0/*extended style flags*/, g_korl_windows_window_className, _T("KORL C"), 
            windowStyle, 
            rectCenteredClient.left/*X*/, rectCenteredClient.top/*Y*/, 
            rectCenteredClient.right  - rectCenteredClient.left/*width*/, 
            rectCenteredClient.bottom - rectCenteredClient.top/*height*/, 
            NULL/*hWndParent*/, NULL/*hMenu*/, hInstance, 
            NULL/*lpParam; passed to WM_CREATE*/);
    if(!hWnd) korl_logLastError("CreateWindowEx failed!");
}
korl_internal void korl_windows_window_loop(void)
{
    bool quit = false;
    while(!quit)
    {
        KORL_ZERO_STACK(MSG, windowMessage);
        while(
            PeekMessage(
                &windowMessage, NULL/*hWnd; NULL -> get all thread messages*/, 
                0/*filterMin*/, 0/*filterMax*/, PM_REMOVE))
        {
            if(windowMessage.message == WM_QUIT) quit = true;
            const BOOL messageTranslated = TranslateMessage(&windowMessage);
            const LRESULT messageResult  = DispatchMessage (&windowMessage);
        }
        if(quit)
            break;
        korl_vulkan_draw();
    }
}
