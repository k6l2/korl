#include "game.h"
#include "global-defines.h"
// crt io
#include <stdio.h>
#include "win32-main.h"
#include <Xinput.h>
#include <dsound.h>
struct GameCode
{
	fnSig_GameRenderAudio* renderAudio;
	fnSig_GameUpdateAndDraw* updateAndDraw;
	HMODULE hLib;
	FILETIME dllLastWriteTime;
	bool isValid;
};
global_variable bool g_running;
global_variable W32OffscreenBuffer g_backBuffer;
global_variable LPDIRECTSOUNDBUFFER g_dsBufferSecondary;
global_variable LARGE_INTEGER g_perfCounterHz;
PLATFORM_PRINT_DEBUG_STRING(platformPrintDebugString)
{
	OutputDebugStringA(string);
}
#if INTERNAL_BUILD
PLATFORM_READ_ENTIRE_FILE(platformReadEntireFile)
{
	PlatformDebugReadFileResult result = {};
	const HANDLE fileHandle = 
		CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, 
		            OPEN_EXISTING, 
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if(fileHandle == INVALID_HANDLE_VALUE)
	{
		///TODO: handle GetLastError
		return result;
	}
	u32 fileSize32;
	{
		LARGE_INTEGER fileSize;
		if(!GetFileSizeEx(fileHandle, &fileSize))
		{
			///TODO: handle GetLastError
			if(!CloseHandle(fileHandle))
			{
				///TODO: handle GetLastError
			}
			return result;
		}
		fileSize32 = kmath::safeTruncateU64(fileSize.QuadPart);
		if(fileSize32 != fileSize.QuadPart)
		{
			///TODO: log this error case
			if(!CloseHandle(fileHandle))
			{
				///TODO: handle GetLastError
			}
			return result;
		}
	}
	result.data = VirtualAlloc(nullptr, fileSize32, 
	                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if(!result.data)
	{
		///TODO: handle GetLastError
		if(!CloseHandle(fileHandle))
		{
			///TODO: handle GetLastError
		}
		return result;
	}
	DWORD numBytesRead;
	if(!ReadFile(fileHandle, result.data, fileSize32, &numBytesRead, nullptr))
	{
		///TODO: handle GetLastError
		if(!VirtualFree(result.data, 0, MEM_RELEASE))
		{
			///TODO: handle GetLastError
		}
		if(!CloseHandle(fileHandle))
		{
			///TODO: handle GetLastError
		}
		result.data = nullptr;
		return result;
	}
	if(numBytesRead != fileSize32)
	{
		///TODO: handle this case
		if(!VirtualFree(result.data, 0, MEM_RELEASE))
		{
			///TODO: handle GetLastError
		}
		if(!CloseHandle(fileHandle))
		{
			///TODO: handle GetLastError
		}
		result.data = nullptr;
		return result;
	}
	if(!CloseHandle(fileHandle))
	{
		///TODO: handle GetLastError
		if(!VirtualFree(result.data, 0, MEM_RELEASE))
		{
			///TODO: handle GetLastError
		}
		if(!CloseHandle(fileHandle))
		{
			///TODO: handle GetLastError
		}
		result.data = nullptr;
		return result;
	}
	result.dataBytes = fileSize32;
	return result;
}
PLATFORM_FREE_FILE_MEMORY(platformFreeFileMemory)
{
	if(!VirtualFree(fileMemory, 0, MEM_RELEASE))
	{
		///TODO: handle GetLastError
		OutputDebugStringA("ERROR: Failed to free file memory!!!!!\n");
	}
}
PLATFORM_WRITE_ENTIRE_FILE(platformWriteEntireFile)
{
	const HANDLE fileHandle = 
		CreateFileA(fileName, GENERIC_WRITE, 0, nullptr, 
		            CREATE_ALWAYS, 
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
	if(fileHandle == INVALID_HANDLE_VALUE)
	{
		///TODO: handle GetLastError
		return false;
	}
	DWORD bytesWritten;
	if(!WriteFile(fileHandle, data, dataBytes, &bytesWritten, nullptr))
	{
		///TODO: handle GetLastError
		return false;
	}
	if(bytesWritten != dataBytes)
	{
		///TODO: handle this error case
		return false;
	}
	if(!CloseHandle(fileHandle))
	{
		///TODO: handle GetLastError
		return false;
	}
	return false;
}
#endif
/* XINPUT SUPPORT *************************************************************/
#define XINPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, \
                                                 XINPUT_STATE* pState)
#define XINPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, \
                                                 XINPUT_VIBRATION *pVibration)
typedef XINPUT_GET_STATE(fnSig_XInputGetState);
typedef XINPUT_SET_STATE(fnSig_XInputSetState);
XINPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_NOT_SUPPORTED;
}
XINPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_NOT_SUPPORTED;
}
global_variable fnSig_XInputGetState* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_
global_variable fnSig_XInputSetState* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_
internal void w32LoadXInput()
{
	HMODULE LibXInput = LoadLibraryA("xinput1_4.dll");
	if(!LibXInput)
	{
		///TODO: handle GetLastError (or not, and let the next load's error be
		///      handled instead?)
		LibXInput = LoadLibraryA("xinput9_1_0.dll");
	}
	if(!LibXInput)
	{
		///TODO: handle GetLastError (or not, and let the next load's error be
		///      handled instead?)
		LibXInput = LoadLibraryA("xinput1_3.dll");
	}
	if(LibXInput)
	{
		XInputGetState = 
			(fnSig_XInputGetState*)GetProcAddress(LibXInput, "XInputGetState");
		if(!XInputGetState)
		{
			///TODO: handle GetLastError
		}
		XInputSetState = 
			(fnSig_XInputSetState*)GetProcAddress(LibXInput, "XInputSetState");
		if(!XInputSetState)
		{
			///TODO: handle GetLastError
		}
	}
	else
	{
		///TODO: handle GetLastError
	}
}
internal void w32ProcessXInputStick(SHORT xiThumbX, SHORT xiThumbY,
                                    u16 circularDeadzoneMagnitude,
                                    f32* o_normalizedStickX, 
                                    f32* o_normalizedStickY)
{
	if(xiThumbX < -0x7FFF) xiThumbX = -0x7FFF;
	if(xiThumbY < -0x7FFF) xiThumbY = -0x7FFF;
	local_persist const f32 MAX_THUMB_MAG = static_cast<f32>(0x7FFF);
	f32 thumbMag = 
		sqrtf(static_cast<f32>(xiThumbX)*xiThumbX + xiThumbY*xiThumbY);
	const f32 thumbNormX = 
		!kmath::isNearlyZero(thumbMag) ? xiThumbX / thumbMag : 0.f;
	const f32 thumbNormY = 
		!kmath::isNearlyZero(thumbMag) ? xiThumbY / thumbMag : 0.f;
	if(thumbMag <= circularDeadzoneMagnitude)
	{
		*o_normalizedStickX = 0.f;
		*o_normalizedStickY = 0.f;
		return;
	}
	if(thumbMag > MAX_THUMB_MAG) 
	{
		thumbMag = MAX_THUMB_MAG;
	}
	const f32 thumbMagNorm = (thumbMag - circularDeadzoneMagnitude) / 
	                    (MAX_THUMB_MAG - circularDeadzoneMagnitude);
	*o_normalizedStickX = thumbNormX * thumbMagNorm;
	*o_normalizedStickY = thumbNormY * thumbMagNorm;
}
internal void w32ProcessXInputButton(bool xInputButtonPressed, 
                                     ButtonState buttonStatePrevious,
                                     ButtonState* o_buttonStateCurrent)
{
	if(xInputButtonPressed)
	{
		if(buttonStatePrevious == ButtonState::NOT_PRESSED)
		{
			*o_buttonStateCurrent = ButtonState::PRESSED;
		}
		else
		{
			*o_buttonStateCurrent = ButtonState::HELD;
		}
	}
	else
	{
		*o_buttonStateCurrent = ButtonState::NOT_PRESSED;
	}
}
internal void w32ProcessXInputTrigger(BYTE padTrigger,
                                      BYTE padTriggerDeadzone,
                                      f32* o_gamePadNormalizedTrigger)
{
	if(padTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
	{
		*o_gamePadNormalizedTrigger = 0.f;
	}
	else
	{
		*o_gamePadNormalizedTrigger = 
			(padTrigger - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) / 
			(255.f      - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
	}
}
internal void w32XInputGetGamePadStates(u8* io_numGamePads,
                                        GamePad* gamePadArrayCurrentFrame,
                                        GamePad* gamePadArrayPreviousFrame)
{
	*io_numGamePads = 0;
	for(u32 ci = 0; ci < XUSER_MAX_COUNT; ci++)
	{
		XINPUT_STATE controllerState;
		if( XInputGetState(ci, &controllerState) != ERROR_SUCCESS )
		{
			///TODO: handle controller not available
			continue;
		}
		///TODO: investigate controllerState.dwPacketNumber for 
		///      input polling performance
		const XINPUT_GAMEPAD& pad = controllerState.Gamepad;
		w32ProcessXInputStick(
			pad.sThumbLX, pad.sThumbLY,
			XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 2,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedStickLeft.x,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedStickLeft.y);
		w32ProcessXInputStick(
			pad.sThumbRX, pad.sThumbRY,
			XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 2,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedStickRight.x,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedStickRight.y);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_UP,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.dPadUp,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.dPadUp);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.dPadDown,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.dPadDown);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.dPadLeft,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.dPadLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.dPadRight,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.dPadRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_START,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.start,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.start);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_BACK,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.back,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.back);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.stickClickLeft,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.stickClickLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.stickClickRight,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.stickClickRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.shoulderLeft,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.shoulderLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.shoulderRight,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.shoulderRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_A,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.faceDown,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.faceDown);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_B,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.faceRight,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.faceRight);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_X,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.faceLeft,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.faceLeft);
		w32ProcessXInputButton(
			pad.wButtons & XINPUT_GAMEPAD_Y,
			gamePadArrayPreviousFrame[*io_numGamePads].buttons.faceUp,
			&gamePadArrayCurrentFrame[*io_numGamePads].buttons.faceUp);
		w32ProcessXInputTrigger(
			pad.bLeftTrigger,
			XINPUT_GAMEPAD_TRIGGER_THRESHOLD,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedTriggerLeft);
		w32ProcessXInputTrigger(
			pad.bRightTrigger,
			XINPUT_GAMEPAD_TRIGGER_THRESHOLD,
			&gamePadArrayCurrentFrame[*io_numGamePads].normalizedTriggerRight);
		gamePadArrayCurrentFrame[*io_numGamePads].
			normalizedTriggerRight = pad.bRightTrigger/255.f;
		(*io_numGamePads)++;
	}
}
/********************************************************* END XINPUT SUPPORT */
/* DIRECT SOUND SUPPORT *******************************************************/
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
		///TODO: handle GetLastError
		return;
	}
	fnSig_DirectSoundCreate*const DirectSoundCreate = (fnSig_DirectSoundCreate*)	
		GetProcAddress(LibDSound, "DirectSoundCreate");
	if(!DirectSoundCreate)
	{
		///TODO: handle GetLastError
		return;
	}
	LPDIRECTSOUND dSound;
	if(DirectSoundCreate(0, &dSound, NULL) != DS_OK)
	{
		///TODO: handle returned error code
		return;
	}
	if(dSound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY) != DS_OK)
	{
		///TODO: handle returned error code
		return;
	}
	LPDIRECTSOUNDBUFFER dsBufferPrimary;
	// create primary buffer //
	{
		DSBUFFERDESC bufferDescPrimary = {};
		bufferDescPrimary.dwSize  = sizeof(DSBUFFERDESC);
		bufferDescPrimary.dwFlags = DSBCAPS_PRIMARYBUFFER;
		if(dSound->CreateSoundBuffer(&bufferDescPrimary, 
		                             &dsBufferPrimary, NULL) != DS_OK)
		{
			///TODO: handle returned error code
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
	if(dsBufferPrimary->SetFormat(&waveFormat) != DS_OK)
	{
		///TODO: handle returned error code
		return;
	}
	// create secondary buffer //
	{
		///TODO: ensure bufferBytes is in the range of [DSBSIZE_MIN, DSBSIZE_MAX]
		DSBUFFERDESC bufferDescSecondary = {};
		bufferDescSecondary.dwSize        = sizeof(DSBUFFERDESC);
		bufferDescSecondary.lpwfxFormat   = &waveFormat;
		bufferDescSecondary.dwBufferBytes = bufferBytes;
		if(dSound->CreateSoundBuffer(&bufferDescSecondary, 
		                             &g_dsBufferSecondary, NULL) != DS_OK)
		{
			///TODO: handle returned error code
			return;
		}
	}
	///TODO: initialize the buffer to ZERO probably?..
	///TODO: initialize the buffer total # bytes written & prev write cursor
	if(g_dsBufferSecondary->Play(0, 0, DSBPLAY_LOOPING) != DS_OK)
	{
		///TODO: handle returned error code
		return;
	}
	// initialize the running sound sample to be ahead of the volatile region //
	//	NOTE: this does not actually guarantee that the running sound sample 
	//	      will be placed ahead of the play cursor, but it should help.
	{
		DWORD cursorPlay;
		DWORD cursorWrite;
		if(g_dsBufferSecondary->GetCurrentPosition(&cursorPlay, 
		                                           &cursorWrite) != DS_OK)
		{
			///TODO: handle error code
			return;
		}
		o_runningSoundSample = cursorWrite/(sizeof(i16)*numChannels) + 
			soundLatencySamples;
	}
}
/*************************************************** END DIRECT SOUND SUPPORT */
internal FILETIME w32GetLastWriteTime(const char* fileName)
{
	FILETIME result = {};
	WIN32_FIND_DATAA findFileData;
	const HANDLE hFindFile = FindFirstFileA(fileName, &findFileData);
	if(hFindFile == INVALID_HANDLE_VALUE)
	{
		///TODO: handle GetLastError
		return result;
	}
	result = findFileData.ftLastWriteTime;
	if(!FindClose(hFindFile))
	{
		///TODO: handle GetLastError
	}
	return result;
}
internal GameCode w32LoadGameCode(const char* fileNameDll, 
                                  const char* fileNameDllTemp)
{
	GameCode result = {};
	result.dllLastWriteTime = w32GetLastWriteTime(fileNameDll);
	if(!CopyFileA(fileNameDll, fileNameDllTemp, false))
	{
		///TODO: handle GetLastError
	}
	result.hLib = LoadLibraryA(fileNameDllTemp);
	if(result.hLib)
	{
		result.renderAudio = reinterpret_cast<fnSig_GameRenderAudio*>(
			GetProcAddress(result.hLib, "gameRenderAudio"));
		result.updateAndDraw = reinterpret_cast<fnSig_GameUpdateAndDraw*>(
			GetProcAddress(result.hLib, "gameUpdateAndDraw"));
		result.isValid = (result.renderAudio && result.updateAndDraw);
	}
	else
	{
		///TODO: handle GetLastError
	}
	if(!result.isValid)
	{
		result.renderAudio   = gameRenderAudioStub;
		result.updateAndDraw = gameUpdateAndDrawStub;
	}
	return result;
}
internal void w32UnloadGameCode(GameCode& gameCode)
{
	if(gameCode.hLib)
	{
		if(!FreeLibrary(gameCode.hLib))
		{
			///TODO: handle GetLastError
		}
		gameCode.hLib = NULL;
	}
	gameCode.isValid = false;
	gameCode.renderAudio   = gameRenderAudioStub;
	gameCode.updateAndDraw = gameUpdateAndDrawStub;
}
internal W32Dimension2d w32GetWindowDimensions(HWND hwnd)
{
	RECT clientRect;
	if(GetClientRect(hwnd, &clientRect))
	{
		const u32 w = clientRect.right  - clientRect.left;
		const u32 h = clientRect.bottom - clientRect.top;
		return W32Dimension2d{.width=w, .height=h};
	}
	else
	{
		///TODO: log GetLastError
		return W32Dimension2d{.width=0, .height=0};
	}
}
internal void w32WriteDSoundAudio(u32 soundBufferBytes, 
                                  u32 soundSampleHz,
                                  u8 numSoundChannels,
                                  u32 soundLatencySamples,
                                  u32& io_runningSoundSample,
                                  VOID* gameSoundBufferMemory,
                                  GamePad* gamePadArray, u8 numGamePads,
                                  GameMemory& gameMemory, GameCode& game)
{
	const u32 bytesPerSample = sizeof(SoundSample)*numSoundChannels;
	// Determine the region in the audio buffer which is "volatile" and 
	//	shouldn't be touched since the sound card is probably reading from it.
	DWORD cursorPlay;
	DWORD cursorWrite;
	if(g_dsBufferSecondary->GetCurrentPosition(&cursorPlay, 
	                                           &cursorWrite) != DS_OK)
	{
		///TODO: handle error code
		return;
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
		}
		// |---BxxxxxW----------------------------------------------P----------|
		else if(byteToLock < cursorWrite)
		{
			const u32 samplesToSkip = 
				(cursorWrite - byteToLock)/bytesPerSample;
			io_runningSoundSample += samplesToSkip;
		}
	}
#endif
	const u32 runningSoundByte = io_runningSoundSample*bytesPerSample;
	const DWORD byteToLock = runningSoundByte % soundBufferBytes;
	if(byteToLock == cursorPlay)
	{
		OutputDebugStringA("byteToLock == cursorPlay!!!\n");
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
	// At this point, we know how many bytes need to be filled into the sound
	//	buffer, so now we can request this data from the game code via a 
	//	temporary buffer whose contents will be dumped into the DSound buffer.
	{
		GameAudioBuffer gameAudioBuffer = {
			.memory            = reinterpret_cast<SoundSample*>(
			                                             gameSoundBufferMemory),
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
	if(g_dsBufferSecondary->Lock(byteToLock, maxLockedBytes,
	                             &lockRegion1, &lockRegion1Size,
	                             &lockRegion2, &lockRegion2Size,
	                             0) != DS_OK)
	{
		///TODO: handle error code
		return;
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
	if(g_dsBufferSecondary->Unlock(lockRegion1, lockRegion1Size,
	                               lockRegion2, lockRegion2Size) != DS_OK)
	{
		///TODO: handle error code
		return;
	}
}
internal void w32ResizeDibSection(W32OffscreenBuffer& buffer, 
                                  int width, int height)
{
	// negative biHeight makes this bitmap TOP->DOWN instead of BOTTOM->UP
	buffer.bitmapInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	buffer.bitmapInfo.bmiHeader.biWidth       = width;
	buffer.bitmapInfo.bmiHeader.biHeight      = -height;
	buffer.bitmapInfo.bmiHeader.biPlanes      = 1;
	buffer.bitmapInfo.bmiHeader.biBitCount    = 32;
	buffer.bitmapInfo.bmiHeader.biCompression = BI_RGB;
	buffer.width         = width;
	buffer.height        = height;
	buffer.bytesPerPixel = 4;
	buffer.pitch         = buffer.bytesPerPixel * width;
	if(buffer.bitmapMemory)
	{
		VirtualFree(buffer.bitmapMemory, 0, MEM_RELEASE);
	}
	// allocate buffer's bitmap memory //
	{
		const int bitmapMemorySize = buffer.bytesPerPixel*width*height;
		buffer.bitmapMemory = VirtualAlloc(NULL, bitmapMemorySize, 
		                                   MEM_RESERVE | MEM_COMMIT, 
		                                   PAGE_READWRITE);
	}
	if(!buffer.bitmapMemory)
	{
		///TODO: handle GetLastError
	}
}
internal void w32UpdateWindow(const W32OffscreenBuffer& buffer, 
                              HDC hdc, int windowWidth, int windowHeight)
{
	const int signedNumCopiedScanlines =
		StretchDIBits(hdc,
			0, 0, windowWidth, windowHeight,
			0, 0, buffer.width, buffer.height,
			buffer.bitmapMemory,
			&buffer.bitmapInfo,
			DIB_RGB_COLORS,
			SRCCOPY);
	if( signedNumCopiedScanlines == GDI_ERROR ||
		signedNumCopiedScanlines == 0)
	{
		///TODO: handle error cases
	}
}
internal void w32ProcessKeyEvent(MSG& msg, GameKeyboard& gameKeyboard)
{
	// Virtual-Key Codes: 
	//	https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	const WPARAM vKeyCode = msg.wParam;
#if SLOW_BUILD
	char outputBuffer[256] = {};
	snprintf(outputBuffer, sizeof(outputBuffer), "vKeyCode=%i,0x%x\n", 
	         static_cast<int>(vKeyCode), static_cast<int>(vKeyCode));
	OutputDebugStringA(outputBuffer);
#endif
	const bool keyDown     = (msg.lParam & (1<<31)) == 0;
	const bool keyDownPrev = (msg.lParam & (1<<30)) != 0;
#if 0
	const bool altKeyDown  = (msg.lParam & (1<<29)) != 0;
#endif
	ButtonState* buttonState = nullptr;
	switch(vKeyCode)
	{
		case VK_OEM_COMMA:
			buttonState = &gameKeyboard.comma; break;
		case VK_OEM_PERIOD:
			buttonState = &gameKeyboard.period; break;
		case VK_OEM_2: 
			buttonState = &gameKeyboard.slashForward; break;
		case VK_OEM_5: 
			buttonState = &gameKeyboard.slashBack; break;
		case VK_OEM_4: 
			buttonState = &gameKeyboard.curlyBraceLeft; break;
		case VK_OEM_6: 
			buttonState = &gameKeyboard.curlyBraceRight; break;
		case VK_OEM_1: 
			buttonState = &gameKeyboard.semicolon; break;
		case VK_OEM_7: 
			buttonState = &gameKeyboard.quote; break;
		case VK_OEM_3: 
			buttonState = &gameKeyboard.grave; break;
		case VK_OEM_MINUS: 
			buttonState = &gameKeyboard.tenkeyless_minus; break;
		case VK_OEM_PLUS: 
			buttonState = &gameKeyboard.equals; break;
		case VK_BACK: 
			buttonState = &gameKeyboard.backspace; break;
		case VK_ESCAPE: 
			buttonState = &gameKeyboard.escape; break;
		case VK_RETURN: 
			buttonState = &gameKeyboard.enter; break;
		case VK_SPACE: 
			buttonState = &gameKeyboard.space; break;
		case VK_TAB: 
			buttonState = &gameKeyboard.tab; break;
		case VK_LSHIFT: 
			buttonState = &gameKeyboard.shiftLeft; break;
		case VK_RSHIFT: 
			buttonState = &gameKeyboard.shiftRight; break;
		case VK_LCONTROL: 
			buttonState = &gameKeyboard.controlLeft; break;
		case VK_RCONTROL: 
			buttonState = &gameKeyboard.controlRight; break;
		case VK_MENU: 
			buttonState = &gameKeyboard.alt; break;
		case VK_UP: 
			buttonState = &gameKeyboard.arrowUp; break;
		case VK_DOWN: 
			buttonState = &gameKeyboard.arrowDown; break;
		case VK_LEFT: 
			buttonState = &gameKeyboard.arrowLeft; break;
		case VK_RIGHT: 
			buttonState = &gameKeyboard.arrowRight; break;
		case VK_INSERT: 
			buttonState = &gameKeyboard.insert; break;
		case VK_DELETE: 
			buttonState = &gameKeyboard.deleteKey; break;
		case VK_HOME: 
			buttonState = &gameKeyboard.home; break;
		case VK_END: 
			buttonState = &gameKeyboard.end; break;
		case VK_PRIOR: 
			buttonState = &gameKeyboard.pageUp; break;
		case VK_NEXT: 
			buttonState = &gameKeyboard.pageDown; break;
		case VK_DECIMAL: 
			buttonState = &gameKeyboard.numpad_period; break;
		case VK_DIVIDE: 
			buttonState = &gameKeyboard.numpad_divide; break;
		case VK_MULTIPLY: 
			buttonState = &gameKeyboard.numpad_multiply; break;
		case VK_SEPARATOR: 
			buttonState = &gameKeyboard.numpad_minus; break;
		case VK_ADD: 
			buttonState = &gameKeyboard.numpad_add; break;
		default:
		{
			if(vKeyCode >= 0x41 && vKeyCode <= 0x5A)
			{
				buttonState = &gameKeyboard.a + (vKeyCode - 0x41);
			}
			else if(vKeyCode >= 0x30 && vKeyCode <= 0x39)
			{
				buttonState = &gameKeyboard.tenkeyless_0 + (vKeyCode - 0x30);
			}
			else if(vKeyCode >= 0x70 && vKeyCode <= 0x7B)
			{
				buttonState = &gameKeyboard.f1 + (vKeyCode - 0x70);
			}
			else if(vKeyCode >= 0x60 && vKeyCode <= 0x69)
			{
				buttonState = &gameKeyboard.numpad_0 + (vKeyCode - 0x60);
			}
		} break;
	}
	if(buttonState)
	{
		*buttonState = keyDown
			? (keyDownPrev
				? ButtonState::HELD
				: ButtonState::PRESSED)
			: ButtonState::NOT_PRESSED;
	}
}
internal LRESULT CALLBACK w32MainWindowCallback(HWND hwnd, UINT uMsg, 
                                                WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	switch(uMsg)
	{
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		{
			kassert(!"Unhandled keyboard event!");
		} break;
		case WM_SIZE:
		{
			OutputDebugStringA("WM_SIZE\n");
		} break;
		case WM_DESTROY:
		{
			///TODO: handle this error: recreate window?
			g_running = false;
			OutputDebugStringA("WM_DESTROY\n");
		} break;
		case WM_CLOSE:
		{
			///TODO: ask user first before destroying
			g_running = false;
			OutputDebugStringA("WM_CLOSE\n");
		} break;
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT paintStruct;
			const HDC hdc = BeginPaint(hwnd, &paintStruct);
			if(hdc)
			{
				const int w = 
					paintStruct.rcPaint.right  - paintStruct.rcPaint.left;
				const int h = 
					paintStruct.rcPaint.bottom - paintStruct.rcPaint.top;
				const W32Dimension2d winDims = w32GetWindowDimensions(hwnd);
				w32UpdateWindow(g_backBuffer, hdc, 
				                winDims.width, winDims.height);
			}
			EndPaint(hwnd, &paintStruct);
		} break;
		default:
		{
			result = DefWindowProcA(hwnd, uMsg, wParam, lParam);
		}break;
	}
	return result;
}
internal LARGE_INTEGER w32QueryPerformanceCounter()
{
	LARGE_INTEGER result;
	if(!QueryPerformanceCounter(&result))
	{
		///TODO: handle GetLastError
	}
	return result;
}
internal f32 w32ElapsedSeconds(const LARGE_INTEGER& previousPerformanceCount, 
                               const LARGE_INTEGER& performanceCount)
{
	kassert(performanceCount.QuadPart > previousPerformanceCount.QuadPart);
	const LONGLONG perfCountDiff = 
		performanceCount.QuadPart - previousPerformanceCount.QuadPart;
	const f32 elapsedSeconds = 
		static_cast<f32>(perfCountDiff) / g_perfCounterHz.QuadPart;
	return elapsedSeconds;
}
extern int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, 
                             PWSTR /*pCmdLine*/, int /*nCmdShow*/)
{
	local_persist const char FILE_NAME_GAME_DLL[]      = "game.dll";
	local_persist const char FILE_NAME_GAME_DLL_TEMP[] = "game_temp.dll";
	///TODO: handle file paths longer than MAX_PATH in the future...
	size_t pathToExeSize = 0;
	char pathToExe[MAX_PATH];
	// locate where our exe file is on the OS so we can search for DLL files 
	//	there instead of whatever the current working directory is //
	{
		if(!GetModuleFileNameA(NULL, pathToExe, MAX_PATH))
		{
			///TODO: handle GetLastError
			return -1;
		}
		char* lastBackslash = pathToExe;
		for(char* c = pathToExe; *c; c++)
		{
			if(*c == '\\')
			{
				lastBackslash = c;
			}
		}
		*(lastBackslash + 1) = 0;
		pathToExeSize = lastBackslash - pathToExe;
	}
	///TODO: handle file paths longer than MAX_PATH in the future...
	char fullPathToGameDll[MAX_PATH] = {};
	char fullPathToGameDllTemp[MAX_PATH] = {};
	// use the `pathToExe` to determine the FULL path to the game DLL file //
	{
		if(strcat_s(fullPathToGameDll, MAX_PATH, pathToExe))
		{
			///TODO: log error
			return -1;
		}
		if(strcat_s(fullPathToGameDll + pathToExeSize, 
		            MAX_PATH - pathToExeSize, FILE_NAME_GAME_DLL))
		{
			///TODO: log error
			return -1;
		}
		if(strcat_s(fullPathToGameDllTemp, MAX_PATH, pathToExe))
		{
			///TODO: log error
			return -1;
		}
		if(strcat_s(fullPathToGameDllTemp + pathToExeSize, 
		            MAX_PATH - pathToExeSize, FILE_NAME_GAME_DLL_TEMP))
		{
			///TODO: log error
			return -1;
		}
	}
	GameCode game = w32LoadGameCode(fullPathToGameDll, 
	                                fullPathToGameDllTemp);
	if(!QueryPerformanceFrequency(&g_perfCounterHz))
	{
		///TODO: log GetLastError
		return -1;
	}
	w32LoadXInput();
	w32ResizeDibSection(g_backBuffer, 1280, 720);
	const WNDCLASS wndClass = {
		.style         = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc   = w32MainWindowCallback,
		.hInstance     = hInstance,
		.lpszClassName = "K10WindowClass" };
	const ATOM atomWindowClass = RegisterClassA(&wndClass);
	if(atomWindowClass == 0)
	{
		///TODO: log error
		return -1;
	}
	const HWND mainWindow = CreateWindowExA(
		0,
		wndClass.lpszClassName,
		"K10 Window",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL );
	if(!mainWindow)
	{
		///TODO: log error
		return -1;
	}
	///TODO: get window's monitor refresh rate (use MonitorFromWindow?..)
	u32 monitorRefreshHz = 60;
	u32 gameUpdateHz = monitorRefreshHz / 2;
	f32 targetSecondsElapsedPerFrame = 1.f / gameUpdateHz;
	GameKeyboard gameKeyboard = {};
#if INTERNAL_BUILD
	// ensure that the size of the keyboard's vKeys array matches the size of
	//	the anonymous struct which defines the names of all the game keys //
	kassert(static_cast<size_t>(&gameKeyboard.DUMMY_LAST_BUTTON_STATE - 
	                            &gameKeyboard.vKeys[0]) ==
	        (sizeof(gameKeyboard.vKeys) / sizeof(gameKeyboard.vKeys[0])));
#endif
	GamePad gamePadArrayA[XUSER_MAX_COUNT] = {};
	GamePad gamePadArrayB[XUSER_MAX_COUNT] = {};
	GamePad* gamePadArrayCurrentFrame  = gamePadArrayA;
	GamePad* gamePadArrayPreviousFrame = gamePadArrayB;
	u8 numGamePads = 0;
	local_persist const u8 SOUND_CHANNELS = 2;
	local_persist const u32 SOUND_SAMPLE_HZ = 48000;
	local_persist const u32 SOUND_BYTES_PER_SAMPLE = sizeof(i16)*SOUND_CHANNELS;
	local_persist const u32 SOUND_BUFFER_BYTES = 
		SOUND_SAMPLE_HZ * SOUND_BYTES_PER_SAMPLE;
	local_persist const u32 SOUND_LATENCY_SAMPLES = SOUND_SAMPLE_HZ / 15;
	u32 runningSoundSample = 0;
	VOID*const gameSoundMemory = VirtualAlloc(NULL, SOUND_BUFFER_BYTES, 
	                                          MEM_RESERVE | MEM_COMMIT, 
	                                          PAGE_READWRITE);
	if(!gameSoundMemory)
	{
		///TODO: handle GetLastError
		return -1;
	}
	GameMemory gameMemory = {};
#if INTERNAL_BUILD
	const LPVOID baseAddress = reinterpret_cast<LPVOID>(kmath::terabytes(1));
#else
	const LPVOID baseAddress = 0;
#endif
	gameMemory.permanentMemoryBytes = kmath::megabytes(64);
	gameMemory.transientMemoryBytes = kmath::gigabytes(4);
	const u64 totalGameMemoryBytes = 
		gameMemory.permanentMemoryBytes + gameMemory.transientMemoryBytes;
	gameMemory.permanentMemory = VirtualAlloc(baseAddress, 
	                                          totalGameMemoryBytes, 
	                                          MEM_RESERVE | MEM_COMMIT, 
	                                          PAGE_READWRITE);
	if(!gameMemory.permanentMemory)
	{
		///TODO: handle GetLastError
		return -1;
	}
	gameMemory.transientMemory = 
		reinterpret_cast<u8*>(gameMemory.permanentMemory) + 
		gameMemory.permanentMemoryBytes;
	gameMemory.platformPrintDebugString = platformPrintDebugString;
	gameMemory.platformReadEntireFile   = platformReadEntireFile;
	gameMemory.platformFreeFileMemory   = platformFreeFileMemory;
	gameMemory.platformWriteEntireFile  = platformWriteEntireFile;
	w32InitDSound(mainWindow, SOUND_SAMPLE_HZ, SOUND_BUFFER_BYTES, 
	              SOUND_CHANNELS, SOUND_LATENCY_SAMPLES, runningSoundSample);
	const HDC hdc = GetDC(mainWindow);
	if(!hdc)
	{
		///TODO: log error
		return -1;
	}
	local_persist const UINT DESIRED_OS_TIMER_GRANULARITY_MS = 1;
	// Determine if the system is capable of our desired timer granularity //
	bool systemSupportsDesiredTimerGranularity = false;
	{
		TIMECAPS timingCapabilities;
		if(timeGetDevCaps(&timingCapabilities, sizeof(TIMECAPS)) == 
			MMSYSERR_NOERROR)
		{
			systemSupportsDesiredTimerGranularity = 
				(timingCapabilities.wPeriodMin <= 
				 DESIRED_OS_TIMER_GRANULARITY_MS);
		}
		else
		{
			///TODO: log warning
		}
	}
	// set scheduler granularity using timeBeginPeriod //
	const bool sleepIsGranular = systemSupportsDesiredTimerGranularity &&
		timeBeginPeriod(DESIRED_OS_TIMER_GRANULARITY_MS) == TIMERR_NOERROR;
	if(!sleepIsGranular)
	{
		kassert(!"Failed to set OS sleep granularity!");
		///TODO: log warning
	}
#if 1
	if(sleepIsGranular)
	{
		const HANDLE hProcess = GetCurrentProcess();
		if(!SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS))
		{
			///TODO: handle GetLastError
			return -1;
		}
	}
#endif
#if LIMIT_FRAMERATE_USING_TIMER
	const HANDLE hTimerMainThread = 
		CreateWaitableTimerA(nullptr, true, "hTimerMainThread");
	if(!hTimerMainThread)
	{
		///TODO: handle GetLastError
		return -1;
	}
#endif
	// main window loop //
	{
		g_running = true;
		LARGE_INTEGER perfCountPrev = w32QueryPerformanceCounter();
		u64 clockCyclesPrev = __rdtsc();
		while(g_running)
		{
			// dynamically hot-reload game code!!! //
			{
				const FILETIME gameCodeLastWriteTime = 
					w32GetLastWriteTime(fullPathToGameDll);
				if(CompareFileTime(&gameCodeLastWriteTime, 
				                   &game.dllLastWriteTime))
				{
					w32UnloadGameCode(game);
					game = w32LoadGameCode(fullPathToGameDll, 
					                       fullPathToGameDllTemp);
				}
			}
			MSG windowMessage;
			while(PeekMessageA(&windowMessage, NULL, 0, 0, PM_REMOVE))
			{
				switch(windowMessage.message)
				{
					case WM_QUIT:
					{
						g_running = false;
					} break;
#if 0
					case WM_CHAR:
					{
#if SLOW_BUILD
						char outputBuffer[256] = {};
						snprintf(outputBuffer, sizeof(outputBuffer), 
						         "character code=%i\n", 
						         static_cast<int>(windowMessage.wParam));
						OutputDebugStringA(outputBuffer);
#endif
					} break;
#endif
					case WM_KEYDOWN:
					case WM_KEYUP:
					case WM_SYSKEYDOWN:
					case WM_SYSKEYUP:
					{
						w32ProcessKeyEvent(windowMessage, gameKeyboard);
					} break;
					default:
					{
						TranslateMessage(&windowMessage);
						DispatchMessageA(&windowMessage);
					} break;
				}
			}
			gameKeyboard.modifiers.shift =
				(gameKeyboard.shiftLeft  > ButtonState::NOT_PRESSED ||
				 gameKeyboard.shiftRight > ButtonState::NOT_PRESSED);
			gameKeyboard.modifiers.control =
				(gameKeyboard.controlLeft  > ButtonState::NOT_PRESSED ||
				 gameKeyboard.controlRight > ButtonState::NOT_PRESSED);
			gameKeyboard.modifiers.alt =
				gameKeyboard.alt > ButtonState::NOT_PRESSED;
			if(gamePadArrayCurrentFrame == gamePadArrayA)
			{
				gamePadArrayPreviousFrame = gamePadArrayA;
				gamePadArrayCurrentFrame  = gamePadArrayB;
			}
			else
			{
				gamePadArrayPreviousFrame = gamePadArrayB;
				gamePadArrayCurrentFrame  = gamePadArrayA;
			}
			w32XInputGetGamePadStates(&numGamePads,
			                          gamePadArrayCurrentFrame,
			                          gamePadArrayPreviousFrame);
			GameGraphicsBuffer gameGraphicsBuffer = {
				.bitmapMemory  = g_backBuffer.bitmapMemory,
				.width         = g_backBuffer.width,
				.height        = g_backBuffer.height,
				.pitch         = g_backBuffer.pitch,
				.bytesPerPixel = g_backBuffer.bytesPerPixel
			};
			if(!game.updateAndDraw(gameMemory, gameGraphicsBuffer, 
			                       gameKeyboard,
			                       gamePadArrayCurrentFrame, numGamePads))
			{
				g_running = false;
			}
			w32WriteDSoundAudio(SOUND_BUFFER_BYTES, SOUND_SAMPLE_HZ, 
			                    SOUND_CHANNELS, SOUND_LATENCY_SAMPLES, 
			                    runningSoundSample, gameSoundMemory, 
			                    gamePadArrayCurrentFrame, numGamePads,
			                    gameMemory, game);
			// set XInput state //
			for(u8 ci = 0; ci < numGamePads; ci++)
			{
				XINPUT_VIBRATION vibration;
				vibration.wLeftMotorSpeed = static_cast<WORD>(
					gamePadArrayCurrentFrame[ci].normalizedMotorSpeedLeft * 0xFFFF);
				vibration.wRightMotorSpeed = static_cast<WORD>(
					gamePadArrayCurrentFrame[ci].normalizedMotorSpeedRight * 0xFFFF);
				if(XInputSetState(ci, &vibration) != ERROR_SUCCESS)
				{
					///TODO: error & controller not connected return values
				}
			}
			// update window //
			{
				const W32Dimension2d winDims = 
					w32GetWindowDimensions(mainWindow);
				w32UpdateWindow(g_backBuffer, hdc,
				                winDims.width, winDims.height);
			}
			// update window //
			{
				const W32Dimension2d winDims = 
					w32GetWindowDimensions(mainWindow);
				w32UpdateWindow(g_backBuffer, hdc,
				                winDims.width, winDims.height);
			}
#if LIMIT_FRAMERATE_USING_TIMER
			// enforce targetSecondsElapsedPerFrame //
			{
				const f32 awakeSeconds = 
					w32ElapsedSeconds(perfCountPrev, 
					                  w32QueryPerformanceCounter());
				if(awakeSeconds < targetSecondsElapsedPerFrame)
				{
					const LONGLONG frameWaitHectoNanoseconds = 
						static_cast<LONGLONG>(10000000 *
						(targetSecondsElapsedPerFrame - awakeSeconds));
					// negative waitDueTime indicates relative time apparently
					LARGE_INTEGER waitDueTime;
					waitDueTime.QuadPart = -frameWaitHectoNanoseconds;
					if(!SetWaitableTimer(hTimerMainThread, &waitDueTime, 0, 
					                     nullptr, nullptr, false))
					{
						kassert(!"SetWaitableTimer failure!");
						///TODO: handle GetLastError
					}
					local_persist const DWORD MAX_SLEEP_MILLISECONDS = 34;
					const DWORD waitResult = 
						WaitForSingleObject(hTimerMainThread, 
						                    MAX_SLEEP_MILLISECONDS);
					kassert(waitResult == WAIT_OBJECT_0);
					if(waitResult == WAIT_FAILED)
					{
						kassert(!"WaitForSingleObject failure!");
						///TODO: handle GetLastError
					}
				}
			}
#else
			// enforce targetSecondsElapsedPerFrame //
			{
				// It is possible for windows to sleep us for longer than we 
				//	would want it to, so we will ask the OS to wake us up a 
				//	little bit earlier than desired.  Then we will spin the CPU
				//	until the actual target FPS is achieved.
				// THREAD_WAKE_SLACK_SECONDS is how early we should wake up by.
				local_persist const f32 THREAD_WAKE_SLACK_SECONDS = 0.001f;
				f32 awakeSeconds = 
					w32ElapsedSeconds(perfCountPrev, 
					                  w32QueryPerformanceCounter());
				const f32 targetAwakeSeconds = 
					targetSecondsElapsedPerFrame - THREAD_WAKE_SLACK_SECONDS;
				while(awakeSeconds < targetSecondsElapsedPerFrame)
				{
					if(sleepIsGranular && awakeSeconds < targetAwakeSeconds)
					{
						const DWORD sleepMilliseconds = 
							static_cast<DWORD>(1000 * 
							(targetAwakeSeconds - awakeSeconds));
						Sleep(sleepMilliseconds);
					}
					awakeSeconds = 
						w32ElapsedSeconds(perfCountPrev, 
						                  w32QueryPerformanceCounter());
				}
			}
#endif
			// measure main loop performance //
			{
				const LARGE_INTEGER perfCount = w32QueryPerformanceCounter();
				const f32 elapsedSeconds = 
					w32ElapsedSeconds(perfCountPrev, perfCount);
				const u64 clockCycles = __rdtsc();
				const u64 clockCycleDiff = clockCycles - clockCyclesPrev;
#if 0
				const LONGLONG elapsedMicroseconds = 
					(perfCountDiff*1000000) / g_perfCounterHz.QuadPart;
#endif
#if SLOW_BUILD
				// send performance measurement to debugger as a string //
				{
					char outputBuffer[256] = {};
					snprintf(outputBuffer, sizeof(outputBuffer), 
					         "%.2f ms/f | %.2f Mc/f\n", 
					         elapsedSeconds*1000, clockCycleDiff/1000000.f);
					OutputDebugStringA(outputBuffer);
				}
#endif
				perfCountPrev   = perfCount;
				clockCyclesPrev = clockCycles;
			}
		}
	}
	if(sleepIsGranular)
	{
		if(timeEndPeriod(DESIRED_OS_TIMER_GRANULARITY_MS) != TIMERR_NOERROR)
		{
			///TODO: log error
		}
	}
	return 0;
}