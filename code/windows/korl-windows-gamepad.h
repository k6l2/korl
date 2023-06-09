#pragma once
#include "korl-windows-globalDefines.h"
#include "korl-interface-game.h"
korl_internal void korl_windows_gamepad_initialize(void);
korl_internal void korl_windows_gamepad_registerWindow(HWND windowHandle, Korl_Memory_AllocatorHandle allocatorHandleLocal);
/** \return \c true if the message was processed by this call.  If the message 
 * was processed, \c out_result will be populated with the desired message result */
korl_internal bool korl_windows_gamepad_processMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* out_result, fnSig_korl_game_onGamepadEvent* onGamepadEvent);
korl_internal void korl_windows_gamepad_poll(fnSig_korl_game_onGamepadEvent* onGamepadEvent);
