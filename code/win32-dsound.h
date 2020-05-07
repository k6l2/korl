#pragma once
#include "win32-main.h"
internal void w32InitDSound(HWND hwnd, u32 samplesPerSecond, u32 bufferBytes,
                            u8 numChannels, u32 soundLatencySamples, 
                            u32& o_runningSoundSample);
internal void w32WriteDSoundAudio(u32 soundBufferBytes, 
                                  u32 soundSampleHz,
                                  u8 numSoundChannels,
                                  u32 soundLatencySamples,
                                  u32& io_runningSoundSample,
                                  VOID* gameSoundBufferMemory,
                                  GameMemory& gameMemory, GameCode& game);