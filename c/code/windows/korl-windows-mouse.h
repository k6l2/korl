/** This module is used to obtain & process mouse movement inputs at the highest 
 * possible resolution using RawInput API.  For details, see:
 * https://docs.microsoft.com/en-us/windows/win32/dxtecharts/taking-advantage-of-high-dpi-mouse-movement?redirectedfrom=MSDN */
#pragma once
#include "korl-windows-globalDefines.h"
#include "korl-interface-game.h"
korl_internal void korl_windows_mouse_registerWindow(HWND windowHandle);
/** \return \c true if the message was processed by this call.  If the message 
 * was processed, \c out_result will be populated with the desired message result */
korl_internal bool korl_windows_mouse_processMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* out_result, fnSig_korl_game_onMouseEvent* korl_game_onMouseEvent);
