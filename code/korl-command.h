#pragma once
#include "korl-interface-platform.h"
#include "korl-memory.h"
korl_internal void korl_command_initialize(acu8 utf8PlatformModuleName);
//@TODO: remove platform-specific HMODULE datatype
korl_internal void korl_command_registerModule(HMODULE moduleHandle, acu8 utf8ModuleName);
korl_internal void korl_command_defragment(Korl_Memory_AllocatorHandle stackAllocator);
korl_internal u32  korl_command_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer);
korl_internal void korl_command_memoryStateRead(const u8* memoryState);
