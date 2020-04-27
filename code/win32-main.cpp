#include <windows.h>
#include <Xinput.h>
#include <dsound.h>
#define internal        static
#define local_persist   static
#define global_variable static
using u8  = UINT8;
using u16 = UINT16;
using u32 = UINT32;
using u64 = UINT64;
using i8  = INT8;
using i16 = INT16;
using i32 = INT32;
using i64 = INT64;
struct W32OffscreenBuffer
{
	void* bitmapMemory;
	BITMAPINFO bitmapInfo;
	u32 width;
	u32 height;
	u32 pitch;
	u8 bytesPerPixel;
};
global_variable bool g_running;
global_variable W32OffscreenBuffer g_backBuffer;
global_variable LPDIRECTSOUNDBUFFER g_dsBufferSecondary;
struct W32Dimension2d
{
	u32 width;
	u32 height;
};
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
/********************************************************* END XINPUT SUPPORT */
/* DIRECT SOUND SUPPORT *******************************************************/
#define DSOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, \
                                                LPDIRECTSOUND* ppDS, \
                                                LPUNKNOWN pUnkOuter)
typedef DSOUND_CREATE(fnSig_DirectSoundCreate);
internal void w32InitDSound(HWND hwnd, u32 samplesPerSecond, u32 bufferBytes,
                            u8 numChannels)
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
#if 0
	// initialize the buffer to a simple square wave //
	{
		// D3 == 146.8hz
		//	48,000 / 146.8 = 327 samples per wave period
		const u32 hzD4 = 294;
		const u32 samplesPerWaveD4 = samplesPerSecond / hzD4;
		VOID* section1;
		DWORD section1Size;
		VOID* section2;
		DWORD section2Size;
		if(g_dsBufferSecondary->Lock(0, bufferBytes, &section1, &section1Size, 
		                             &section2, &section2Size, 0) != DS_OK)
		{
			///TODO: handle returned error code
			return;
		}
		i16* sample = (i16*)section1;
		const u32 sampleCount = bufferBytes/sizeof(i16)/numChannels;
		for(u32 s = 0; s < sampleCount; s++)
		{
			const u32 waveMod = s % samplesPerWaveD4;
			const i16 waveSign = waveMod < samplesPerWaveD4/2 ? 1 : -1;
			for(u8 c = 0; c < numChannels; c++)
			{
				*sample++ = 5000*waveSign;
			}
		}
		if(g_dsBufferSecondary->Unlock(section1, section1Size, section2, 0))
		{
			///TODO: handle returned error code
			return;
		}
	}
#endif
	///TODO: initialize the buffer total # bytes written & prev write cursor
	if(g_dsBufferSecondary->Play(0, 0, DSBPLAY_LOOPING) != DS_OK)
	{
		///TODO: handle returned error code
		return;
	}
}
/*************************************************** END DIRECT SOUND SUPPORT */
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
internal void renderWeirdGradient(W32OffscreenBuffer& buffer,
                                  int offsetX, int offsetY)
{
	u8* row = reinterpret_cast<u8*>(buffer.bitmapMemory);
	for (int y = 0; y < buffer.height; y++)
	{
		u32* pixel = reinterpret_cast<u32*>(row);
		for (int x = 0; x < buffer.width; x++)
		{
			// pixel format: 0xXxRrGgBb
			*pixel++ = ((u8)(x + offsetX) << 16) | ((u8)(y + offsetY) << 8);
		}
		row += buffer.pitch;
	}
}
internal void renderDebugAudio(u32 soundBufferBytes, 
                               u32 soundSampleHz,
                               u8 numSoundChannels,
                               i16 volume,
                               u32& io_runningSoundSample)
{
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
	// If the byteToLock is inside the range [P, W), let's skip 
	//	bytesToSamples(W-B) samples in our running sound sample count to avoid
	//	that case entirely.  Two cases must be handled due to circular sound 
	//	buffer shenanigans, as usual:
	// x == the region whose size represents how far we want to fast-forward our
	//	running sound sample variable to leave the volatile region
	if(cursorPlay < cursorWrite)
	{
		const u32 runningSoundByte = 
			io_runningSoundSample*sizeof(i16)*numSoundChannels;
		const DWORD byteToLock = runningSoundByte % soundBufferBytes;
		// |---------------------------P----BxxW-------------------------------|
		if(byteToLock >= cursorPlay && byteToLock < cursorWrite)
		{
			const u32 samplesToSkip = 
				(cursorWrite - byteToLock)/sizeof(i16)/numSoundChannels;
			io_runningSoundSample += samplesToSkip;
		}
	}
	else
	{
		const u32 runningSoundByte = 
			io_runningSoundSample*sizeof(i16)*numSoundChannels;
		const DWORD byteToLock = runningSoundByte % soundBufferBytes;
		// |xxxxxxxxxW----------------------------------------------P----Bxxxxx|
		// |---BxxxxxW----------------------------------------------P----------|
		if(byteToLock >= cursorPlay)
		{
			const u32 samplesToSkip = 
				((cursorWrite - byteToLock) + (soundBufferBytes - byteToLock))/
				sizeof(i16)/numSoundChannels;
			io_runningSoundSample += samplesToSkip;
		}
		if(byteToLock < cursorWrite)
		{
			const u32 samplesToSkip = 
				(cursorWrite - byteToLock)/sizeof(i16)/numSoundChannels;
			io_runningSoundSample += samplesToSkip;
		}
	}
	const u32 runningSoundByte = 
		io_runningSoundSample*sizeof(i16)*numSoundChannels;
	const DWORD byteToLock = runningSoundByte % soundBufferBytes;
	// x == the region we want to fill in with new sound samples
	const DWORD lockedBytes = (byteToLock < cursorPlay)
		// |------------BxxxxxxxxxxxxxxP-------W-------------------------------|
		? (cursorPlay - byteToLock) - 1
		// It should also be possible for B==W, but that should be okay I think!
		// |xxxxxxxxxxxxxxxxxxxxxxxxxxxP-------WBxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
		: (soundBufferBytes - byteToLock) + 
			(cursorPlay > 0 ? cursorPlay - 1 : 0);
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
	if(g_dsBufferSecondary->Lock(byteToLock, lockedBytes,
	                             &lockRegion1, &lockRegion1Size,
	                             &lockRegion2, &lockRegion2Size,
	                             0) != DS_OK)
	{
		///TODO: handle error code
		return;
	}
	const u32 lockedSampleCount = 
		(lockRegion1Size + lockRegion2Size)/sizeof(i16)/numSoundChannels;
	DWORD lockedRegion1BytesWritten = 0;
	DWORD lockedRegion2BytesWritten = 0;
	// actually write data to the buffer //
	local_persist const u32 HZ_D4 = 294;
	local_persist const u32 HZ_G4 = 392;
	const u32 samplesPerWaveD4 = soundSampleHz / HZ_D4;
	const u32 samplesPerWaveG4 = soundSampleHz / HZ_G4;
#if 0
	const u32 totalSamplesSent = 
		io_bufferTotalBytesSent/sizeof(i16)/numSoundChannels;
#endif
	const u32 lockRegion1Samples = lockRegion1Size/sizeof(i16)/numSoundChannels;
	const u32 lockRegion2Samples = lockRegion2Size/sizeof(i16)/numSoundChannels;
	for(u32 s = 0; s < lockedSampleCount; s++)
	{
		i16* sample = (s < lockRegion1Samples)
			? (i16*)lockRegion1 + (s*numSoundChannels)
			: (i16*)lockRegion2 + ((s - lockRegion1Samples) * numSoundChannels);
		DWORD& regionBytesWritten = (s < lockRegion1Samples)
			? lockedRegion1BytesWritten
			: lockedRegion2BytesWritten;
		// Determine what the debug waveforms should look like at this point of
		//	the running sound sample //
		u32 waveformModD4 = 
			(s + io_runningSoundSample) % samplesPerWaveD4;
		const i16 waveformSignD4 = 
			waveformModD4 < samplesPerWaveD4 / 2 ? 1 : -1;
		u32 waveformModG4 = 
			(s + io_runningSoundSample) % samplesPerWaveG4;
		const i16 waveformSignG4 = 
			waveformModG4 < samplesPerWaveG4 / 2 ? 1 : -1;
		for(u8 c = 0; c < numSoundChannels; c++)
		{
			// combine the D4 + G4 waves to create a chord! //
			*sample++ = (volume * waveformSignD4) + (volume * waveformSignG4);
			regionBytesWritten += sizeof(i16);
		}
	}
	io_runningSoundSample += lockedSampleCount;
	if(g_dsBufferSecondary->Unlock(
		lockRegion1, lockedRegion1BytesWritten,
		lockRegion2, lockedRegion2BytesWritten) != DS_OK)
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
LRESULT CALLBACK w32MainWindowCallback(HWND hwnd, UINT uMsg, WPARAM wParam, 
                                       LPARAM lParam)
{
	LRESULT result = 0;
	switch(uMsg)
	{
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		{
			const WPARAM vKeyCode = wParam;
			const bool keyDown     = (lParam & (1<<31)) == 0;
			const bool keyDownPrev = (lParam & (1<<30)) != 0;
			const bool altKeyDown  = (lParam & (1<<29)) != 0;
			if(!keyDown || keyDownPrev)
			{
				break;
			}
			switch(vKeyCode)
			{
				case VK_ESCAPE:
				{
					OutputDebugStringA("ESCAPE\n");
					g_running = false;
				} break;
				case VK_F4:
				{
					if(altKeyDown)
					{
						g_running = false;
					}
				} break;
			}
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
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		}break;
	}
	return result;
}
internal int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, 
                             PWSTR /*pCmdLine*/, int /*nCmdShow*/)
{
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
	local_persist const u8 SOUND_CHANNELS = 2;
	local_persist const u32 SOUND_SAMPLE_HZ = 48000;
	local_persist const u32 SOUND_BUFFER_BYTES = SOUND_SAMPLE_HZ*sizeof(i16)*2;
	u32 runningSoundSample = 0;
	w32InitDSound(mainWindow, SOUND_SAMPLE_HZ, SOUND_BUFFER_BYTES, 
	              SOUND_CHANNELS);
	const HDC hdc = GetDC(mainWindow);
	if(!hdc)
	{
		///TODO: log error
		return -1;
	}
	// main window loop //
	{
		g_running = true;
		int bgOffsetX = 0;
		int bgOffsetY = 0;
		while(g_running)
		{
			MSG windowMessage;
			while(PeekMessageA(&windowMessage, NULL, 0, 0, PM_REMOVE))
			{
				if(windowMessage.message == WM_QUIT)
				{
					g_running = false;
				}
				TranslateMessage(&windowMessage);
				DispatchMessageA(&windowMessage);
			}
			for(u32 ci = 0; ci < XUSER_MAX_COUNT; ci++)
			{
				XINPUT_STATE controllerState;
				XINPUT_VIBRATION vibration;
				if( XInputGetState(ci, &controllerState) == ERROR_SUCCESS )
				{
					///TODO: investigate controllerState.dwPacketNumber for 
					///      input polling performance
					const XINPUT_GAMEPAD& pad = controllerState.Gamepad;
					if(pad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
					{
						bgOffsetY -= 1;
					}
					if(pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
					{
						bgOffsetY += 1;
					}
					if(pad.wButtons & XINPUT_GAMEPAD_BACK)
					{
						g_running = false;
					}
					vibration.wLeftMotorSpeed = 
						(WORD)((pad.bLeftTrigger/255.f)*0xFFFF);
					vibration.wRightMotorSpeed = 
						(WORD)((pad.bRightTrigger/255.f)*0xFFFF);
					bgOffsetX += pad.sThumbLX>>11;
					bgOffsetY -= pad.sThumbLY>>11;
				}
				else
				{
					///TODO: handle controller not available
				}
				if(XInputSetState(ci, &vibration) != ERROR_SUCCESS)
				{
					///TODO: error & controller not connected return values
				}
			}
			renderWeirdGradient(g_backBuffer, bgOffsetX, bgOffsetY);
			renderDebugAudio(SOUND_BUFFER_BYTES, SOUND_SAMPLE_HZ, 
			                 SOUND_CHANNELS, 5000, runningSoundSample);
			// update window //
			{
				const W32Dimension2d winDims = 
					w32GetWindowDimensions(mainWindow);
				w32UpdateWindow(g_backBuffer, hdc,
				                winDims.width, winDims.height);
			}
		}
	}
	return 0;
}