#include "win32-dsound.h"
#include <dsound.h>
global_variable LPDIRECTSOUNDBUFFER g_dsBufferSecondary;
#define DSOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, \
                                                LPDIRECTSOUND* ppDS, \
                                                LPUNKNOWN pUnkOuter)
typedef DSOUND_CREATE(fnSig_DirectSoundCreate);
internal void w32InitDSound(HWND hwnd, u32 samplesPerSecond, u32 bufferBytes,
                            u8 numChannels, u32 soundLatencySamples, 
                            u32& o_runningSoundSample)
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
		o_runningSoundSample = cursorWrite/(sizeof(i16)*numChannels) + 
			soundLatencySamples;
	}
}
internal void w32WriteDSoundAudio(u32 soundBufferBytes, 
                                  u32 soundSampleHz,
                                  u8 numSoundChannels,
                                  u32 soundLatencySamples,
                                  u32& io_runningSoundSample,
                                  VOID* gameSoundBufferMemory,
                                  GameMemory& gameMemory, GameCode& game)
{
	const u32 bytesPerSample = sizeof(SoundSample)*numSoundChannels;
	// Determine the region in the audio buffer which is "volatile" and 
	//	shouldn't be touched since the sound card is probably reading from it.
	DWORD cursorPlay;
	DWORD cursorWrite;
	{
		const HRESULT result = 
			g_dsBufferSecondary->GetCurrentPosition(&cursorPlay, &cursorWrite);
		if(result != DS_OK)
		{
			KLOG(ERROR, "Failed to get current position! result=%li", result);
			return;
		}
	}
	// Based on our current running sound sample index, we need to determine the
	//	region of the buffer that needs to be locked and written to.
	// There are two cases that need to be handled because the sound buffer is
	//	circular.
	// P == cursorPlay
	// W == cursorWrite
	// B == byteToLock
#if 1
	// If the byteToLock is inside the range [P, W), let's skip 
	//	bytesToSamples(W-B) samples in our running sound sample count to avoid
	//	that case entirely.  Two cases must be handled due to circular sound 
	//	buffer shenanigans, as usual:
	// x == the region whose size represents how far we want to fast-forward our
	//	running sound sample variable to leave the volatile region
	u32 skippedSamples = 0;
	if(cursorPlay < cursorWrite)
	{
		const u32 runningSoundByte = 
			io_runningSoundSample*bytesPerSample;
		const DWORD byteToLock = runningSoundByte % soundBufferBytes;
		// |---------------------------P----BxxW-------------------------------|
		if(byteToLock >= cursorPlay && byteToLock < cursorWrite)
		{
			const u32 samplesToSkip = 
				(cursorWrite - byteToLock)/bytesPerSample;
			io_runningSoundSample += samplesToSkip;
			skippedSamples        += samplesToSkip;
		}
	}
	else
	{
		const u32 runningSoundByte = 
			io_runningSoundSample*bytesPerSample;
		const DWORD byteToLock = runningSoundByte % soundBufferBytes;
		// |xxxxxxxxxW----------------------------------------------P----Bxxxxx|
		if(byteToLock >= cursorPlay)
		{
			const u32 samplesToSkip = 
				((cursorWrite - byteToLock) + (soundBufferBytes - byteToLock)) /
				bytesPerSample;
			io_runningSoundSample += samplesToSkip;
			skippedSamples        += samplesToSkip;
		}
		// |---BxxxxxW----------------------------------------------P----------|
		else if(byteToLock < cursorWrite)
		{
			const u32 samplesToSkip = 
				(cursorWrite - byteToLock)/bytesPerSample;
			io_runningSoundSample += samplesToSkip;
			skippedSamples        += samplesToSkip;
		}
	}
	if(skippedSamples > 0)
	{
		KLOG(WARNING, "Audio buffer under-run (failed to keep up w/ dsound?)! "
		     "Skipped %i samples...", skippedSamples);
	}
#endif
	const u32 runningSoundByte = io_runningSoundSample*bytesPerSample;
	const DWORD byteToLock = runningSoundByte % soundBufferBytes;
	if(byteToLock == cursorPlay)
	{
		KLOG(WARNING, "byteToLock == cursorPlay == %i!!!", byteToLock);
	}
	// x == the region we want to fill in with new sound samples
	const DWORD lockedBytes = (byteToLock < cursorPlay)
		// |------------Bxxxxxxxxxxxxx-P-------W-------------------------------|
		// EDGE CASE: make sure that there is enough padding between B & P such
		//	that we don't ever end up in a situation where B == P!!!
		? (cursorPlay - byteToLock > bytesPerSample
			? (cursorPlay - byteToLock) - bytesPerSample
			: 0)
		// It should also be possible for B==W, but that should be okay I think!
		// |xxxxxxxxxxxxxxxxxxxxxxxxxx-P-------WBxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
		// EDGE CASE: make sure that there is enough padding between B & P such
		//	that we don't ever end up in a situation where B == P!!!
		// |P--------------------------------------------------------WBxxxxxxx-|
		: ((soundBufferBytes - byteToLock) + cursorPlay > bytesPerSample
			? (soundBufferBytes - byteToLock) + 
				(cursorPlay > 0 
					? cursorPlay - bytesPerSample 
					: -static_cast<i32>(bytesPerSample))
			: 0);
#if 0
	// advance our total bytes sent to the audio device //
	//	Need to handle two cases because the write cursor diff from the previous
	//	frame might overlap with the circular buffer start/end.
	//	P == io_cursorWritePrev
	//	W == cursorWrite
	//	x == the region we want to add to the total # of buffer bytes sent
	if(cursorWrite > io_cursorWritePrev)
	{
		// |--------------PxxxxxxW---------------------------------------------|
		io_bufferTotalBytesSent += cursorWrite - io_cursorWritePrev;
	}
	else
	{
		// |xxW-----------------------------------------------------------Pxxxx|
		io_bufferTotalBytesSent += 
			(soundBufferBytes - io_cursorWritePrev) + cursorWrite;
	}
	io_cursorWritePrev = cursorWrite;
#endif
	// The maximum amount of the buffer we should lock and subsequently write 
	//	data to is NOT the entire section between W and P.  There is an area 
	//	shown below as 'l' which is the data within the maximum latency samples
	//	range which should be a strict subset of lockedBytes in most cases.  We
	//	want to lock & write the MINIMUM of these two values.
	// |xxxxxxxxxxxxx-P------WlllllBlllxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
	//	Because of the code that runs above this, we know that B is never going 
	//	to be inside the range of [P, W)!
	// bytes_ahead_of_write = 
	//	B < W ? (bufferSize - W) + B
	//	      : B - W
	// maximum_bytes_ahead_of_write = soundLatencySamples*bytesPerSample
	// maximum_lock_byte_count = min(lockable_bytes, 
	//                               maximum_bytes_ahead_of_write - 
	//                                       bytes_ahead_of_write)
	const u32 bytesAheadOfWrite = byteToLock < cursorWrite
		? (soundBufferBytes - cursorWrite) + byteToLock
		: byteToLock - cursorWrite;
	const u32 maxBytesAheadOfWrite = soundLatencySamples*bytesPerSample;
	const DWORD maxLockedBytes = maxBytesAheadOfWrite > bytesAheadOfWrite
		? min(lockedBytes, maxBytesAheadOfWrite - bytesAheadOfWrite)
		: 0;
	if(maxLockedBytes <= 0)
	{
		KLOG(WARNING, "maxBytesAheadOfWrite==%i <= bytesAheadOfWrite==%i",
		     maxBytesAheadOfWrite, bytesAheadOfWrite);
		return;
	}
	// At this point, we know how many bytes need to be filled into the sound
	//	buffer, so now we can request this data from the game code via a 
	//	temporary buffer whose contents will be dumped into the DSound buffer.
	{
		GameAudioBuffer gameAudioBuffer = {
			.memory     = reinterpret_cast<SoundSample*>(gameSoundBufferMemory),
			.lockedSampleCount = maxLockedBytes / bytesPerSample,
			.soundSampleHz     = soundSampleHz,
			.numSoundChannels  = numSoundChannels
		};
		game.renderAudio(gameMemory, gameAudioBuffer);
	}
	// Because audio buffer is circular, we have to handle two cases.  In one 
	//	case, the volatile region is contiguous inside the buffer.  In the other
	//	case, the volatile region is occupying the beginning & and of the 
	//	circular buffer.
	// case 0: cursorPlay  < cursorWrite
	//	we can lock (soundBufferBytes - cursorWrite) + cursorPlay
	// case 1: cursorWrite < cursorPlay
	//	volatile play region wraps around the circular buffer;
	//	we can lock (cursorPlay - cursorWrite)
#if 0
	const DWORD lockedBytes = (cursorPlay < cursorWrite)
		? ((soundBufferBytes - cursorWrite) + cursorPlay)
		: (cursorPlay - cursorWrite);
#endif
	VOID* lockRegion1;
	DWORD lockRegion1Size;
	VOID* lockRegion2;
	DWORD lockRegion2Size;
	{
		const HRESULT result = 
			g_dsBufferSecondary->Lock(byteToLock, maxLockedBytes,
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
	const u32 lockedSampleCount = 
		(lockRegion1Size + lockRegion2Size) / bytesPerSample;
	const u32 lockRegion1Samples = lockRegion1Size/bytesPerSample;
	const u32 lockRegion2Samples = lockRegion2Size/bytesPerSample;
	// Fill the locked DSound buffer regions with the sound we should have 
	//	rendered from the game code.
	for(u32 s = 0; s < lockedSampleCount; s++)
	{
		SoundSample* sample = (s < lockRegion1Samples)
			? reinterpret_cast<SoundSample*>(lockRegion1) + (s*numSoundChannels)
			: reinterpret_cast<SoundSample*>(lockRegion2) + 
				((s - lockRegion1Samples) * numSoundChannels);
		SoundSample* gameSample = 
			reinterpret_cast<SoundSample*>(gameSoundBufferMemory) +
			(s*numSoundChannels);
		for(u8 c = 0; c < numSoundChannels; c++)
		{
			*sample++ = *gameSample++;
		}
	}
	io_runningSoundSample += lockedSampleCount;
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