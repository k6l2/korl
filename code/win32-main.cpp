#include <windows.h>
#define internal        static
#define local_persist   static
#define global_variable static
global_variable bool running;
global_variable void* bitmapMemory;
global_variable HBITMAP bitmapHandle;
global_variable BITMAPINFO bitmapInfo;
global_variable HDC bitmapDeviceContext;
internal void w32ResizeDibSection(int width, int height)
{
	if(bitmapHandle)
	{
		DeleteObject(bitmapHandle);
	}
	if(!bitmapDeviceContext)
	{
		bitmapDeviceContext = CreateCompatibleDC(NULL);
		if(!bitmapDeviceContext)
		{
			///TODO: handle error
		}
	}
	bitmapInfo = {
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biWidth = width,
			.biHeight = width,
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
			.biSizeImage = 0,
			.biXPelsPerMeter = 0,
			.biYPelsPerMeter = 0,
			.biClrUsed = 0,
			.biClrImportant = 0 } };
	//TODO: bitmapMemory = ;
	bitmapHandle = CreateDIBSection(
		bitmapDeviceContext,
		&bitmapInfo,
		DIB_RGB_COLORS,
		&bitmapMemory,
		NULL, 0);
	if(!bitmapHandle)// || bitmapHandle == ERROR_INVALID_PARAMETER)
	{
		///TODO: handle error
	}
}
internal void w32UpdateWindow(HDC hdc, int x, int y, int width, int height)
{
	const int signedNumCopiedScanlines =
		StretchDIBits(hdc,
			x, y, width, height, 
			x, y, width, height, 
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
				w32UpdateWindow(hdc, 
				                paintStruct.rcPaint.left, 
				                paintStruct.rcPaint.top, w, h);
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
		while(running)
		{
			const BOOL getMsgResult = GetMessageA(&windowMessage, NULL, 0, 0 );
			if(getMsgResult > 0)
			{
				TranslateMessage(&windowMessage);
				DispatchMessageA(&windowMessage);
			}
			else
			{
				// 0 == quit, -1 == error.  We're done.
				if(getMsgResult == -1)
				{
					///TODO: log error
				}
				break;
			}
		}
	}
	return 0;
}