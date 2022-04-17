#include "korl-windows-window.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-io.h"
#include "korl-vulkan.h"
#include "korl-windows-vulkan.h"
#include "korl-math.h"
#include "korl-gfx.h"
#include "korl-windows-gui.h"
#include "korl-assert.h"
#include "korl-interface-platform.h"
#include "korl-interface-game.h"
korl_global_const TCHAR g_korl_windows_window_className[] = _T("KorlWindowClass");
korl_global_variable Korl_KeyboardCode g_korl_windows_window_virtualKeyMap[0xFF];
LRESULT CALLBACK _korl_windows_window_windowProcedure(
    _In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    korl_gui_windows_processMessage(hWnd, uMsg, wParam, lParam);
    switch(uMsg)
    {
    case WM_CREATE:{
        const CREATESTRUCT*const createStruct = 
            KORL_C_CAST(CREATESTRUCT*, lParam);
        /* obtain the client rect of the window */
        RECT clientRect;
        if(!GetClientRect(hWnd, &clientRect))
            korl_logLastError("GetClientRect failed!");
        /* create vulkan surface for this window */
        KORL_ZERO_STACK(Korl_Windows_Vulkan_SurfaceUserData, surfaceUserData);
        surfaceUserData.hInstance = createStruct->hInstance;
        surfaceUserData.hWnd      = hWnd;
        korl_vulkan_createSurface(&surfaceUserData, 
                                  clientRect.right  - clientRect.left, 
                                  clientRect.bottom - clientRect.top);
        }break;
    case WM_DESTROY:{
        korl_vulkan_destroySurface();
        PostQuitMessage(KORL_EXIT_SUCCESS);
        } break;
    case WM_KEYDOWN:
    case WM_KEYUP:{
        if(wParam >= korl_arraySize(g_korl_windows_window_virtualKeyMap))
        {
            korl_log(WARNING, "wParam virtual key code is out of range");
            break;
        }
        korl_game_onKeyboardEvent(g_korl_windows_window_virtualKeyMap[wParam], uMsg == WM_KEYDOWN);
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
    korl_memory_nullify(g_korl_windows_window_virtualKeyMap, sizeof(g_korl_windows_window_virtualKeyMap));
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
    const HWND hWnd = CreateWindowEx(0/*extended style flags*/, g_korl_windows_window_className, _T("KORL C"), 
                                     windowStyle, 
                                     rectCenteredClient.left/*X*/, rectCenteredClient.top/*Y*/, 
                                     rectCenteredClient.right  - rectCenteredClient.left/*width*/, 
                                     rectCenteredClient.bottom - rectCenteredClient.top/*height*/, 
                                     NULL/*hWndParent*/, NULL/*hMenu*/, hInstance, 
                                     NULL/*lpParam; passed to WM_CREATE*/);
    if(!hWnd) korl_logLastError("CreateWindowEx failed!");
}
//KORL-ISSUE-000-000-035: hack: delete this function later
korl_internal void _korl_windows_window_step(Korl_Memory_Allocator allocatorHeapStack)
{
    korl_shared_variable bool initialized = false;
    korl_shared_variable Korl_Gfx_Camera camera3d;
    korl_shared_variable bool buttonClicked = false;
    korl_shared_variable bool thirdWindowOpen = false;
    if(!initialized)
    {
        camera3d = korl_gfx_createCameraFov(90.f, 0.001f, 10.f, (Korl_Vulkan_Position){1, 0, 1}, KORL_MATH_V3F32_ZERO);
        initialized = true;
    }
    u8 guiButtonActuations;
    korl_gui_windowBegin(L"first window", NULL, KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT);
        korl_gui_widgetTextFormat(L"%s", "Hello, sir! Can you spare a breb? o^o");
        korl_gui_widgetTextFormat(L"PLS, sir! I only wish for a breb...");
        korl_gui_widgetTextFormat(L"PLEEZ!");
        korl_gui_widgetTextFormat(L"max f32 == %.10e", KORL_F32_MAX);
        korl_gui_widgetTextFormat(L"min f32 == %.10e", KORL_F32_MIN);
    korl_gui_windowEnd();
    korl_gui_windowBegin(L"second window", NULL, KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE);
        if(guiButtonActuations = korl_gui_widgetButtonFormat(L"Hey, kid!  Click me!"))
        {
            korl_log(INFO, "You clicked the button %u time(s)!", guiButtonActuations);
            buttonClicked = !buttonClicked;
            thirdWindowOpen |= buttonClicked;
        }
        if(buttonClicked)
        {
            korl_gui_widgetTextFormat(L"YO, WHat the fuuuuuuu-?");
            korl_gui_widgetTextFormat(L"--------------------------------");
            korl_gui_widgetTextFormat(L"'-'");
            korl_gui_widgetTextFormat(L"YOU WANT SOME PENIS ENLARGEMENT PILLS?!");
        }
    korl_gui_windowEnd();
    korl_gui_windowBegin(L"third window - the quick brown fox jumps over the lazy dog", &thirdWindowOpen, KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT);
    korl_gui_windowEnd();
    korl_gfx_cameraFov_rotateAroundTarget(&camera3d, KORL_MATH_V3F32_Z, 0.01f);
    korl_gfx_useCamera(camera3d);
    Korl_Gfx_Batch*const birbSprite = korl_gfx_createBatchRectangleTextured(allocatorHeapStack, (Korl_Math_V2f32){1, 1}, L"test-assets/birb.jpg");
    korl_gfx_batch(birbSprite, KORL_GFX_BATCH_FLAG_NONE);
    korl_gfx_batchSetPosition(birbSprite, (Korl_Vulkan_Position){0, 0, 0.4f});
    korl_gfx_batch(birbSprite, KORL_GFX_BATCH_FLAG_NONE);
    Korl_Gfx_Batch*const originAxes = korl_gfx_createBatchLines(allocatorHeapStack, 3);
    korl_gfx_batchSetLine(originAxes, 0, (Korl_Vulkan_Position){0, 0, 0}, (Korl_Vulkan_Position){1, 0, 0}, (Korl_Vulkan_Color){255,   0,   0});
    korl_gfx_batchSetLine(originAxes, 1, (Korl_Vulkan_Position){0, 0, 0}, (Korl_Vulkan_Position){0, 1, 0}, (Korl_Vulkan_Color){  0, 255,   0});
    korl_gfx_batchSetLine(originAxes, 2, (Korl_Vulkan_Position){0, 0, 0}, (Korl_Vulkan_Position){0, 0, 1}, (Korl_Vulkan_Color){  0,   0, 255});
    korl_gfx_batch(originAxes, KORL_GFX_BATCH_FLAG_NONE);
    Korl_Gfx_Camera cameraHud = korl_gfx_createCameraOrthoFixedHeight(600.f, 1.f);
    korl_gfx_useCamera(cameraHud);
    Korl_Gfx_Batch*const originAxesHud = korl_gfx_createBatchLines(allocatorHeapStack, 3);
    //korl_gfx_batchSetPosition(originAxesHud, (Korl_Vulkan_Position){1, 0, 0});//rounding error in the bottom-left corner causes the y-axis to not be shown :(
    korl_gfx_batchSetScale(originAxesHud, (Korl_Vulkan_Position){100, 100, 100});
    korl_gfx_batchSetLine(originAxesHud, 0, (Korl_Vulkan_Position){0, 0, 0}, (Korl_Vulkan_Position){1, 0, 0}, (Korl_Vulkan_Color){255,   0,   0});
    korl_gfx_batchSetLine(originAxesHud, 1, (Korl_Vulkan_Position){0, 0, 0}, (Korl_Vulkan_Position){0, 1, 0}, (Korl_Vulkan_Color){  0, 255,   0});
    korl_gfx_batch(originAxesHud, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    Korl_Gfx_Batch*const hudBox = korl_gfx_createBatchRectangleColored(allocatorHeapStack, (Korl_Math_V2f32){1, 1}, (Korl_Math_V2f32){0.5f, 0.5f}, (Korl_Vulkan_Color){255, 255, 255});
    korl_gfx_batchSetPosition(hudBox, (Korl_Vulkan_Position){250.f*camera3d.position.xyz.x, 250.f*camera3d.position.xyz.y, 0});
    korl_gfx_batchSetScale(hudBox, (Korl_Vulkan_Position){200, 200, 200});
    korl_gfx_batchSetVertexColor(hudBox, 0, (Korl_Vulkan_Color){255,   0,   0});
    korl_gfx_batchSetVertexColor(hudBox, 1, (Korl_Vulkan_Color){  0, 255,   0});
    korl_gfx_batchSetVertexColor(hudBox, 2, (Korl_Vulkan_Color){  0,   0, 255});
    korl_gfx_cameraSetScissorPercent(&cameraHud, 0,0, 0.5f,1);
    korl_gfx_useCamera(cameraHud);
    korl_gfx_batch(hudBox, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    cameraHud = korl_gfx_createCameraOrtho(1.f);
    korl_gfx_useCamera(cameraHud);
    Korl_Gfx_Batch*const hudText = korl_gfx_createBatchText(allocatorHeapStack, NULL, L"the quick, brown fox jumped over the lazy dog?...", 32.f);
    korl_gfx_batchSetPosition(hudText, (Korl_Vulkan_Position){-350, 0, 0});
    korl_gfx_batch(hudText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    Korl_Gfx_Batch*const hudText2 = korl_gfx_createBatchText(allocatorHeapStack, NULL, L"THE QUICK, BROWN FOX JUMPED OVER THE LAZY DOG!", 32.f);
    korl_gfx_batchSetPosition(hudText2, (Korl_Vulkan_Position){-350, -64, 0});
    korl_gfx_batch(hudText2, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
}
korl_internal void korl_windows_window_loop(void)
{
    KORL_ZERO_STACK(GameMemory, gameMemory);
    KORL_ZERO_STACK(KorlPlatformApi, korlApi);
    korlApi.korl_assertConditionFailed            = korl_assertConditionFailed;
    korlApi.korl_logVariadicArguments             = korl_logVariadicArguments;
    korlApi.korl_gfx_createCameraOrtho            = korl_gfx_createCameraOrtho;
    korlApi.korl_gfx_useCamera                    = korl_gfx_useCamera;
    korlApi.korl_gfx_batch                        = korl_gfx_batch;
    korlApi.korl_gfx_createBatchRectangleTextured = korl_gfx_createBatchRectangleTextured;
    korl_game_initialize(&gameMemory, korlApi);
    Korl_Memory_Allocator allocatorHeapStack = korl_memory_createAllocatorLinear(korl_math_megabytes(1));
    bool quit = false;
    while(!quit)
    {
        allocatorHeapStack.callbackEmpty(allocatorHeapStack.userData);
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
        korl_game_update(1/60.f, swapchainSize.xy.x, swapchainSize.xy.y, true/*TODO: fix me later*/);
        //KORL-ISSUE-000-000-035: hack: delete this
        //_korl_windows_window_step(allocatorHeapStack);
        korl_gui_frameEnd();
        korl_vulkan_frameEnd();
    }
}
