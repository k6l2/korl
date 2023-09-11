#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-memory.h"
#include "korl-file.h"
#include "korl-memory.h"
#include "utility/korl-utility-memory.h"
korl_internal Korl_Memory_ByteBuffer* korl_memoryState_create(Korl_Memory_AllocatorHandle allocatorHandleResult);
korl_internal void                    korl_memoryState_save(Korl_Memory_ByteBuffer* context, Korl_File_PathType pathType, const wchar_t* fileName);
/** NOTE: this is a _partial_ load of the memoryState; we need a chance for the 
 * caller to re-register dynamic code modules with certain KORL modules which 
 * require as such (example: korl-functionDynamo) before performing memoryState 
 * loading tasks which utilize those KORL modules; example: korl-resource 
 * utilizes korl-functionDynamo, so we _must_ ensure that all code modules are 
 * registered before calling `korl_resource_memoryStateRead`; as a side-note, I 
 * think this is pretty janky, and I would prefer a better solution at some 
 * point in the future */
korl_internal Korl_Memory_ByteBuffer* korl_memoryState_load(Korl_Memory_AllocatorHandle allocatorHandleResult, Korl_File_PathType pathType, const wchar_t* fileName);
korl_internal void                    korl_memoryState_loadPostCodeModuleRegistration(Korl_Memory_ByteBuffer* memoryStateByteBuffer);
