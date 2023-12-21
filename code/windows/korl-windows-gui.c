#include "korl-windows-gui.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-gui-internal-common.h"
#include "utility/korl-checkCast.h"
#include "korl-vulkan.h"
#include "korl-stb-ds.h"
#include "korl-interface-platform.h"
korl_internal void korl_gui_windows_processMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, Korl_KeyboardCode* virtualKeyMap, u$ virtualKeyMapSize)
{
    _Korl_Gui_Context*const context = _korl_gui_context;
    /* identify & process mouse events 
        https://docs.microsoft.com/en-us/windows/win32/learnwin32/mouse-clicks */
    RECT clientRect;
    KORL_WINDOWS_CHECK(GetClientRect(hWnd, &clientRect));
    // remember, in Windows client-space the +Y axis points _down_ on the screen...
    const Korl_Math_V2i32 clientRectSize = {clientRect.right  - clientRect.left
                                           ,clientRect.bottom - clientRect.top};
    switch(message)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:{
        const bool isDown = (message % 2 == 1);
        const Korl_Math_V2i32 mousePositionClient = {GET_X_LPARAM(lParam)
                                                    ,GET_Y_LPARAM(lParam)};
        KORL_ZERO_STACK(_Korl_Gui_MouseEvent, guiMouseEvent);
        guiMouseEvent.type                    = _KORL_GUI_MOUSE_EVENT_TYPE_BUTTON;
        guiMouseEvent.subType.button.id       = _KORL_GUI_MOUSE_EVENT_BUTTON_ID_LEFT;
        guiMouseEvent.subType.button.pressed  = isDown;
        guiMouseEvent.subType.button.position = (Korl_Math_V2f32){korl_checkCast_i$_to_f32(mousePositionClient.x)
                                                                 ,korl_checkCast_i$_to_f32(clientRectSize.y - mousePositionClient.y)/*every KORL module should define the +Y axis as UP on the screen*/};
        if(GetKeyState(VK_SHIFT) & 0x8000)
            guiMouseEvent.subType.button.keyboardModifierFlags |= _KORL_GUI_KEYBOARD_MODIFIER_FLAG_SHIFT;
        if(GetKeyState(VK_CONTROL) & 0x8000)
            guiMouseEvent.subType.button.keyboardModifierFlags |= _KORL_GUI_KEYBOARD_MODIFIER_FLAG_CONTROL;
        if(GetKeyState(VK_MENU) & 0x8000)
            guiMouseEvent.subType.button.keyboardModifierFlags |= _KORL_GUI_KEYBOARD_MODIFIER_FLAG_ALTERNATE;
        korl_gui_onMouseEvent(&guiMouseEvent);
        break;}
    case WM_MOUSEMOVE:{
        const Korl_Math_V2i32 mousePositionClient = {GET_X_LPARAM(lParam)
                                                    ,GET_Y_LPARAM(lParam)};
        KORL_ZERO_STACK(_Korl_Gui_MouseEvent, guiMouseEvent);
        guiMouseEvent.type                  = _KORL_GUI_MOUSE_EVENT_TYPE_MOVE;
        guiMouseEvent.subType.move.position = (Korl_Math_V2f32){korl_checkCast_i$_to_f32(mousePositionClient.x)
                                                               ,korl_checkCast_i$_to_f32(clientRectSize.y - mousePositionClient.y)/*every KORL module should define the +Y axis as UP on the screen*/};
        korl_gui_onMouseEvent(&guiMouseEvent);
        break;}
    case WM_MOUSEWHEEL:{
        POINT pointMouse = {.x = GET_X_LPARAM(lParam)
                           ,.y = GET_Y_LPARAM(lParam)};//screen-space, NOT client-space like the other mouse events!!! >:[
        KORL_WINDOWS_CHECK(ScreenToClient(hWnd, &pointMouse));
        const bool keyDownShift = LOWORD(wParam) & MK_SHIFT;
        KORL_ZERO_STACK(_Korl_Gui_MouseEvent, guiMouseEvent);
        guiMouseEvent.type                   = _KORL_GUI_MOUSE_EVENT_TYPE_WHEEL;
        guiMouseEvent.subType.wheel.axis     = keyDownShift 
                                             ? _KORL_GUI_MOUSE_EVENT_WHEEL_AXIS_X
                                             : _KORL_GUI_MOUSE_EVENT_WHEEL_AXIS_Y;
        guiMouseEvent.subType.wheel.value    = GET_WHEEL_DELTA_WPARAM(wParam) / KORL_C_CAST(f32, WHEEL_DELTA);
        guiMouseEvent.subType.wheel.position = (Korl_Math_V2f32){korl_checkCast_i$_to_f32(pointMouse.x)
                                                                ,korl_checkCast_i$_to_f32(clientRectSize.y - pointMouse.y)/*every KORL module should define the +Y axis as UP on the screen*/};
        korl_gui_onMouseEvent(&guiMouseEvent);
        break;}
    case WM_KEYDOWN:
    case WM_KEYUP:{
        if(wParam >= virtualKeyMapSize)
            break;
        KORL_ZERO_STACK(_Korl_Gui_KeyEvent, keyEvent);
        keyEvent.virtualKey = virtualKeyMap[wParam];
        keyEvent.isDown     = message == WM_KEYDOWN;
        keyEvent.isRepeat   = (HIWORD(lParam) & KF_REPEAT) != 0;
        if(GetKeyState(VK_SHIFT) & 0x8000)
            keyEvent.keyboardModifierFlags |= _KORL_GUI_KEYBOARD_MODIFIER_FLAG_SHIFT;
        if(GetKeyState(VK_CONTROL) & 0x8000)
            keyEvent.keyboardModifierFlags |= _KORL_GUI_KEYBOARD_MODIFIER_FLAG_CONTROL;
        if(GetKeyState(VK_MENU) & 0x8000)
            keyEvent.keyboardModifierFlags |= _KORL_GUI_KEYBOARD_MODIFIER_FLAG_ALTERNATE;
        korl_gui_onKeyEvent(&keyEvent);
        break;}
    case WM_CHAR:{
        #ifdef UNICODE
            KORL_ZERO_STACK(_Korl_Gui_CodepointEvent, codepointEvent);
            codepointEvent.utf16Unit = korl_checkCast_u$_to_u16(wParam);
            if(GetKeyState(VK_SHIFT) & 0x8000)
                codepointEvent.keyboardModifierFlags |= _KORL_GUI_KEYBOARD_MODIFIER_FLAG_SHIFT;
            if(GetKeyState(VK_CONTROL) & 0x8000)
                codepointEvent.keyboardModifierFlags |= _KORL_GUI_KEYBOARD_MODIFIER_FLAG_CONTROL;
            if(GetKeyState(VK_MENU) & 0x8000)
                codepointEvent.keyboardModifierFlags |= _KORL_GUI_KEYBOARD_MODIFIER_FLAG_ALTERNATE;
            korl_gui_onCodepointEvent(&codepointEvent);
        #else
            // in order to support non-UNICODE mode, we would probably have to check the application's codepage to see if we're UTF-8 or w/e, and I don't want to have to deal with that garbo ~K6L2
            #error "only UNICODE (UTF-16) mode is supported"
        #endif
        break;}
#if 0
    /*case WM_LBUTTONDBLCLK:*/
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    /*case WM_RBUTTONDBLCLK:*/
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    /*case WM_MBUTTONDBLCLK:*/
    //case WM_XBUTTONUP:
    //case WM_XBUTTONDOWN:
    //case WM_XBUTTONDBLCLK:
    case WM_GESTURE:{
        korl_log(VERBOSE, "WM_GESTURE");
        break;}
    case WM_VSCROLL:{
        korl_log(VERBOSE, "WM_VSCROLL");
        break;}
    case WM_HSCROLL:{
        korl_log(VERBOSE, "WM_HSCROLL");
        break;}
    case WM_MOUSEHWHEEL:{
        const i32 mouseX =  GET_X_LPARAM(lParam);
        const i32 mouseY = -GET_Y_LPARAM(lParam);//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        const Korl_Math_V2f32 mouseV2f32 = { korl_checkCast_i$_to_f32(mouseX)
                                           , korl_checkCast_i$_to_f32(mouseY) };
        const i32 mouseWheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        break;}
#endif
    }
}
