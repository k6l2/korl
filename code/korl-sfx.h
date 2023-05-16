/**
 * This module is just a simple audio mixer within the platform layer which 
 * exports fire-and-forget audio APIs.  By allowing KORL to maintain audio data 
 * in korl-resource, we can do things like pre-cache resampled audio data fit 
 * for direct copying to the audio renderer's write buffers, as well as 
 * hot-reloading of audio file assets.  Technically there should be no 
 * platform-specific code within this module, as file I/O & audio I/O are 
 * executed in other modules (korl-file, korl-audio), but there isn't a great 
 * reason to burdon client code modules (such as game module) with the 
 * management of audio mixing, as far as I can tell so far.
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-sfx.h"
#include "korl-interface-platform-resource.h"
#include "utility/korl-utility-memory.h"
korl_internal void korl_sfx_initialize(void);
korl_internal void korl_sfx_mix(void);
korl_internal u32  korl_sfx_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer);
korl_internal void korl_sfx_memoryStateRead(const u8* memoryState);
