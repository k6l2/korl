#pragma once
#include "kutil.h"
#include "opengl/glext.h"
/* core extension API; these should be GUARANTEED valid! */
/* core since 1.3 */
#if 0/* DEPRECATED - we are using uniforms instead of old GL matrix stack */
global_variable PFNGLLOADTRANSPOSEMATRIXFPROC glLoadTransposeMatrixf;
global_variable PFNGLMULTTRANSPOSEMATRIXFPROC glMultTransposeMatrixf;
#endif//0
global_variable PFNGLACTIVETEXTUREPROC glActiveTexture;
/* core since 1.5 */
global_variable PFNGLGENBUFFERSPROC glGenBuffers;
global_variable PFNGLBUFFERDATAPROC glBufferData;
global_variable PFNGLBUFFERSUBDATAPROC glBufferSubData;
global_variable PFNGLBINDBUFFERPROC glBindBuffer;
global_variable PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
global_variable PFNGLDELETEBUFFERSPROC glDeleteBuffers;
/* core since 2.0 */
global_variable PFNGLCREATESHADERPROC glCreateShader;
global_variable PFNGLSHADERSOURCEPROC glShaderSource;
global_variable PFNGLCOMPILESHADERPROC glCompileShader;
global_variable PFNGLGETSHADERIVPROC glGetShaderiv;
global_variable PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
global_variable PFNGLCREATEPROGRAMPROC glCreateProgram;
global_variable PFNGLATTACHSHADERPROC glAttachShader;
global_variable PFNGLLINKPROGRAMPROC glLinkProgram;
global_variable PFNGLGETPROGRAMIVPROC glGetProgramiv;
global_variable PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
global_variable PFNGLUSEPROGRAMPROC glUseProgram;
global_variable PFNGLDELETESHADERPROC glDeleteShader;
global_variable PFNGLDELETEPROGRAMPROC glDeleteProgram;
global_variable PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
global_variable PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
global_variable PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
global_variable PFNGLUNIFORM1IPROC glUniform1i;
global_variable PFNGLUNIFORM4FPROC glUniform4f;
global_variable PFNGLUNIFORM4FVPROC glUniform4fv;
global_variable PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
global_variable PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
/* core since 3.0 */
global_variable PFNGLGETSTRINGIPROC glGetStringi;
global_variable PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
global_variable PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
/* core since 3.1 */
global_variable PFNGLCOPYBUFFERSUBDATAPROC glCopyBufferSubData;
/* advanced extension API; these might NOT be valid, the caller must check! */
global_variable PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
global_variable PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl;