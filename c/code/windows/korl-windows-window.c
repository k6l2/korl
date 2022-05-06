#include "korl-windows-window.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-log.h"
#include "korl-time.h"
#include "korl-vulkan.h"
#include "korl-windows-vulkan.h"
#include "korl-math.h"
#include "korl-gfx.h"
#include "korl-windows-gui.h"
#include "korl-assert.h"
#include "korl-interface-platform.h"
#include "korl-interface-game.h"
#include "korl-gui.h"
korl_global_const TCHAR g_korl_windows_window_className[] = _T("KorlWindowClass");
typedef struct _Korl_Windows_Window_Context
{
    HWND handleWindow;// for now, we will only ever have _one_ window
} _Korl_Windows_Window_Context;
korl_global_variable _Korl_Windows_Window_Context _korl_windows_window_context;
korl_global_variable Korl_KeyboardCode g_korl_windows_window_virtualKeyMap[0xFF];
LRESULT CALLBACK _korl_windows_window_windowProcedure(
    _In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    korl_gui_windows_processMessage(hWnd, uMsg, wParam, lParam);
    switch(uMsg)
    {
    case WM_CREATE:{
        }break;
    case WM_DESTROY:{
        PostQuitMessage(KORL_EXIT_SUCCESS);
        } break;
    case WM_KEYDOWN:
    case WM_KEYUP:{
        if(wParam >= korl_arraySize(g_korl_windows_window_virtualKeyMap))
        {
            korl_log(WARNING, "wParam virtual key code is out of range");
            break;
        }
        korl_game_onKeyboardEvent(g_korl_windows_window_virtualKeyMap[wParam], uMsg == WM_KEYDOWN, HIWORD(lParam) & KF_REPEAT);
        break;}
    case WM_SIZE:{
        const UINT clientWidth  = LOWORD(lParam);
        const UINT clientHeight = HIWORD(lParam);
        korl_log(VERBOSE, "WM_SIZE - clientSize: %ux%u", clientWidth, clientHeight);
        korl_vulkan_deferredResize(clientWidth, clientHeight);
        } break;
    //KORL-ISSUE-000-000-034: investigate: do we need WM_PAINT+ValidateRect?
    default: 
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;// the most common user-handled result
}
korl_internal void korl_windows_window_initialize(void)
{
    korl_memory_zero(&_korl_windows_window_context, sizeof(_korl_windows_window_context));
    korl_memory_zero(g_korl_windows_window_virtualKeyMap, sizeof(g_korl_windows_window_virtualKeyMap));
    for(u32 i = 0; i <= 9; ++i)
        g_korl_windows_window_virtualKeyMap[0x30 + i] = KORL_KEY_TENKEYLESS_0 + i;
    for(u32 i = 0; i <= 26; ++i)
        g_korl_windows_window_virtualKeyMap[0x41 + i] = KORL_KEY_A + i;
    for(u32 i = 0; i <= 9; ++i)
        g_korl_windows_window_virtualKeyMap[0x60 + i] = KORL_KEY_NUMPAD_0 + i;
    for(u32 i = 0; i <= 12; ++i)
        g_korl_windows_window_virtualKeyMap[0x70 + i] = KORL_KEY_F1 + i;
    g_korl_windows_window_virtualKeyMap[VK_OEM_COMMA]     = KORL_KEY_COMMA;
    g_korl_windows_window_virtualKeyMap[VK_OEM_PERIOD]    = KORL_KEY_PERIOD;
    g_korl_windows_window_virtualKeyMap[VK_OEM_2]         = KORL_KEY_SLASH_FORWARD;
    g_korl_windows_window_virtualKeyMap[VK_OEM_5]         = KORL_KEY_SLASH_BACK;
    g_korl_windows_window_virtualKeyMap[VK_OEM_4]         = KORL_KEY_CURLYBRACE_LEFT;
    g_korl_windows_window_virtualKeyMap[VK_OEM_6]         = KORL_KEY_CURLYBRACE_RIGHT;
    g_korl_windows_window_virtualKeyMap[VK_OEM_1]         = KORL_KEY_SEMICOLON;
    g_korl_windows_window_virtualKeyMap[VK_OEM_7]         = KORL_KEY_QUOTE;
    g_korl_windows_window_virtualKeyMap[VK_OEM_3]         = KORL_KEY_GRAVE;
    g_korl_windows_window_virtualKeyMap[VK_OEM_MINUS]     = KORL_KEY_TENKEYLESS_MINUS;
    g_korl_windows_window_virtualKeyMap[VK_OEM_NEC_EQUAL] = KORL_KEY_EQUALS;
    g_korl_windows_window_virtualKeyMap[VK_BACK]          = KORL_KEY_BACKSPACE;
    g_korl_windows_window_virtualKeyMap[VK_ESCAPE]        = KORL_KEY_ESCAPE;
    g_korl_windows_window_virtualKeyMap[VK_RETURN]        = KORL_KEY_ENTER;
    g_korl_windows_window_virtualKeyMap[VK_SPACE]         = KORL_KEY_SPACE;
    g_korl_windows_window_virtualKeyMap[VK_TAB]           = KORL_KEY_TAB;
    g_korl_windows_window_virtualKeyMap[VK_SHIFT]         = KORL_KEY_SHIFT_LEFT;
    g_korl_windows_window_virtualKeyMap[VK_SHIFT]         = KORL_KEY_SHIFT_RIGHT;
    g_korl_windows_window_virtualKeyMap[VK_CONTROL]       = KORL_KEY_CONTROL_LEFT;
    g_korl_windows_window_virtualKeyMap[VK_CONTROL]       = KORL_KEY_CONTROL_RIGHT;
    g_korl_windows_window_virtualKeyMap[VK_MENU]          = KORL_KEY_ALT_LEFT;
    g_korl_windows_window_virtualKeyMap[VK_MENU]          = KORL_KEY_ALT_RIGHT;
    g_korl_windows_window_virtualKeyMap[VK_UP]            = KORL_KEY_ARROW_UP;
    g_korl_windows_window_virtualKeyMap[VK_DOWN]          = KORL_KEY_ARROW_DOWN;
    g_korl_windows_window_virtualKeyMap[VK_LEFT]          = KORL_KEY_ARROW_LEFT;
    g_korl_windows_window_virtualKeyMap[VK_RIGHT]         = KORL_KEY_ARROW_RIGHT;
    g_korl_windows_window_virtualKeyMap[VK_INSERT]        = KORL_KEY_INSERT;
    g_korl_windows_window_virtualKeyMap[VK_DELETE]        = KORL_KEY_DELETE;
    g_korl_windows_window_virtualKeyMap[VK_HOME]          = KORL_KEY_HOME;
    g_korl_windows_window_virtualKeyMap[VK_END]           = KORL_KEY_END;
    g_korl_windows_window_virtualKeyMap[VK_PRIOR]         = KORL_KEY_PAGE_UP;
    g_korl_windows_window_virtualKeyMap[VK_NEXT]          = KORL_KEY_PAGE_DOWN;
    g_korl_windows_window_virtualKeyMap[VK_DECIMAL]       = KORL_KEY_NUMPAD_PERIOD;
    g_korl_windows_window_virtualKeyMap[VK_DIVIDE]        = KORL_KEY_NUMPAD_DIVIDE;
    g_korl_windows_window_virtualKeyMap[VK_MULTIPLY]      = KORL_KEY_NUMPAD_MULTIPLY;
    g_korl_windows_window_virtualKeyMap[VK_SUBTRACT]      = KORL_KEY_NUMPAD_MINUS;
    g_korl_windows_window_virtualKeyMap[VK_ADD]           = KORL_KEY_NUMPAD_ADD;
    /* note: since this is a global shared handle, it does NOT need to be 
        cleaned up via DestroyCursor later */
    const HCURSOR hCursorArrow = LoadCursor(NULL, IDC_ARROW);
    if(!hCursorArrow) korl_logLastError("LoadCursor failed!");
    /* Note: Windows will automatically delete Window class brushes when they 
        are unregistered, so we don't need to clean this up.  Also, stock 
        objects do not need to be cleaned up, so we gucci. */
    HBRUSH hBrushWindowBackground = KORL_C_CAST(HBRUSH, GetStockObject(BLACK_BRUSH));
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
    KORL_ZERO_STACK(RECT, rectCenteredClient);
    rectCenteredClient.left   = primaryDisplayPixelSizeX/2 - sizeX/2;
    rectCenteredClient.top    = primaryDisplayPixelSizeY/2 - sizeY/2;
    rectCenteredClient.right  = rectCenteredClient.left + sizeX;
    rectCenteredClient.bottom = rectCenteredClient.top  + sizeY;
    const DWORD windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    const BOOL successAdjustClientRect = 
        AdjustWindowRect(&rectCenteredClient, windowStyle, FALSE/*menu?*/);
    if(!successAdjustClientRect) korl_logLastError("AdjustWindowRect failed!");
    const HWND hWnd = CreateWindowEx(0/*extended style flags*/, g_korl_windows_window_className, 
                                     KORL_APPLICATION_NAME, windowStyle, 
                                     rectCenteredClient.left/*X*/, rectCenteredClient.top/*Y*/, 
                                     rectCenteredClient.right  - rectCenteredClient.left/*width*/, 
                                     rectCenteredClient.bottom - rectCenteredClient.top/*height*/, 
                                     NULL/*hWndParent*/, NULL/*hMenu*/, hInstance, 
                                     NULL/*lpParam; passed to WM_CREATE*/);
    if(!hWnd) korl_logLastError("CreateWindowEx failed!");
    korl_assert(_korl_windows_window_context.handleWindow == NULL);
    _korl_windows_window_context.handleWindow = hWnd;
}
korl_internal void korl_windows_window_loop(void)
{
    /* get a handle to the file used to create the calling process */
    const HMODULE hInstance = GetModuleHandle(NULL/*lpModuleName*/);
    korl_assert(hInstance);
    /* now we can create the Vulkan surfaces for the windows that were created 
        before the Vulkan module was initialized */
    {
        /* obtain the client rect of the window */
        RECT clientRect;
        if(!GetClientRect(_korl_windows_window_context.handleWindow, &clientRect))
            korl_logLastError("GetClientRect failed!");
        /* create vulkan surface for this window */
        KORL_ZERO_STACK(Korl_Windows_Vulkan_SurfaceUserData, surfaceUserData);
        surfaceUserData.hInstance = hInstance;
        surfaceUserData.hWnd      = _korl_windows_window_context.handleWindow;
        korl_vulkan_createSurface(&surfaceUserData, 
                                  clientRect.right  - clientRect.left, 
                                  clientRect.bottom - clientRect.top);
    }
    /**/
    KORL_ZERO_STACK(GameMemory, gameMemory);
    KORL_ZERO_STACK(KorlPlatformApi, korlApi);
    KORL_INTERFACE_PLATFORM_API_SET(korlApi);
    korl_game_initialize(&gameMemory, korlApi);
    bool quit = false;
    while(!quit)
    {
        KORL_ZERO_STACK(MSG, windowMessage);
        while(PeekMessage(&windowMessage, NULL/*hWnd; NULL -> get all thread messages*/, 
                          0/*filterMin*/, 0/*filterMax*/, PM_REMOVE))
        {
            if(windowMessage.message == WM_QUIT) quit = true;
            const BOOL messageTranslated = TranslateMessage(&windowMessage);
            const LRESULT messageResult  = DispatchMessage (&windowMessage);
        }
        if(quit)
            break;
        korl_vulkan_frameBegin((f32[]){0.05f, 0.f, 0.05f});
        korl_gui_frameBegin();
        const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize();
        if(!korl_game_update(1/60.f, swapchainSize.xy.x, swapchainSize.xy.y, GetFocus() != NULL))
            break;
        korl_gui_frameEnd();
        korl_vulkan_frameEnd();
    }
    korl_vulkan_destroySurface();
}
