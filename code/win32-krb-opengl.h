#pragma once
#include "krb-interface.h"
#include "win32-main.h"
#include <GL/GL.h>
#include "opengl/glext.h"
internal void w32KrbOglInitialize(HWND hwnd);
internal void w32KrbOglSetVSyncPreference(bool value);
internal bool w32KrbOglGetVSync();