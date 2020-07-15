#pragma once
#include "global-defines.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
global_variable IDirectInput8* g_pIDInput;
global_variable LPDIRECTINPUTDEVICE8 g_dInputDevices[8];
internal void w32LoadDInput(HINSTANCE hinst);
internal void w32DInputGetGamePadStates(GamePad* gamePadArrayCurrentFrame,
                                        GamePad* gamePadArrayPreviousFrame);