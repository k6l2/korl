#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
#include "korl-file.h"
korl_internal void korl_windows_window_initialize(void);
korl_internal void korl_windows_window_create(u32 sizeX, u32 sizeY);
korl_internal void korl_windows_window_loop(void);
korl_internal u32  korl_windows_window_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer);
korl_internal void korl_windows_window_memoryStateRead(const u8* memoryState);
korl_internal void korl_windows_window_saveLastMemoryState(Korl_File_PathType pathType, const wchar_t *fileName);
