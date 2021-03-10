#include "win32-main.h"
#include "win32-dsound.h"
#include "win32-xinput.h"
#include "win32-directinput.h"
#include "win32-krb-opengl.h"
#include "win32-network.h"
#include "win32-platform.h"
#include "win32-dynApp.h"
#include "win32-log.h"
#include "win32-time.h"
#include "win32-crash.h"
#include "win32-window.h"
#include "win32-input.h"
#include "kgtAllocator.h"
#include <cstdio>
#include <ctime>
#include <strsafe.h>
#include <ShlObj.h>
#include <Dbt.h>
#include <shellscalingapi.h>/* GetDpiForMonitor */
#include <windowsx.h>/* GET_X_LPARAM */
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
/* @todo: define this at the project shell level somewhere so it propagates to 
	vscode intellisense builds (low priority convenience) */
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM "korl-ogl-loader.h"
#include "imgui/imgui_impl_opengl3.h"
#include "stb/stb_image.h"
#include "stb/stb_vorbis.h"
global_const f32 MAX_GAME_DELTA_SECONDS = 1.f / KORL_MINIMUM_FRAME_RATE;
global_variable bool g_running;
global_variable bool g_isFocused;
/* this variable exists because mysterious COM incantations require us to check 
	whether or not a DirectInput device is Xinput compatible OUTSIDE of the 
	windows message loop */
global_variable bool g_deviceChangeDetected;
global_variable KgtAllocatorHandle g_genAllocStbImage;
global_variable KgtAllocatorHandle g_genAllocImgui;
global_variable CRITICAL_SECTION g_stbiAllocationCsLock;
global_variable GamePad* g_gamePadArrayCurrentFrame  = g_gamePadArrayA;
global_variable GamePad* g_gamePadArrayPreviousFrame = g_gamePadArrayB;
global_const u8 SOUND_CHANNELS = 2;
global_const u32 SOUND_SAMPLE_HZ = 44100;
global_const u32 SOUND_BYTES_PER_SAMPLE = sizeof(i16)*SOUND_CHANNELS;
global_const u32 SOUND_BUFFER_BYTES = 
	SOUND_SAMPLE_HZ/2 * SOUND_BYTES_PER_SAMPLE;
global_variable UINT g_dpi;
global_variable f32 g_dpiScaleFactor;
/* @TODO: make these memory quantities configurable per-project */
global_const size_t STATIC_MEMORY_ALLOC_SIZES[] = 
	{ kmath::megabytes(64)
	, SOUND_BUFFER_BYTES
	, kmath::megabytes(128)
	, kmath::megabytes(2)
	, kmath::megabytes(1)
	, kmath::megabytes(1)
	, kmath::megabytes(16) };
enum class StaticMemoryAllocationIndex : u8// sub-255 memory chunks please, god!
	{ GAME_PERMANENT
	, GAME_SOUND
	, GAME_TRANSIENT
	, STB_IMAGE
	, STB_VORBIS
	, IMGUI
	, RAW_FILES
	/* obviously, this value does not have a matching allocation size */
	, ENUM_SIZE };
struct W32ThreadInfo
{
	u32 index;
	JobQueue* jobQueue;
};
DWORD WINAPI w32WorkThread(_In_ LPVOID lpParameter)
{
	W32ThreadInfo*const threadInfo = 
		reinterpret_cast<W32ThreadInfo*>(lpParameter);
	KLOG(INFO, "thread[%i]: started!", threadInfo->index);
	while(true)
		if(!jobQueuePerformWork(threadInfo->jobQueue, threadInfo->index))
			jobQueueWaitForWork(threadInfo->jobQueue);
	return 0;
}
#if 0
#include <hidsdi.h>
internal bool w32RawInputEnumerateDevices()
{
	RAWINPUTDEVICELIST rawInputDeviceList[64];
	UINT rawInputDeviceBufferSize = CARRAY_SIZE(rawInputDeviceList);
	const UINT numDevices = 
		GetRawInputDeviceList(rawInputDeviceList, 
		                      &rawInputDeviceBufferSize, 
		                      sizeof(rawInputDeviceList[0]));
	if(numDevices == (UINT)-1)
	{
		KLOG(ERROR, "Failed to get RawInput device list! getlasterror=%i",
		     GetLastError());
		return false;
	}
	for(UINT rid = 0; rid < numDevices; rid++)
	{
		/* get the raw input device's registry address */
		TCHAR deviceNameBuffer[256];
		UINT deviceNameBufferSize = CARRAY_SIZE(deviceNameBuffer);
		const UINT deviceNameCharacterCount = 
			GetRawInputDeviceInfo(rawInputDeviceList[rid].hDevice,
			                      RIDI_DEVICENAME, deviceNameBuffer, 
			                      &deviceNameBufferSize);
		if(deviceNameCharacterCount < 0)
		{
			KLOG(ERROR, "Failed to get RawInput device name! getlasterror=%i", 
			     GetLastError());
			return false;
		}
		/* get the raw input device's info */
		RID_DEVICE_INFO rawInputDeviceInfo = {};
		rawInputDeviceInfo.cbSize = sizeof(rawInputDeviceInfo);
		UINT deviceInfoSize       = sizeof(rawInputDeviceInfo);
		const UINT deviceInfoByteCount = 
			GetRawInputDeviceInfo(rawInputDeviceList[rid].hDevice,
			                      RIDI_DEVICEINFO, &rawInputDeviceInfo, 
			                      &deviceInfoSize);
		if(deviceInfoByteCount != sizeof(rawInputDeviceInfo))
		{
			KLOG(ERROR, "Failed to get RawInput device info! getlasterror=%i", 
			     GetLastError());
			return false;
		}
		switch(rawInputDeviceInfo.dwType)
		{
			case RIM_TYPEMOUSE:
			case RIM_TYPEKEYBOARD:
			{
				/* if this is a mouse/keyboard, just ignore it */
				continue;
			}break;
			case RIM_TYPEHID:
			{
				/* only certain types of devices can be opened with shared 
					access mode.  If a device can't be opened with shared 
					access, then there isn't really a point in continuing since 
					we can't even query the device's product name.  For info on 
					what devices can be opened in shared access, see:
					https://docs.microsoft.com/en-us/windows-hardware/drivers/hid/hid-clients-supported-in-windows */
				if(!(rawInputDeviceInfo.hid.usUsagePage == 0x1
					&& (rawInputDeviceInfo.hid.usUsage == 0x4
						|| rawInputDeviceInfo.hid.usUsage == 0x5)))
				// if it's anything that isn't a game controller
				{
					/* just skip it because it's likely not allowed to be opened 
						in shared mode anyways */
					continue;
				}
				/* get the product name of the HID */
				HANDLE hHid = CreateFile(deviceNameBuffer, 
				                         GENERIC_READ | GENERIC_WRITE, 
				                         FILE_SHARE_READ | FILE_SHARE_WRITE, 
				                         NULL, OPEN_EXISTING, NULL, NULL);
				if(hHid == INVALID_HANDLE_VALUE)
				{
					KLOG(ERROR, "Failed to open handle to HID! getlasterror=%i", 
					     GetLastError());
					return false;
				}
				defer(CloseHandle(hHid));
				WCHAR deviceProductNameBuffer[256];
				if(!HidD_GetProductString(hHid, deviceProductNameBuffer, 
				                          sizeof(deviceProductNameBuffer)))
				{
					KLOG(ERROR, "Failed to get HID product string! "
					     "getlasterror=%i", GetLastError());
					return false;
				}
				KLOG(INFO, "--- RawInput device [%i] (Game Controller) ---", 
				     rid);
				KLOG(INFO, "\tdeviceName='%s'", deviceNameBuffer);
				KLOG(INFO, "\tproductString='%ws'", deviceProductNameBuffer);
			}break;
		}
	}
	return true;
}
#endif//0
internal u32 w32FindUnusedTempGameDllPostfix()
{
	TCHAR fileQueryString[MAX_PATH];
	StringCchPrintf(fileQueryString, MAX_PATH, TEXT("%s\\*.dll"), g_pathTemp);
	/* Attempt to delete all the DLLs in the `g_pathTemp` directory.  If another 
		game is currently running, the attempt to delete the file should fail 
		gracefully */
	WIN32_FIND_DATA findFileData;
	HANDLE findHandleTempGameDll = 
		FindFirstFile(fileQueryString, &findFileData);
	if(findHandleTempGameDll == INVALID_HANDLE_VALUE)
	{
		if(GetLastError() != ERROR_FILE_NOT_FOUND)
		/* ERROR CASE */
		{
			KLOG(ERROR, "Failed to begin search for '%s'!  GetLastError=%i",
			     fileQueryString, GetLastError());
			return 0;
		}
		/* otherwise, there are just no files to delete, so do nothing */
	}
	else
	{
		for(;;)
		{
			/* attempt to delete this file */
			TCHAR deleteFilePath[MAX_PATH];
			StringCchPrintf(deleteFilePath, MAX_PATH, TEXT("%s\\%s"), 
			                g_pathTemp, findFileData.cFileName);
			DeleteFile(deleteFilePath);
			/* go to the next file */
			const BOOL findNextFileResult = 
				FindNextFile(findHandleTempGameDll, &findFileData);
			if(!findNextFileResult)
			{
				if(GetLastError() == ERROR_NO_MORE_FILES)
				{
					break;
				}
				else
				{
					KLOG(ERROR, "Failed to find next for '%s'!  "
					     "GetLastError=%i", fileQueryString, GetLastError());
					return 0;
				}
			}
		}
		if(!FindClose(findHandleTempGameDll))
		{
			KLOG(ERROR, "Failed to close search for '%s'!  GetLastError=%i",
			     fileQueryString, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
	}
	/* count the # of game DLLs in the `g_pathTemp` directory */
	u32 tempGameDllCount = 1;
	findHandleTempGameDll = FindFirstFile(fileQueryString, &findFileData);
	if(findHandleTempGameDll == INVALID_HANDLE_VALUE)
	{
		if(GetLastError() == ERROR_FILE_NOT_FOUND)
		/* if there are no existing temp game DLLs, then we can just use 0 as the 
			first unused postfix */
		{
			return 0;
		}
		KLOG(ERROR, "Failed to begin search for '%s'!  GetLastError=%i",
		     fileQueryString, GetLastError());
		return 0;
	}
	for(;;)
	{
		const BOOL findNextFileResult = 
			FindNextFile(findHandleTempGameDll, &findFileData);
		if(!findNextFileResult)
		{
			if(GetLastError() == ERROR_NO_MORE_FILES)
			{
				break;
			}
			else
			{
				KLOG(ERROR, "Failed to find next for '%s'!  GetLastError=%i",
				     fileQueryString, GetLastError());
				return 0;
			}
		}
		tempGameDllCount++;
	}
	if(!FindClose(findHandleTempGameDll))
	{
		KLOG(ERROR, "Failed to close search for '%s'!  GetLastError=%i",
		     fileQueryString, GetLastError());
		return EXCEPTION_EXECUTE_HANDLER;
	}
	/* find the lowest unused postfix within the range [0, tempGameDllCount] */
	u32 lowestUnusedPostfix = 0;
	for(; lowestUnusedPostfix <= tempGameDllCount; lowestUnusedPostfix++)
	{
		bool postfixInUse = false;
		findHandleTempGameDll = FindFirstFile(fileQueryString, &findFileData);
		if(findHandleTempGameDll == INVALID_HANDLE_VALUE)
		{
			KLOG(ERROR, "Failed to begin search for '%s'!  GetLastError=%i",
			     fileQueryString, GetLastError());
			return 0;
		}
		for(;;)
		{
			/* extract the postfix from the file's name and check to see if it 
				== lowestUnusedPostfix, indicating the postfix is in use */
			const char* cStrPostfix = 
				strstr(findFileData.cFileName, "_temp_");
			const char*const cStrDll = 
				strstr(findFileData.cFileName, ".dll");
			korlAssert(cStrPostfix && cStrDll);
			cStrPostfix += 6;
			char cStrUlBuffer[32];
			const size_t filePostfixCharCount = 
				((cStrDll - cStrPostfix) / sizeof(cStrPostfix[0]));
			korlAssert(CARRAY_SIZE(cStrUlBuffer) >= filePostfixCharCount + 1);
			for(size_t c = 0; c < filePostfixCharCount; c++)
			{
				cStrUlBuffer[c] = cStrPostfix[c];
			}
			cStrUlBuffer[filePostfixCharCount] = '\0';
			errno = 0;
			const u32 extractedPostfix = strtoul(cStrUlBuffer, nullptr, 10);
			if(errno == ERANGE)
			{
				KLOG(ERROR, "Error reading cStrUlBuffer==\"%s\"", cStrUlBuffer);
			}
			if(extractedPostfix == lowestUnusedPostfix)
			{
				postfixInUse = true;
				break;
			}
			const BOOL findNextFileResult = 
				FindNextFile(findHandleTempGameDll, &findFileData);
			if(!findNextFileResult)
			{
				if(GetLastError() == ERROR_NO_MORE_FILES)
				{
					break;
				}
				else
				{
					KLOG(ERROR, "Failed to find next for '%s'!  "
						"GetLastError=%i", fileQueryString, GetLastError());
					return 0;
				}
			}
		}
		if(!FindClose(findHandleTempGameDll))
		{
			KLOG(ERROR, "Failed to close search for '%s'!  GetLastError=%i",
				fileQueryString, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		if(!postfixInUse)
			break;
	}
	return lowestUnusedPostfix;
}
internal LRESULT CALLBACK 
	w32MainWindowCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	/* @todo: this API design for handling window messages for subsystems is 
		kinda bad imo; consider refactoring the way ImGui windows implementation 
		does this or something (like what I'm about to do with 
		korl-w32-window) */
	result = ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
	switch(uMsg)
	{
	case WM_CREATE: {
		w32KrbOglInitialize(hwnd);
		w32KrbOglSetVSyncPreference(true);
		/* Get the handle to the nearest monitor during creation & obtain the 
			DPI so we can scale the application contents accordingly later. */
		const HMONITOR monitorNearest = 
			MonitorFromWindow(g_mainWindow, MONITOR_DEFAULTTONEAREST);
		UINT dpiX, dpiY;
		const HRESULT resultGetDpiForMonitor = 
			GetDpiForMonitor(monitorNearest, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
		// MSDN: "The values of *dpiX and *dpiY are identical."
		korlAssert(dpiX == dpiY);
		if(resultGetDpiForMonitor != S_OK)
			KLOG(ERROR, "GetDpiForMonitor failed!");
		g_dpi = dpiX;
		g_dpiScaleFactor = 
			static_cast<f32>(g_dpi) / static_cast<f32>(USER_DEFAULT_SCREEN_DPI);
		} break;
	case WM_DPICHANGED: {
		const UINT dpiNew = HIWORD(wParam);
		korlAssert(dpiNew == LOWORD(wParam));// MSDN says this MUST be true!
		KLOG(INFO, "WM_DPICHANGED: dpiNew=%u", dpiNew);
		/* save the new DPI & scale factor */
		g_dpi = dpiNew;
		g_dpiScaleFactor = 
			static_cast<f32>(g_dpi) / static_cast<f32>(USER_DEFAULT_SCREEN_DPI);
		/* resize the window in a way that doesn't disturb the resize/move 
			mode if it is currently active */
		/* @robustness: IMPORTANT!!!!!  It seems that due to rounding errors or 
			something, these dimensions are not guaranteed to be restored to 
			their original values.  If we want the window to be restored to the 
			original values at the default screen DPI, we need to do some 
			additional work here!  
			(surprise surprise, windows isn't good enough) */
		const LPRECT suggestedNewPositionSize = 
			reinterpret_cast<LPRECT>(lParam);
		const BOOL successMoveWindow = 
			MoveWindow(
				hwnd, 
				suggestedNewPositionSize->left, 
				suggestedNewPositionSize->top, 
				suggestedNewPositionSize->right - 
					suggestedNewPositionSize->left, 
				suggestedNewPositionSize->bottom - 
					suggestedNewPositionSize->top, 
				/* repaint? */TRUE);
		if(!successMoveWindow)
			KLOG(ERROR, "MoveWindow failed! GetLastError=%i", 
				GetLastError());
		} break;
	case WM_SYSCOMMAND: {
		/* syscommand wparam values use the first 4 low-order bits internally, 
			so we have to mask them out to get the correct values (see MSDN) */
		const WPARAM wParamSysCommand = wParam & 0xFFF0;
		KLOG(INFO, "WM_SYSCOMMAND: wParam=0x%x wParamSysCommand=0x%x", 
			wParam, wParamSysCommand);
		/* pass message to the win32-window module for further processing */
		const Korl_W32_Window_WndProcResult resultWindowSysCommand = 
			korl_w32_window_wndProc_sysCommand(hwnd, uMsg, wParam, lParam);
		if(resultWindowSysCommand.callerActionRequired == 
				Korl_W32_Window_WndProcResult::CallerAction::NONE)
			result = resultWindowSysCommand.lResultOverride;
		else
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_LBUTTONDOWN: {
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "WM_LBUTTONDOWN");
#endif// KORL_W32_VERBOSE_EVENT_LOG
		/* pass message to the win32-window module for further processing */
		const Korl_W32_Window_WndProcResult resultWindowLButtonDown = 
			korl_w32_window_wndProc_lButtonDown(hwnd, uMsg, wParam, lParam);
		if(resultWindowLButtonDown.callerActionRequired == 
				Korl_W32_Window_WndProcResult::CallerAction::NONE)
			result = resultWindowLButtonDown.lResultOverride;
		else
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_NCLBUTTONDOWN: {
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "WM_NCLBUTTONDOWN");
#endif// KORL_W32_VERBOSE_EVENT_LOG
		/* pass message to the win32-window module for further processing */
		const Korl_W32_Window_WndProcResult resultWindowNclButtonDown = 
			korl_w32_window_wndProc_nclButtonDown(hwnd, uMsg, wParam, lParam);
		if(resultWindowNclButtonDown.callerActionRequired == 
				Korl_W32_Window_WndProcResult::CallerAction::NONE)
			result = resultWindowNclButtonDown.lResultOverride;
		else
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_NCLBUTTONUP: {
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "WM_NCLBUTTONUP");
#endif// KORL_W32_VERBOSE_EVENT_LOG
		/* pass message to the win32-window module for further processing */
		const Korl_W32_Window_WndProcResult resultWindowNclButtonUp = 
			korl_w32_window_wndProc_nclButtonUp(hwnd, uMsg, wParam, lParam);
		if(resultWindowNclButtonUp.callerActionRequired == 
				Korl_W32_Window_WndProcResult::CallerAction::NONE)
			result = resultWindowNclButtonUp.lResultOverride;
		else
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_LBUTTONUP: {
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "WM_LBUTTONUP");
#endif// KORL_W32_VERBOSE_EVENT_LOG
		/* pass message to the win32-window module for further processing */
		const Korl_W32_Window_WndProcResult resultWindowLButtonUp = 
			korl_w32_window_wndProc_lButtonUp(hwnd, uMsg, wParam, lParam);
		if(resultWindowLButtonUp.callerActionRequired == 
				Korl_W32_Window_WndProcResult::CallerAction::NONE)
			result = resultWindowLButtonUp.lResultOverride;
		else
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_KEYDOWN: {
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "WM_KEYDOWN: vKey=%i", wParam);
#endif// KORL_W32_VERBOSE_EVENT_LOG
		/* pass message to the win32-window module for further processing */
		const Korl_W32_Window_WndProcResult resultWindowKeyDown = 
			korl_w32_window_wndProc_keyDown(hwnd, uMsg, wParam, lParam);
		if(resultWindowKeyDown.callerActionRequired == 
				Korl_W32_Window_WndProcResult::CallerAction::NONE)
			result = resultWindowKeyDown.lResultOverride;
		else
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_MOUSEMOVE: {
		const POINT mouseClientPosition = 
			{ .x = GET_X_LPARAM(lParam)
			, .y = GET_Y_LPARAM(lParam) };
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "WM_MOUSEMOVE: clientPosition={%i, %i}", 
			mouseClientPosition.x, mouseClientPosition.y);
#endif// KORL_W32_VERBOSE_EVENT_LOG
		/* pass message to the win32-window module for further processing */
		const Korl_W32_Window_WndProcResult resultWindowMouseMove = 
			korl_w32_window_wndProc_mouseMove(hwnd, uMsg, wParam, lParam);
		if(resultWindowMouseMove.callerActionRequired == 
				Korl_W32_Window_WndProcResult::CallerAction::NONE)
			result = resultWindowMouseMove.lResultOverride;
		else
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_CAPTURECHANGED: {/* sent to window that LOSES mouse capture! */
		KLOG(INFO, "WM_CAPTURECHANGED");
		/* pass message to the win32-window module for further processing */
		const Korl_W32_Window_WndProcResult resultWindowCaptureChanged = 
			korl_w32_window_wndProc_captureChanged(hwnd, uMsg, wParam, lParam);
		if(resultWindowCaptureChanged.callerActionRequired == 
				Korl_W32_Window_WndProcResult::CallerAction::NONE)
			result = resultWindowCaptureChanged.lResultOverride;
		else
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_ENTERSIZEMOVE: {
		KLOG(INFO, "WM_ENTERSIZEMOVE");
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_EXITSIZEMOVE: {
		KLOG(INFO, "WM_EXITSIZEMOVE");
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_SETCURSOR: {
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "WM_SETCURSOR");
#endif// KORL_W32_VERBOSE_EVENT_LOG
		HCURSOR cursor = NULL;
		switch(LOWORD(lParam))
		{
		case HTBOTTOM: 
		case HTTOP: {
			cursor = g_cursorSizeVertical; } break;
		case HTLEFT: 
		case HTRIGHT: {
			cursor = g_cursorSizeHorizontal; } break;
		case HTBOTTOMLEFT: 
		case HTTOPRIGHT: {
			cursor = g_cursorSizeNeSw; } break;
		case HTBOTTOMRIGHT: 
		case HTTOPLEFT: {
			cursor = g_cursorSizeNwSe; } break;
		case HTCLIENT: 
		default: {
			ImGuiIO& imguiIo = ImGui::GetIO();
			if(imguiIo.MouseDrawCursor)
				break;
			ImGuiMouseCursor imguiCursor = ImGui::GetMouseCursor();
			switch (imguiCursor)
			{
			case ImGuiMouseCursor_TextInput:  cursor = g_cursorTextBeam; break;
			case ImGuiMouseCursor_ResizeAll:  cursor = g_cursorSizeAll; break;
			case ImGuiMouseCursor_ResizeEW: 
				cursor = g_cursorSizeHorizontal; break;
			case ImGuiMouseCursor_ResizeNS: 
				cursor = g_cursorSizeVertical; break;
			case ImGuiMouseCursor_ResizeNESW: cursor = g_cursorSizeNeSw; break;
			case ImGuiMouseCursor_ResizeNWSE: cursor = g_cursorSizeNwSe; break;
			case ImGuiMouseCursor_Hand:       cursor = g_cursorHand; break;
			case ImGuiMouseCursor_NotAllowed: cursor = g_cursorNo; break;
			default: 
			case ImGuiMouseCursor_None: 
			case ImGuiMouseCursor_Arrow: {
				cursor = g_cursorArrow;
				} break;
			}
			} break;
		}
		HCURSOR currentCursor = GetCursor();
		if(cursor != currentCursor)
			SetCursor(cursor);
		} break;
	case WM_MOVE: {
		const POINT clientAreaScreenPosition = 
			{ .x = GET_X_LPARAM(lParam)
			, .y = GET_Y_LPARAM(lParam) };
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "WM_MOVE: clientAreaScreenPosition={%i, %i}", 
			clientAreaScreenPosition.x, clientAreaScreenPosition.y);
#endif// KORL_W32_VERBOSE_EVENT_LOG
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_SIZE: {
		KLOG(INFO, "WM_SIZE: type=%i area={%i,%i}", 
			wParam, LOWORD(lParam), HIWORD(lParam));
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_DESTROY: {
		g_running = false;
		KLOG(INFO, "WM_DESTROY");
		} break;
	case WM_CLOSE: {
		///TODO: ask dynamic application module first before destroying
		g_running = false;
		KLOG(INFO, "WM_CLOSE");
		} break;
	case WM_ACTIVATEAPP: {
		g_isFocused = wParam;
		KLOG(INFO, "WM_ACTIVATEAPP: activated=%s threadId=%i",
			(wParam ? "TRUE" : "FALSE"), lParam);
		if(!g_isFocused)
			korl_w32_window_setMoveSizeMode(KorlWin32MoveSizeMode::OFF, hwnd);
		} break;
	case WM_DEVICECHANGE: {
		KLOG(INFO, "WM_DEVICECHANGE: event=0x%x", wParam);
		switch(wParam)
		{
		case DBT_DEVNODES_CHANGED: {
			KLOG(INFO, "\tA device has been added or removed!");
			g_deviceChangeDetected = true;
			} break;
		}
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	case WM_NCMOUSEMOVE: {
		const POINT mouseScreenPosition = 
			{ .x = GET_X_LPARAM(lParam)
			, .y = GET_Y_LPARAM(lParam) };
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "WM_NCMOUSEMOVE: screenPosition={%i, %i}", 
			mouseScreenPosition.x, mouseScreenPosition.y);
#endif// KORL_W32_VERBOSE_EVENT_LOG
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
	default: {
#if KORL_W32_VERBOSE_EVENT_LOG
		KLOG(INFO, "Window Message uMsg==0x%04x", uMsg);
#endif //KORL_W32_VERBOSE_EVENT_LOG
		result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		}break;
	}/* switch(uMsg) */
	return result;
}
extern int WINAPI 
	wWinMain(
		HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, 
		PWSTR /*pCmdLine*/, int /*nCmdShow*/)
{
	local_persist const int RETURN_CODE_SUCCESS = 0;
	///TODO: OR the failure code with a more specific # to indicate why it 
	///      failed probably?
	local_persist const int RETURN_CODE_FAILURE = 0xBADC0DE0;
	/* initialize global assert & log function pointers */
	korlPlatformAssertFailure = w32PlatformAssertFailure;
	platformLog               = w32PlatformLog;
	// Initialize locks to ensure thread safety for various systems //
	{
		InitializeCriticalSection(&g_logCsLock);
		InitializeCriticalSection(&g_assetAllocationCsLock);
		InitializeCriticalSection(&g_vorbisAllocationCsLock);
		InitializeCriticalSection(&g_stbiAllocationCsLock);
		InitializeCriticalSection(&g_csLockAllocatorRawFiles);
		for(size_t l = 0; l < CARRAY_SIZE(g_platformExposedLocks); l++)
		{
			g_platformExposedLocks[l].allocated = false;
			InitializeCriticalSection(&g_platformExposedLocks[l].csLock);
		}
	}
	// Reserve stack space for stack overflow exceptions //
	{
		// Experimentally, this is about the minimum # of reserved stack bytes
		//	required to carry out my debug dump/log routines when a stack 
		//	overflow exception occurs.
		local_persist const ULONG DESIRED_RESERVED_STACK_BYTES = 
			static_cast<ULONG>(kmath::kilobytes(16));
		ULONG stackSizeInBytes = DESIRED_RESERVED_STACK_BYTES;
		if(SetThreadStackGuarantee(&stackSizeInBytes))
		{
			KLOG(INFO, "Previous reserved stack=%ld, new reserved stack=%ld", 
			     stackSizeInBytes, DESIRED_RESERVED_STACK_BYTES);
		}
		else
		{
			KLOG(WARNING, "Failed to set & retrieve reserved stack size!  "
			     "The system probably won't be able to log stack "
			     "overflow exceptions.");
		}
	}
#if 0
	if(SetUnhandledExceptionFilter(w32TopLevelExceptionHandler))
	{
		KLOG(INFO, "Replaced top level exception filter!");
	}
	else
	{
		KLOG(INFO, "Set new top level exception filter!");
	}
#endif
	if(!AddVectoredExceptionHandler(1, w32VectoredExceptionHandler))
	{
		KLOG(WARNING, "Failed to add vectored exception handler!");
	}
	defer(w32WriteLogToFile());
	KLOG(INFO, "START!");
	/* @todo: do I REALLY need to call LSMCleanup?  I'm going to guess no... */
	if(!w32InitializeNetwork())
	{
		KLOG(ERROR, "Failed to initialize network!");
	}
#if 0
	if(!w32RawInputEnumerateDevices())
	{
		KLOG(ERROR, "Failed to enumerate raw input devices!");
		return RETURN_CODE_FAILURE;
	}
#endif// 0
	// parse command line arguments //
	{
		int argc;
		LPWSTR*const argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if(argv)
		{
			for(int arg = 0; arg < argc; arg++)
			{
				KLOG(INFO, "arg[%i]=='%ws'", arg, argv[arg]);
				if(arg == 1)
				{
					/* @todo: make this less CRYPTIC */
					g_echoLogToDebug = (wcscmp(argv[arg], L"0") != 0);
				}
			}
			LocalFree(argv);
		}
		else
		{
			KLOG(ERROR, "CommandLineToArgvW failed!");
		}
	}
	if(!jobQueueInit(&g_jobQueue))
	{
		KLOG(ERROR, "Failed to initialize job queue!");
		return RETURN_CODE_FAILURE;
	}
	// Spawn worker threads for g_jobQueue //
	W32ThreadInfo threadInfos[7];
	for(u32 threadIndex = 0; 
		threadIndex < sizeof(threadInfos)/sizeof(threadInfos[0]); 
		threadIndex++)
	{
		threadInfos[threadIndex].index    = threadIndex;
		threadInfos[threadIndex].jobQueue = &g_jobQueue;
		DWORD threadId;
		const HANDLE hThread = 
			CreateThread(nullptr, 0, w32WorkThread, 
			             &threadInfos[threadIndex], 0, &threadId);
		if(!hThread)
		{
			KLOG(ERROR, "Failed to create thread! getlasterror=%i", 
			     GetLastError());
			return RETURN_CODE_FAILURE;
		}
		if(!CloseHandle(hThread))
		{
			KLOG(ERROR, "Failed to close thread handle! getlasterror=%i", 
			     GetLastError());
			return RETURN_CODE_FAILURE;
		}
	}
	// Obtain and save a global copy of the app data folder path //
	//	Source: https://stackoverflow.com/a/2899042
	{
		TCHAR szPath[MAX_PATH];
		{
			const HRESULT result = 
				SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 
				                SHGFP_TYPE_CURRENT, szPath);
			if(result != S_OK)
			{
				KLOG(ERROR, "Failed to get APPDATA path! result=%ld", result);
				return RETURN_CODE_FAILURE;
			}
		}
		if(FAILED(StringCchPrintf(g_pathLocalAppData, MAX_PATH, TEXT("%s\\%s"), 
		                          szPath, APPLICATION_NAME)))
		{
			KLOG(ERROR, "Failed to build application APPDATA path!");
			return RETURN_CODE_FAILURE;
		}
		KLOG(INFO, "g_pathLocalAppData='%s'", g_pathLocalAppData);
		if(!CreateDirectory(g_pathLocalAppData, NULL))
		{
			const int errorCode = GetLastError();
			switch(errorCode)
			{
				case ERROR_ALREADY_EXISTS:
				{
					KLOG(INFO, "Application APPDATA path '%s' already exists.", 
					     g_pathLocalAppData);
				} break;
				default:
				{
					KLOG(ERROR, "Failed to create APPDATA path '%s'! "
					     "GetLastError=%i", 
					     g_pathLocalAppData, errorCode);
					return RETURN_CODE_FAILURE;
				} break;
			}
		}
	}
	// Obtain and save a global copy of the app temp data folder path //
	{
		TCHAR szPath[MAX_PATH];
		if(!GetTempPath(MAX_PATH, szPath))
		{
			KLOG(ERROR, "Failed to get OS temp path! GetLastError=%i", 
			     GetLastError());
			return RETURN_CODE_FAILURE;
		}
		if(FAILED(StringCchPrintf(g_pathTemp, MAX_PATH, TEXT("%s%s"), szPath, 
		                          APPLICATION_NAME)))
		{
			KLOG(ERROR, "Failed to build application temp path!");
			return RETURN_CODE_FAILURE;
		}
		KLOG(INFO, "g_pathTemp='%s'", g_pathTemp);
		if(!CreateDirectory(g_pathTemp, NULL))
		{
			const int errorCode = GetLastError();
			switch(errorCode)
			{
				case ERROR_ALREADY_EXISTS:
				{
					KLOG(INFO, "Application temp path '%s' already exists.", 
					     g_pathTemp);
				} break;
				default:
				{
					KLOG(ERROR, "Failed to create app temp path '%s'! "
					     "GetLastError=%i", g_pathTemp, errorCode);
					return RETURN_CODE_FAILURE;
				} break;
			}
		}
	}
	// Locate where our exe file is on the OS.  This should also be the location
	//	where all our application's code modules live! //
	{
		if(!GetModuleFileNameA(NULL, g_pathToExe, MAX_PATH))
		{
			KLOG(ERROR, "Failed to get exe file path+name! GetLastError=%i", 
			     GetLastError());
			return RETURN_CODE_FAILURE;
		}
		char* lastBackslash = g_pathToExe;
		for(char* c = g_pathToExe; *c; c++)
		{
			if(*c == '\\')
			{
				lastBackslash = c;
			}
		}
		*(lastBackslash + 1) = 0;
	}
	/* obtain the current working directory from the operating system */
	{
		const DWORD bytesWrittenGetCurrDir = 
			GetCurrentDirectory(MAX_PATH, g_pathCurrentDirectory);
		if(bytesWrittenGetCurrDir < 1)
		{
			KLOG(ERROR, "GetCurrentDirectory failed! getlasterror=%i", 
			     GetLastError());
			return RETURN_CODE_FAILURE;
		}
		KLOG(INFO, "g_pathCurrentDirectory='%s'", g_pathCurrentDirectory);
	}
	///TODO: handle file paths longer than MAX_PATH in the future...
	TCHAR fullPathToGameDll    [MAX_PATH] = {};
	TCHAR fullPathToGameDllTemp[MAX_PATH] = {};
	// determine the FULL path to the game DLL file //
	{
		if(FAILED(StringCchPrintf(fullPathToGameDll, MAX_PATH, 
		                          TEXT("%s\\%s.dll"), g_pathToExe, 
		                          FILE_NAME_GAME_DLL)))
		{
			KLOG(ERROR, "Failed to build fullPathToGameDll!  "
			     "g_pathToExe='%s' FILE_NAME_GAME_DLL='%s'", 
			     g_pathToExe, FILE_NAME_GAME_DLL);
			return RETURN_CODE_FAILURE;
		}
		const u32 unusedTempGameDllPostfix = w32FindUnusedTempGameDllPostfix();
		if(FAILED(StringCchPrintf(fullPathToGameDllTemp, MAX_PATH, 
		                          TEXT("%s\\%s_temp_%i.dll"), g_pathTemp, 
		                          FILE_NAME_GAME_DLL, 
		                          unusedTempGameDllPostfix)))
		{
			KLOG(ERROR, "Failed to build fullPathToGameDllTemp!  "
			     "g_pathTemp='%s' FILE_NAME_GAME_DLL='%s'", 
			     g_pathTemp, FILE_NAME_GAME_DLL);
			return RETURN_CODE_FAILURE;
		}
	}
	/* allocate a single large chunk of memory - this is the MINIMUM required 
		memory for the entire KORL application to run!!! */
	VOID* minimumApplicationMemory = nullptr;
	size_t staticMemoryAllocOffsets[
		static_cast<u8>(StaticMemoryAllocationIndex::ENUM_SIZE)];
	{
		/* calculate the total # of bytes required for all our static memory 
			allocation chunks */
		size_t minimumRequiredMemoryBytes = 0;
		for(u8 a = 0; 
			a < static_cast<u8>(StaticMemoryAllocationIndex::ENUM_SIZE); a++)
		{
			/* while we're at it, calculate the byte offsets of each static 
				allocation chunk */
			if(a == 0)
				staticMemoryAllocOffsets[a] = 0;
			else
				staticMemoryAllocOffsets[a] = staticMemoryAllocOffsets[a - 1] + 
					STATIC_MEMORY_ALLOC_SIZES[a - 1];
			minimumRequiredMemoryBytes += STATIC_MEMORY_ALLOC_SIZES[a];
		}
		KLOG(INFO, "Minimum required application memory == %i bytes", 
		     minimumRequiredMemoryBytes);
#if INTERNAL_BUILD
		const LPVOID baseAddress = 
			reinterpret_cast<LPVOID>(kmath::terabytes(1));
#else
		/* setting a baseAddress to 0 allows the OS to put the memory anywhere, 
			which should be more secure I suppose */
		const LPVOID baseAddress = 0;
#endif
		minimumApplicationMemory = 
			VirtualAlloc(baseAddress, minimumRequiredMemoryBytes, 
			             MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if(!minimumApplicationMemory)
		{
			KLOG(ERROR, "Failed to VirtualAlloc %i bytes!  GetLastError=%i", 
			     minimumRequiredMemoryBytes, GetLastError());
			return RETURN_CODE_FAILURE;
		}
	}
	/* assign a pre-allocated dynamic memory arena for STB_IMAGE */
	{
		const u8 allocId = static_cast<u8>(
			StaticMemoryAllocationIndex::STB_IMAGE);
		size_t memoryOffset = staticMemoryAllocOffsets[allocId];
		void*const memoryAddressStart = 
			static_cast<u8*>(minimumApplicationMemory) + memoryOffset;
		const size_t memoryBytes = STATIC_MEMORY_ALLOC_SIZES[allocId];
		g_genAllocStbImage = kgtAllocInit(
			KgtAllocatorType::GENERAL, memoryAddressStart, memoryBytes);
#if SLOW_BUILD && INTERNAL_BUILD
		kgtAllocUnitTest(g_genAllocStbImage);
#endif//SLOW_BUILD && INTERNAL_BUILD
	}
	/* assign a pre-allocated dynamic memory arena for ImGui */
	{
		const u8 allocId = 
			static_cast<u8>(StaticMemoryAllocationIndex::IMGUI);
		size_t memoryOffset = staticMemoryAllocOffsets[allocId];
		void*const memoryAddressStart = 
			static_cast<u8*>(minimumApplicationMemory) + memoryOffset;
		const size_t memoryBytes = STATIC_MEMORY_ALLOC_SIZES[allocId];
		g_genAllocImgui = kgtAllocInit(
			KgtAllocatorType::GENERAL, memoryAddressStart, memoryBytes);
	}
	/* assign a pre-allocated dynamic memory arena for loading files into memory 
		note: this memory definitely needs to be thread-safe */
	{
		const u8 allocId = static_cast<u8>(
			StaticMemoryAllocationIndex::RAW_FILES);
		size_t memoryOffset = staticMemoryAllocOffsets[allocId];
		void*const memoryAddressStart = 
			static_cast<u8*>(minimumApplicationMemory) + memoryOffset;
		const size_t memoryBytes = STATIC_MEMORY_ALLOC_SIZES[allocId];
		g_hKalRawFiles = kgtAllocInit(
			KgtAllocatorType::GENERAL, memoryAddressStart, memoryBytes);
	}
	/* assign a pre-allocated dynamic memory arena for decoding vorbis data */
	{
		const u8 allocId = static_cast<u8>(
			StaticMemoryAllocationIndex::STB_VORBIS);
		size_t memoryOffset = staticMemoryAllocOffsets[allocId];
		void*const memoryAddressStart = 
			static_cast<u8*>(minimumApplicationMemory) + memoryOffset;
		const size_t memoryBytes = STATIC_MEMORY_ALLOC_SIZES[allocId];
		g_oggVorbisAlloc.alloc_buffer_length_in_bytes = 
			kmath::safeTruncateI32(memoryBytes);
		g_oggVorbisAlloc.alloc_buffer = reinterpret_cast<char*>(
			memoryAddressStart);
	}
	Korl_Win32_DynamicApplicationModule dynApp = 
		korl_w32_dynApp_load(fullPathToGameDll, fullPathToGameDllTemp);
	korlAssert(dynApp.isValid);
	if(!QueryPerformanceFrequency(&g_perfCounterHz))
	{
		KLOG(ERROR, "Failed to query for performance frequency! "
		     "GetLastError=%i", GetLastError());
		return RETURN_CODE_FAILURE;
	}
	/* load cursor icons */
	g_cursorArrow          = LoadCursorA(NULL, IDC_ARROW);
	g_cursorSizeHorizontal = LoadCursorA(NULL, IDC_SIZEWE);
	g_cursorSizeVertical   = LoadCursorA(NULL, IDC_SIZENS);
	g_cursorSizeNeSw       = LoadCursorA(NULL, IDC_SIZENESW);
	g_cursorSizeNwSe       = LoadCursorA(NULL, IDC_SIZENWSE);
	g_cursorSizeAll        = LoadCursorA(NULL, IDC_SIZEALL);
	g_cursorTextBeam       = LoadCursorA(NULL, IDC_IBEAM);
	g_cursorHand           = LoadCursorA(NULL, IDC_HAND);
	g_cursorNo             = LoadCursorA(NULL, IDC_NO);
	/* create the main KORL window's WNDCLASS */
	const WNDCLASS wndClass = 
		{ .style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC
		, .lpfnWndProc   = w32MainWindowCallback
		, .hInstance     = hInstance
		, .hIcon         = LoadIcon(hInstance, TEXT("korl-application-icon"))
		, .hCursor       = g_cursorArrow
		, .hbrBackground = CreateSolidBrush(RGB(0, 0, 0))
		, .lpszClassName = "KorlWindowClass" };
	const ATOM atomWindowClass = RegisterClassA(&wndClass);
	if(atomWindowClass == 0)
	{
		KLOG(ERROR, "Failed to register WNDCLASS! GetLastError=%i", 
		     GetLastError());
		return RETURN_CODE_FAILURE;
	}
	/* take control of DPI-awareness from the system before creating any 
		windows */
	{
		DPI_AWARENESS_CONTEXT dpiContextOld = 
			SetThreadDpiAwarenessContext(
				/* DPI awareness per monitor V2 allows the application to 
					automatically scale the non-client region for us 
					(title bar, etc...) */
				DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		korlAssert(dpiContextOld != NULL);
	}
	/* create the main KORL window */
	g_mainWindow = CreateWindowExA(
		0, wndClass.lpszClassName, APPLICATION_NAME,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL );
	if(!g_mainWindow)
	{
		KLOG(ERROR, "Failed to create window! GetLastError=%i", GetLastError());
		return RETURN_CODE_FAILURE;
	}
	w32LoadDInput(hInstance);
	w32LoadXInput();
	///TODO: update the monitorRefreshHz and dependent variable realtime when 
	///      the window gets moved around to another monitor.
	u32 monitorRefreshHz = 
		korl_w32_window_queryNearestMonitorRefreshRate(g_mainWindow);
	f32 targetSecondsElapsedPerFrame = 1.f / monitorRefreshHz;
	GameMouse gameMouseA = {};
	GameMouse gameMouseB = {};
	GameMouse* gameMouseFrameCurrent  = &gameMouseA;
	GameMouse* gameMouseFramePrevious = &gameMouseB;
	GameKeyboard gameKeyboardA = {};
	GameKeyboard gameKeyboardB = {};
	GameKeyboard* gameKeyboardCurrentFrame  = &gameKeyboardA;
	GameKeyboard* gameKeyboardPreviousFrame = &gameKeyboardB;
#if INTERNAL_BUILD
	// ensure that the size of the mouse's vKeys array matches the size of
	//	the anonymous struct which defines the names of all the game keys //
	korlAssert( static_cast<size_t>(&gameMouseA.DUMMY_LAST_BUTTON_STATE - 
	                                &gameMouseA.vKeys[0]) ==
	            CARRAY_SIZE(gameMouseA.vKeys) );
	// ensure that the size of the keyboard's vKeys array matches the size of
	//	the anonymous struct which defines the names of all the game keys //
	korlAssert( static_cast<size_t>(&gameKeyboardA.DUMMY_LAST_BUTTON_STATE - 
	                                &gameKeyboardA.vKeys[0]) ==
	            CARRAY_SIZE(gameKeyboardA.vKeys) );
	// ensure that the size of the gamepad's axis array matches the size of
	//	the anonymous struct which defines the names of all the axes //
	korlAssert( static_cast<size_t>(&g_gamePadArrayA[0].DUMMY_LAST_AXIS - 
	                                &g_gamePadArrayA[0].axes[0]) ==
	            CARRAY_SIZE(g_gamePadArrayA[0].axes) );
	// ensure that the size of the gamepad's button array matches the size of
	//	the anonymous struct which defines the names of all the buttons //
	korlAssert(static_cast<size_t>(&g_gamePadArrayA[0].DUMMY_LAST_BUTTON_STATE - 
	                               &g_gamePadArrayA[0].buttons[0]) ==
	           CARRAY_SIZE(g_gamePadArrayA[0].buttons) );
#endif// INTERNAL_BUILD
	DWORD gameSoundCursorWritePrev;
	/* assign a pre-allocated memory arena for the game sound buffer */
	VOID* gameSoundMemory = nullptr;
	{
		const u8 allocId = static_cast<u8>(
			StaticMemoryAllocationIndex::GAME_SOUND);
		size_t memoryOffset = staticMemoryAllocOffsets[allocId];
		void*const memoryAddressStart = 
			static_cast<u8*>(minimumApplicationMemory) + memoryOffset;
		gameSoundMemory = memoryAddressStart;
	}
	// Initialize ImGui~~~ //
	{
		ImGui::SetAllocatorFunctions(
			w32PlatformImguiAlloc, w32PlatformImguiFree, g_genAllocImgui);
		ImGui::CreateContext();
		if(!ImGui_ImplOpenGL3_Init())
		{
			KLOG(ERROR, "ImGui_ImplOpenGL3_Init failure!");
			return RETURN_CODE_FAILURE;
		}
		if(!ImGui_ImplWin32_Init(g_mainWindow))
		{
			KLOG(ERROR, "ImGui_ImplWin32_Init failure!");
			return RETURN_CODE_FAILURE;
		}
		/* turn off backend modification of mouse cursor; we will handle it 
			ourselves */
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	}
	GameMemory gameMemory = {};
	/* initialize GameMemory */
	{
		gameMemory.permanentMemoryBytes = STATIC_MEMORY_ALLOC_SIZES[
			static_cast<u8>(StaticMemoryAllocationIndex::GAME_PERMANENT)];
		gameMemory.transientMemoryBytes = STATIC_MEMORY_ALLOC_SIZES[
			static_cast<u8>(StaticMemoryAllocationIndex::GAME_TRANSIENT)];
		gameMemory.permanentMemory = reinterpret_cast<u8*>(
			minimumApplicationMemory) + staticMemoryAllocOffsets[
				static_cast<u8>(StaticMemoryAllocationIndex::GAME_PERMANENT)];
		gameMemory.transientMemory = reinterpret_cast<u8*>(
			minimumApplicationMemory) + staticMemoryAllocOffsets[
				static_cast<u8>(StaticMemoryAllocationIndex::GAME_TRANSIENT)];
		gameMemory.kpl                 = KORL_PLATFORM_API_WIN32;
		gameMemory.krb                 = {};
		gameMemory.imguiContext        = ImGui::GetCurrentContext();
		gameMemory.platformImguiAlloc  = w32PlatformImguiAlloc;
		gameMemory.platformImguiFree   = w32PlatformImguiFree;
		gameMemory.imguiAllocUserData  = g_genAllocImgui;
	}
	dynApp.onReloadCode(gameMemory, false);
	dynApp.initialize(gameMemory);
	w32InitDSound(g_mainWindow, SOUND_SAMPLE_HZ, SOUND_BUFFER_BYTES, 
	              SOUND_CHANNELS, gameSoundCursorWritePrev);
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
			KLOG(WARNING, "System doesn't support custom scheduler granularity."
			     " Main thread will not be put to sleep!");
		}
	}
	// set scheduler granularity using timeBeginPeriod //
	const bool sleepIsGranular = systemSupportsDesiredTimerGranularity &&
		timeBeginPeriod(DESIRED_OS_TIMER_GRANULARITY_MS) == TIMERR_NOERROR;
	if(!sleepIsGranular)
	{
		KLOG(WARNING, "System supports custom scheduler granularity, but "
		     "setting this value has failed! Main thread will not be "
		     "put to sleep!");
	}
	defer({
		if (sleepIsGranular && 
			timeEndPeriod(DESIRED_OS_TIMER_GRANULARITY_MS) != TIMERR_NOERROR)
		{
			KLOG(ERROR, "Failed to timeEndPeriod!");
		}
	});
	// Set the process to a higher priority to minimize the chance of another 
	//	process keeping us asleep hopefully. //
	if(sleepIsGranular)
	{
		const HANDLE hProcess = GetCurrentProcess();
		if(!SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS))
		{
			KLOG(ERROR, "Failed to set the process priority class to "
			     "HIGH_PRIORITY_CLASS! GetLastError=%i", GetLastError());
			return RETURN_CODE_FAILURE;
		}
	}
	// main window loop //
	{
		g_running = true;
		LARGE_INTEGER perfCountFrameLimiterPrev = w32QueryPerformanceCounter();
		LARGE_INTEGER perfCountPrev = w32QueryPerformanceCounter();
		u64 clockCyclesPrev = __rdtsc();
		while(g_running)
		{
			// dynamically hot-reload game code!!! //
			{
				const FILETIME gameCodeLastWriteTime = 
					w32GetLastWriteTime(fullPathToGameDll);
				if( gameCodeLastWriteTime.dwHighDateTime == 0 &&
				    gameCodeLastWriteTime.dwLowDateTime  == 0 )
				// Immediately invalidate the game code if we fail to get the 
				//	last write time of the dll, since it means the dll is 
				//	probably in the process of getting recompiled!
				{
					if(dynApp.isValid)
					{
						KLOG(INFO, "Unloading dynApp...");
						dynApp.onPreUnload();
					}
					dynApp.isValid = false;
				}
				if((CompareFileTime(&gameCodeLastWriteTime, 
				                    &dynApp.dllLastWriteTime) 
				    || !dynApp.isValid) 
				    && !jobQueueHasIncompleteJobs(&g_jobQueue))
				{
					korl_w32_dynApp_unload(&dynApp);
					dynApp = korl_w32_dynApp_load(
						fullPathToGameDll, fullPathToGameDllTemp);
					if(dynApp.isValid)
					{
						KLOG(INFO, "dynApp hot-reload complete!");
						dynApp.onReloadCode(gameMemory, true);
					}
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
					default:
					{
						TranslateMessage(&windowMessage);
						//SizingCheck(&windowMessage);
						DispatchMessageA(&windowMessage);
					} break;
				}
			}
			if(g_deviceChangeDetected)
			{
				g_deviceChangeDetected = false;
				w32DInputEnumerateDevices();
			}
			/* mouse input: swap mouse input frame states, including pure 
				relative movement from DirectInput */
			if(gameMouseFrameCurrent == &gameMouseA)
			{
				gameMouseFrameCurrent  = &gameMouseB;
				gameMouseFramePrevious = &gameMouseA;
			}
			else
			{
				gameMouseFrameCurrent  = &gameMouseA;
				gameMouseFramePrevious = &gameMouseB;
			}
			w32GetMouseStates(gameMouseFrameCurrent, gameMouseFramePrevious);
			w32DInputGetMouseStates(gameMouseFrameCurrent);
			// Process gameKeyboard by comparing the keys to the previous 
			//	frame's keys, because we cannot determine if a key has been 
			//	newly pressed this frame just from the windows message loop data
			if(gameKeyboardCurrentFrame == &gameKeyboardA)
			{
				gameKeyboardCurrentFrame  = &gameKeyboardB;
				gameKeyboardPreviousFrame = &gameKeyboardA;
			}
			else
			{
				gameKeyboardCurrentFrame  = &gameKeyboardA;
				gameKeyboardPreviousFrame = &gameKeyboardB;
			}
			w32GetKeyboardKeyStates(
				gameKeyboardCurrentFrame, gameKeyboardPreviousFrame);
			// swap game pad arrays & update the current frame //
			if(g_gamePadArrayCurrentFrame == g_gamePadArrayA)
			{
				g_gamePadArrayPreviousFrame = g_gamePadArrayA;
				g_gamePadArrayCurrentFrame  = g_gamePadArrayB;
			}
			else
			{
				g_gamePadArrayPreviousFrame = g_gamePadArrayB;
				g_gamePadArrayCurrentFrame  = g_gamePadArrayA;
			}
			/* read game pads from DirectInput & XInput */
			w32DInputGetGamePadStates(
				g_gamePadArrayCurrentFrame  + XUSER_MAX_COUNT, 
				g_gamePadArrayPreviousFrame + XUSER_MAX_COUNT);
			w32XInputGetGamePadStates(
				g_gamePadArrayCurrentFrame, g_gamePadArrayPreviousFrame);
			/* update ImGui back end */
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			const v2u32 windowDims = 
				korl_w32_window_getWindowDimensions(g_mainWindow);
			if(!dynApp.isValid)
			// display a "loading" message while we wait for cl.exe to 
			//	relinquish control of the game binary //
			{
				const ImVec2 displayCenter(
					windowDims.x*0.5f, windowDims.y*0.5f);
				ImGui::SetNextWindowPos(
					displayCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
				ImGui::Begin(
					"Reloading Game Code", nullptr
					, ImGuiWindowFlags_NoTitleBar 
					| ImGuiWindowFlags_NoResize 
					| ImGuiWindowFlags_NoMove 
					| ImGuiWindowFlags_NoScrollbar 
					| ImGuiWindowFlags_NoScrollWithMouse 
					| ImGuiWindowFlags_AlwaysAutoResize 
					| ImGuiWindowFlags_NoSavedSettings 
					| ImGuiWindowFlags_NoMouseInputs );
				if(jobQueueHasIncompleteJobs(&g_jobQueue))
					ImGui::Text(
						"Waiting for job queue to finish...%c", 
						"|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
				else
					ImGui::Text(
						"Loading Game Code...%c", 
						"|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
				ImGui::End();
			}
			const f32 deltaSeconds = kmath::min(
				MAX_GAME_DELTA_SECONDS, targetSecondsElapsedPerFrame);
			/* perform custom non-modal window resize/move logic */
			korl_w32_window_stepMoveSize(
				deltaSeconds, gameMouseFrameCurrent, gameKeyboardCurrentFrame);
			if(!dynApp.updateAndDraw(
					deltaSeconds, windowDims, 
					*gameMouseFrameCurrent, *gameKeyboardCurrentFrame,
					g_gamePadArrayCurrentFrame, CARRAY_SIZE(g_gamePadArrayA), 
					g_isFocused))
				g_running = false;
			w32WriteDSoundAudio(
				SOUND_BUFFER_BYTES, SOUND_SAMPLE_HZ, SOUND_CHANNELS, 
				gameSoundMemory, gameSoundCursorWritePrev, dynApp);
#if 0
			// set XInput state //
			for(u8 ci = 0; ci < numGamePads; ci++)
			{
				XINPUT_VIBRATION vibration;
				vibration.wLeftMotorSpeed = static_cast<WORD>(0xFFFF *
					gamePadArrayCurrentFrame[ci].normalizedMotorSpeedLeft);
				vibration.wRightMotorSpeed = static_cast<WORD>(0xFFFF *
					gamePadArrayCurrentFrame[ci].normalizedMotorSpeedRight);
				if(XInputSetState(ci, &vibration) != ERROR_SUCCESS)
				{
					///TODO: error & controller not connected return values
				}
			}
#endif // 0
			/* flush ImGui graphics */
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			// update window graphics //
			const HDC hdc = wglGetCurrentDC();//GetDC(g_mainWindow);
			if(!hdc)
			{
				KLOG(ERROR, "Failed to get main window device context!");
				return RETURN_CODE_FAILURE;
			}
			if(!SwapBuffers(hdc))
				KLOG(WARNING, "Failed to SwapBuffers! GetLastError=%i", 
					GetLastError());
			// enforce targetSecondsElapsedPerFrame //
			// we still have to Sleep/wait when VSync is on if SwapBuffers
			//      completes too early!!! (like when the double-buffer is not
			//      yet filled up at beginning of execution) //
			///TODO: maybe just don't sleep/wait the first frame at all because
			///      the windows message queue needs to be unclogged and game
			///      state memory needs to be initialized.
			{
				// It is possible for windows to sleep us for longer than we 
				//	would want it to, so we will ask the OS to wake us up a 
				//	little bit earlier than desired.  Then we will spin the CPU
				//	until the actual target FPS is achieved.
				// THREAD_WAKE_SLACK_SECONDS is how early we should wake up by.
				local_persist const f32 THREAD_WAKE_SLACK_SECONDS = 0.001f;
				f32 awakeSeconds = 
					w32ElapsedSeconds(perfCountFrameLimiterPrev, 
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
						w32ElapsedSeconds(perfCountFrameLimiterPrev, 
						                  w32QueryPerformanceCounter());
				}
				perfCountFrameLimiterPrev = w32QueryPerformanceCounter();
			}
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
#if SLOW_BUILD && 0
				// send performance measurement to debugger as a string //
				KLOG(INFO, "%.2f ms/f | %.2f Mc/f", 
				     elapsedSeconds*1000, clockCycleDiff/1000000.f);
#endif
				perfCountPrev   = perfCount;
				clockCyclesPrev = clockCycles;
			}
		}
	}
	korlAssert(kgtAllocUsedBytes(g_genAllocStbImage) == 0);
	KLOG(INFO, "END! :)");
	return RETURN_CODE_SUCCESS;
}
#include "win32-dsound.cpp"
#include "win32-xinput.cpp"
#include "win32-directinput.cpp"
#include "win32-krb-opengl.cpp"
#include "win32-jobQueue.cpp"
#include "win32-network.cpp"
#include "win32-platform.cpp"
#include "win32-dynApp.cpp"
#include "win32-log.cpp"
#include "win32-time.cpp"
#include "win32-crash.cpp"
#include "win32-window.cpp"
#include "win32-input.cpp"
#include "krb-opengl.cpp"
#include "z85.cpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ASSERT(x) korlAssert(x)
#define DEBUG_PRINT_STBI_MEMORY 0
internal void* stbiMalloc(size_t allocationByteCount)
{
	EnterCriticalSection(&g_stbiAllocationCsLock);
#if SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	KLOG(INFO, "stbiMalloc(%i)", allocationByteCount);
#endif// SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	void*const result = kgtAllocAlloc(g_genAllocStbImage, allocationByteCount);
#if SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	KLOG(INFO, "stbiMalloc: result=%x", result);
#endif// SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	LeaveCriticalSection(&g_stbiAllocationCsLock);
	return result;
}
internal void* stbiRealloc(void* allocatedAddress, 
                           size_t newAllocationByteCount)
{
	EnterCriticalSection(&g_stbiAllocationCsLock);
#if SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	KLOG(INFO, "stbiRealloc(%x, %i)", allocatedAddress, newAllocationByteCount);
#endif// SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	void*const result = kgtAllocRealloc(
		g_genAllocStbImage, allocatedAddress, newAllocationByteCount);
#if SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	KLOG(INFO, "stbiRealloc: result=%x", result);
#endif// SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	LeaveCriticalSection(&g_stbiAllocationCsLock);
	return result;
}
internal void stbiFree(void* allocatedAddress)
{
	if(!allocatedAddress)
		return;
	EnterCriticalSection(&g_stbiAllocationCsLock);
#if SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	KLOG(INFO, "stbiFree(0x%x)", allocatedAddress);
#endif// SLOW_BUILD && DEBUG_PRINT_STBI_MEMORY 
	kgtAllocFree(g_genAllocStbImage, allocatedAddress);
	LeaveCriticalSection(&g_stbiAllocationCsLock);
}
#define STBI_MALLOC(sz)       stbiMalloc(sz)
#define STBI_REALLOC(p,newsz) stbiRealloc(p,newsz)
#define STBI_FREE(p)          stbiFree(p)
#include "stb/stb_image.h"
/* we need to include VersionHelpers.h here so that ImGui knows not to declare 
	its own versions of this API, since it is included in later code (such as in 
	win32loopl-code/*) */
#include <VersionHelpers.h>
#pragma warning( push )
	// warning C4127: conditional expression is constant
	#pragma warning( disable : 4127 )
	/* warning C5219: implicit conversion from 'int' to 'float', possible loss 
		of data */
	#pragma warning( disable : 5219 )
	#include "imgui/imgui_demo.cpp"
	#include "imgui/imgui_draw.cpp"
	#include "imgui/imgui_impl_opengl3.cpp"
	#include "imgui/imgui_impl_win32.cpp"
	#include "imgui/imgui_widgets.cpp"
	#include "imgui/imgui.cpp"
#pragma warning( pop )
#pragma warning( push )
	#pragma warning( disable : 4244 )
	#pragma warning( disable : 4245 )
	#pragma warning( disable : 4369 )
	#pragma warning( disable : 4456 )
	#pragma warning( disable : 4457 )
	#pragma warning( disable : 4701 )
	#include "stb/stb_vorbis.c"
#pragma warning( pop )
#define STB_DS_IMPLEMENTATION
internal void* kStbDsRealloc(
	void* allocatedAddress, size_t newAllocationSize, void* context)
{
	KLOG(ERROR, "Win32 platform layer should not be using STB_DS!");
	korlAssert(context);
	KgtAllocatorHandle hKal = reinterpret_cast<KgtAllocatorHandle>(context);
	void*const result = 
		kgtAllocRealloc(hKal, allocatedAddress, newAllocationSize);
	korlAssert(result);
	return result;
}
internal void kStbDsFree(void* allocatedAddress, void* context)
{
	KLOG(ERROR, "Win32 platform layer should not be using STB_DS!");
	korlAssert(context);
	KgtAllocatorHandle hKal = reinterpret_cast<KgtAllocatorHandle>(context);
	kgtAllocFree(hKal, allocatedAddress);
}
#pragma warning( push )
	// warning C4365: 'argument': conversion
	#pragma warning( disable : 4365 )
	// warning C4456: declaration of 'i' hides previous local declaration
	#pragma warning( disable : 4456 )
	#include "stb/stb_ds.h"
#pragma warning( pop )
#include "kmath.cpp"
#include "kutil.cpp"
#include "kgtAllocator.cpp"