#pragma once
#include "kutil.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
global_variable IDirectInput8* g_pIDInput;
global_variable LPDIRECTINPUTDEVICE8 g_dInputDevices[8];
global_variable bool g_dInputDeviceJustAcquiredFlags[8];
global_variable size_t g_dInputDeviceGamePadMapIndices[8];
internal void w32LoadDInput(HINSTANCE hinst);
internal void w32DInputEnumerateDevices();
internal void w32DInputGetGamePadStates(
	GamePad* gamePadArrayCurrentFrame, GamePad* gamePadArrayPreviousFrame);
internal void w32DInputGetMouseStates(GameMouse* gameMouseFrameCurrent);
internal PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(w32DInputGetGamePadActiveButton);
internal PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(w32DInputGetGamePadActiveAxis);
internal PLATFORM_GET_GAME_PAD_PRODUCT_NAME(w32DInputGetGamePadProductName);
internal PLATFORM_GET_GAME_PAD_PRODUCT_GUID(w32DInputGetGamePadProductGuid);