#pragma once
#include "korl-interface-platform.h"
korl_internal void korl_command_initialize(acu8 utf8PlatformModuleName);
korl_internal void korl_command_registerModule(HMODULE moduleHandle, acu8 utf8ModuleName);
korl_internal KORL_FUNCTION_korl_command_invoke(korl_command_invoke);
korl_internal KORL_FUNCTION__korl_command_register(_korl_command_register);
korl_internal void korl_command_defragment(Korl_Memory_AllocatorHandle stackAllocator);
korl_internal u32 korl_command_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer);
korl_internal void korl_command_memoryStateRead(const u8* memoryState);
