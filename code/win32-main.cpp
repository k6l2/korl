#include <windows.h>
#include <Xinput.h>
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
	return 0;
}
XINPUT_SET_STATE(XInputSetStateStub)
{
	return 0;
}
global_variable fnSig_XInputGetState* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_
global_variable fnSig_XInputSetState* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_
internal void w32LoadXInput()
{
	const HMODULE LibXInput = LoadLibraryA("xinput1_3.dll");
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
		                                    MEM_COMMIT, PAGE_READWRITE);
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