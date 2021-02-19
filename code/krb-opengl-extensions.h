#pragma once
#include "kutil.h"
#include "opengl/glext.h"
/* core extension API; these should be GUARANTEED valid! */
/* core since 1.3 */
global_variable PFNGLLOADTRANSPOSEMATRIXFPROC glLoadTransposeMatrixf;
global_variable PFNGLMULTTRANSPOSEMATRIXFPROC glMultTransposeMatrixf;
/* core since 1.5 */
global_variable PFNGLGENBUFFERSPROC glGenBuffers;
global_variable PFNGLBUFFERDATAPROC glBufferData;
global_variable PFNGLBUFFERSUBDATAPROC glBufferSubData;
global_variable PFNGLBINDBUFFERPROC glBindBuffer;
global_variable PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
global_variable PFNGLDELETEBUFFERSPROC glDeleteBuffers;
/* core since 3.0 */
global_variable PFNGLGETSTRINGIPROC glGetStringi;
/* core since 3.1 */
global_variable PFNGLCOPYBUFFERSUBDATAPROC glCopyBufferSubData;
/* advanced extension API; these might NOT be valid, the caller must check! */
global_variable PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
global_variable PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl;