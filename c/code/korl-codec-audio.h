#pragma once
#include "korl-globalDefines.h"
#include "korl-memory.h"
#include "korl-audio.h"
korl_internal void* korl_codec_audio_decode(const void* rawFileData, u32 rawFileDataBytes, Korl_Memory_AllocatorHandle resultAllocator, u$* o_resultBytes, Korl_Audio_Format* o_resultFormat);
