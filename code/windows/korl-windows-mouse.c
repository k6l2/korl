#include "korl-windows-mouse.h"
#include <hidusage.h>
korl_internal void korl_windows_mouse_registerWindow(HWND windowHandle)
{
    RAWINPUTDEVICE rawInputDeviceList[1];
    rawInputDeviceList[0].usUsagePage = HID_USAGE_PAGE_GENERIC; 
    rawInputDeviceList[0].usUsage     = HID_USAGE_GENERIC_MOUSE; 
    rawInputDeviceList[0].dwFlags     = RIDEV_INPUTSINK;   
    rawInputDeviceList[0].hwndTarget  = windowHandle;
    RegisterRawInputDevices(rawInputDeviceList, korl_arraySize(rawInputDeviceList), sizeof(rawInputDeviceList[0]));
}
korl_internal bool korl_windows_mouse_processMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* out_result, fnSig_korl_game_onMouseEvent* korl_game_onMouseEvent)
{
    switch(message)
    {
    case WM_INPUT:{
        KORL_ZERO_STACK(RAWINPUT, rawInput);
        UINT dwSize = sizeof(rawInput);
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, KORL_C_CAST(LPVOID, &rawInput), &dwSize, sizeof(RAWINPUTHEADER));
        if (rawInput.header.dwType == RIM_TYPEMOUSE) 
        {
            if(korl_game_onMouseEvent)
                korl_game_onMouseEvent((Korl_MouseEvent){.type = KORL_MOUSE_EVENT_MOVE_RAW
                                                        ,.x    = rawInput.data.mouse.lLastX
                                                        ,.y    = rawInput.data.mouse.lLastY});
            *out_result = 0;
            return true;
        } 
        break;}
    }
    return false;
}
