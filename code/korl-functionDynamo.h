/** This code module maintains a database of registered code modules & function 
 * pointers which are registered to a given code module.  When a code module is 
 * then re-registered (such as after a code hot-reload), all registered function 
 * pointers will be automatically re-obtained.  The APIs for registering a 
 * function & getting its function pointer are declared in \c korl-interface-platform.h . */
#pragma once
#include "korl-interface-platform.h"
#include "korl-memory.h"
#include "utility/korl-utility-memory.h"
typedef void* Korl_FunctionDynamo_CodeModuleHandle;// platform-specific opaque handle; example: on Windows it is an HMODULE
korl_internal void korl_functionDynamo_initialize(acu8 utf8PlatformModuleName);
korl_internal void korl_functionDynamo_registerModule(Korl_FunctionDynamo_CodeModuleHandle moduleHandle, acu8 utf8ModuleName);
korl_internal void korl_functionDynamo_defragment(Korl_Memory_AllocatorHandle stackAllocator);
korl_internal u32  korl_functionDynamo_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer);
korl_internal void korl_functionDynamo_memoryStateRead(const u8* memoryState);
