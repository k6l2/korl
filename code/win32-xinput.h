#pragma once
#include "global-defines.h"
#include <Xinput.h>
#define XINPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, \
                                                 XINPUT_STATE* pState)
#define XINPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, \
                                                 XINPUT_VIBRATION *pVibration)
typedef XINPUT_GET_STATE(fnSig_XInputGetState);
typedef XINPUT_SET_STATE(fnSig_XInputSetState);
global_variable fnSig_XInputGetState* XInputGetState_;
global_variable fnSig_XInputSetState* XInputSetState_;
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_
internal void w32LoadXInput();
internal void w32XInputGetGamePadStates(u8* io_numGamePads,
                                        GamePad* gamePadArrayCurrentFrame,
                                        GamePad* gamePadArrayPreviousFrame);