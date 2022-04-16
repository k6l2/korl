#include "korl-windows-window.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-log.h"
#include "korl-vulkan.h"
#include "korl-windows-vulkan.h"
#include "korl-math.h"
#include "korl-gfx.h"
#include "korl-windows-gui.h"
#include "platform-game-interface.h"
GAME_INITIALIZE(korl_game_initialize);
GAME_UPDATE(korl_game_update);
GAME_ON_KEYBOARD_EVENT(korl_game_onKeyboardEvent);
korl_global_const TCHAR g_korl_windows_window_className[] = _T("KorlWindowClass");
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
    case WM_KEYDOWN:{
        //korl_game_onKeyboardEvent(, true);
        break;}
    case WM_KEYUP:{
        //korl_game_onKeyboardEvent(, false);
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
    korl_game_initialize(&gameMemory);
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
        //_korl_windows_window_step(allocatorHeapStack);
        korl_gui_frameEnd();
        korl_vulkan_frameEnd();
    }
}
