#pragma once
#include "korl-interface-platform.h"
korl_internal void korl_command_initialize(acu8 utf8PlatformModuleName);
korl_internal void korl_command_registerModule(HMODULE moduleHandle, acu8 utf8ModuleName);
korl_internal KORL_FUNCTION_korl_command_invoke(korl_command_invoke);
korl_internal KORL_FUNCTION__korl_command_register(_korl_command_register);
korl_internal void korl_command_defragment(Korl_Memory_AllocatorHandle stackAllocator);
korl_internal void korl_command_memoryStateWrite(void* memoryContext, u8** pStbDaMemoryState);
korl_internal bool korl_command_memoryStateRead(u8* memoryState);
