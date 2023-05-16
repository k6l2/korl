#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-memory.h"
#include "korl-file.h"
#include "korl-memory.h"
#include "utility/korl-utility-memory.h"
korl_internal Korl_Memory_ByteBuffer* korl_memoryState_create(Korl_Memory_AllocatorHandle allocatorHandleResult);
korl_internal void                    korl_memoryState_save(Korl_Memory_ByteBuffer* context, Korl_File_PathType pathType, const wchar_t* fileName);
korl_internal Korl_Memory_ByteBuffer* korl_memoryState_load(Korl_Memory_AllocatorHandle allocatorHandleResult, Korl_File_PathType pathType, const wchar_t* fileName);
