#pragma once
#include "global-defines.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
global_variable IDirectInput8* g_pIDInput;
global_variable LPDIRECTINPUTDEVICE8 g_dInputDevices[8];
#if 0
#define DINPUT_CREATE(name) HRESULT name(HINSTANCE hinst,    \
                                         DWORD dwVersion,    \
                                         REFIID riidltf,     \
                                         LPVOID * ppvOut,    \
                                         LPUNKNOWN punkOuter )
typedef DINPUT_CREATE(fnSig_dInputCreate);
global_variable fnSig_dInputCreate* dInputCreate_;
#define DirectInput8Create dInputCreate_;
#endif // 0
internal void w32LoadDInput(HINSTANCE hinst);