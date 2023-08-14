#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-gfx.h"
#include "korl-memory.h"
#include "utility/korl-utility-math.h"
#include "utility/korl-utility-memory.h"
korl_internal void korl_gfx_initialize(void);
korl_internal void korl_gfx_initializePostRendererLogicalDevice(void);
korl_internal void korl_gfx_update(Korl_Math_V2u32 surfaceSize, f32 deltaSeconds);
korl_internal void korl_gfx_defragment(Korl_Memory_AllocatorHandle stackAllocator);
korl_internal u32  korl_gfx_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer);
korl_internal void korl_gfx_memoryStateRead(const u8* memoryState);
