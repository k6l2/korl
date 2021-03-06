#pragma once
#include "win32-main.h"
#include "win32-dynApp.h"
internal void 
	w32InitDSound(
		HWND hwnd, u32 samplesPerSecond, u32 bufferBytes, u8 numChannels, 
		DWORD& o_cursorWritePrev);
internal void 
	w32WriteDSoundAudio(
		u32 soundBufferBytes, u32 soundSampleHz, u8 numSoundChannels, 
		VOID* gameSoundBufferMemory, DWORD& io_cursorWritePrev, 
		Korl_Win32_DynamicApplicationModule& game);