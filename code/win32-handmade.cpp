#include <windows.h>
LRESULT CALLBACK mainWindowCallback(HWND hwnd, UINT uMsg, WPARAM wParam, 
                                    LPARAM lParam)
{
	LRESULT result = 0;
	switch(uMsg)
	{
		case WM_SIZE:
		{
			OutputDebugStringA("WM_SIZE\n");
		} break;
		case WM_DESTROY:
		{
			OutputDebugStringA("WM_DESTROY\n");
		} break;
		case WM_CLOSE:
		{
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
				PatBlt(hdc, paintStruct.rcPaint.left, paintStruct.rcPaint.top,
				       paintStruct.rcPaint.right  - paintStruct.rcPaint.left,
				       paintStruct.rcPaint.bottom - paintStruct.rcPaint.top,
				       BLACKNESS);
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
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                    PWSTR pCmdLine, int nCmdShow)
{
	const WNDCLASS wndClass = {
		.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc   = mainWindowCallback,
		.hInstance     = hInstance,
		.lpszClassName = "K10WindowClass" };
	const ATOM atomWindowClass = RegisterClass(&wndClass);
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
		while(true)
		{
			const BOOL getMsgResult = GetMessage(&windowMessage, NULL, 0, 0 );
			if(getMsgResult > 0)
			{
				TranslateMessage(&windowMessage);
				DispatchMessage (&windowMessage);
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