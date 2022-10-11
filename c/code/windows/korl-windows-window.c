#include "korl-windows-window.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-windows-gamepad.h"
#include "korl-windows-mouse.h"
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
#include "korl-file.h"
#include "korl-stb-ds.h"
#include "korl-stringPool.h"
#include "korl-bluetooth.h"
#if defined(_LOCAL_STRING_POOL_POINTER)
#   undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_windows_window_context.stringPool))
korl_global_const TCHAR _KORL_WINDOWS_WINDOW_CLASS_NAME[] = _T("KorlWindowClass");
typedef struct _Korl_Windows_Window_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool stringPool;
    GameMemory* gameMemory;
    struct
    {
        HWND handle;
        DWORD style;
        BOOL hasMenu;
    } window;// for now, we will only ever have _one_ window
    struct
    {
        fnSig_korl_game_onReload*        korl_game_onReload;
        fnSig_korl_game_initialize*      korl_game_initialize;
        fnSig_korl_game_onKeyboardEvent* korl_game_onKeyboardEvent;
        fnSig_korl_game_onMouseEvent*    korl_game_onMouseEvent;
        fnSig_korl_game_onGamepadEvent*  korl_game_onGamepadEvent;
        fnSig_korl_game_update*          korl_game_update;
        fnSig_korl_game_onAssetReloaded* korl_game_onAssetReloaded;
    } gameApi;
    HMODULE gameDll;
    KorlPlatformDateStamp gameDllLastWriteDateStamp;
    bool deferSaveStateSave;// defer until the beginning of the next frame; the best place to synchronize save state operations
    bool deferSaveStateLoad;// defer until the beginning of the next frame; the best place to synchronize save state operations
} _Korl_Windows_Window_Context;
korl_global_variable _Korl_Windows_Window_Context _korl_windows_window_context;
korl_global_variable Korl_KeyboardCode _korl_windows_window_virtualKeyMap[0xFF];
LRESULT CALLBACK _korl_windows_window_windowProcedure(
    _In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    /* ignore all window events that don't belong to the windows we are 
        responsible for in this code module */
    if(hWnd != context->window.handle)
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    LRESULT result;
    korl_gui_windows_processMessage(hWnd, uMsg, wParam, lParam);
    if(korl_windows_gamepad_processMessage(hWnd, uMsg, wParam, lParam, &result))
        return result;
    if(korl_windows_mouse_processMessage(hWnd, uMsg, wParam, lParam, &result, context->gameApi.korl_game_onMouseEvent))
        return result;
    switch(uMsg)
    {
    case WM_CREATE:{
        break;}
    case WM_DESTROY:{
        PostQuitMessage(KORL_EXIT_SUCCESS);
        break;}
    case WM_KEYDOWN:
    case WM_KEYUP:{
        if(wParam >= korl_arraySize(_korl_windows_window_virtualKeyMap))
        {
            korl_log(WARNING, "wParam virtual key code is out of range");
            break;
        }
        if(context->gameApi.korl_game_onKeyboardEvent)
            context->gameApi.korl_game_onKeyboardEvent(_korl_windows_window_virtualKeyMap[wParam], uMsg == WM_KEYDOWN, HIWORD(lParam) & KF_REPEAT);
#if 1//KORL-ISSUE-000-000-068: window: maybe expose more general API
        if(_korl_windows_window_virtualKeyMap[wParam] == KORL_KEY_F1 && uMsg == WM_KEYDOWN && !(HIWORD(lParam) & KF_REPEAT))
            context->deferSaveStateSave = true;
        if(_korl_windows_window_virtualKeyMap[wParam] == KORL_KEY_F2 && uMsg == WM_KEYDOWN && !(HIWORD(lParam) & KF_REPEAT))
            context->deferSaveStateLoad = true;
#endif
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
        const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize(); 
        mouseEvent.y = surfaceSize.y - GET_Y_LPARAM(lParam); 
        if(context->gameApi.korl_game_onMouseEvent)
            context->gameApi.korl_game_onMouseEvent(mouseEvent);
        break;}
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
        const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize(); 
        mouseEvent.y = surfaceSize.y - GET_Y_LPARAM(lParam); 
        if(context->gameApi.korl_game_onMouseEvent)
            context->gameApi.korl_game_onMouseEvent(mouseEvent);
        break;}
    case WM_MOUSEMOVE:{
        const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize(); 
        Korl_MouseEvent mouseEvent;
        mouseEvent.type = KORL_MOUSE_EVENT_MOVE;
        mouseEvent.x    = GET_X_LPARAM(lParam);
        mouseEvent.y    = surfaceSize.y - GET_Y_LPARAM(lParam); 
        if(context->gameApi.korl_game_onMouseEvent)
            context->gameApi.korl_game_onMouseEvent(mouseEvent);
        break;}
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
        const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize(); 
        mouseEvent.x = mousePoint.x;
        mouseEvent.y = surfaceSize.y - mousePoint.y;
        if(context->gameApi.korl_game_onMouseEvent)
            context->gameApi.korl_game_onMouseEvent(mouseEvent);
        break;}
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
        const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize(); 
        mouseEvent.x = mousePoint.x;
        mouseEvent.y = surfaceSize.y - mousePoint.y;
        if(context->gameApi.korl_game_onMouseEvent)
            context->gameApi.korl_game_onMouseEvent(mouseEvent);
        break;}
    //KORL-ISSUE-000-000-034: investigate: do we need WM_PAINT+ValidateRect?
    case WM_MOVE:{
        break;}
    default: 
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;// the most common user-handled result
}
korl_internal KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(_korl_windows_window_onAssetHotReloaded)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    korl_vulkan_onAssetHotReload(rawUtf16AssetName, assetData);
    if(context->gameApi.korl_game_onAssetReloaded)
        context->gameApi.korl_game_onAssetReloaded(rawUtf16AssetName, assetData);
}
korl_internal void _korl_windows_window_findGameApiAddresses(HMODULE hModule)
{
    _korl_windows_window_context.gameApi.korl_game_onReload        = KORL_C_CAST(fnSig_korl_game_onReload*,        GetProcAddress(hModule, "korl_game_onReload"));
    _korl_windows_window_context.gameApi.korl_game_initialize      = KORL_C_CAST(fnSig_korl_game_initialize*,      GetProcAddress(hModule, "korl_game_initialize"));
    _korl_windows_window_context.gameApi.korl_game_onKeyboardEvent = KORL_C_CAST(fnSig_korl_game_onKeyboardEvent*, GetProcAddress(hModule, "korl_game_onKeyboardEvent"));
    _korl_windows_window_context.gameApi.korl_game_onMouseEvent    = KORL_C_CAST(fnSig_korl_game_onMouseEvent*,    GetProcAddress(hModule, "korl_game_onMouseEvent"));
    _korl_windows_window_context.gameApi.korl_game_onGamepadEvent  = KORL_C_CAST(fnSig_korl_game_onGamepadEvent*,  GetProcAddress(hModule, "korl_game_onGamepadEvent"));
    _korl_windows_window_context.gameApi.korl_game_update          = KORL_C_CAST(fnSig_korl_game_update*,          GetProcAddress(hModule, "korl_game_update"));
    _korl_windows_window_context.gameApi.korl_game_onAssetReloaded = KORL_C_CAST(fnSig_korl_game_onAssetReloaded*, GetProcAddress(hModule, "korl_game_onAssetReloaded"));
}
korl_internal void korl_windows_window_initialize(void)
{
    korl_memory_zero(&_korl_windows_window_context, sizeof(_korl_windows_window_context));
    _korl_windows_window_context.allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_megabytes(8/*@TODO: change this back to 1 when we're done testing*/), L"korl-windows-window", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, NULL/*let platform choose address*/);
    _korl_windows_window_context.stringPool      = korl_stringPool_create(_korl_windows_window_context.allocatorHandle);
    /* attempt to obtain function pointers to the game interface API from within 
        the exe file; if we fail to get them, then we can assume that we're 
        running the game module as a DLL */
    HMODULE hModuleExe = GetModuleHandle(NULL/*get the handle of the exe file*/);
    if(!hModuleExe)
        korl_logLastError("GetModuleHandle(NULL) failed");
    _korl_windows_window_findGameApiAddresses(hModuleExe);
    korl_memory_zero(_korl_windows_window_virtualKeyMap, sizeof(_korl_windows_window_virtualKeyMap));
    for(u32 i = 0; i <= 9; ++i)
        _korl_windows_window_virtualKeyMap[0x30 + i] = KORL_KEY_TENKEYLESS_0 + i;
    for(u32 i = 0; i <= 26; ++i)
        _korl_windows_window_virtualKeyMap[0x41 + i] = KORL_KEY_A + i;
    for(u32 i = 0; i <= 9; ++i)
        _korl_windows_window_virtualKeyMap[0x60 + i] = KORL_KEY_NUMPAD_0 + i;
    for(u32 i = 0; i <= 12; ++i)
        _korl_windows_window_virtualKeyMap[0x70 + i] = KORL_KEY_F1 + i;
    _korl_windows_window_virtualKeyMap[VK_OEM_COMMA]     = KORL_KEY_COMMA;
    _korl_windows_window_virtualKeyMap[VK_OEM_PERIOD]    = KORL_KEY_PERIOD;
    _korl_windows_window_virtualKeyMap[VK_OEM_2]         = KORL_KEY_SLASH_FORWARD;
    _korl_windows_window_virtualKeyMap[VK_OEM_5]         = KORL_KEY_SLASH_BACK;
    _korl_windows_window_virtualKeyMap[VK_OEM_4]         = KORL_KEY_CURLYBRACE_LEFT;
    _korl_windows_window_virtualKeyMap[VK_OEM_6]         = KORL_KEY_CURLYBRACE_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_OEM_1]         = KORL_KEY_SEMICOLON;
    _korl_windows_window_virtualKeyMap[VK_OEM_7]         = KORL_KEY_QUOTE;
    _korl_windows_window_virtualKeyMap[VK_OEM_3]         = KORL_KEY_GRAVE;
    _korl_windows_window_virtualKeyMap[VK_OEM_MINUS]     = KORL_KEY_TENKEYLESS_MINUS;
    _korl_windows_window_virtualKeyMap[VK_OEM_NEC_EQUAL] = KORL_KEY_EQUALS;
    _korl_windows_window_virtualKeyMap[VK_BACK]          = KORL_KEY_BACKSPACE;
    _korl_windows_window_virtualKeyMap[VK_ESCAPE]        = KORL_KEY_ESCAPE;
    _korl_windows_window_virtualKeyMap[VK_RETURN]        = KORL_KEY_ENTER;
    _korl_windows_window_virtualKeyMap[VK_SPACE]         = KORL_KEY_SPACE;
    _korl_windows_window_virtualKeyMap[VK_TAB]           = KORL_KEY_TAB;
    _korl_windows_window_virtualKeyMap[VK_SHIFT]         = KORL_KEY_SHIFT_LEFT;
    _korl_windows_window_virtualKeyMap[VK_SHIFT]         = KORL_KEY_SHIFT_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_CONTROL]       = KORL_KEY_CONTROL_LEFT;
    _korl_windows_window_virtualKeyMap[VK_CONTROL]       = KORL_KEY_CONTROL_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_MENU]          = KORL_KEY_ALT_LEFT;
    _korl_windows_window_virtualKeyMap[VK_MENU]          = KORL_KEY_ALT_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_UP]            = KORL_KEY_ARROW_UP;
    _korl_windows_window_virtualKeyMap[VK_DOWN]          = KORL_KEY_ARROW_DOWN;
    _korl_windows_window_virtualKeyMap[VK_LEFT]          = KORL_KEY_ARROW_LEFT;
    _korl_windows_window_virtualKeyMap[VK_RIGHT]         = KORL_KEY_ARROW_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_INSERT]        = KORL_KEY_INSERT;
    _korl_windows_window_virtualKeyMap[VK_DELETE]        = KORL_KEY_DELETE;
    _korl_windows_window_virtualKeyMap[VK_HOME]          = KORL_KEY_HOME;
    _korl_windows_window_virtualKeyMap[VK_END]           = KORL_KEY_END;
    _korl_windows_window_virtualKeyMap[VK_PRIOR]         = KORL_KEY_PAGE_UP;
    _korl_windows_window_virtualKeyMap[VK_NEXT]          = KORL_KEY_PAGE_DOWN;
    _korl_windows_window_virtualKeyMap[VK_DECIMAL]       = KORL_KEY_NUMPAD_PERIOD;
    _korl_windows_window_virtualKeyMap[VK_DIVIDE]        = KORL_KEY_NUMPAD_DIVIDE;
    _korl_windows_window_virtualKeyMap[VK_MULTIPLY]      = KORL_KEY_NUMPAD_MULTIPLY;
    _korl_windows_window_virtualKeyMap[VK_SUBTRACT]      = KORL_KEY_NUMPAD_MINUS;
    _korl_windows_window_virtualKeyMap[VK_ADD]           = KORL_KEY_NUMPAD_ADD;
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
    windowClass.lpszClassName = _KORL_WINDOWS_WINDOW_CLASS_NAME;
    const ATOM atomWindowClass = RegisterClassEx(&windowClass);
    if(!atomWindowClass) korl_logLastError("RegisterClassEx failed");
}
korl_internal void korl_windows_window_create(u32 sizeX, u32 sizeY)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
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
    const HMENU hMenu = NULL;
    const BOOL windowHasMenu = hMenu != NULL;
    const BOOL successAdjustClientRect = 
        AdjustWindowRect(&rectCenteredClient, windowStyle, windowHasMenu);
    if(!successAdjustClientRect) korl_logLastError("AdjustWindowRect failed!");
    const HWND hWnd = CreateWindowEx(0/*extended style flags*/, _KORL_WINDOWS_WINDOW_CLASS_NAME, 
                                     KORL_APPLICATION_NAME, windowStyle, 
                                     rectCenteredClient.left/*X*/, rectCenteredClient.top/*Y*/, 
                                     rectCenteredClient.right  - rectCenteredClient.left/*width*/, 
                                     rectCenteredClient.bottom - rectCenteredClient.top/*height*/, 
                                     NULL/*hWndParent*/, hMenu, hInstance, 
                                     NULL/*lpParam; passed to WM_CREATE*/);
    if(!hWnd) korl_logLastError("CreateWindowEx failed!");
    korl_assert(_korl_windows_window_context.window.handle == NULL);
    _korl_windows_window_context.window.handle  = hWnd;
    _korl_windows_window_context.window.style   = windowStyle;
    _korl_windows_window_context.window.hasMenu = windowHasMenu;
    korl_windows_gamepad_registerWindow(hWnd, context->allocatorHandle);
    korl_windows_mouse_registerWindow(hWnd);
}
korl_internal void _korl_windows_window_gameInitialize(KorlPlatformApi* korlApi)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    if(context->gameMemory)
        return;
    if(   !context->gameApi.korl_game_initialize
       || !context->gameApi.korl_game_onReload)
       return;
    context->gameMemory = korl_allocate(context->allocatorHandle, sizeof(GameMemory));
    context->gameApi.korl_game_onReload(context->gameMemory, *korlApi);
    context->gameApi.korl_game_initialize();
}
korl_internal void _korl_windows_window_dynamicGameLoad(const wchar_t*const utf16GameDllFileName)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    const bool successCopyGameDll = korl_file_copy(KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY, utf16GameDllFileName, 
                                                   KORL_FILE_PATHTYPE_TEMPORARY_DATA,       utf16GameDllFileName, 
                                                   true/*replace new file if it exists, which is fine since we shouldn't be storing the base file in this directory in the first place*/);
    /* if successful, we know we are running the game in dynamic mode */
    if(successCopyGameDll)
    {
        /* if we're running in dynamic mode, continue renaming the dynamic 
            code module until the file rename operation is successful */
        Korl_File_ResultRenameReplace resultRenameReplace = KORL_FILE_RESULT_RENAME_REPLACE_FAIL_MOVE_OLD_FILE;
        for(u32 i = 0; i < 255/*just some arbitrary high enough # which determines the max # of game clients that can run on a computer*/; i++)
        {
            Korl_StringPool_String stringGameDllTemp = string_newFormatUtf16(L"%ws_%u.dll", KORL_DYNAMIC_APPLICATION_NAME, i);
            resultRenameReplace = 
                korl_file_renameReplace(KORL_FILE_PATHTYPE_TEMPORARY_DATA, utf16GameDllFileName, 
                                        KORL_FILE_PATHTYPE_TEMPORARY_DATA, string_getRawUtf16(stringGameDllTemp));
            korl_assert(resultRenameReplace != KORL_FILE_RESULT_RENAME_REPLACE_SOURCE_FILE_DOES_NOT_EXIST);
            if(resultRenameReplace == KORL_FILE_RESULT_RENAME_REPLACE_SUCCESS)
            {
                if(context->gameDll)
                    FreeLibrary(context->gameDll);
                context->gameDll = korl_file_loadDynamicLibrary(KORL_FILE_PATHTYPE_TEMPORARY_DATA, string_getRawUtf16(stringGameDllTemp));
                korl_assert(context->gameDll);
                korl_assert(korl_file_getDateStampLastWriteFileName(KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY, utf16GameDllFileName, &context->gameDllLastWriteDateStamp));
            }
            string_free(stringGameDllTemp);
            if(resultRenameReplace == KORL_FILE_RESULT_RENAME_REPLACE_SUCCESS)
                break;
        }
        korl_assert(resultRenameReplace == KORL_FILE_RESULT_RENAME_REPLACE_SUCCESS);
    }
    else if(!context->gameApi.korl_game_update)
        korl_log(WARNING, "Preparation of dynamic game module, but the symbols also weren't exported in the application!");
    if(context->gameDll)
        _korl_windows_window_findGameApiAddresses(context->gameDll);
}
typedef struct _Korl_Windows_Window_CodepointTest_Log
{
    u8 trailingMetaTagCodepoints;
    u8 metaTagComponent;// 0=>log level, 1=>time stamp, 2=>line, 3=>file, 4=>function
    const u16* pCodepointMetaTagStart;
} _Korl_Windows_Window_CodepointTest_Log;
korl_internal KORL_GFX_TEXT_CODEPOINT_TEST(_korl_windows_window_codepointTest_log)///@TODO: remove/move this; this is just scaffolding
{
    // log meta data example:
    //╟INFO   ┆11:48'00"525┆   58┆korl-vulkan.c┆_korl_vulkan_debugUtilsMessengerCallback╢ 
    _Korl_Windows_Window_CodepointTest_Log*const data = KORL_C_CAST(_Korl_Windows_Window_CodepointTest_Log*, userData);
    if(data->pCodepointMetaTagStart)
    {
        bool endOfMetaTagComponent = false;
        if(*pCodepoint == L'╢')
        {
            data->trailingMetaTagCodepoints = 2;
            data->pCodepointMetaTagStart    = NULL;
            endOfMetaTagComponent = true;
        }
        else if(*pCodepoint == L'┆')
            endOfMetaTagComponent = true;
        if(endOfMetaTagComponent)
        {
            switch(data->metaTagComponent)
            {
            case 0:{// log level
                korl_assert(currentLineColor);
                switch(*data->pCodepointMetaTagStart)
                {
                case L'A':{// ASSERT
                    currentLineColor->xyz = (Korl_Math_V3f32){1, 0, 0};// red
                    break;}
                case L'E':{// ERROR
                    currentLineColor->xyz = (Korl_Math_V3f32){1, 0, 0};// red
                    break;}
                case L'W':{// WARNING
                    currentLineColor->xyz = (Korl_Math_V3f32){1, 1, 0};// yellow
                    break;}
                case L'I':{// INFO
                    // do nothing; the line color defaults to white!
                    break;}
                case L'V':{// VERBOSE
                    currentLineColor->xyz = (Korl_Math_V3f32){0, 1, 1};// cyan
                    break;}
                }
                break;}
            case 1:{// time
                break;}
            case 2:{// line #
                break;}
            case 3:{// file
                break;}
            case 4:{// function
                break;}
            }
            data->metaTagComponent++;
        }
    }
    else if(*pCodepoint == L'╟')
    {
        data->pCodepointMetaTagStart = pCodepoint + 1;
        data->metaTagComponent       = 0;
    }
    if(data->trailingMetaTagCodepoints)
        data->trailingMetaTagCodepoints--;
    if(data->pCodepointMetaTagStart || data->trailingMetaTagCodepoints)
        return false;
    return true;
}
korl_internal void korl_windows_window_loop(void)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    Korl_StringPool stringPool = korl_stringPool_create(_korl_windows_window_context.allocatorHandle);
    /* get a handle to the file used to create the calling process */
    const HMODULE hInstance = GetModuleHandle(NULL/*lpModuleName*/);
    korl_assert(hInstance);
    /* now we can create the Vulkan surfaces for the windows that were created 
        before the Vulkan module was initialized */
    {
        /* obtain the client rect of the window */
        RECT clientRect;
        if(!GetClientRect(context->window.handle, &clientRect))
            korl_logLastError("GetClientRect failed!");
        /* create vulkan surface for this window */
        KORL_ZERO_STACK(Korl_Windows_Vulkan_SurfaceUserData, surfaceUserData);
        surfaceUserData.hInstance = hInstance;
        surfaceUserData.hWnd      = context->window.handle;
        korl_vulkan_createSurface(&surfaceUserData, 
                                  clientRect.right  - clientRect.left, 
                                  clientRect.bottom - clientRect.top);
        korl_vulkan_setSurfaceClearColor((f32[]){0.05f, 0.f, 0.05f});// set default clear color to ~purplish~
    }
    /* initialize game memory & game module */
    KORL_ZERO_STACK(KorlPlatformApi, korlApi);
    KORL_INTERFACE_PLATFORM_API_SET(korlApi);
    korl_time_probeStart(game_initialization);
    /* attempt to copy the game DLL to the application temp directory */
    Korl_StringPool_String stringGameDll = string_newFormatUtf16(L"%ws.dll", KORL_DYNAMIC_APPLICATION_NAME);
    _korl_windows_window_dynamicGameLoad(string_getRawUtf16(stringGameDll));
    _korl_windows_window_gameInitialize(&korlApi);
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
    PlatformTimeStamp timeStampLast                     = korl_timeStamp();
    typedef struct _Korl_Window_LoopStats
    {
        Korl_Time_Counts timeTotal;
        Korl_Time_Counts timeSleep;
    } _Korl_Window_LoopStats;
    KORL_MEMORY_POOL_DECLARE(_Korl_Window_LoopStats, loopStats, 1024);
    loopStats_korlMemoryPoolSize = 0;
    bool deferProbeReport = false;
    while(!quit)
    {
        _Korl_Window_LoopStats*const stats = KORL_MEMORY_POOL_ISFULL(loopStats) 
            ? NULL
            : KORL_MEMORY_POOL_ADD(loopStats);
        if(stats)
            korl_memory_zero(stats, sizeof(*stats));
        if(renderFrameCount == 1 || renderFrameCount == 3 || deferProbeReport)
        {
            korl_log(INFO, "generating reports for frame %llu:", renderFrameCount - 1);
            korl_time_probeLogReport();
            korl_memory_reportLog(korl_memory_reportGenerate());
            deferProbeReport = false;
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
        korl_windows_gamepad_poll(context->gameApi.korl_game_onGamepadEvent);
        /* check the dynamic game module file to see if it has been updated; if 
            it has, we should do a hot-reload of this module! */
        if(context->gameDll)
        {
            KorlPlatformDateStamp dateStampLatestFileWrite;
            if(   korl_file_getDateStampLastWriteFileName(KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY, 
                                                          string_getRawUtf16(stringGameDll), &dateStampLatestFileWrite)
               && KORL_TIME_DATESTAMP_COMPARE_RESULT_FIRST_TIME_EARLIER == korl_time_dateStampCompare(context->gameDllLastWriteDateStamp, dateStampLatestFileWrite))
            {
                _korl_windows_window_dynamicGameLoad(string_getRawUtf16(stringGameDll));
                context->gameApi.korl_game_onReload(context->gameMemory, korlApi);
            }
        }
        korl_assetCache_checkAssetObsolescence(_korl_windows_window_onAssetHotReloaded);
        korl_time_probeStart(save_state_create);
        korl_file_saveStateCreate();
        korl_time_probeStop(save_state_create);
        if(context->deferSaveStateSave)
        {
            context->deferSaveStateSave = false;
            deferProbeReport = true;
            korl_time_probeStart(save_state_save);
            korl_file_saveStateSave(KORL_FILE_PATHTYPE_LOCAL_DATA, L"save-states/savestate");//KORL-ISSUE-000-000-077: crash/window: savestates do not properly "save" crashes that occur inside game module callbacks on window events
            korl_time_probeStop(save_state_save);
        }
        if(context->deferSaveStateLoad)
        {
            context->deferSaveStateLoad = false;
            deferProbeReport = true;
            korl_time_probeStart(save_state_load);
            korl_vulkan_clearAllDeviceAssets();
            korl_gfx_clearFontCache();
            korl_log_clearAsyncIo();
            korl_file_finishAllAsyncOperations();
            korl_assetCache_clearAllFileHandles();
            korl_file_saveStateLoad(KORL_FILE_PATHTYPE_LOCAL_DATA, L"save-states/savestate");
            korl_memory_reportLog(korl_memory_reportGenerate());// just for diagnostic...
            if(context->gameApi.korl_game_onReload)
                context->gameApi.korl_game_onReload(context->gameMemory, korlApi);
            korl_time_probeStop(save_state_load);
        }
        korl_time_probeStart(vulkan_frame_begin);        korl_vulkan_frameBegin();                                           korl_time_probeStop(vulkan_frame_begin);
        korl_time_probeStart(gui_frame_begin);           korl_gui_frameBegin();                                              korl_time_probeStop(gui_frame_begin);
        korl_time_probeStart(vulkan_get_swapchain_size); const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSurfaceSize(); korl_time_probeStop(vulkan_get_swapchain_size);
        korl_time_probeStart(game_update);
#if 0///@TODO: test code for new vulkan implementation; delete later
        // draw a simple triangle //
        {
            Korl_Math_V2f32 positions[] = {{ 0.f ,  0.5f}
                                          ,{ 0.5f, -0.5f}
                                          ,{-0.5f, -0.5f}};
            KORL_ZERO_STACK(Korl_Vulkan_DrawVertexData, vertexData);
            vertexData.vertexCount        = korl_arraySize(positions);
            vertexData.positionDimensions = 2;
            vertexData.positionsStride    = sizeof(*positions);
            vertexData.positions          = KORL_C_CAST(f32*, positions);
            korl_vulkan_draw(&vertexData);
        }
#elif 0
        // draw a quad //
        {
            Korl_Vulkan_VertexIndex indices[] = {0, 1, 2, 2, 3, 0};
            Korl_Math_V2f32 positions[] = {{-0.5f, -0.5f}
                                          ,{-0.5f,  0.5f}
                                          ,{ 0.5f,  0.5f}
                                          ,{ 0.5f, -0.5f}};
            KORL_ZERO_STACK(Korl_Vulkan_DrawVertexData, vertexData);
            vertexData.indexCount         = korl_arraySize(indices);
            vertexData.indices            = indices;
            vertexData.vertexCount        = korl_arraySize(positions);
            vertexData.positionDimensions = 2;
            vertexData.positionsStride    = sizeof(*positions);
            vertexData.positions          = KORL_C_CAST(f32*, positions);
            korl_vulkan_draw(&vertexData);
        }
#elif 0
        // draw 3D axis at the world-space origin //
        {
            const Korl_Vulkan_DrawState_Projection projection = (Korl_Vulkan_DrawState_Projection){.type        = KORL_VULKAN_DRAW_STATE_PROJECTION_TYPE_FOV
                                                                                                  ,.subType.fov = {.horizontalFovDegrees = 90
                                                                                                                  ,.clipNear             = 1
                                                                                                                  ,.clipFar              = 100}};
            const Korl_Vulkan_DrawState_View view = (Korl_Vulkan_DrawState_View){.positionEye    = {10,10,10}
                                                                                ,.positionTarget = KORL_MATH_V3F32_ZERO
                                                                                ,.worldUpNormal  = KORL_MATH_V3F32_Z};
            const Korl_Vulkan_DrawState_Model model = (Korl_Vulkan_DrawState_Model){.translation = {0,0,0}
                                                                                   ,.rotation    = KORL_MATH_QUATERNION_IDENTITY
                                                                                   ,.scale       = {100,100,100}};
            KORL_ZERO_STACK(Korl_Vulkan_DrawState, drawState);
            drawState.projection = &projection;
            drawState.view       = &view;
            drawState.model      = &model;
            korl_vulkan_setDrawState(&drawState);
            Korl_Math_V3f32 positions[] = {{0, 0, 0},{1, 0, 0}
                                          ,{0, 0, 0},{0, 1, 0}
                                          ,{0, 0, 0},{0, 0, 1}};
            Korl_Vulkan_Color4u8 colors[] = {{255,0,0,255},{255,0,0,255}///@TODO: support no alpha channel (if we find out that it's possible to copy less data, which might actually not be possible due to alignment now that I'm thinking about it)
                                            ,{0,255,0,255},{0,255,0,255}
                                            ,{0,0,255,255},{0,0,255,255}};
            KORL_ZERO_STACK(Korl_Vulkan_DrawVertexData, vertexData);
            vertexData.primitiveType      = KORL_VULKAN_PRIMITIVETYPE_LINES;
            vertexData.vertexCount        = korl_arraySize(positions);
            vertexData.positionDimensions = korl_arraySize(positions->elements);
            vertexData.positionsStride    = sizeof(*positions);
            vertexData.positions          = KORL_C_CAST(f32*, positions);
            vertexData.colorsStride       = sizeof(*colors);
            vertexData.colors             = colors;
            korl_vulkan_draw(&vertexData);
        }
#endif
        if(    context->gameApi.korl_game_update
           && !context->gameApi.korl_game_update(1.f/KORL_APP_TARGET_FRAME_HZ, swapchainSize.x, swapchainSize.y, GetFocus() != NULL))
            break;
#if 1///@TODO: delete later; just testing new korl-gfx API
        // test obtaining log buffer & updating a graphics cache for it //
        {
            korl_shared_const u$ MAX_TEXT_LINES = 1000;
            korl_shared_variable u$ cacheLoggedBytes = 0;
            korl_shared_variable Korl_Gfx_Text* gfxText = NULL;
            u$ loggedBytes = 0;
            acu16 logBuffer = korl_log_getBuffer(&loggedBytes);
            const u$ newLoggedBytes = loggedBytes - cacheLoggedBytes;
            if(!gfxText)
            {
                const wchar_t assetNameFont[] = L"submodules/korl/c/test-assets/source-sans/SourceSans3-Semibold.otf";
                gfxText = korl_gfx_text_create(context->allocatorHandle, (acu16){.data = assetNameFont, .size = korl_arraySize(assetNameFont)}, 24.f);
                // gfxText->modelTranslate.xy = (Korl_Math_V2f32){-500,8750};
                KORL_ZERO_STACK(_Korl_Windows_Window_CodepointTest_Log, codepointTestData);
                korl_gfx_text_fifoAdd(gfxText, logBuffer, context->allocatorHandle, _korl_windows_window_codepointTest_log, &codepointTestData);
            }
            else if(newLoggedBytes)
            {
                const u$ newLoggedCharacters = newLoggedBytes/sizeof(*logBuffer.data);
                KORL_ZERO_STACK(_Korl_Windows_Window_CodepointTest_Log, codepointTestData);
                korl_gfx_text_fifoAdd(gfxText
                                     ,(acu16){.data = logBuffer.data + (logBuffer.size - newLoggedCharacters)
                                             ,.size = newLoggedCharacters}
                                     ,context->allocatorHandle, _korl_windows_window_codepointTest_log, &codepointTestData);
                // korl_gfx_text_eraseFront(&gfxText, newLoggedBytes / sizeof(*logBuffer.data));
            }
            const u$ textLines = arrlenu(gfxText->stbDaLines);
            if(textLines > MAX_TEXT_LINES)
                korl_gfx_text_fifoRemove(gfxText, textLines - MAX_TEXT_LINES);
            korl_gfx_text_draw(gfxText, korl_math_aabb2f32_fromExpandedV2(context->gameMemory->gameCamera.pos
                                                                         ,0.5f*context->gameMemory->gameCamera.viewSizeZoomed.x
                                                                         ,0.5f*context->gameMemory->gameCamera.viewSizeZoomed.y));
            cacheLoggedBytes = loggedBytes;
        }
#endif
        korl_time_probeStop(game_update);
        korl_time_probeStart(gui_frame_end);     korl_gui_frameEnd();        korl_time_probeStop(gui_frame_end);
        korl_time_probeStart(flush_glyph_pages); korl_gfx_flushGlyphPages(); korl_time_probeStop(flush_glyph_pages);
        korl_time_probeStart(vulkan_frame_end);  korl_vulkan_frameEnd();     korl_time_probeStop(vulkan_frame_end);
        /* regulate frame rate to our game module's target frame rate */
        //KORL-ISSUE-000-000-059: window: find a frame timing solution that works if vulkan API blocks for some reason
        const PlatformTimeStamp timeStampRenderLoopBottom = korl_timeStamp();
        const Korl_Time_Counts timeCountsRenderLoop = korl_time_timeStampCountDifference(timeStampRenderLoopBottom, timeStampLast);
        korl_time_probeStart(sleep);
        if(timeCountsRenderLoop < timeCountsTargetGamePerFrame)
            korl_time_sleep(timeCountsTargetGamePerFrame - timeCountsRenderLoop);
        const Korl_Time_Counts timeCountsSleep = korl_time_probeStop(sleep);
        timeStampLast = korl_timeStamp();
        if(renderFrameCount < ~(u$)0)
            renderFrameCount++;
        const Korl_Time_Counts timeCountsMainLoop = korl_time_probeStop(Main_Loop);
        if(stats)
        {
            stats->timeTotal = timeCountsMainLoop;
            stats->timeSleep = timeCountsSleep;
        }
    }
    /* report frame stats */
    korl_log(INFO, "Window Render Loop Times:");
    Korl_Time_Counts timeCountAverageTotal = 0;
    Korl_Time_Counts timeCountAverageSleep = 0;
    wchar_t durationBuffer[32];
    wchar_t durationBuffer2[32];
    for(Korl_MemoryPool_Size s = 0; s < KORL_MEMORY_POOL_SIZE(loopStats); s++)
    {
        korl_assert(0 < korl_time_countsFormatBuffer(loopStats[s].timeTotal, durationBuffer, sizeof(durationBuffer)));
        korl_assert(0 < korl_time_countsFormatBuffer(loopStats[s].timeSleep, durationBuffer2, sizeof(durationBuffer2)));
        korl_log_noMeta(INFO, "[% 2u]: { total: %ws, sleep: %ws }", s, durationBuffer, durationBuffer2);
        if(s == 2)
        {
            timeCountAverageTotal = loopStats[s].timeTotal;
            timeCountAverageSleep = loopStats[s].timeSleep;
        }
        else if (s > 1)// avoid the first couple frames for now since there is a lot of spin-up work that throws off metrics
        {
            timeCountAverageTotal += loopStats[s].timeTotal;
            timeCountAverageSleep += loopStats[s].timeSleep;
            // timeCountAverageTotal = (timeCountAverageTotal + loopStats[s].timeTotal) / 2;
            // timeCountAverageSleep = (timeCountAverageSleep + loopStats[s].timeSleep) / 2;
        }
    }
    timeCountAverageTotal /= KORL_MEMORY_POOL_SIZE(loopStats);
    timeCountAverageSleep /= KORL_MEMORY_POOL_SIZE(loopStats);
    korl_assert(0 < korl_time_countsFormatBuffer(timeCountAverageTotal, durationBuffer, sizeof(durationBuffer)));
    korl_log(INFO, "Average Loop Total Time: %ws", durationBuffer);
    korl_assert(0 < korl_time_countsFormatBuffer(timeCountAverageSleep, durationBuffer, sizeof(durationBuffer)));
    korl_log(INFO, "Average Sleep Time     : %ws", durationBuffer);
    korl_log(INFO, "Average Unused Frame %% : %f", timeCountAverageSleep * 100.f / timeCountsTargetGamePerFrame);
    /**/
    korl_vulkan_destroySurface();
    string_free(stringGameDll);
}
korl_internal void korl_windows_window_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer)
{
    const Korl_Math_V2u32 surfaceSize = korl_vulkan_getSurfaceSize();
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_windows_window_context.stringPool, sizeof(_korl_windows_window_context.stringPool));
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_windows_window_context.gameMemory, sizeof(_korl_windows_window_context.gameMemory));
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &surfaceSize.x, sizeof(surfaceSize.x));
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &surfaceSize.y, sizeof(surfaceSize.y));
}
korl_internal bool korl_windows_window_saveStateRead(HANDLE hFile)
{
    //KORL-ISSUE-000-000-079: stringPool/file/savestate: either create a (de)serialization API for stringPool, or just put context state into a single allocation?
    if(!ReadFile(hFile, &_korl_windows_window_context.stringPool, sizeof(_korl_windows_window_context.stringPool), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    u64 gameMemory;
    if(!ReadFile(hFile, &gameMemory, sizeof(gameMemory), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    _korl_windows_window_context.gameMemory = KORL_C_CAST(void*, gameMemory);
    Korl_Math_V2u32 surfaceSize;
    if(!ReadFile(hFile, &surfaceSize.x, sizeof(surfaceSize.x), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    if(!ReadFile(hFile, &surfaceSize.y, sizeof(surfaceSize.y), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    POINT clientOrigin = {0, 0};
    korl_assert(ClientToScreen(_korl_windows_window_context.window.handle, &clientOrigin));
    RECT windowRect = (RECT){.left = clientOrigin.x, .top = clientOrigin.y, .right = clientOrigin.x + surfaceSize.x, .bottom = clientOrigin.y + surfaceSize.y};
    korl_assert(AdjustWindowRect(&windowRect, _korl_windows_window_context.window.style, _korl_windows_window_context.window.hasMenu));
    korl_assert(MoveWindow(_korl_windows_window_context.window.handle, windowRect.left, windowRect.top, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, TRUE));
    korl_vulkan_deferredResize(surfaceSize.x, surfaceSize.y);
    return true;
}
#undef _LOCAL_STRING_POOL_POINTER
