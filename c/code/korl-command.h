#pragma once
#include "korl-interface-platform.h"
korl_internal void korl_command_initialize(acu8 utf8PlatformModuleName);
korl_internal void korl_command_registerModule(HMODULE moduleHandle, acu8 utf8ModuleName);
korl_internal KORL_FUNCTION_korl_command_invoke(korl_command_invoke);
korl_internal KORL_FUNCTION__korl_command_register(_korl_command_register);
korl_internal void korl_command_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer);
/** I don't like how this API requires us to do file I/O in modules outside of 
 * korl-file; maybe improve this in the future to use korl-file API instea of 
 * Win32 API?
 * KORL-ISSUE-000-000-069: savestate/file: contain filesystem API to korl-file? */
korl_internal bool korl_command_saveStateRead(HANDLE hFile);
