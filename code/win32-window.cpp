#include "win32-window.h"
#include "win32-main.h"
#include <tchar.h>/* _stprintf_s */
#include <windowsx.h>/* GET_X_LPARAM */
#define KORL_W32_SIZE_KEYBOARD 0x0
#define KORL_W32_SIZE_LEFT     0x1
#define KORL_W32_SIZE_TOP      0x2
#define KORL_W32_SIZE_RIGHT    0x4
#define KORL_W32_SIZE_BOTTOM   0x8
#define KORL_W32_MOVE_KEYBOARD 0x0
#define KORL_W32_MOVE_MOUSE    0x1
global_variable KorlWin32MoveSizeMode g_moveSizeMode = 
	KorlWin32MoveSizeMode::OFF;
global_variable RECT g_moveSizeStartWindowRect;
global_variable POINT g_moveSizeStartMouseScreen;
global_variable POINT g_moveSizeStartMouseClient;
global_variable POINT g_moveSizeLastMouseScreen;
//global_variable v2f32 g_moveSizeMouseAnchor;
global_variable v2f32 g_moveSizeKeyVelocity;
global_variable bool g_moveSizeKeyMoved;
/* Uses a combination of KORL_W32_SIZE_X flags.  This should never contain two 
	sides on the same axis!  This should only ever contain 0, 1, or 2 active 
	bits at any given time.  */
global_variable u8 g_moveSizeSides;
global_const Korl_W32_Window_WndProcResult KORL_W32_WNDPROC_RESULT_OVERRIDDEN = 
	{ .lResultOverride = 0
	, .callerActionRequired = 
		Korl_W32_Window_WndProcResult::CallerAction::NONE };
global_const Korl_W32_Window_WndProcResult KORL_W32_WNDPROC_RESULT_USE_DEFAULT = 
	{ .callerActionRequired = 
		Korl_W32_Window_WndProcResult::CallerAction::DEFAULT_WINDOW_PROCEDURE };
internal v2u32 korl_w32_window_getWindowDimensions(HWND hwnd)
{
	RECT clientRect;
	if(GetClientRect(hwnd, &clientRect))
	{
		const u32 w = clientRect.right  - clientRect.left;
		const u32 h = clientRect.bottom - clientRect.top;
		return {w, h};
	}
	else
	{
		KLOG(ERROR, "Failed to get window dimensions! GetLastError=%i", 
			GetLastError());
		return {0, 0};
	}
}
internal DWORD korl_w32_window_queryNearestMonitorRefreshRate(HWND hwnd)
{
	local_persist const DWORD DEFAULT_RESULT = 60;
	const HMONITOR nearestMonitor = 
		MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEXA monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFOEXA);
	if(!GetMonitorInfoA(nearestMonitor, &monitorInfo))
	{
		KLOG(ERROR, "Failed to get monitor info!");
		return DEFAULT_RESULT;
	}
	DEVMODEA monitorDevMode;
	monitorDevMode.dmSize = sizeof(DEVMODEA);
	monitorDevMode.dmDriverExtra = 0;
	if(!EnumDisplaySettingsA(
		monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &monitorDevMode))
	{
		KLOG(ERROR, "Failed to enum display settings!");
		return DEFAULT_RESULT;
	}
	if(monitorDevMode.dmDisplayFrequency < 2)
	{
		KLOG(WARNING, "Unknown hardware-defined refresh rate! "
			"Defaulting to %ihz...", DEFAULT_RESULT);
		return DEFAULT_RESULT;
	}
	return monitorDevMode.dmDisplayFrequency;
}
internal POINT korl_w32_window_findCaptionBarCenter(HWND hWnd)
{
	TITLEBARINFOEX titleBarInfoEx = {sizeof(TITLEBARINFOEX), 0};
	const LRESULT resultSendMsgTitleBarInfoEx = 
		SendMessage(
			hWnd, WM_GETTITLEBARINFOEX, 0, 
			reinterpret_cast<LPARAM>(&titleBarInfoEx));
	const POINT titleBarCenter = 
		{ titleBarInfoEx.rcTitleBar.left + 
			(titleBarInfoEx.rcTitleBar.right - 
			 titleBarInfoEx.rcTitleBar.left) / 2
		, titleBarInfoEx.rcTitleBar.top + 
			(titleBarInfoEx.rcTitleBar.bottom - 
			 titleBarInfoEx.rcTitleBar.top) / 2};
	return titleBarCenter;
}
internal void korl_w32_window_centerCursorOnCaptionBar(HWND hWnd)
{
	const POINT titleBarCenter = korl_w32_window_findCaptionBarCenter(hWnd);
	/* set the mouse cursor to be the center of the window's title bar */
	const BOOL successSetCursorPos = 
		SetCursorPos(titleBarCenter.x, titleBarCenter.y);
	if(!successSetCursorPos)
		KLOG(ERROR, "SetCursorPos failed! GetLastError=%i", 
			GetLastError());
}
internal void korl_w32_window_setMoveSizeMode(
	KorlWin32MoveSizeMode mode, HWND hWnd)
{
	if(g_moveSizeMode == mode)
		return;
	switch(mode)
	{
	case KorlWin32MoveSizeMode::MOVE_MOUSE: {
		SetCursor(g_cursorSizeAll);
		} break;
	case KorlWin32MoveSizeMode::MOVE_KEYBOARD: {
		g_moveSizeKeyVelocity = v2f32::ZERO;
		g_moveSizeKeyMoved = false;
		/* WM_SETCURSOR doesn't seem to occur during move/resize mode so we can 
			just set it once here apparently */
		SetCursor(g_cursorSizeAll);
		korl_w32_window_centerCursorOnCaptionBar(hWnd);
		/* display simple controls for the user in the title bar */
		TCHAR charBufferWindowText[256];
		const int charsWritten = 
			_stprintf_s(
				charBufferWindowText, CARRAY_SIZE(charBufferWindowText), "%s%s", 
				APPLICATION_NAME, 
				" (MOVE ENABLED! [enter]:confirm [escape]:cancel)");
		korlAssert(charsWritten > 0);
		const BOOL successSetWindowText = 
			SetWindowText(hWnd, charBufferWindowText);
		if(!successSetWindowText)
			KLOG(ERROR, "SetWindowText failed! GetLastError=%i", 
				GetLastError());
		} break;
	case KorlWin32MoveSizeMode::SIZE_KEYBOARD: {
		g_moveSizeSides = 0;
		/* WM_SETCURSOR doesn't seem to occur during move/resize mode so we can 
			just set it once here apparently */
		SetCursor(g_cursorSizeAll);
		/* calculate the center of the window in screen-space */
		RECT windowRect;
		const BOOL successGetWindowRect = GetWindowRect(hWnd, &windowRect);
		if(!successGetWindowRect)
			KLOG(ERROR, "GetWindowRect failed! GetLastError=%i", 
				GetLastError());
		const POINT windowCenter = 
			{ windowRect.left + (windowRect.right  - windowRect.left) / 2
			, windowRect.top  + (windowRect.bottom - windowRect.top ) / 2 };
		/* move the cursor to the center of the window */
		const BOOL successSetCursorPos = 
			SetCursorPos(windowCenter.x, windowCenter.y);
		if(!successSetCursorPos)
			KLOG(ERROR, "SetCursorPos failed! GetLastError=%i", 
				GetLastError());
		} break;
	case KorlWin32MoveSizeMode::OFF: {
		/* release global mouse event capture */
		const BOOL resultReleaseCapture = ReleaseCapture();
		if(!resultReleaseCapture)
			KLOG(ERROR, "ReleaseCapture failed! GetLastError=%i", 
				GetLastError());
		/* ensure the title bar text of the main window is reset */
		const BOOL successSetWindowText = 
			SetWindowText(hWnd, APPLICATION_NAME);
		if(!successSetWindowText)
			KLOG(ERROR, "SetWindowText failed! GetLastError=%i", 
				GetLastError());
		/* conform to default windows behavior */
		SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
		} break;
	}
	g_moveSizeMode = mode;
	if(mode != KorlWin32MoveSizeMode::OFF)
	{
		/* set this window to be the global mouse capture so we can process 
			all mouse events outside of our window */
		SetCapture(hWnd);
		/* conform to default windows behavior */
		SendMessage(hWnd, WM_ENTERSIZEMOVE, 0, 0);
	}
}
internal Korl_W32_Window_WndProcResult korl_w32_window_wndProc_sysCommand(
	HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	/* syscommand wparam values use the first 4 low-order bits internally, 
		so we have to mask them out to get the correct values (see MSDN) */
	const WPARAM wParamSysCommand = wParam & 0xFFF0;
	/* These window messages need to be captured in order to stop 
		windows from needlessly beeping when we use ALT+KEY combinations */
	if(wParamSysCommand == SC_KEYMENU)
	{
		return KORL_W32_WNDPROC_RESULT_OVERRIDDEN;
	}
	else if(wParamSysCommand == SC_MOVE)
	{
		korl_w32_window_setMoveSizeMode(KorlWin32MoveSizeMode::OFF, hWnd);
		/* enter "move window" mode 
			- save the current mouse position
			- wherever mouse absolute position gets polled, move the window 
				to a new position if the mouse position has changed
			- dispatch WM_EXITSIZEMOVE if the mouse gets released?
			- when this callback gets a WM_EXITSIZEMOVE msg (?), exit 
				"move window" mode */
		/* get the windows starting RECT */
		const bool successGetWindowRect = 
			GetWindowRect(hWnd, &g_moveSizeStartWindowRect);
		/* if we fail to get the window's start RECT, we can't properly 
			begin moving the window around relative to an undefined 
			position */
		if(!successGetWindowRect)
		{
			KLOG(ERROR, "GetWindowRect failed! GetLastError=%i", 
				GetLastError());
			return KORL_W32_WNDPROC_RESULT_OVERRIDDEN;
		}
		/* cache the mouse position at the beginning of the command */
		g_moveSizeStartMouseScreen = 
			{ .x = GET_X_LPARAM(lParam)
			, .y = GET_Y_LPARAM(lParam) };
		g_moveSizeLastMouseScreen = g_moveSizeStartMouseScreen;
		/* Test to see if the sys command is the result of a mouse click. */
		if((wParam & 0xF) == KORL_W32_MOVE_MOUSE)
		{
			KLOG(INFO, "SC_MOVE - mouse mode!");
			/* calculate the client-space mouse position, since we need to use 
				this for calculating the new position when mouse moves */
			g_moveSizeStartMouseClient = g_moveSizeStartMouseScreen;
			const bool successScreenToClient = 
				ScreenToClient(hWnd, &g_moveSizeStartMouseClient);
			if(!successScreenToClient)
			{
				KLOG(ERROR, "ScreenToClient failed!");
				return KORL_W32_WNDPROC_RESULT_OVERRIDDEN;
			}
			/* begin non-modal window movement logic */
			korl_w32_window_setMoveSizeMode(
				KorlWin32MoveSizeMode::MOVE_MOUSE, hWnd);
		}
		else
		{
			KLOG(INFO, "SC_MOVE - keyboard mode!");
			/* begin non-modal window movement logic */
			korl_w32_window_setMoveSizeMode(
				KorlWin32MoveSizeMode::MOVE_KEYBOARD, hWnd);
		}
		return KORL_W32_WNDPROC_RESULT_OVERRIDDEN;
	}
	else if(wParamSysCommand == SC_SIZE)
	{
		/* enter "resize window" mode
			- save the current mouse position
			- determine which edge(s) the user is resizing
			- wherever mouse absolute position gets polled, resize the 
				window (& re-position if necessary for top/left edges) if 
				the mouse position has changed
			- when this callback gets a WM_EXITSIZEMOVE msg, exit 
				"resize window" mode */
		/* get the windows starting RECT */
		const bool successGetWindowRect = 
			GetWindowRect(hWnd, &g_moveSizeStartWindowRect);
		/* if we fail to get the window's start RECT, we can't properly 
			begin moving the window around relative to an undefined 
			position */
		if(!successGetWindowRect)
		{
			KLOG(ERROR, "GetWindowRect failed! GetLastError=%i", 
				GetLastError());
			return KORL_W32_WNDPROC_RESULT_OVERRIDDEN;
		}
		/* cache the mouse position at the beginning of the command */
		g_moveSizeStartMouseScreen = 
			{ .x = GET_X_LPARAM(lParam)
			, .y = GET_Y_LPARAM(lParam) };
		g_moveSizeLastMouseScreen = g_moveSizeStartMouseScreen;
		if((wParam & 0xF) == KORL_W32_SIZE_KEYBOARD)
		{
			KLOG(INFO, "SC_SIZE - keyboard mode!");
			/* begin non-modal window resize logic */
			korl_w32_window_setMoveSizeMode(
				KorlWin32MoveSizeMode::SIZE_KEYBOARD, hWnd);
		}
		else
		{
			KLOG(INFO, "SC_SIZE - mouse mode!");
			g_moveSizeSides = (wParam & 0xF);
			korl_w32_window_setMoveSizeMode(
				KorlWin32MoveSizeMode::SIZE_MOUSE, hWnd);
		}
		return KORL_W32_WNDPROC_RESULT_OVERRIDDEN;
	}
	return KORL_W32_WNDPROC_RESULT_USE_DEFAULT;
}
internal Korl_W32_Window_WndProcResult korl_w32_window_wndProc_lButtonDown(
	HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	/* allow the user to begin moving the window with left-click+drag at any 
		time during the MOVE_KEYBOARD mode */
	if(g_moveSizeMode == KorlWin32MoveSizeMode::MOVE_KEYBOARD)
	{
		const POINT mouseClient = 
			{ GET_X_LPARAM(lParam)
			, GET_Y_LPARAM(lParam) };
		POINT mouseScreen = mouseClient;
		const BOOL successClientToScreen = ClientToScreen(hWnd, &mouseScreen);
		korlAssert(successClientToScreen);
		/* I don't really think we need to actually do anything with the result 
			of this SendMessage call... */
		const LRESULT lResultSysCommandMove = 
			SendMessage(
				hWnd, WM_SYSCOMMAND, SC_MOVE | KORL_W32_MOVE_MOUSE, 
				MAKELONG(mouseScreen.x, mouseScreen.y));
		return KORL_W32_WNDPROC_RESULT_OVERRIDDEN;
	}
	return KORL_W32_WNDPROC_RESULT_USE_DEFAULT;
}
internal Korl_W32_Window_WndProcResult korl_w32_window_wndProc_nclButtonDown(
	HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	/* dispatch move sys commands based on where we clicked */
	WPARAM wParamSizeMove = 0;
	switch(wParam)
	{
	case HTLEFT: {
		wParamSizeMove = SC_SIZE | KORL_W32_SIZE_LEFT;
		} break;
	case HTTOPLEFT: {
		wParamSizeMove = SC_SIZE | KORL_W32_SIZE_LEFT | KORL_W32_SIZE_TOP;
		} break;
	case HTBOTTOMLEFT: {
		wParamSizeMove = 
			SC_SIZE | KORL_W32_SIZE_LEFT | KORL_W32_SIZE_BOTTOM;
		} break;
	case HTRIGHT: {
		wParamSizeMove = SC_SIZE | KORL_W32_SIZE_RIGHT;
		} break;
	case HTTOPRIGHT: {
		wParamSizeMove = SC_SIZE | KORL_W32_SIZE_RIGHT | KORL_W32_SIZE_TOP;
		} break;
	case HTBOTTOMRIGHT: {
		wParamSizeMove = 
			SC_SIZE | KORL_W32_SIZE_RIGHT | KORL_W32_SIZE_BOTTOM;
		} break;
	case HTTOP: {
		wParamSizeMove = SC_SIZE | KORL_W32_SIZE_TOP;
		} break;
	case HTBOTTOM: {
		wParamSizeMove = SC_SIZE | KORL_W32_SIZE_BOTTOM;
		} break;
	case HTCAPTION: {
		wParamSizeMove = SC_MOVE | KORL_W32_MOVE_MOUSE;
		} break;
	default: {
		} break;
	}
	if(wParamSizeMove)
	{
		/* I don't really think we need to actually do anything with the result 
			of this SendMessage call... */
		const LRESULT lResultSysCommandSizeMove = 
			SendMessage(hWnd, WM_SYSCOMMAND, wParamSizeMove, lParam);
		return KORL_W32_WNDPROC_RESULT_OVERRIDDEN;
	}
	return KORL_W32_WNDPROC_RESULT_USE_DEFAULT;
}
internal Korl_W32_Window_WndProcResult korl_w32_window_wndProc_lButtonUp(
	HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(g_moveSizeMode == KorlWin32MoveSizeMode::MOVE_MOUSE)
		korl_w32_window_setMoveSizeMode(KorlWin32MoveSizeMode::OFF, hWnd);
	return KORL_W32_WNDPROC_RESULT_USE_DEFAULT;
}
internal Korl_W32_Window_WndProcResult korl_w32_window_wndProc_nclButtonUp(
	HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(g_moveSizeMode == KorlWin32MoveSizeMode::MOVE_MOUSE)
		korl_w32_window_setMoveSizeMode(KorlWin32MoveSizeMode::OFF, hWnd);
	return KORL_W32_WNDPROC_RESULT_USE_DEFAULT;
}
internal Korl_W32_Window_WndProcResult korl_w32_window_wndProc_keyDown(
	HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(   g_moveSizeMode == KorlWin32MoveSizeMode::MOVE_KEYBOARD 
	   || g_moveSizeMode == KorlWin32MoveSizeMode::SIZE_KEYBOARD)
	{
		switch(wParam)
		{
		case VK_ESCAPE: {
			/* if escape is pressed, revert move changes entirely */
			const BOOL successMoveWindow = 
				MoveWindow(
					hWnd, 
					g_moveSizeStartWindowRect.left, 
					g_moveSizeStartWindowRect.top, 
					g_moveSizeStartWindowRect.right - 
						g_moveSizeStartWindowRect.left, 
					g_moveSizeStartWindowRect.bottom - 
						g_moveSizeStartWindowRect.top, 
					/* repaint? */FALSE);
			if(!successMoveWindow)
				KLOG(ERROR, "MoveWindow failed! GetLastError=%i", 
					GetLastError());
			const BOOL successSetCursorPos = 
				SetCursorPos(
					g_moveSizeStartMouseScreen.x, g_moveSizeStartMouseScreen.y);
			if(!successSetCursorPos)
				KLOG(ERROR, "SetCursorPos failed! GetLastError=%i", 
					GetLastError());
			korl_w32_window_setMoveSizeMode(KorlWin32MoveSizeMode::OFF, hWnd);
			} break;
		case VK_RETURN: {
			korl_w32_window_setMoveSizeMode(KorlWin32MoveSizeMode::OFF, hWnd);
			} break;
		}
	}
	if(g_moveSizeMode == KorlWin32MoveSizeMode::SIZE_KEYBOARD)
	{
		/* get the current cursor position & cache a copy, in case we need 
			to change it as a result of the arrow key presses */
		POINT cursorPosition;
		const BOOL successGetCursorPosition = GetCursorPos(&cursorPosition);
		if(!successGetCursorPosition)
			KLOG(ERROR, "GetCursorPosition failed! GetLastError=%i", 
				GetLastError());
		const POINT cursorPositionOriginal = cursorPosition;
		/* get the window's screen RECT for later */
		RECT windowRect;
		const BOOL successGetWindowRect = GetWindowRect(hWnd, &windowRect);
		if(!successGetWindowRect)
			KLOG(ERROR, "GetWindowRect failed! GetLastError=%i", 
				GetLastError());
		/* manual non-modal handling of the keyboard resize mode... 
			thanks,  microsoft */
		local_const LONG RESIZE_KEYPRESS_DELTA = 8;
		const bool isSideAxisSelectedX = 
			(   (g_moveSizeSides & KORL_W32_SIZE_LEFT) 
			 || (g_moveSizeSides & KORL_W32_SIZE_RIGHT));
		const bool isSideAxisSelectedY = 
			(   (g_moveSizeSides & KORL_W32_SIZE_TOP) 
			 || (g_moveSizeSides & KORL_W32_SIZE_BOTTOM));
		const u8 moveSizeSidesOriginal = g_moveSizeSides;
		switch(wParam)
		{
		case VK_LEFT: {
			if(isSideAxisSelectedX)
				cursorPosition.x -= RESIZE_KEYPRESS_DELTA;
			else
			{
				g_moveSizeSides |= KORL_W32_SIZE_LEFT;
				cursorPosition.x = windowRect.left;
			}
			} break;
		case VK_UP: {
			if(isSideAxisSelectedY)
				cursorPosition.y -= RESIZE_KEYPRESS_DELTA;
			else
			{
				g_moveSizeSides |= KORL_W32_SIZE_TOP;
				cursorPosition.y = windowRect.top;
			}
			} break;
		case VK_RIGHT: {
			if(isSideAxisSelectedX)
				cursorPosition.x += RESIZE_KEYPRESS_DELTA;
			else
			{
				g_moveSizeSides |= KORL_W32_SIZE_RIGHT;
				cursorPosition.x = windowRect.right - 1;
			}
			} break;
		case VK_DOWN: {
			if(isSideAxisSelectedY)
				cursorPosition.y += RESIZE_KEYPRESS_DELTA;
			else
			{
				g_moveSizeSides |= KORL_W32_SIZE_BOTTOM;
				cursorPosition.y = windowRect.bottom - 1;
			}
			} break;
		}
		/* set the new cursor position if we received certain keyboard 
			inputs while in certain states */
		if(   cursorPosition.x != cursorPositionOriginal.x 
		   || cursorPosition.y != cursorPositionOriginal.y)
		{
			/* update the cursor icon depending on which sides flags we're 
				using */
			if(g_moveSizeSides & KORL_W32_SIZE_TOP)
			{
				if(g_moveSizeSides & KORL_W32_SIZE_LEFT)
					SetCursor(g_cursorSizeNwSe);
				else if(g_moveSizeSides & KORL_W32_SIZE_RIGHT)
					SetCursor(g_cursorSizeNeSw);
				else
					SetCursor(g_cursorSizeVertical);
			}
			else if(g_moveSizeSides & KORL_W32_SIZE_BOTTOM)
			{
				if(g_moveSizeSides & KORL_W32_SIZE_LEFT)
					SetCursor(g_cursorSizeNeSw);
				else if(g_moveSizeSides & KORL_W32_SIZE_RIGHT)
					SetCursor(g_cursorSizeNwSe);
				else
					SetCursor(g_cursorSizeVertical);
			}
			else
				SetCursor(g_cursorSizeHorizontal);
			/* set the cursor position for the relevant axes defined 
				above */
			if(moveSizeSidesOriginal != g_moveSizeSides)
				g_moveSizeLastMouseScreen = cursorPosition;
			const BOOL successSetCursorPos = 
				SetCursorPos(cursorPosition.x, cursorPosition.y);
			if(!successSetCursorPos)
				KLOG(ERROR, "SetCursorPos failed! GetLastError=%i", 
					GetLastError());
		}
	}
	return KORL_W32_WNDPROC_RESULT_USE_DEFAULT;
}
internal Korl_W32_Window_WndProcResult korl_w32_window_wndProc_mouseMove(
	HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	const POINT mouseClientPosition = 
		{ .x = GET_X_LPARAM(lParam)
		, .y = GET_Y_LPARAM(lParam) };
	if(g_moveSizeMode == KorlWin32MoveSizeMode::MOVE_MOUSE)
	{
		/* if the mouse client position has changed, we need to move the 
			window such that it maintains the same position */
		if(   mouseClientPosition.x != g_moveSizeStartMouseClient.x 
		   || mouseClientPosition.y != g_moveSizeStartMouseClient.y)
		{
			/* if the window is maximized, restore the window first */
			WINDOWPLACEMENT windowPlacement = {};
			const BOOL successGetWindowPlacement = 
				GetWindowPlacement(hWnd, &windowPlacement);
			if(!successGetWindowPlacement)
				KLOG(ERROR, "GetWindowPlacement failed! GetLastError=%i", 
					GetLastError());
			if(windowPlacement.showCmd == SW_SHOWMAXIMIZED)
			{
				windowPlacement.showCmd = SW_SHOWNORMAL;
				const BOOL successSetWindowPlacement = 
					SetWindowPlacement(hWnd, &windowPlacement);
				if(!successSetWindowPlacement)
					KLOG(ERROR, "SetWindowPlacement failed! "
						"GetLastError=%i", GetLastError());
				/* ensure that the window movement of the restored window 
					will continue to appear to be "bound" to the mouse 
					cursor by adjusting g_moveSizeStartMouseClient with 
					respect to the new window size */
				// get the new window rect //
				RECT windowRect;
				const BOOL successGetWindowRect = 
					GetWindowRect(hWnd, &windowRect);
				if(!successGetWindowRect)
					KLOG(ERROR, "GetWindowRect failed! GetLastError=%i", 
						GetLastError());
				// convert g_moveSizeStartMouseClient to screen-space //
				POINT moveSizeStartMouse = g_moveSizeStartMouseClient;
				korlAssert(ClientToScreen(hWnd, &moveSizeStartMouse));
				// offset the windowRect based on system GUI metrics //
				/* at first I attempted to use `GetSystemMetricsForDpi` API 
					to determine the metrics of the buttons in the caption 
					bar (https://i.stack.imgur.com/TR01p.png), but that API 
					kept giving me bogus values for some reason on Win10 */
				TITLEBARINFOEX titleBarInfoEx = {sizeof(TITLEBARINFOEX), 0};
				const LRESULT resultSendMsgTitleBarInfoEx = 
					SendMessage(
						hWnd, WM_GETTITLEBARINFOEX, NULL, 
						reinterpret_cast<LPARAM>(&titleBarInfoEx));
				/* I'm not actually sure what the result value should be... 
					MSDN is not very helpful 😑 */
				local_const u32 TITLE_BAR_INFO_EX_BUTTON_INDEX_MINIMIZE = 2;
				windowRect.right = 
					titleBarInfoEx.rgrect[
						TITLE_BAR_INFO_EX_BUTTON_INDEX_MINIMIZE].left - 1;
				// clamp this value to the new window rect //
				if(moveSizeStartMouse.x < windowRect.left)
					moveSizeStartMouse.x = windowRect.left;
				if(moveSizeStartMouse.x >= windowRect.right)
					moveSizeStartMouse.x = windowRect.right - 1;
				if(moveSizeStartMouse.y < windowRect.top)
					moveSizeStartMouse.y = windowRect.top;
				if(moveSizeStartMouse.y >= windowRect.bottom)
					moveSizeStartMouse.y = windowRect.bottom - 1;
				// convert this clamped value back to client-space //
				korlAssert(ScreenToClient(hWnd, &moveSizeStartMouse));
				// save this as the new g_moveSizeStartMouseClient //
				g_moveSizeStartMouseClient = moveSizeStartMouse;
			}
			/* move the window to satisfy the original mouseClientPosition 
				constraint */
			RECT windowRect;
			const BOOL successGetWindowRect = 
				GetWindowRect(hWnd, &windowRect);
			if(!successGetWindowRect)
				KLOG(ERROR, "GetWindowRect failed! GetLastError=%i", 
					GetLastError());
			const POINT mouseClientOldToNew = 
				{ mouseClientPosition.x - g_moveSizeStartMouseClient.x
				, mouseClientPosition.y - g_moveSizeStartMouseClient.y };
			const POINT windowSize = 
				{ windowRect.right  - windowRect.left
				, windowRect.bottom - windowRect.top };
			const BOOL successMoveWindow = 
				MoveWindow(
					hWnd, 
					windowRect.left + mouseClientOldToNew.x, 
					windowRect.top  + mouseClientOldToNew.y, 
					windowSize.x, windowSize.y, 
					/* repaint? */FALSE);
			if(!successMoveWindow)
				KLOG(ERROR, "MoveWindow failed! GetLastError=%i", 
					GetLastError());
		}
	}
	return KORL_W32_WNDPROC_RESULT_USE_DEFAULT;
}
internal Korl_W32_Window_WndProcResult korl_w32_window_wndProc_captureChanged(
	HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	korl_w32_window_setMoveSizeMode(KorlWin32MoveSizeMode::OFF, hWnd);
	return KORL_W32_WNDPROC_RESULT_USE_DEFAULT;
}
internal void korl_w32_window_stepMoveSize(
	const f32 deltaSeconds, const GameMouse*const mouse, 
	const GameKeyboard*const keyboard)
{
	switch(g_moveSizeMode)
	{
	case KorlWin32MoveSizeMode::MOVE_KEYBOARD: {
		v2f32 moveDirection = v2f32::ZERO;
		if(KORL_BUTTON_ON(keyboard->arrowLeft))
			moveDirection.x -= 1;
		if(KORL_BUTTON_ON(keyboard->arrowRight))
			moveDirection.x += 1;
		if(KORL_BUTTON_ON(keyboard->arrowUp))
			moveDirection.y -= 1;
		if(KORL_BUTTON_ON(keyboard->arrowDown))
			moveDirection.y += 1;
		/* decelerate towards 0 speed if we're not trying to move */
		local_const f32 DECEL = 1000.f;
		if(moveDirection.isNearlyZero())
		{
			const v2f32 decelDir = kmath::normal(-g_moveSizeKeyVelocity);
			if(!g_moveSizeKeyVelocity.isNearlyZero())
				g_moveSizeKeyVelocity += deltaSeconds*DECEL*decelDir;
			/* if we have decelerated too much or we're close enough to 0 speed, 
				just zero out the velocity */
			if(g_moveSizeKeyVelocity.dot(decelDir) > 0 
					|| g_moveSizeKeyVelocity.isNearlyZero())
				g_moveSizeKeyVelocity = v2f32::ZERO;
		}
		/* otherwise accelerate towards arrow key direction */
		else
		{
			local_const f32 ACCEL = 100.f;
			g_moveSizeKeyVelocity += deltaSeconds*ACCEL*moveDirection;
			/* decelerate in the direction perpendicular to `moveDirection` */
			{
				/* split the velocity into moveDirection & perpendicular 
					components */
				v2f32 velCompMoveDir = 
					g_moveSizeKeyVelocity.projectOnto(moveDirection);
				v2f32 velCompMoveDirPerp = 
					g_moveSizeKeyVelocity - velCompMoveDir;
				/* decelerate perpendicular component towards zero */
				if(velCompMoveDirPerp.isNearlyZero())
					velCompMoveDirPerp = v2f32::ZERO;
				else
				{
					const v2f32 decelDir = kmath::normal(-velCompMoveDirPerp);
					velCompMoveDirPerp += deltaSeconds*DECEL*decelDir;
					if(velCompMoveDirPerp.dot(decelDir) >= 0)
						velCompMoveDirPerp = v2f32::ZERO;
				}
				/* while we're at it, ensure that the `moveDirection` component 
					of velocity is never flowing AWAY from `moveDirection` */
				if(velCompMoveDir.dot(moveDirection) < 0)
					velCompMoveDir += deltaSeconds*DECEL*moveDirection;
				/* recombine the velocity components */
				g_moveSizeKeyVelocity = velCompMoveDir + velCompMoveDirPerp;
			}
			/* enforce maximum move speed */
			f32 speed = g_moveSizeKeyVelocity.normalize();
			local_const f32 MAX_SPEED = 100;
			if(speed > MAX_SPEED)
				speed = MAX_SPEED;
			g_moveSizeKeyVelocity *= speed;
		}
		/* if movement velocity is non-zero, move the mouse cursor, which in 
			turn determines where the center of the window's caption bar goes */
		if(!g_moveSizeKeyVelocity.isNearlyZero())
		{
			POINT cursorPosition;
			/* if this is the first time we moved with keyboard controls, we 
				have to reposition the cursor to be the center of the window's 
				caption bar again since the user may have moved the mouse since 
				activating the move mode */
			if(!g_moveSizeKeyMoved)
			{
				korl_w32_window_centerCursorOnCaptionBar(g_mainWindow);
				g_moveSizeKeyMoved = true;
			}
			/* move the cursor around relative to its previous position */
			const BOOL successGetCursorPosition = GetCursorPos(&cursorPosition);
			if(!successGetCursorPosition)
				KLOG(ERROR, "GetCursorPosition failed! GetLastError=%i", 
					GetLastError());
			const LONG dX = static_cast<LONG>(g_moveSizeKeyVelocity.x);
			const LONG dY = static_cast<LONG>(g_moveSizeKeyVelocity.y);
			cursorPosition.x += dX;
			cursorPosition.y += dY;
			const BOOL successSetCursorPosition = 
				SetCursorPos(cursorPosition.x, cursorPosition.y);
			if(!successSetCursorPosition)
				KLOG(ERROR, "SetCursorPos failed! GetLastError=%i", 
					GetLastError());
		}
		/* we need to check here if the mouse cursor has ever moved, because 
			even if we call SetCapture, we wont actually get any mouse move 
			events if the user isn't holding a mouse button apparently... */
		POINT cursorPositionScreen;
		const BOOL successGetCursorPos = 
			GetCursorPos(&cursorPositionScreen);
		if(!successGetCursorPos)
			KLOG(ERROR, "GetCursorPos failed! GetLastError=%i", GetLastError());
		if((   cursorPositionScreen.x != g_moveSizeLastMouseScreen.x 
		    || cursorPositionScreen.y != g_moveSizeLastMouseScreen.y) 
		   && g_moveSizeKeyMoved)
		{
			g_moveSizeLastMouseScreen = cursorPositionScreen;
			/* move the window such that the center of the title bar is directly 
				under the mouse cursor */
			RECT windowRect;
			const bool successGetWindowRect = 
				GetWindowRect(g_mainWindow, &windowRect);
			if(!successGetWindowRect)
				KLOG(ERROR, "GetWindowRect failed! GetLastError=%i", 
					GetLastError());
			const POINT windowTitleCenter = 
				korl_w32_window_findCaptionBarCenter(g_mainWindow);
			const POINT titleCenterOffset = 
				{ windowTitleCenter.x - windowRect.left
				, windowTitleCenter.y - windowRect.top };
			const BOOL successMoveWindow = 
				MoveWindow(
					g_mainWindow, 
					cursorPositionScreen.x - titleCenterOffset.x, 
					cursorPositionScreen.y - titleCenterOffset.y, 
					windowRect.right  - windowRect.left, 
					windowRect.bottom - windowRect.top, 
					/* repaint? */FALSE);
			if(!successMoveWindow)
				KLOG(ERROR, "MoveWindow failed! GetLastError=%i", 
					GetLastError());
		}
		} break;
	case KorlWin32MoveSizeMode::SIZE_MOUSE: 
	case KorlWin32MoveSizeMode::SIZE_KEYBOARD: {
		/* we need to check here if the mouse cursor has ever moved, because 
			even if we call SetCapture, we wont actually get any mouse move 
			events if the user isn't holding a mouse button apparently... */
		POINT cursorPositionScreen;
		const BOOL successGetCursorPos = 
			GetCursorPos(&cursorPositionScreen);
		if(!successGetCursorPos)
			KLOG(ERROR, "GetCursorPos failed! GetLastError=%i", GetLastError());
		/* if the mouse position has moved since the last known position, update 
			the window's size based on the selected side(s) */
		if((   cursorPositionScreen.x != g_moveSizeLastMouseScreen.x 
		    || cursorPositionScreen.y != g_moveSizeLastMouseScreen.y) 
		   /* at least one side has been selected by keyboard input */
		   && g_moveSizeSides)
		{
			g_moveSizeLastMouseScreen = cursorPositionScreen;
			/* get the window screen-space RECT */
			RECT windowRect;
			const bool successGetWindowRect = 
				GetWindowRect(g_mainWindow, &windowRect);
			if(!successGetWindowRect)
				KLOG(ERROR, "GetWindowRect failed! GetLastError=%i", 
					GetLastError());
			/* modify the window RECT based on which sides are selected */
			if(g_moveSizeSides & KORL_W32_SIZE_LEFT)
				windowRect.left = cursorPositionScreen.x;
			if(g_moveSizeSides & KORL_W32_SIZE_TOP)
				windowRect.top = cursorPositionScreen.y;
			if(g_moveSizeSides & KORL_W32_SIZE_RIGHT)
				windowRect.right = cursorPositionScreen.x + 1;
			if(g_moveSizeSides & KORL_W32_SIZE_BOTTOM)
				windowRect.bottom = cursorPositionScreen.y + 1;
			/* set the window RECT to the modified values */
			const BOOL successMoveWindow = 
				MoveWindow(
					g_mainWindow, 
					windowRect.left, windowRect.top, 
					windowRect.right  - windowRect.left, 
					windowRect.bottom - windowRect.top, 
					/* repaint? */TRUE);
			if(!successMoveWindow)
				KLOG(ERROR, "MoveWindow failed! GetLastError=%i", 
					GetLastError());
		}
		} break;
	default: {
		} break;
	}
}