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
#include "korl-interface-game.h"
#include "korl-gui.h"
#include "korl-file.h"
#include "korl-stb-ds.h"
#include "korl-stringPool.h"
#include "korl-bluetooth.h"
#include "korl-resource.h"
#include "korl-codec-configuration.h"
#include "korl-command.h"
#include "korl-clipboard.h"
#include "korl-string.h"
#include "korl-crash.h"
#include "korl-sfx.h"
#include "korl-memoryState.h"
// we should probably delete all the log reporting code in here when KORL-FEATURE-000-000-009 & KORL-FEATURE-000-000-028 are complete
// #define _KORL_WINDOWS_WINDOW_LOG_REPORTS
#if KORL_DEBUG
    //@TODO: comment all these out
    // #define _KORL_WINDOWS_WINDOW_DEBUG_DISPLAY_MEMORY_STATE
    // #define _KORL_WINDOWS_WINDOW_DEBUG_TEST_GFX_TEXT
    #define _KORL_WINDOWS_WINDOW_DEBUG_HEAP_UNIT_TESTS
#endif
#if defined(_LOCAL_STRING_POOL_POINTER)
#   undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_windows_window_context.stringPool))
korl_global_const TCHAR    _KORL_WINDOWS_WINDOW_CLASS_NAME[]     = _T("KorlWindowClass");
korl_shared_const wchar_t* _KORL_WINDOWS_WINDOW_CONFIG_FILE_NAME = L"korl-windows-window.ini";
typedef struct _Korl_Windows_Window_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool stringPool;
    void* gameContext;
    void* memoryStateLast;
    struct
    {
        HWND handle;
        DWORD style;
        BOOL hasMenu;
    } window;// for now, we will only ever have _one_ window
    struct
    {
        fnSig_korl_game_initialize*      korl_game_initialize;
        fnSig_korl_game_onReload*        korl_game_onReload;
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
    bool deferMemoryReport; // defer until the beginning of the next frame
    bool deferCpuReport;
    i32  deferCpuReportMaxDepth;
    struct
    {
        bool deferSaveConfiguration;
        bool deferMaximize;/* extremely stupid hack; we need this because if a window is maximized via ShowWindow during WM_CREATE, the window's WINDOWPLACEMENT will contain the _wrong_ values for rcNormalPosition !!!!!  Without this hack, we would lose the original window size between application runs... */
        Korl_File_Descriptor fileDescriptor;
        struct
        {
            Korl_File_AsyncIoHandle handle;
            enum
                {_KORL_WINDOWS_WINDOW_CONFIGURATION_ASYNCIO_OPERATION_READ
                ,_KORL_WINDOWS_WINDOW_CONFIGURATION_ASYNCIO_OPERATION_WRITE
            } operation;
        } asyncIo;
        acu8 fileDataBuffer;// temporary storage of the UTF-8 configuration file itself, for both reading & writing
    } configuration;
} _Korl_Windows_Window_Context;
korl_global_variable _Korl_Windows_Window_Context _korl_windows_window_context;
korl_global_variable Korl_KeyboardCode _korl_windows_window_virtualKeyMap[0xFF];
//KORL-ISSUE-000-000-113: windows-window: (minor) korl_clipboard_* API is being placed inside the korl-windows-window module due to the fact that we require a HWND to use these API on Windows; not sure if there is a better way to organize this...
korl_internal KORL_FUNCTION_korl_clipboard_set(korl_clipboard_set)
{
    if(!OpenClipboard(_korl_windows_window_context.window.handle))
        return;
    KORL_WINDOWS_CHECK(EmptyClipboard());
    HGLOBAL clipboardMemory     = 0;
    UINT    clipboardDataFormat = 0;
    switch(dataFormat)
    {
    case KORL_CLIPBOARD_DATA_FORMAT_UTF8:{
        clipboardDataFormat = CF_UNICODETEXT;
        Korl_StringPool_String stringTemp = korl_stringNewAcu8(&_korl_windows_window_context.stringPool, data);
        acu16 rawU16 = korl_stringPool_getRawAcu16(&stringTemp);
        u16* clipboardMemoryLocked = NULL;
        clipboardMemory = GlobalAlloc(GMEM_MOVEABLE, (rawU16.size + 1/*null-terminator*/) * sizeof(*rawU16.data));
        if(!clipboardMemory)
            goto data_format_utf8_cleanup;
        clipboardMemoryLocked = GlobalLock(clipboardMemory);
        if(!clipboardMemoryLocked)
        {
            korl_logLastError("GlobalLock failed");
            goto data_format_utf8_cleanup;
        }
        korl_memory_copy(clipboardMemoryLocked, rawU16.data, rawU16.size*sizeof(*rawU16.data));
        clipboardMemoryLocked[rawU16.size] = 0;//null-terminate the clipboard string
        data_format_utf8_cleanup:
            string_free(&stringTemp);
            if(clipboardMemoryLocked)
                KORL_WINDOWS_CHECK(GlobalUnlock(clipboardMemoryLocked));
            if(!clipboardMemory)
                goto close_clipboard;
        break;}
    }
    if(SetClipboardData(clipboardDataFormat, clipboardMemory) == NULL)
        korl_logLastError("SetClipboardData failed");
    close_clipboard:
        KORL_WINDOWS_CHECK(CloseClipboard());
}
//KORL-ISSUE-000-000-113: windows-window: (minor) korl_clipboard_* API is being placed inside the korl-windows-window module due to the fact that we require a HWND to use these API on Windows; not sure if there is a better way to organize this...
korl_internal KORL_FUNCTION_korl_clipboard_get(korl_clipboard_get)
{
    KORL_ZERO_STACK(acu8, result);
    if(!OpenClipboard(_korl_windows_window_context.window.handle))
        return result;
    switch(dataFormat)
    {
    case KORL_CLIPBOARD_DATA_FORMAT_UTF8:{
        if(!IsClipboardFormatAvailable(CF_UNICODETEXT))
            goto close_clipboard_return_result;
        const HANDLE clipboardData = GetClipboardData(CF_UNICODETEXT);
        if(!clipboardData)
        {
            korl_log(WARNING, "GetClipboardData(CF_UNICODETEXT) failed, GetLastError=0x%X", GetLastError());
            goto close_clipboard_return_result;
        }
        const u16* clipboardUtf16 = GlobalLock(clipboardData);
        if(!clipboardUtf16)
        {
            korl_logLastError("GlobalLock failed");
            goto close_clipboard_return_result;
        }
        Korl_StringPool_String stringTemp = korl_stringNewUtf16(&_korl_windows_window_context.stringPool, clipboardUtf16);
        acu8 stringTempUtf8 = string_getRawAcu8(&stringTemp);
        result.size = stringTempUtf8.size + 1/*null-terminator*/;
        u8* resultData = korl_allocate(allocator, result.size);
        result.data = resultData;
        korl_memory_copy(resultData, stringTempUtf8.data, stringTempUtf8.size);
        resultData[stringTempUtf8.size] = 0;
        string_free(&stringTemp);
        KORL_WINDOWS_CHECK(GlobalUnlock(clipboardData));
        break;}
    }
    close_clipboard_return_result:
        KORL_WINDOWS_CHECK(CloseClipboard());
        return result;
}
korl_internal void _korl_windows_window_configurationLoad(void)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    korl_assert(context->configuration.fileDescriptor.handle == NULL);
    if(!korl_file_open(KORL_FILE_PATHTYPE_LOCAL_DATA, _KORL_WINDOWS_WINDOW_CONFIG_FILE_NAME, &context->configuration.fileDescriptor, true/*async*/))
    {
        /* there is no config file here, so we need to make a blank one */
        if(!korl_file_create(KORL_FILE_PATHTYPE_LOCAL_DATA, _KORL_WINDOWS_WINDOW_CONFIG_FILE_NAME, &context->configuration.fileDescriptor, true/*async*/))
            korl_log(ERROR, "failed to create file \"%ws\"", _KORL_WINDOWS_WINDOW_CONFIG_FILE_NAME);
        korl_file_close(&context->configuration.fileDescriptor);
        context->configuration.deferSaveConfiguration = true;// save configuration ASAP so that it is valid on the next run
    }
    else/* the config file exists; let's read it & apply the settings when it finishes loading */
    {
        const u32 fileBytes = korl_file_getTotalBytes(context->configuration.fileDescriptor);
        context->configuration.fileDataBuffer.size = fileBytes;
        context->configuration.fileDataBuffer.data = korl_reallocate(context->allocatorHandle, KORL_C_CAST(void*, context->configuration.fileDataBuffer.data), fileBytes);
        context->configuration.asyncIo.handle      = korl_file_readAsync(context->configuration.fileDescriptor, KORL_C_CAST(void*, context->configuration.fileDataBuffer.data), fileBytes);
        context->configuration.asyncIo.operation   = _KORL_WINDOWS_WINDOW_CONFIGURATION_ASYNCIO_OPERATION_READ;
    }
}
korl_internal void _korl_windows_window_configurationStep(void)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    /* if there is currently an asyncIo operation in progress, we have to do 
        specific behavior based on whether or not we're loading/saving from/to 
        the config file */
    if(context->configuration.asyncIo.handle)
    {
        const Korl_File_GetAsyncIoResult asyncIoResult = korl_file_getAsyncIoResult(&context->configuration.asyncIo.handle, false/*don't block*/);
        switch(asyncIoResult)
        {
        case KORL_FILE_GET_ASYNC_IO_RESULT_DONE:{
            /* close the file handle */
            korl_file_close(&context->configuration.fileDescriptor);
            switch(context->configuration.asyncIo.operation)
            {
            case _KORL_WINDOWS_WINDOW_CONFIGURATION_ASYNCIO_OPERATION_READ:{
                KORL_ZERO_STACK(WINDOWPLACEMENT, windowPlacement);
                windowPlacement.length = sizeof(WINDOWPLACEMENT);
                /* we have read in the config file; let's use the data we 
                    obtained to set the current window configuration */
                KORL_ZERO_STACK(Korl_Codec_Configuration, configCodec);
                korl_codec_configuration_create(&configCodec, _korl_windows_window_context.allocatorHandle);
                korl_codec_configuration_fromUtf8(&configCodec, context->configuration.fileDataBuffer);
                /* it is possible to get an invalid/corrupt config file that 
                    isn't fully populated with all these keys; in this case, 
                    let's just save a fresh config if we detect this condition */
                //KORL-ISSUE-000-000-105: window, configuration: detect invalid/corrupt configuration file, and maybe just save a fresh configuration file in this case
                /**/
                windowPlacement.flags                 = korl_codec_configuration_getU32(&configCodec, KORL_RAW_CONST_UTF8("flags"));
                windowPlacement.showCmd               = korl_codec_configuration_getU32(&configCodec, KORL_RAW_CONST_UTF8("showCmd"));
                windowPlacement.ptMinPosition.x       = korl_codec_configuration_getI32(&configCodec, KORL_RAW_CONST_UTF8("ptMinPosition.x"));
                windowPlacement.ptMinPosition.y       = korl_codec_configuration_getI32(&configCodec, KORL_RAW_CONST_UTF8("ptMinPosition.y"));
                windowPlacement.ptMaxPosition.x       = korl_codec_configuration_getI32(&configCodec, KORL_RAW_CONST_UTF8("ptMaxPosition.x"));
                windowPlacement.ptMaxPosition.y       = korl_codec_configuration_getI32(&configCodec, KORL_RAW_CONST_UTF8("ptMaxPosition.y"));
                windowPlacement.rcNormalPosition.left = korl_codec_configuration_getI32(&configCodec, KORL_RAW_CONST_UTF8("rcNormalPosition.left"));
                windowPlacement.rcNormalPosition.top  = korl_codec_configuration_getI32(&configCodec, KORL_RAW_CONST_UTF8("rcNormalPosition.top"));
                const i32 rcNormalSizeX               = korl_codec_configuration_getI32(&configCodec, KORL_RAW_CONST_UTF8("rcNormalPosition.size.x"));
                const i32 rcNormalSizeY               = korl_codec_configuration_getI32(&configCodec, KORL_RAW_CONST_UTF8("rcNormalPosition.size.y"));
                korl_codec_configuration_destroy(&configCodec);
                windowPlacement.rcNormalPosition.right  = windowPlacement.rcNormalPosition.left + rcNormalSizeX;
                windowPlacement.rcNormalPosition.bottom = windowPlacement.rcNormalPosition.top  + rcNormalSizeY;
                if(windowPlacement.showCmd == SW_SHOWMAXIMIZED)
                {
                    context->configuration.deferMaximize = true;
                    windowPlacement.showCmd = SW_SHOWNORMAL;
                }
                KORL_WINDOWS_CHECK(SetWindowPlacement(_korl_windows_window_context.window.handle, &windowPlacement));
                break;}
            case _KORL_WINDOWS_WINDOW_CONFIGURATION_ASYNCIO_OPERATION_WRITE:{
                /* we have written to the config file; nothing else to do probably */
                break;}
            default:
                korl_log(ERROR, "invalid asyncIo operation: %i", context->configuration.asyncIo.operation);
                break;
            }
            break;}
        case KORL_FILE_GET_ASYNC_IO_RESULT_PENDING:{
            /* if async io is still processing, just do nothing */
            break;}
        default:
            korl_log(ERROR, "invalid asyncIoResult: %i", asyncIoResult);
            break;
        }
    }
    /* if there's no asyncIo operation, we can process pending write config requests */
    if(!context->configuration.asyncIo.handle && context->configuration.deferSaveConfiguration)
    {
        /* construct a data buffer with window configuration to write to the config file */
        KORL_ZERO_STACK(WINDOWPLACEMENT, windowPlacement);
        KORL_WINDOWS_CHECK(GetWindowPlacement(_korl_windows_window_context.window.handle, &windowPlacement));
        KORL_ZERO_STACK(Korl_Codec_Configuration, configCodec);
        korl_codec_configuration_create(&configCodec, _korl_windows_window_context.allocatorHandle);
        korl_codec_configuration_setU32(&configCodec, KORL_RAW_CONST_UTF8("flags"                  ), windowPlacement.flags);
        korl_codec_configuration_setU32(&configCodec, KORL_RAW_CONST_UTF8("showCmd"                ), windowPlacement.showCmd);
        korl_codec_configuration_setI32(&configCodec, KORL_RAW_CONST_UTF8("ptMinPosition.x"        ), windowPlacement.ptMinPosition.x);
        korl_codec_configuration_setI32(&configCodec, KORL_RAW_CONST_UTF8("ptMinPosition.y"        ), windowPlacement.ptMinPosition.y);
        korl_codec_configuration_setI32(&configCodec, KORL_RAW_CONST_UTF8("ptMaxPosition.x"        ), windowPlacement.ptMaxPosition.x);
        korl_codec_configuration_setI32(&configCodec, KORL_RAW_CONST_UTF8("ptMaxPosition.y"        ), windowPlacement.ptMaxPosition.y);
        korl_codec_configuration_setI32(&configCodec, KORL_RAW_CONST_UTF8("rcNormalPosition.left"  ), windowPlacement.rcNormalPosition.left);
        korl_codec_configuration_setI32(&configCodec, KORL_RAW_CONST_UTF8("rcNormalPosition.top"   ), windowPlacement.rcNormalPosition.top);
        korl_codec_configuration_setI32(&configCodec, KORL_RAW_CONST_UTF8("rcNormalPosition.size.x"), windowPlacement.rcNormalPosition.right  - windowPlacement.rcNormalPosition.left);
        korl_codec_configuration_setI32(&configCodec, KORL_RAW_CONST_UTF8("rcNormalPosition.size.y"), windowPlacement.rcNormalPosition.bottom - windowPlacement.rcNormalPosition.top);
        acu8 configUtf8 = korl_codec_configuration_toUtf8(&configCodec, _korl_windows_window_context.allocatorHandle);
        korl_codec_configuration_destroy(&configCodec);
        korl_free(context->allocatorHandle, KORL_C_CAST(void*, context->configuration.fileDataBuffer.data));
        context->configuration.fileDataBuffer = configUtf8;
        /* open async file handle */
        if(korl_file_openClear(KORL_FILE_PATHTYPE_LOCAL_DATA, _KORL_WINDOWS_WINDOW_CONFIG_FILE_NAME, &context->configuration.fileDescriptor, true/*async*/))
        {
            /* dispatch asyncIo write command */
            context->configuration.asyncIo.handle    = korl_file_writeAsync(context->configuration.fileDescriptor, context->configuration.fileDataBuffer.data, context->configuration.fileDataBuffer.size);
            context->configuration.asyncIo.operation = _KORL_WINDOWS_WINDOW_CONFIGURATION_ASYNCIO_OPERATION_WRITE;
            /* clear the defer write flag so that we can raise it again when config changes */
            context->configuration.deferSaveConfiguration = false;
        }
    }
}
korl_internal LRESULT CALLBACK _korl_windows_window_windowProcedure(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    if(korl_crash_pending())
        /* do not process any window messages during a crash; we _must_ do this, 
            because during a crash the user _may_ be presented with a modal 
            dialog on the same thread, and that will cause window messages to 
            be pumped into here, which we definitely want to ignore */
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    LRESULT result = 0;
    /* ignore all window events that don't belong to the windows we are 
        responsible for in this code module */
    if(context->window.handle)// only do this logic if the context's window handle is set, because the first window message is a WM_CREATE, which we will use to set the context window handle
    {
        // korl_log(VERBOSE, "uMsg=%u|0x%X", uMsg, uMsg);
        if(hWnd != context->window.handle)
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        korl_gui_windows_processMessage(hWnd, uMsg, wParam, lParam, _korl_windows_window_virtualKeyMap, korl_arraySize(_korl_windows_window_virtualKeyMap));
        if(korl_windows_gamepad_processMessage(hWnd, uMsg, wParam, lParam, &result))
            return result;
        if(korl_windows_mouse_processMessage(hWnd, uMsg, wParam, lParam, &result, context->gameApi.korl_game_onMouseEvent))
            return result;
    }
    switch(uMsg)
    {
    //KORL-FEATURE-000-000-024: gui/gfx: add DPI-awareness & support; process WM_DPICHANGED messages
    case WM_CREATE:{
        //KORL-FEATURE-000-000-024: gui/gfx: add DPI-awareness & support; query the monitor the window was created on to determine DPI scale-factor
        korl_assert(context->window.handle == NULL);
        context->window.handle = hWnd;
        /* before the window actually appears on screen, we attempt to load the 
            window configuration file from disk storage; if we are able to 
            obtain the configuration, we can issue a window resize/reposition 
            event to the last known value before the window actually appears */
        _korl_windows_window_configurationLoad();
        /* if we have configuration to load, just spin here until we load the 
            window configuration; this process is extremely fast in my testing, 
            so it should be fine to block the application startup to get this 
            data before the window appears */
        while(context->configuration.asyncIo.handle)
            _korl_windows_window_configurationStep();
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
        break;}
    case WM_SIZE:{
        const UINT clientWidth  = LOWORD(lParam);
        const UINT clientHeight = HIWORD(lParam);
        korl_vulkan_deferredResize(clientWidth, clientHeight);
        context->configuration.deferSaveConfiguration = true;
        break;}
    case WM_MOVE:{
        // const POINTS clientTopLeft = MAKEPOINTS(lParam);// we can't actually use this macro, because I undefined the stupid Microsoft "far" define lol; but who cares?
        const POINTS clientTopLeft = *KORL_C_CAST(POINTS*, &lParam);
        context->configuration.deferSaveConfiguration = true;
        // KORL-ISSUE-000-000-107: window: when we process a WM_MOVE event with wParam==SIZE_RESTORED, ensure that the window is never completely off-screen
        //  Currently, this is possible if you:
        //  - with an extra monitor, move the normalized window to the external monitor
        //  - maximize the window
        //  - close the application
        //  - disconnect the monitor
        //  - open the application
        //  - restore the window to its normal state; it will attempt to be moved to the desktop of the disconnected monitor!
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
    default:
        result = DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return result;// the most common user-handled result
}
korl_internal KORL_ASSETCACHE_ON_ASSET_HOT_RELOADED_CALLBACK(_korl_windows_window_onAssetHotReloaded)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    korl_resource_onAssetHotReload(rawUtf16AssetName, assetData);
    if(context->gameApi.korl_game_onAssetReloaded)
        context->gameApi.korl_game_onAssetReloaded(rawUtf16AssetName, assetData);
}
korl_internal void _korl_windows_window_findGameApiAddresses(HMODULE hModule)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    context->gameApi.korl_game_initialize      = KORL_C_CAST(fnSig_korl_game_initialize*,      GetProcAddress(hModule, "korl_game_initialize"));
    context->gameApi.korl_game_onReload        = KORL_C_CAST(fnSig_korl_game_onReload*,        GetProcAddress(hModule, "korl_game_onReload"));
    context->gameApi.korl_game_onKeyboardEvent = KORL_C_CAST(fnSig_korl_game_onKeyboardEvent*, GetProcAddress(hModule, "korl_game_onKeyboardEvent"));
    context->gameApi.korl_game_onMouseEvent    = KORL_C_CAST(fnSig_korl_game_onMouseEvent*,    GetProcAddress(hModule, "korl_game_onMouseEvent"));
    context->gameApi.korl_game_onGamepadEvent  = KORL_C_CAST(fnSig_korl_game_onGamepadEvent*,  GetProcAddress(hModule, "korl_game_onGamepadEvent"));
    context->gameApi.korl_game_update          = KORL_C_CAST(fnSig_korl_game_update*,          GetProcAddress(hModule, "korl_game_update"));
    context->gameApi.korl_game_onAssetReloaded = KORL_C_CAST(fnSig_korl_game_onAssetReloaded*, GetProcAddress(hModule, "korl_game_onAssetReloaded"));
}
KORL_EXPORT KORL_FUNCTION_korl_command_callback(_korl_windows_window_commandSave)
{
    _korl_windows_window_context.deferSaveStateSave = true;
}
KORL_EXPORT KORL_FUNCTION_korl_command_callback(_korl_windows_window_commandLoad)
{
    _korl_windows_window_context.deferSaveStateLoad = true;
}
KORL_EXPORT KORL_FUNCTION_korl_command_callback(_korl_windows_window_commandMemoryReport)
{
    _korl_windows_window_context.deferMemoryReport = true;
}
KORL_EXPORT KORL_FUNCTION_korl_command_callback(_korl_windows_window_commandCpuReport)
{
    if(parametersSize < 2)
        korl_log(INFO, "cpu command usage: `cpu <maxProbeDepth>`, where <maxProbeDepth> == {< 0 => log all probes, >= 0 => exclude probes above this depth in the probe tree}");
    else
    {
        bool stringConversionValid = false;
        _korl_windows_window_context.deferCpuReportMaxDepth = korl_checkCast_i$_to_i32(korl_string_utf8_to_i64(parameters[1], &stringConversionValid));
        if(!stringConversionValid)
        {
            korl_log(WARNING, "invalid parameter[1] \"%hs\"", parameters[1].data);
            return;
        }
        _korl_windows_window_context.deferCpuReport = true;
    }
}
korl_internal void _korl_windows_window_commandCrash_stackOverflow(bool recurse)
{
    if(recurse)
        _korl_windows_window_commandCrash_stackOverflow(recurse);
}
KORL_EXPORT KORL_FUNCTION_korl_command_callback(_korl_windows_window_commandCrash)
{
    if(parametersSize != 2)
        korl_log(INFO, "crash command usage: `crash <type>`, where <type> == {\"write-zero\", \"assert\", \"stack-overflow\", \"log-error\"}");
    else if(0 == korl_string_compareUtf8(KORL_C_CAST(const char*, parameters[1].data), "write-zero"))
        *(int*)0 = 0;
    else if(0 == korl_string_compareUtf8(KORL_C_CAST(const char*, parameters[1].data), "assert"))
        korl_assert(false);
    else if(0 == korl_string_compareUtf8(KORL_C_CAST(const char*, parameters[1].data), "stack-overflow"))
        _korl_windows_window_commandCrash_stackOverflow(true);
    else if(0 == korl_string_compareUtf8(KORL_C_CAST(const char*, parameters[1].data), "log-error"))
        korl_log(ERROR, "rest in pepperonis");
    else
        korl_log(WARNING, "invalid parameter \"%hs\"", parameters[1].data);
}
korl_internal void korl_windows_window_initialize(void)
{
    korl_memory_zero(&_korl_windows_window_context, sizeof(_korl_windows_window_context));
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(1);
    _korl_windows_window_context.allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, L"korl-windows-window", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
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
    _korl_windows_window_virtualKeyMap[VK_OEM_COMMA    ] = KORL_KEY_COMMA;
    _korl_windows_window_virtualKeyMap[VK_OEM_PERIOD   ] = KORL_KEY_PERIOD;
    _korl_windows_window_virtualKeyMap[VK_OEM_2        ] = KORL_KEY_SLASH_FORWARD;
    _korl_windows_window_virtualKeyMap[VK_OEM_5        ] = KORL_KEY_SLASH_BACK;
    _korl_windows_window_virtualKeyMap[VK_OEM_4        ] = KORL_KEY_CURLYBRACE_LEFT;
    _korl_windows_window_virtualKeyMap[VK_OEM_6        ] = KORL_KEY_CURLYBRACE_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_OEM_1        ] = KORL_KEY_SEMICOLON;
    _korl_windows_window_virtualKeyMap[VK_OEM_7        ] = KORL_KEY_QUOTE;
    _korl_windows_window_virtualKeyMap[VK_OEM_3        ] = KORL_KEY_GRAVE;
    _korl_windows_window_virtualKeyMap[VK_OEM_MINUS    ] = KORL_KEY_TENKEYLESS_MINUS;
    _korl_windows_window_virtualKeyMap[VK_OEM_NEC_EQUAL] = KORL_KEY_EQUALS;
    _korl_windows_window_virtualKeyMap[VK_BACK         ] = KORL_KEY_BACKSPACE;
    _korl_windows_window_virtualKeyMap[VK_ESCAPE       ] = KORL_KEY_ESCAPE;
    _korl_windows_window_virtualKeyMap[VK_RETURN       ] = KORL_KEY_ENTER;
    _korl_windows_window_virtualKeyMap[VK_SPACE        ] = KORL_KEY_SPACE;
    _korl_windows_window_virtualKeyMap[VK_TAB          ] = KORL_KEY_TAB;
    _korl_windows_window_virtualKeyMap[VK_SHIFT        ] = KORL_KEY_SHIFT_LEFT;
    _korl_windows_window_virtualKeyMap[VK_SHIFT        ] = KORL_KEY_SHIFT_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_CONTROL      ] = KORL_KEY_CONTROL_LEFT;
    _korl_windows_window_virtualKeyMap[VK_CONTROL      ] = KORL_KEY_CONTROL_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_MENU         ] = KORL_KEY_ALT_LEFT;
    _korl_windows_window_virtualKeyMap[VK_MENU         ] = KORL_KEY_ALT_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_UP           ] = KORL_KEY_ARROW_UP;
    _korl_windows_window_virtualKeyMap[VK_DOWN         ] = KORL_KEY_ARROW_DOWN;
    _korl_windows_window_virtualKeyMap[VK_LEFT         ] = KORL_KEY_ARROW_LEFT;
    _korl_windows_window_virtualKeyMap[VK_RIGHT        ] = KORL_KEY_ARROW_RIGHT;
    _korl_windows_window_virtualKeyMap[VK_INSERT       ] = KORL_KEY_INSERT;
    _korl_windows_window_virtualKeyMap[VK_DELETE       ] = KORL_KEY_DELETE;
    _korl_windows_window_virtualKeyMap[VK_HOME         ] = KORL_KEY_HOME;
    _korl_windows_window_virtualKeyMap[VK_END          ] = KORL_KEY_END;
    _korl_windows_window_virtualKeyMap[VK_PRIOR        ] = KORL_KEY_PAGE_UP;
    _korl_windows_window_virtualKeyMap[VK_NEXT         ] = KORL_KEY_PAGE_DOWN;
    _korl_windows_window_virtualKeyMap[VK_DECIMAL      ] = KORL_KEY_NUMPAD_PERIOD;
    _korl_windows_window_virtualKeyMap[VK_DIVIDE       ] = KORL_KEY_NUMPAD_DIVIDE;
    _korl_windows_window_virtualKeyMap[VK_MULTIPLY     ] = KORL_KEY_NUMPAD_MULTIPLY;
    _korl_windows_window_virtualKeyMap[VK_SUBTRACT     ] = KORL_KEY_NUMPAD_MINUS;
    _korl_windows_window_virtualKeyMap[VK_ADD          ] = KORL_KEY_NUMPAD_ADD;
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
#if 0// KORL-FEATURE-000-000-024: gui/gfx: add DPI-awareness & support
    /* take control of DPI-awareness from the system before creating any windows */
    DPI_AWARENESS_CONTEXT dpiContextOld = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2/* DPI awareness per monitor V2 allows the application to automatically scale the non-client region for us (title bar, etc...) */);
    korl_assert(dpiContextOld != NULL);
#endif
    /* register some internal KORL commands */
    korl_command_register(KORL_RAW_CONST_UTF8("save")  , _korl_windows_window_commandSave);
    korl_command_register(KORL_RAW_CONST_UTF8("load")  , _korl_windows_window_commandLoad);
    korl_command_register(KORL_RAW_CONST_UTF8("memory"), _korl_windows_window_commandMemoryReport);
    korl_command_register(KORL_RAW_CONST_UTF8("cpu")   , _korl_windows_window_commandCpuReport);
    korl_command_register(KORL_RAW_CONST_UTF8("crash") , _korl_windows_window_commandCrash);
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
    const BOOL successAdjustClientRect = AdjustWindowRect(&rectCenteredClient, windowStyle, windowHasMenu);
    if(!successAdjustClientRect) korl_logLastError("AdjustWindowRect failed!");
    const HWND hWnd = CreateWindowEx(0/*extended style flags*/, _KORL_WINDOWS_WINDOW_CLASS_NAME
                                    ,KORL_APPLICATION_NAME, windowStyle
                                    ,rectCenteredClient.left/*X*/, rectCenteredClient.top/*Y*/
                                    ,rectCenteredClient.right  - rectCenteredClient.left/*width*/
                                    ,rectCenteredClient.bottom - rectCenteredClient.top/*height*/
                                    ,NULL/*hWndParent*/, hMenu, hInstance
                                    ,NULL/*lpParam; passed to WM_CREATE*/);
    if(!hWnd) korl_logLastError("CreateWindowEx failed!");
    context->window.style   = windowStyle;
    context->window.hasMenu = windowHasMenu;
    if(context->configuration.deferMaximize)
    {
        context->configuration.deferMaximize = false;
        KORL_WINDOWS_CHECK(ShowWindow(_korl_windows_window_context.window.handle, SW_SHOWMAXIMIZED));
    }
    korl_windows_gamepad_registerWindow(hWnd, context->allocatorHandle);
    korl_windows_mouse_registerWindow(hWnd);
}
korl_internal void _korl_windows_window_gameInitialize(const KorlPlatformApi* korlApi)
{
    _Korl_Windows_Window_Context*const context = &_korl_windows_window_context;
    if(context->gameContext)
        return;
    if(   !context->gameApi.korl_game_initialize
       || !context->gameApi.korl_game_onReload)
       return;
    context->gameContext = context->gameApi.korl_game_initialize(*korlApi);
    context->gameApi.korl_game_onReload(context->gameContext, *korlApi);
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
                                        KORL_FILE_PATHTYPE_TEMPORARY_DATA, string_getRawUtf16(&stringGameDllTemp));
            korl_assert(resultRenameReplace != KORL_FILE_RESULT_RENAME_REPLACE_SOURCE_FILE_DOES_NOT_EXIST);
            if(resultRenameReplace == KORL_FILE_RESULT_RENAME_REPLACE_SUCCESS)
            {
                if(context->gameDll)
                    FreeLibrary(context->gameDll);
                context->gameDll = korl_file_loadDynamicLibrary(KORL_FILE_PATHTYPE_TEMPORARY_DATA, string_getRawUtf16(&stringGameDllTemp));
                korl_assert(context->gameDll);
                korl_assert(korl_file_getDateStampLastWriteFileName(KORL_FILE_PATHTYPE_EXECUTABLE_DIRECTORY, utf16GameDllFileName, &context->gameDllLastWriteDateStamp));
                korl_command_registerModule(context->gameDll, KORL_RAW_CONST_UTF8("korl-game"));
            }
            string_free(&stringGameDllTemp);
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
#ifdef _KORL_WINDOWS_WINDOW_DEBUG_DISPLAY_MEMORY_STATE
typedef struct _Korl_Windows_Window_DebugMemoryEnumContext_AllocatorData
{
    u32 allocationCount;
    const wchar_t* name;
} _Korl_Windows_Window_DebugMemoryEnumContext_AllocatorData;
typedef struct _Korl_Windows_Window_DebugMemoryEnumContext
{
    _Korl_Windows_Window_DebugMemoryEnumContext_AllocatorData* stbDaAllocatorData;
} _Korl_Windows_Window_DebugMemoryEnumContext;
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS_CALLBACK(_korl_windows_window_loop_allocationEnumCallback)
{
    _Korl_Windows_Window_Context*const                context     = &_korl_windows_window_context;
    _Korl_Windows_Window_DebugMemoryEnumContext*const enumContext = KORL_C_CAST(_Korl_Windows_Window_DebugMemoryEnumContext*, userData);
    arrlast(enumContext->stbDaAllocatorData).allocationCount++;
    return true;// true => continue enumerating
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_windows_window_loop_allocatorEnumCallback)
{
    _Korl_Windows_Window_Context*const                context     = &_korl_windows_window_context;
    _Korl_Windows_Window_DebugMemoryEnumContext*const enumContext = KORL_C_CAST(_Korl_Windows_Window_DebugMemoryEnumContext*, userData);
    // if(!(allocatorFlags & KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE))
    //     return true;//true => continue iterating over allocators
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), enumContext->stbDaAllocatorData, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Windows_Window_DebugMemoryEnumContext_AllocatorData));
    arrlast(enumContext->stbDaAllocatorData).name = allocatorName;
    korl_memory_allocator_enumerateAllocations(opaqueAllocator, allocatorUserData, _korl_windows_window_loop_allocationEnumCallback, enumContext, NULL/*allocatorVirtualAddressEnd; don't care*/);
    return true;//true => continue iterating over allocators
}
#endif
korl_internal KorlPlatformApi _korl_windows_window_createPlatformApi(void)
{
    KORL_ZERO_STACK(KorlPlatformApi, korlPlatformApi);
    #define _KORL_PLATFORM_API_MACRO_OPERATION(x) (korlPlatformApi).x = (x);
    #include "korl-interface-platform-api.h"
    #undef _KORL_PLATFORM_API_MACRO_OPERATION
    return korlPlatformApi;
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
        korl_vulkan_createSurface(&surfaceUserData
                                 ,clientRect.right  - clientRect.left
                                 ,clientRect.bottom - clientRect.top);
        korl_vulkan_setSurfaceClearColor((f32[]){0.05f, 0.f, 0.05f});// set default clear color to ~purplish~
    }
    /* initialize game memory & game module */
    const KorlPlatformApi korlApi = _korl_windows_window_createPlatformApi();
    korl_time_probeStart(game_initialization);
    /* attempt to copy the game DLL to the application temp directory */
    Korl_StringPool_String stringGameDll = string_newFormatUtf16(L"%ws.dll", KORL_DYNAMIC_APPLICATION_NAME);
    _korl_windows_window_dynamicGameLoad(string_getRawUtf16(&stringGameDll));
    _korl_windows_window_gameInitialize(&korlApi);
    korl_time_probeStop(game_initialization);
    #ifdef _KORL_WINDOWS_WINDOW_LOG_REPORTS
        korl_log(INFO, "KORL initialization time probe report:");
        korl_time_probeLogReport();
        u$ renderFrameCount = 0;
    #endif
    bool quit = false;
    /* For reference, this is essentially what the application developers who 
        consume KORL have rated their application as the _maximum_ amount of 
        time between logical frames to maintain a stable simulation.  It should 
        be _OKAY_ to use smaller values, but definitely _NOT_ okay to use higher 
        values. */
    const Korl_Time_Counts timeCountsTargetGamePerFrame = korl_time_countsFromHz(KORL_APP_TARGET_FRAME_HZ);
    PlatformTimeStamp timeStampLast                     = korl_timeStamp();
    #ifdef _KORL_WINDOWS_WINDOW_LOG_REPORTS
        typedef struct _Korl_Window_LoopStats
        {
            Korl_Time_Counts timeTotal;
            Korl_Time_Counts timeSleep;
        } _Korl_Window_LoopStats;
        KORL_MEMORY_POOL_DECLARE(_Korl_Window_LoopStats, loopStats, 1024);
        loopStats_korlMemoryPoolSize = 0;
        bool deferProbeReport = false;
    #endif
    #ifdef _KORL_WINDOWS_WINDOW_DEBUG_TEST_GFX_TEXT
        Korl_Gfx_Text* debugText = korl_gfx_text_create(context->allocatorHandle, (acu16){.data = L"submodules/korl/c/test-assets/source-sans/SourceSans3-Semibold.otf", .size = 66}, 24);
        {
            const wchar_t* text = L"test line 0\n"
                                  L"line 1\n";
            korl_gfx_text_fifoAdd(debugText, (acu16){.data = text, .size = korl_string_sizeUtf16(text)}, context->allocatorHandle, NULL, NULL);
        }
    #endif
    #ifdef _KORL_WINDOWS_WINDOW_DEBUG_HEAP_UNIT_TESTS
    {
        korl_log(VERBOSE, "============= _KORL_WINDOWS_WINDOW_DEBUG_HEAP_UNIT_TESTS");
        KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
        heapCreateInfo.initialHeapBytes = 2 * korl_memory_pageBytes();
        korl_shared_const wchar_t DEBUG_HEAP_NAME[] = L"DEBUG-linear-unit-test";
        KORL_MEMORY_POOL_DECLARE(u8*                        , allocations   , 32);
        KORL_MEMORY_POOL_DECLARE(Korl_Heap_DefragmentPointer, defragPointers, 32);
        KORL_MEMORY_POOL_SIZE(allocations) = 0;
        _Korl_Heap_Linear* heap = korl_heap_linear_create(&heapCreateInfo);
        korl_log(VERBOSE, "::::: create allocations :::::");
            *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__, NULL);
            *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__, NULL);
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        korl_log(VERBOSE, "::::: create partial internal fragmentation :::::");
            allocations[0] = korl_heap_linear_reallocate(heap, DEBUG_HEAP_NAME, allocations[0], 16, __FILEW__, __LINE__);
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        korl_log(VERBOSE, "::::: defragment :::::");
            KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
            korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers));
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
        korl_log(VERBOSE, "::::: create full internal fragmentation :::::");
            korl_heap_linear_free(heap, allocations[0], __FILEW__, __LINE__);
            KORL_MEMORY_POOL_REMOVE(allocations, 0);
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        korl_log(VERBOSE, "::::: defragment :::::");
            KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
            korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers));
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
        korl_log(VERBOSE, "::::: attempt to create partial trailing fragmentation (this should not cause fragmentation) :::::");
            *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__, NULL);
            allocations[1] = korl_heap_linear_reallocate(heap, DEBUG_HEAP_NAME, allocations[1], 16, __FILEW__, __LINE__);
            *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__, NULL);
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        korl_log(VERBOSE, "::::: attempt to create full trailing fragmentation (this should not cause fragmentation) :::::");
            korl_heap_linear_free(heap, KORL_MEMORY_POOL_POP(allocations), __FILEW__, __LINE__);
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        korl_log(VERBOSE, "::::: create partial mid fragmentation :::::");
            allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1] = korl_heap_linear_reallocate(heap, DEBUG_HEAP_NAME, KORL_MEMORY_POOL_LAST(allocations), 32, __FILEW__, __LINE__);
            *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__, NULL);
            allocations[1] = korl_heap_linear_reallocate(heap, DEBUG_HEAP_NAME, allocations[1], 16, __FILEW__, __LINE__);
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        korl_log(VERBOSE, "::::: defragment :::::");
            KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
            korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers));
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
        korl_log(VERBOSE, "::::: create full mid fragmentation :::::");
            korl_heap_linear_free(heap, allocations[1], __FILEW__, __LINE__);
            KORL_MEMORY_POOL_REMOVE(allocations, 1);
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        korl_log(VERBOSE, "::::: defragment :::::");
            KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
            korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers));
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
        korl_log(VERBOSE, "::::: create pseudo-stb_ds-array :::::");
            *KORL_MEMORY_POOL_ADD(allocations) = korl_heap_linear_allocate(heap, DEBUG_HEAP_NAME, 32, __FILEW__, __LINE__, NULL);
            *allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1] = 3;// dynamic array size = 3
            allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1] += 1;// header is 1 byte; advance the allocation to the array payload
            for(u8 i = 0; i < 3; i++)
                allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1][i] = i;// initialize our array values
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
            for(u8 i = 0; i < 3; i++)
                korl_log(INFO, "testDynamicArray[%hhu]==%hhu", i, allocations[KORL_MEMORY_POOL_SIZE(allocations) - 1][i]);
        korl_log(VERBOSE, "::::: create full mid fragmentation :::::");
            korl_heap_linear_free(heap, allocations[1], __FILEW__, __LINE__);
            KORL_MEMORY_POOL_REMOVE(allocations, 1);
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
        korl_log(VERBOSE, "::::: defragment :::::");
            KORL_MEMORY_POOL_RESIZE(defragPointers, KORL_MEMORY_POOL_SIZE(allocations));
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                defragPointers[i] = (Korl_Heap_DefragmentPointer){&(allocations[i]), 0};
            defragPointers[1].userAddressByteOffset = -1;// dynamic array header is the _true_ allocation address
            korl_heap_linear_defragment(heap, DEBUG_HEAP_NAME, defragPointers, KORL_MEMORY_POOL_SIZE(defragPointers));
            korl_heap_linear_log(heap, DEBUG_HEAP_NAME);
            for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(allocations); i++)
                korl_log(INFO, "allocations[%llu]: &=0x%p", i, allocations[i]);
            for(u8 i = 0; i < 3; i++)
                korl_log(INFO, "testDynamicArray[%hhu]==%hhu", i, allocations[1][i]);// we should still be able to use our dynamic array as normal
        korl_heap_linear_destroy(heap);
        korl_log(VERBOSE, "END _KORL_WINDOWS_WINDOW_DEBUG_HEAP_UNIT_TESTS ===============");
    }
    #endif
    while(!quit)
    {
        #ifdef _KORL_WINDOWS_WINDOW_LOG_REPORTS
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
        #endif
        if(context->deferCpuReport)
        {
            korl_time_probeLogReport(context->deferCpuReportMaxDepth);
            context->deferCpuReport = false;
        }
        if(context->deferMemoryReport)
        {
            korl_memory_reportLog(korl_memory_reportGenerate());
            context->deferMemoryReport = false;
        }
        korl_time_probeReset();
        korl_memory_allocator_emptyStackAllocators();
        korl_time_probeStart(Main_Loop);
        _korl_windows_window_configurationStep();
        KORL_ZERO_STACK(MSG, windowMessage);
        korl_time_probeStart(process_window_messages);
        while(PeekMessage(&windowMessage, NULL/*hWnd; NULL -> get all thread messages*/
                         ,0/*filterMin*/, 0/*filterMax*/, PM_REMOVE))
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
                                                          string_getRawUtf16(&stringGameDll), &dateStampLatestFileWrite)
               && KORL_TIME_DATESTAMP_COMPARE_RESULT_FIRST_TIME_EARLIER == korl_time_dateStampCompare(context->gameDllLastWriteDateStamp, dateStampLatestFileWrite))
            {
                _korl_windows_window_dynamicGameLoad(string_getRawUtf16(&stringGameDll));
                context->gameApi.korl_game_onReload(context->gameContext, korlApi);
            }
        }
        korl_time_probeStart(asset_cache_check_obsolescence); korl_assetCache_checkAssetObsolescence(_korl_windows_window_onAssetHotReloaded); korl_time_probeStop(asset_cache_check_obsolescence);
        //@TODO: defragment all modules which are going to be contained in the memory state
        korl_time_probeStart(memory_state_create);
        {
            korl_free(context->allocatorHandle, context->memoryStateLast);
            context->memoryStateLast = korl_memoryState_create(context->allocatorHandle);
        }korl_time_probeStop(memory_state_create);
        if(context->deferSaveStateSave)
        {
            context->deferSaveStateSave = false;
            // deferProbeReport = true;
            korl_time_probeStart(save_state_save);
            korl_file_saveStateSave(KORL_FILE_PATHTYPE_LOCAL_DATA, L"save-states/savestate");//KORL-ISSUE-000-000-077: crash/window: savestates do not properly "save" crashes that occur inside game module callbacks on window events
            korl_time_probeStop(save_state_save);
        }
        if(context->deferSaveStateLoad)
        {
            context->deferSaveStateLoad = false;
            // deferProbeReport = true;
            korl_time_probeStart(save_state_load);
            korl_log_clearAsyncIo();
            korl_file_finishAllAsyncOperations();
            korl_assetCache_clearAllFileHandles();
            korl_vulkan_clearAllDeviceAllocations();
            korl_file_saveStateLoad(KORL_FILE_PATHTYPE_LOCAL_DATA, L"save-states/savestate");
            // korl_memory_reportLog(korl_memory_reportGenerate());// just for diagnostic...
            if(context->gameDll)
                korl_command_registerModule(context->gameDll, KORL_RAW_CONST_UTF8("korl-game"));
            if(context->gameApi.korl_game_onReload)
                context->gameApi.korl_game_onReload(context->gameContext, korlApi);
            korl_time_probeStop(save_state_load);
        }
#ifdef _KORL_WINDOWS_WINDOW_DEBUG_DISPLAY_MEMORY_STATE
        {
            KORL_ZERO_STACK(_Korl_Windows_Window_DebugMemoryEnumContext, debugEnumContext);
            mcarrsetcap(KORL_STB_DS_MC_CAST(context->allocatorHandle), debugEnumContext.stbDaAllocatorData, 32);
            korl_memory_allocator_enumerateAllocators(_korl_windows_window_loop_allocatorEnumCallback, &debugEnumContext);
            korl_gui_widgetTextFormat(L"Memory Allocator State:");
            for(u$ a = 0; a < arrlenu(debugEnumContext.stbDaAllocatorData); a++)
            {
                korl_gui_setLoopIndex(a);
                korl_gui_widgetTextFormat(L"[%u]: {name: \"%ws\", alloc#: %u}", a, debugEnumContext.stbDaAllocatorData[a].name, debugEnumContext.stbDaAllocatorData[a].allocationCount);
            }
            korl_gui_setLoopIndex(0);
            mcarrfree(KORL_STB_DS_MC_CAST(context->allocatorHandle), debugEnumContext.stbDaAllocatorData);
        }
#endif
        const Korl_Math_V2u32 swapchainSize = korl_vulkan_getSurfaceSize();
        korl_gfx_updateSurfaceSize(swapchainSize);
        korl_time_probeStart(game_update);
        if(    context->gameApi.korl_game_update
           && !context->gameApi.korl_game_update(1.f/KORL_APP_TARGET_FRAME_HZ, swapchainSize.x, swapchainSize.y, GetFocus() != NULL))
            break;
#ifdef _KORL_WINDOWS_WINDOW_DEBUG_TEST_GFX_TEXT
        korl_gfx_text_draw(debugText, korl_math_aabb2f32_fromPoints(KORL_F32_MAX,KORL_F32_MAX, -KORL_F32_MAX,-KORL_F32_MAX));
#endif
        korl_time_probeStop(game_update);
        korl_time_probeStart(render_sound);           korl_sfx_mix();               korl_time_probeStop(render_sound);
        korl_time_probeStart(gui_frame_end);          korl_gui_frameEnd();          korl_time_probeStop(gui_frame_end);
        korl_time_probeStart(flush_glyph_pages);      korl_gfx_flushGlyphPages();   korl_time_probeStop(flush_glyph_pages);
        korl_time_probeStart(flush_resource_updates); korl_resource_flushUpdates(); korl_time_probeStop(flush_resource_updates);
        korl_time_probeStart(vulkan_frame_end);       korl_vulkan_frameEnd();       korl_time_probeStop(vulkan_frame_end);
        /* regulate frame rate to our game module's target frame rate */
        //KORL-ISSUE-000-000-059: window: find a frame timing solution that works if vulkan API blocks for some reason
        const PlatformTimeStamp timeStampRenderLoopBottom = korl_timeStamp();
        const Korl_Time_Counts timeCountsRenderLoop = korl_time_timeStampCountDifference(timeStampRenderLoopBottom, timeStampLast);
        korl_time_probeStart(sleep);
        if(timeCountsRenderLoop < timeCountsTargetGamePerFrame)
            korl_time_sleep(timeCountsTargetGamePerFrame - timeCountsRenderLoop);
        const Korl_Time_Counts timeCountsSleep = korl_time_probeStop(sleep);
        timeStampLast = korl_timeStamp();
#ifdef _KORL_WINDOWS_WINDOW_LOG_REPORTS
        if(renderFrameCount < ~(u$)0)
            renderFrameCount++;
#endif
        const Korl_Time_Counts timeCountsMainLoop = korl_time_probeStop(Main_Loop);
#ifdef _KORL_WINDOWS_WINDOW_LOG_REPORTS
        if(stats)
        {
            stats->timeTotal = timeCountsMainLoop;
            stats->timeSleep = timeCountsSleep;
        }
#endif
    }
#ifdef _KORL_WINDOWS_WINDOW_LOG_REPORTS
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
#endif
    korl_vulkan_destroySurface();
    string_free(&stringGameDll);
    /* safely close out any pending config file operations */
    while(context->configuration.deferSaveConfiguration || context->configuration.asyncIo.handle)
        _korl_windows_window_configurationStep();
}
korl_internal void korl_windows_window_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer)
{
    KORL_ZERO_STACK(WINDOWPLACEMENT, windowPlacement);
    KORL_WINDOWS_CHECK(GetWindowPlacement(_korl_windows_window_context.window.handle, &windowPlacement));
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_windows_window_context.stringPool, sizeof(_korl_windows_window_context.stringPool));
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_windows_window_context.gameContext, sizeof(_korl_windows_window_context.gameContext));
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &windowPlacement, sizeof(windowPlacement));
    korl_stb_ds_arrayAppendU8(memoryContext, pStbDaSaveStateBuffer, &_korl_windows_window_context.configuration.fileDataBuffer, sizeof(_korl_windows_window_context.configuration.fileDataBuffer));
}
korl_internal bool korl_windows_window_saveStateRead(HANDLE hFile)
{
    //KORL-ISSUE-000-000-079: stringPool/file/savestate: either create a (de)serialization API for stringPool, or just put context state into a single allocation?
    if(!ReadFile(hFile, &_korl_windows_window_context.stringPool, sizeof(_korl_windows_window_context.stringPool), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    u64 gameContext;
    if(!ReadFile(hFile, &gameContext, sizeof(gameContext), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    _korl_windows_window_context.gameContext = KORL_C_CAST(void*, gameContext);
    WINDOWPLACEMENT windowPlacement;
    if(!ReadFile(hFile, &windowPlacement, sizeof(windowPlacement), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    if(!ReadFile(hFile, &_korl_windows_window_context.configuration.fileDataBuffer, sizeof(_korl_windows_window_context.configuration.fileDataBuffer), NULL/*bytes read*/, NULL/*no overlapped*/))
    {
        korl_logLastError("ReadFile failed");
        return false;
    }
    KORL_WINDOWS_CHECK(SetWindowPlacement(_korl_windows_window_context.window.handle, &windowPlacement));
    return true;
}
#undef _LOCAL_STRING_POOL_POINTER
