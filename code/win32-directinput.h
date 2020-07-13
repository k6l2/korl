#pragma once
#include "global-defines.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
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