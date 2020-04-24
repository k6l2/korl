#include <windows.h>
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
global_variable bool running;
global_variable void* bitmapMemory;
global_variable BITMAPINFO bitmapInfo;
internal void renderWeirdGradient(int offsetX, int offsetY)
{
	const int width  = abs(bitmapInfo.bmiHeader.biWidth);
	const int height = abs(bitmapInfo.bmiHeader.biHeight);
	u8* row = reinterpret_cast<u8*>(bitmapMemory);
	const int pitch = 4 * width;
	for (int y = 0; y < height; y++)
	{
		u32* pixel = reinterpret_cast<u32*>(row);
		for (int x = 0; x < width; x++)
		{
			// pixel format: 0xXxRrGgBb
			*pixel++ = ((u8)(x + offsetX) << 16) | ((u8)(y + offsetY) << 8);
		}
		row += pitch;
	}
}
internal void w32ResizeDibSection(int width, int height)
{
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = width;
	bitmapInfo.bmiHeader.biHeight = -height;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	const int bitmapMemorySize = 4*width*height;
	if(bitmapMemory)
	{
		VirtualFree(bitmapMemory, 0, MEM_RELEASE);
	}
	bitmapMemory = VirtualAlloc(NULL, bitmapMemorySize, 
	                            MEM_COMMIT, PAGE_READWRITE);
	if(!bitmapMemory)
	{
		///TODO: handle GetLastError
	}
}
internal void w32UpdateWindow(HDC hdc, RECT* windowRect, int x, int y, 
                              int width, int height)
{
	const int windowWidth  = windowRect->right  - windowRect->left;
	const int windowHeight = windowRect->bottom - windowRect->top;
	const int signedNumCopiedScanlines =
		StretchDIBits(hdc,
			0, 0, abs(bitmapInfo.bmiHeader.biWidth), 
			      abs(bitmapInfo.bmiHeader.biHeight),
			0, 0, windowWidth, windowHeight,
			bitmapMemory,
			&bitmapInfo,
			DIB_RGB_COLORS,
			SRCCOPY);
	if(signedNumCopiedScanlines == GDI_ERROR ||
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
		case WM_SIZE:
		{
			RECT clientRect;
			if(GetClientRect(hwnd, &clientRect))
			{
				const int w = clientRect.right  - clientRect.left;
				const int h = clientRect.bottom - clientRect.top;
				w32ResizeDibSection(w, h);
			}
			else
			{
				///TODO: handle GetLastError
			}
			OutputDebugStringA("WM_SIZE\n");
		} break;
		case WM_DESTROY:
		{
			///TODO: handle this error: recreate window?
			running = false;
			OutputDebugStringA("WM_DESTROY\n");
		} break;
		case WM_CLOSE:
		{
			///TODO: ask user first before destroying
			running = false;
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
				RECT clientRect;
				if(GetClientRect(hwnd, &clientRect))
				{
					const int w = clientRect.right  - clientRect.left;
					const int h = clientRect.bottom - clientRect.top;
					w32UpdateWindow(hdc, &clientRect,
									paintStruct.rcPaint.left, 
									paintStruct.rcPaint.top, w, h);
				}
				else
				{
					///TODO: handle GetLastError
				}
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
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, 
                    PWSTR /*pCmdLine*/, int /*nCmdShow*/)
{
	const WNDCLASS wndClass = {
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
	// main window loop //
	{
		MSG windowMessage;
		running = true;
		int bgOffsetX = 0;
		int bgOffsetY = 0;
		while(running)
		{
			while(PeekMessageA(&windowMessage, NULL, 0, 0, PM_REMOVE))
			{
				if(windowMessage.message == WM_QUIT)
				{
					running = false;
				}
				TranslateMessage(&windowMessage);
				DispatchMessageA(&windowMessage);
			}
			bgOffsetX += 1;
			bgOffsetY += 1;
			renderWeirdGradient(bgOffsetX, bgOffsetY);
			// update window //
			{
				const HDC hdc = GetDC(mainWindow);
				if(hdc)
				{
					RECT clientRect;
					if(GetClientRect(mainWindow, &clientRect))
					{
						const int w = clientRect.right  - clientRect.left;
						const int h = clientRect.bottom - clientRect.top;
						w32UpdateWindow(hdc, &clientRect, 0, 0, w, h);
					}
					else
					{
						///TODO: handle GetLastError
					}
					if(!ReleaseDC(mainWindow, hdc))
					{
						///TODO: handle error
					}
				}
				else
				{
					///TODO: handle error
				}
			}
		}
	}
	return 0;
}