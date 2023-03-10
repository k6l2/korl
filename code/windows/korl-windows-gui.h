#pragma once
#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#include "korl-gui-common.h"
#include "korl-interface-platform-input.h"
korl_internal void korl_gui_windows_processMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, Korl_KeyboardCode* virtualKeyMap, u$ virtualKeyMapSize);
