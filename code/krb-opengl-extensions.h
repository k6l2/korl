#pragma once
#include "kutil.h"
#include "opengl/glext.h"
/* core extension API; these should be GUARANTEED valid! */
/* core since 1.3 */
global_variable PFNGLLOADTRANSPOSEMATRIXFPROC glLoadTransposeMatrixf;
global_variable PFNGLMULTTRANSPOSEMATRIXFPROC glMultTransposeMatrixf;
/* core since 3.0 */
global_variable PFNGLGETSTRINGIPROC glGetStringi;
/* advanced extension API; these might NOT be valid, the caller must check! */
global_variable PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
global_variable PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl;