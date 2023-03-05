#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-memory.h"
#include "korl-file.h"
korl_internal void* korl_memoryState_create(Korl_Memory_AllocatorHandle allocatorHandleResult);
korl_internal void  korl_memoryState_save(void* context, Korl_File_PathType pathType, const wchar_t* fileName);
korl_internal void  korl_memoryState_load(Korl_File_PathType pathType, const wchar_t* fileName);
