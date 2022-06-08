#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_windows_window_initialize(void);
korl_internal void korl_windows_window_create(u32 sizeX, u32 sizeY);
korl_internal void korl_windows_window_loop(void);
korl_internal void korl_windows_window_saveStateWrite(Korl_Memory_AllocatorHandle allocatorHandle, void** saveStateBuffer, u$* saveStateBufferBytes, u$* saveStateBufferBytesUsed);
/** I don't like how this API requires us to do file I/O in modules outside of 
 * korl-file; maybe improve this in the future to use korl-file API instea of 
 * Win32 API?
 * KORL-ISSUE-000-000-069: savestate/file: contain filesystem API to korl-file? */
korl_internal bool korl_windows_window_saveStateRead(HANDLE hFile);
