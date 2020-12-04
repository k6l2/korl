#include "win32-dsound.h"
#include <dsound.h>
global_variable LPDIRECTSOUNDBUFFER g_dsBufferSecondary;
#define DSOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, \
                                                LPDIRECTSOUND* ppDS, \
                                                LPUNKNOWN pUnkOuter)
typedef DSOUND_CREATE(fnSig_DirectSoundCreate);
internal void w32InitDSound(HWND hwnd, u32 samplesPerSecond, u32 bufferBytes,
                            u8 numChannels, DWORD& o_cursorWritePrev)
{
	const HMODULE LibDSound = LoadLibraryA("dsound.dll");
	if(!LibDSound)
	{
		KLOG(ERROR, "Failed to load dsound.dll! GetLastError=%i", 
		     GetLastError());
		return;
	}
	fnSig_DirectSoundCreate*const DirectSoundCreate = (fnSig_DirectSoundCreate*)	
		GetProcAddress(LibDSound, "DirectSoundCreate");
	if(!DirectSoundCreate)
	{
		KLOG(ERROR, "Failed to get 'DirectSoundCreate'! GetLastError=%i", 
		     GetLastError());
		return;
	}
	LPDIRECTSOUND dSound;
	{
		const HRESULT result = DirectSoundCreate(0, &dSound, NULL);
		if(result != DS_OK)
		{
			KLOG(ERROR, "Failed to create direct sound! result=%li", result);
			return;
		}
	}
	{
		const HRESULT result = 
			dSound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);
		if(result != DS_OK)
		{
			KLOG(ERROR, "Failed to set dsound cooperative level! result=%li", 
			     result);
			return;
		}
	}
	LPDIRECTSOUNDBUFFER dsBufferPrimary;
	// create primary buffer //
	{
		DSBUFFERDESC bufferDescPrimary = {};
		bufferDescPrimary.dwSize  = sizeof(DSBUFFERDESC);
		bufferDescPrimary.dwFlags = DSBCAPS_PRIMARYBUFFER;
		const HRESULT result = 
			dSound->CreateSoundBuffer(&bufferDescPrimary, 
			                          &dsBufferPrimary, NULL);
		if(result != DS_OK)
		{
			KLOG(ERROR, "Failed to create sound buffer! result=%li", result);
			return;
		}
	}
	WAVEFORMATEX waveFormat = {};
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels  = numChannels;
	waveFormat.nSamplesPerSec = samplesPerSecond;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nBlockAlign = 
		(waveFormat.wBitsPerSample/8) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec = 
		waveFormat.nBlockAlign * waveFormat.nSamplesPerSec;
	waveFormat.cbSize = 0;
	{
		const HRESULT result = dsBufferPrimary->SetFormat(&waveFormat);
		if(result != DS_OK)
		{
			KLOG(ERROR, "Failed to set primary buffer format! result=%li", 
			     result);
			return;
		}
	}
	// create secondary buffer //
	{
		///TODO: ensure bufferBytes is in the range of [DSBSIZE_MIN, DSBSIZE_MAX]
		DSBUFFERDESC bufferDescSecondary = {};
		bufferDescSecondary.dwSize        = sizeof(DSBUFFERDESC);
		bufferDescSecondary.lpwfxFormat   = &waveFormat;
		bufferDescSecondary.dwBufferBytes = bufferBytes;
		const HRESULT result = 
			dSound->CreateSoundBuffer(&bufferDescSecondary, 
		                              &g_dsBufferSecondary, NULL);
		if(result != DS_OK)
		{
			KLOG(ERROR, "Failed to create sound buffer! result=%li", result);
			return;
		}
	}
	///TODO: initialize the buffer to ZERO probably?..
	///TODO: initialize the buffer total # bytes written & prev write cursor
	{
		const HRESULT result = g_dsBufferSecondary->Play(0, 0, DSBPLAY_LOOPING);
		if(result != DS_OK)
		{
			KLOG(ERROR, "Failed to play! result=%li", result);
			return;
		}
	}
	// initialize the running sound sample to be ahead of the volatile region //
	//	NOTE: this does not actually guarantee that the running sound sample 
	//	      will be placed ahead of the play cursor, but it should help.
	{
		DWORD cursorPlay;
		DWORD cursorWrite;
		const HRESULT result = 
			g_dsBufferSecondary->GetCurrentPosition(&cursorPlay, &cursorWrite);
		if(result != DS_OK)
		{
			KLOG(ERROR, "Failed to get current position! result=%li", result);
			return;
		}
		o_cursorWritePrev = cursorWrite;
	}
}
internal void w32WriteDSoundAudio(u32 soundBufferBytes, 
                                  u32 soundSampleHz,
                                  u8 numSoundChannels,
                                  VOID* gameSoundBufferMemory,
                                  DWORD& io_cursorWritePrev,
                                  GameCode& game)
{
	const u32 bytesPerSampleBlock = sizeof(SoundSample)*numSoundChannels;
	// Determine the region in the audio buffer which is "volatile" and 
	//	shouldn't be touched since the sound card is probably reading from it.
	DWORD cursorPlay;
	DWORD cursorWrite;
	{
		const HRESULT result = 
			g_dsBufferSecondary->GetCurrentPosition(&cursorPlay, &cursorWrite);
		if(result != DS_OK)
		{
			/* @todo: this code is reached when the user switches audio devices!  
				Instead of just issuing a warning here, maybe we should actually 
				gracefully switch audio devices for them under the hood so that 
				everything just works??? */
			KLOG(WARNING, "Failed to get current position! result=%li", result);
			return;
		}
	}
	// As soon as we obtain the audio cursors, we can calculate how much audio
	//	data DirectSound has consumed, assuming nothing has lagged - which 
	//	should always be a good assumption if I believe I can do my job well.  
	//	Like most things in this function, there are several cases to handle 
	//	because the audio cursors run along a circular buffer.
	const u32 bytesConsumed = cursorWrite > io_cursorWritePrev
		? cursorWrite - io_cursorWritePrev
		: (cursorWrite < io_cursorWritePrev 
			? (soundBufferBytes - io_cursorWritePrev) + cursorWrite
			: 0);
	io_cursorWritePrev = cursorWrite;
	// Based on our current running sound sample index, we need to determine the
	//	region of the buffer that needs to be locked and written to.
	// There are two cases that need to be handled because the sound buffer is
	//	circular.
	// P == cursorPlay
	// W == cursorWrite
	// B == byteToLock
	const DWORD byteToLock = cursorWrite;
	if(byteToLock == cursorPlay)
	{
		KLOG(WARNING, "byteToLock == cursorPlay == %i!!!", byteToLock);
	}
	// x == the region we want to fill in with new sound samples
	const DWORD lockedBytes = (byteToLock < cursorPlay)
		// |------------Bxxxxxxxxxxxxx-P-------W-------------------------------|
		// EDGE CASE: make sure that there is enough padding between B & P such
		//	that we don't ever end up in a situation where B == P!!!
		? (cursorPlay - byteToLock > bytesPerSampleBlock
			? (cursorPlay - byteToLock) - bytesPerSampleBlock
			: 0)
		// It should also be possible for B==W, but that should be okay I think!
		// |xxxxxxxxxxxxxxxxxxxxxxxxxx-P-------WBxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
		// EDGE CASE: make sure that there is enough padding between B & P such
		//	that we don't ever end up in a situation where B == P!!!
		// |P--------------------------------------------------------WBxxxxxxx-|
		: ((soundBufferBytes - byteToLock) + cursorPlay > bytesPerSampleBlock
			? (soundBufferBytes - byteToLock) + 
				(cursorPlay > 0 
					? cursorPlay - bytesPerSampleBlock 
					: -static_cast<i32>(bytesPerSampleBlock))
			: 0);
	// At this point, we know how many bytes need to be filled into the sound
	//	buffer, so now we can request this data from the game code via a 
	//	temporary buffer whose contents will be dumped into the DSound buffer.
	{
		SoundSample*const sampleBlocks = 
			reinterpret_cast<SoundSample*>(gameSoundBufferMemory);
		GameAudioBuffer gameAudioBuffer = {
			.sampleBlocks           = sampleBlocks,
			.lockedSampleBlockCount = lockedBytes / bytesPerSampleBlock,
			.soundSampleHz          = soundSampleHz,
			.numSoundChannels       = numSoundChannels
		};
		game.renderAudio(gameAudioBuffer, bytesConsumed / bytesPerSampleBlock);
	}
	VOID* lockRegion1;
	DWORD lockRegion1Size;
	VOID* lockRegion2;
	DWORD lockRegion2Size;
	{
		const HRESULT result = 
			g_dsBufferSecondary->Lock(byteToLock, lockedBytes,
			                          &lockRegion1, &lockRegion1Size,
			                          &lockRegion2, &lockRegion2Size, 0);
		if(result != DS_OK)
		{
			const char* strErrorCode = nullptr;
			switch(result)
			{
				case DSERR_BUFFERLOST:
				{
					strErrorCode = "DSERR_BUFFERLOST";
				} break;
				case DSERR_INVALIDCALL:
				{
					strErrorCode = "DSERR_INVALIDCALL";
				} break;
				case DSERR_INVALIDPARAM:
				{
					strErrorCode = "DSERR_INVALIDPARAM";
				} break;
				case DSERR_PRIOLEVELNEEDED:
				{
					strErrorCode = "DSERR_PRIOLEVELNEEDED";
				} break;
				default:
				{
					strErrorCode = "UNKNOWN_ERROR";
				} break;
			}
			KLOG(ERROR, "Failed to lock buffer! result=0x%lX(%s)", 
			     result, strErrorCode);
			return;
		}
	}
	const u32 lockedSampleBlockCount = 
		(lockRegion1Size + lockRegion2Size) / bytesPerSampleBlock;
	const u32 lockRegion1SampleBlocks = lockRegion1Size/bytesPerSampleBlock;
	// Fill the locked DSound buffer regions with the sound we should have 
	//	rendered from the game code.
	for(u32 s = 0; s < lockedSampleBlockCount; s++)
	{
		SoundSample* sample = (s < lockRegion1SampleBlocks)
			? reinterpret_cast<SoundSample*>(lockRegion1) + (s*numSoundChannels)
			: reinterpret_cast<SoundSample*>(lockRegion2) + 
				((s - lockRegion1SampleBlocks) * numSoundChannels);
		SoundSample* gameSample = 
			reinterpret_cast<SoundSample*>(gameSoundBufferMemory) +
			(s*numSoundChannels);
		for(u8 c = 0; c < numSoundChannels; c++)
		{
			*sample++ = *gameSample++;
		}
	}
	{
		const HRESULT result =
			g_dsBufferSecondary->Unlock(lockRegion1, lockRegion1Size,
			                            lockRegion2, lockRegion2Size);
		if(result != DS_OK)
		{
			KLOG(ERROR, "Failed to unlock buffer! result=%li", result);
			return;
		}
	}
}