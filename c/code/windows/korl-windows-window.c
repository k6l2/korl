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
    /* ignore all window events that don't belong to the windows we are 
        responsible for in this code module */
    if(hWnd != _korl_windows_window_context.handleWindow)
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    korl_gui_windows_processMessage(hWnd, uMsg, wParam, lParam);
    switch(uMsg)
    {
    case WM_CREATE:{
        break;}
    case WM_DESTROY:{
        PostQuitMessage(KORL_EXIT_SUCCESS);
        break;}
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
        break;}
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_XBUTTONDOWN:{
        Korl_MouseEvent mouseEvent;
        mouseEvent.type = KORL_MOUSE_EVENT_BUTTON_DOWN;
        if(uMsg == WM_LBUTTONDOWN)
        {
            mouseEvent.which.button = KORL_MOUSE_BUTTON_LEFT;
        }
        else if(uMsg == WM_MBUTTONDOWN)
        {
            mouseEvent.which.button = KORL_MOUSE_BUTTON_MIDDLE;
        }
        else if(uMsg == WM_RBUTTONDOWN)
        {
            mouseEvent.which.button = KORL_MOUSE_BUTTON_RIGHT;
        }
        else if(uMsg == WM_XBUTTONDOWN)
        {
            mouseEvent.which.button = (HIWORD(wParam) & XBUTTON1) ? KORL_MOUSE_BUTTON_X1 :
                KORL_MOUSE_BUTTON_X2;
        }
        mouseEvent.x = GET_X_LPARAM(lParam);
        const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize(); 
        mouseEvent.y = swapchainSize.y - GET_Y_LPARAM(lParam); 
        korl_game_onMouseEvent(mouseEvent);
        } break;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_XBUTTONUP:{
        Korl_MouseEvent mouseEvent;
        mouseEvent.type = KORL_MOUSE_EVENT_BUTTON_UP;
        if(uMsg == WM_LBUTTONUP)
        {
            mouseEvent.which.button = KORL_MOUSE_BUTTON_LEFT;
        }
        else if(uMsg == WM_MBUTTONUP)
        {
            mouseEvent.which.button = KORL_MOUSE_BUTTON_MIDDLE;
        }
        else if(uMsg == WM_RBUTTONUP)
        {
            mouseEvent.which.button = KORL_MOUSE_BUTTON_RIGHT;
        }
        else if(uMsg == WM_XBUTTONUP)
        {
            mouseEvent.which.button = (HIWORD(wParam) & XBUTTON1) ? KORL_MOUSE_BUTTON_X1 :
                KORL_MOUSE_BUTTON_X2;
        }
        mouseEvent.x = GET_X_LPARAM(lParam);
        const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize(); 
        mouseEvent.y = swapchainSize.y - GET_Y_LPARAM(lParam); 
        korl_game_onMouseEvent(mouseEvent);
        } break;
    case WM_MOUSEMOVE:{
        Korl_MouseEvent mouseEvent;
        mouseEvent.type = KORL_MOUSE_EVENT_MOVE;
        mouseEvent.x = GET_X_LPARAM(lParam);
        const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize(); 
        mouseEvent.y = swapchainSize.y - GET_Y_LPARAM(lParam); 
        korl_game_onMouseEvent(mouseEvent);
        } break;
    case WM_MOUSEWHEEL:{
        Korl_MouseEvent mouseEvent;
        mouseEvent.type = KORL_MOUSE_EVENT_WHEEL;
        mouseEvent.which.wheel = GET_WHEEL_DELTA_WPARAM(wParam);
        POINT mousePoint;
        mousePoint.x = GET_X_LPARAM(lParam);
        mousePoint.y = GET_Y_LPARAM(lParam);
        if(!ScreenToClient(hWnd, &mousePoint))
        {
            korl_logLastError("ScreenToClient failed.");
        }
        const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize(); 
        mouseEvent.x = mousePoint.x;
        mouseEvent.y = swapchainSize.y - mousePoint.y;
        
        korl_game_onMouseEvent(mouseEvent);
        } break;
    case WM_MOUSEHWHEEL:{
        Korl_MouseEvent mouseEvent;
        mouseEvent.type = KORL_MOUSE_EVENT_HWHEEL;
        mouseEvent.which.wheel = GET_WHEEL_DELTA_WPARAM(wParam);
        POINT mousePoint;
        mousePoint.x = GET_X_LPARAM(lParam);
        mousePoint.y = GET_Y_LPARAM(lParam);
        if(!ScreenToClient(hWnd, &mousePoint))
        {
            korl_logLastError("ScreenToClient failed.");
        }
        const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize(); 
        mouseEvent.x = mousePoint.x;
        mouseEvent.y = swapchainSize.y - mousePoint.y;
        korl_game_onMouseEvent(mouseEvent);
        } break;
    //KORL-ISSUE-000-000-034: investigate: do we need WM_PAINT+ValidateRect?
    case WM_MOVE:{
        break;}
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
    korl_time_probeStart(game_initialization);
    korl_game_initialize(&gameMemory, korlApi);
    korl_time_probeStop(game_initialization);
    korl_log(INFO, "KORL initialization time probe report:");
    korl_time_probeLogReport();
    u$ renderFrameCount = 0;
    bool quit = false;
    /* For reference, this is essentially what the application developers who 
        consume KORL have rated their application as the _maximum_ amount of 
        time between logical frames to maintain a stable simulation.  It should 
        be _OKAY_ to use smaller values, but definitely _NOT_ okay to use higher 
        values. */
    const Korl_Time_Counts timeCountsTargetGamePerFrame = korl_time_countsFromHz(KORL_APP_TARGET_FRAME_HZ);
    const Korl_Time_Counts timeCountsSlowestFrame       = korl_time_countsFromHz(30);
    Korl_Time_Counts gameTimeAccumulator                = 0;
    PlatformTimeStamp timeStampLast                     = korl_timeStamp();
    typedef struct _Korl_Window_StatsLogicLoop
    {
        Korl_Time_Counts time;
        bool framesInProgress;
    } _Korl_Window_StatsLogicLoop;
    typedef struct _Korl_Window_LoopStats
    {
        Korl_Time_Counts timeRenderLoop;
        Korl_Time_Counts timeCountsSleepDiff;//@TODO: get rid of this
        PlatformTimeStamp ptsLastFrameEnd;//@TODO: get rid of this
        PlatformTimeStamp ptsFrameEnd;//@TODO: get rid of this
        KORL_MEMORY_POOL_DECLARE(_Korl_Window_StatsLogicLoop, logicLoops, 8);
    } _Korl_Window_LoopStats;
    KORL_MEMORY_POOL_DECLARE(_Korl_Window_LoopStats, loopStats, 64);
    loopStats_korlMemoryPoolSize = 0;
    while(!quit)
    {
        _Korl_Window_LoopStats*const stats = KORL_MEMORY_POOL_ISFULL(loopStats) 
            ? NULL
            : KORL_MEMORY_POOL_ADD(loopStats);
        if(stats)
            korl_memory_zero(stats, sizeof(*stats));
        if(renderFrameCount == 1 || renderFrameCount == 2)
        {
            korl_log(INFO, "generating reports for frame %llu:", renderFrameCount - 1);
            korl_time_probeLogReport();
            korl_memory_reportLog(korl_memory_reportGenerate());
        }
        korl_time_probeReset();
        korl_memory_allocator_emptyStackAllocators();
        korl_time_probeStart(Main_Loop);
        KORL_ZERO_STACK(MSG, windowMessage);
        korl_time_probeStart(process_window_messages);
        while(PeekMessage(&windowMessage, NULL/*hWnd; NULL -> get all thread messages*/, 
                          0/*filterMin*/, 0/*filterMax*/, PM_REMOVE))
        {
            if(windowMessage.message == WM_QUIT) quit = true;
            const BOOL messageTranslated = TranslateMessage(&windowMessage);
            const LRESULT messageResult  = DispatchMessage (&windowMessage);
        }
        korl_time_probeStop(process_window_messages);
        if(quit)
            break;
        korl_time_probeStart(vulkan_frame_begin);        korl_vulkan_frameBegin((f32[]){0.05f, 0.f, 0.05f});                   korl_time_probeStop(vulkan_frame_begin);
        korl_time_probeStart(gui_frame_begin);           korl_gui_frameBegin();                                                korl_time_probeStop(gui_frame_begin);
        korl_time_probeStart(vulkan_get_swapchain_size); const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSwapchainSize(); korl_time_probeStop(vulkan_get_swapchain_size);
        korl_time_probeStart(game_update);
        if(!korl_game_update(1.f/KORL_APP_TARGET_FRAME_HZ, swapchainSize.x, swapchainSize.y, GetFocus() != NULL))
            break;
        korl_time_probeStop(game_update);
        korl_time_probeStart(gui_frame_end);    korl_gui_frameEnd();    korl_time_probeStop(gui_frame_end);
        korl_time_probeStart(vulkan_frame_end); korl_vulkan_frameEnd(); korl_time_probeStop(vulkan_frame_end);
        /* regulate frame rate to our game module's target frame rate */
        //KORL-ISSUE-000-000-059: window: find a frame timing solution that works if vulkan API blocks for some reason
        korl_time_probeStart(sleep);
        if(stats)
        {
            stats->ptsLastFrameEnd = timeStampLast;
            stats->ptsFrameEnd     = timeStampLast + timeCountsTargetGamePerFrame;
        }
        timeStampLast = korl_time_sleep(timeStampLast, timeCountsTargetGamePerFrame, stats ? &stats->timeCountsSleepDiff : NULL);
        korl_time_probeStop(sleep);
        if(renderFrameCount < ~(u$)0)
            renderFrameCount++;
        const Korl_Time_Counts timeCountsMainLoop = korl_time_probeStop(Main_Loop);
        if(stats)
            stats->timeRenderLoop = timeCountsMainLoop;
    }
    /* report frame stats */
    korl_log(INFO, "Window Render Loop Times:");
    Korl_Time_Counts timeCountAverageRender = 0;
    Korl_Time_Counts timeCountAverageLogic  = 0;
    wchar_t durationBuffer[32];
    for(Korl_MemoryPool_Size s = 0; s < KORL_MEMORY_POOL_SIZE(loopStats); s++)
    {
        korl_assert(0 < korl_time_countsFormatBuffer(loopStats[s].timeRenderLoop, durationBuffer, sizeof(durationBuffer)));
        korl_log_noMeta(INFO, "[% 2u]: %ws, %lli, %llu + %lli = %llu", s, durationBuffer, loopStats[s].timeCountsSleepDiff, loopStats[s].ptsLastFrameEnd, timeCountsTargetGamePerFrame, loopStats[s].ptsFrameEnd);
        if(s == 2)
            timeCountAverageRender = loopStats[s].timeRenderLoop;
        else if (s > 1)// avoid the first couple frames for now since there is a lot of spin-up work that throws off metrics
            timeCountAverageRender = (timeCountAverageRender + loopStats[s].timeRenderLoop) / 2;
        for(Korl_MemoryPool_Size l = 0; l < KORL_MEMORY_POOL_SIZE(loopStats[s].logicLoops); l++)
        {
            if(s == 2 && timeCountAverageLogic == 0)
                timeCountAverageLogic = loopStats[s].timeRenderLoop;
            else if (s > 1)// avoid the first couple frames for now since there is a lot of spin-up work that throws off metrics
                timeCountAverageLogic = (timeCountAverageLogic + loopStats[s].logicLoops[l].time) / 2;
            korl_assert(0 < korl_time_countsFormatBuffer(loopStats[s].logicLoops[l].time, durationBuffer, sizeof(durationBuffer)));
            korl_log_noMeta(INFO, "    logicLoop[% 2u]: %ws | %ws", l, durationBuffer, loopStats[s].logicLoops[l].framesInProgress ? L"Vulkan:WIP" : L"Vulkan:READY");
        }
    }
    korl_assert(0 < korl_time_countsFormatBuffer(timeCountAverageRender, durationBuffer, sizeof(durationBuffer)));
    korl_log_noMeta(INFO, "Average Render Loop Time: %ws", durationBuffer);
    //korl_assert(0 < korl_time_countsFormatBuffer(timeCountAverageLogic, durationBuffer, sizeof(durationBuffer)));
    //korl_log_noMeta(INFO, "Average Logic Loop Time:  %ws", durationBuffer);
    /**/
    korl_vulkan_destroySurface();
}
