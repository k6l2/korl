#include "win32-main.h"
#include "win32-dsound.h"
#include "win32-xinput.h"
#include "win32-directinput.h"
#include "win32-krb-opengl.h"
#include "win32-network.h"
#include "win32-platform.h"
#include "kgtAllocator.h"
#include <cstdio>
#include <ctime>
#include <dbghelp.h>
#include <strsafe.h>
#include <ShlObj.h>
#include <Dbt.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
                             HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include "imgui/imgui_impl_opengl2.h"
#include "stb/stb_image.h"
#include "stb/stb_vorbis.h"
// Allow the pre-processor to store compiler definitions as string literals //
//	Source: https://stackoverflow.com/a/39108392
#define _DEFINE_TO_CSTR(define) #define
#define DEFINE_TO_CSTR(d) _DEFINE_TO_CSTR(d)
global_variable const TCHAR APPLICATION_NAME[] = 
                                    TEXT(DEFINE_TO_CSTR(KORL_APP_NAME));
global_variable const TCHAR APPLICATION_VERSION[] = 
                                    TEXT(DEFINE_TO_CSTR(KORL_APP_VERSION));
global_variable const TCHAR FILE_NAME_GAME_DLL[] = 
                                    TEXT(DEFINE_TO_CSTR(KORL_GAME_DLL_FILENAME));
global_variable const f32 MAX_GAME_DELTA_SECONDS = 1.f / KORL_MINIMUM_FRAME_RATE;
global_variable bool g_running;
global_variable bool g_isFocused;
/* this variable exists because mysterious COM incantations require us to check 
	whether or not a DirectInput device is Xinput compatible OUTSIDE of the 
	windows message loop */
global_variable bool g_deviceChangeDetected;
global_variable HCURSOR g_cursorArrow;
global_variable HCURSOR g_cursorSizeVertical;
global_variable HCURSOR g_cursorSizeHorizontal;
global_variable HCURSOR g_cursorSizeNeSw;
global_variable HCURSOR g_cursorSizeNwSe;
global_variable KgtAllocatorHandle g_genAllocStbImage;
global_variable KgtAllocatorHandle g_genAllocImgui;
global_variable CRITICAL_SECTION g_stbiAllocationCsLock;
// @assumption: once we have written a crash dump, there is never a need to 
//	write any more, let alone continue execution.
global_variable bool g_hasWrittenCrashDump;
///TODO: handle file paths longer than MAX_PATH in the future...
global_variable TCHAR g_pathToExe[MAX_PATH];
global_variable TCHAR g_pathTemp[MAX_PATH];
global_variable TCHAR g_pathLocalAppData[MAX_PATH];
global_variable GamePad* g_gamePadArrayCurrentFrame  = g_gamePadArrayA;
global_variable GamePad* g_gamePadArrayPreviousFrame = g_gamePadArrayB;
global_variable const u8 SOUND_CHANNELS = 2;
global_variable const u32 SOUND_SAMPLE_HZ = 44100;
global_variable const u32 SOUND_BYTES_PER_SAMPLE = sizeof(i16)*SOUND_CHANNELS;
global_variable const u32 SOUND_BUFFER_BYTES = 
                                     SOUND_SAMPLE_HZ/2 * SOUND_BYTES_PER_SAMPLE;
/* @TODO: make these memory quantities configurable per-project */
global_variable const size_t STATIC_MEMORY_ALLOC_SIZES[] = 
	{ kmath::megabytes(64)
	, SOUND_BUFFER_BYTES
	, kmath::megabytes(128)
	, kmath::megabytes(1)
	, kmath::megabytes(1)
	, kmath::megabytes(1)
	, kmath::megabytes(16)
};
enum class StaticMemoryAllocationIndex : u8// sub-255 memory chunks please, god!
	{ GAME_PERMANENT
	, GAME_SOUND
	, GAME_TRANSIENT
	, STB_IMAGE
	, STB_VORBIS
	, IMGUI
	, RAW_FILES
	, ENUM_SIZE// obviously, this value does not have a matching allocation size
};
struct W32ThreadInfo
{
	u32 index;
	JobQueue* jobQueue;
};
internal GAME_INITIALIZE(gameInitializeStub)
{
}
internal GAME_ON_RELOAD_CODE(gameOnReloadCodeStub)
{
}
internal GAME_RENDER_AUDIO(gameRenderAudioStub)
{
}
internal GAME_UPDATE_AND_DRAW(gameUpdateAndDrawStub)
{
	// continue running the application to keep attempting to reload game code!
	return true;
}
internal GAME_ON_PRE_UNLOAD(gameOnPreUnloadStub)
{
}
internal void w32WriteLogToFile()
{
	local_persist const TCHAR fileNameLog[] = TEXT("log.txt");
	local_persist const DWORD MAX_DWORD = ~DWORD(1<<(sizeof(DWORD)*8-1));
	static_assert(MAX_LOG_BUFFER_SIZE < MAX_DWORD);
	///TODO: change this to use a platform-determined write directory
	TCHAR fullPathToNewLog[MAX_PATH] = {};
	TCHAR fullPathToLog   [MAX_PATH] = {};
	// determine full path to log files using platform-determined write 
	//	directory //
	{
		if(FAILED(StringCchPrintf(fullPathToNewLog, MAX_PATH, 
		                          TEXT("%s\\%s.new"), g_pathTemp, fileNameLog)))
		{
			KLOG(ERROR, "Failed to build fullPathToNewLog!  "
			     "g_pathTemp='%s' fileNameLog='%s'", 
			     g_pathTemp, fileNameLog);
			return;
		}
		if(FAILED(StringCchPrintf(fullPathToLog, MAX_PATH, 
		                          TEXT("%s\\%s"), g_pathTemp, fileNameLog)))
		{
			KLOG(ERROR, "Failed to build fullPathToLog!  "
			     "g_pathTemp='%s' fileNameLog='%s'", 
			     g_pathTemp, fileNameLog);
			return;
		}
	}
	const HANDLE fileHandle = 
		CreateFileA(fullPathToNewLog, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
	if(fileHandle == INVALID_HANDLE_VALUE)
	{
		///TODO: handle GetLastError
		korlAssert(!"Failed to create log file handle!");
		return;
	}
	// write the beginning log buffer //
	{
		const DWORD byteWriteCount = 
			static_cast<DWORD>(strnlen_s(g_logBeginning, MAX_LOG_BUFFER_SIZE));
		DWORD bytesWritten;
		if(!WriteFile(fileHandle, g_logBeginning, byteWriteCount, &bytesWritten, 
		              nullptr))
		{
			///TODO: handle GetLastError (close fileHandle)
			korlAssert(!"Failed to write log beginning!");
			return;
		}
		if(bytesWritten != byteWriteCount)
		{
			///TODO: handle this error case(close fileHandle)
			korlAssert(!"Failed to complete write log beginning!");
			return;
		}
	}
	// If the log's beginning buffer is full, that means we have stuff in the 
	//	circular buffer to write out.
	if(g_logCurrentBeginningChar == MAX_LOG_BUFFER_SIZE)
	{
		// If the total bytes written to the circular buffer exceed the buffer
		//	size, that means we have to indicate a discontinuity in the log file
		if(g_logCircularBufferRunningTotal >= MAX_LOG_BUFFER_SIZE - 1)
		{
			// write the log cut string //
			{
				local_persist const char CSTR_LOG_CUT[] = 
					"- - - - - - LOG CUT - - - - - - - -\n";
				const DWORD byteWriteCount = static_cast<DWORD>(
					strnlen_s(CSTR_LOG_CUT, sizeof(CSTR_LOG_CUT)));
				DWORD bytesWritten;
				if(!WriteFile(fileHandle, CSTR_LOG_CUT, byteWriteCount, 
				              &bytesWritten, nullptr))
				{
					///TODO: handle GetLastError (close fileHandle)
					korlAssert(!"Failed to write log cut string!");
					return;
				}
				if(bytesWritten != byteWriteCount)
				{
					///TODO: handle this error case (close fileHandle)
					korlAssert(!"Failed to complete write log cut string!");
					return;
				}
			}
			if(g_logCurrentCircularBufferChar < MAX_LOG_BUFFER_SIZE)
			// write the oldest contents of the circular buffer //
			{
				const char*const data = 
					g_logCircularBuffer + g_logCurrentCircularBufferChar + 1;
				const DWORD byteWriteCount = static_cast<DWORD>(
					strnlen_s(data, MAX_LOG_BUFFER_SIZE - 
					                g_logCurrentCircularBufferChar));
				DWORD bytesWritten;
				if(!WriteFile(fileHandle, data, byteWriteCount, 
				              &bytesWritten, nullptr))
				{
					///TODO: handle GetLastError (close fileHandle)
					korlAssert(!"Failed to write log circular buffer old!");
					return;
				}
				if(bytesWritten != byteWriteCount)
				{
					///TODO: handle this error case (close fileHandle)
					korlAssert(!"Failed to complete write circular buffer "
					            "old!");
					return;
				}
			}
		}
		// write the newest contents of the circular buffer //
		{
			const DWORD byteWriteCount = static_cast<DWORD>(
				strnlen_s(g_logCircularBuffer, MAX_LOG_BUFFER_SIZE));
			DWORD bytesWritten;
			if(!WriteFile(fileHandle, g_logCircularBuffer, byteWriteCount, 
			              &bytesWritten, nullptr))
			{
				///TODO: handle GetLastError (close fileHandle)
				korlAssert(!"Failed to write log circular buffer new!");
				return;
			}
			if(bytesWritten != byteWriteCount)
			{
				///TODO: handle this error case (close fileHandle)
				korlAssert(!"Failed to complete write circular buffer new!");
				return;
			}
		}
	}
	if(!CloseHandle(fileHandle))
	{
		///TODO: handle GetLastError
		korlAssert(!"Failed to close new log file handle!");
		return;
	}
	// Copy the new log file into the finalized file name //
	if(!CopyFileA(fullPathToNewLog, fullPathToLog, false))
	{
		korlAssert(!"Failed to rename new log file!");
		return;
	}
	platformLog("win32-main", __LINE__, PlatformLogCategory::K_INFO,
	            "Log successfully wrote to '%s'!", fullPathToLog);
	// Once the log file has been finalized, we can delete the temporary file //
	if(!DeleteFileA(fullPathToNewLog))
	{
		platformLog("win32-main", __LINE__, PlatformLogCategory::K_WARNING,
		            "Failed to delete '%s'!", fullPathToNewLog);
	}
}
internal FILETIME w32GetLastWriteTime(const char* fileName)
{
	FILETIME result = {};
	WIN32_FILE_ATTRIBUTE_DATA fileAttributeData;
	if(!GetFileAttributesExA(fileName, GetFileExInfoStandard, 
	                         &fileAttributeData))
	{
		if(GetLastError() == ERROR_FILE_NOT_FOUND)
		/* if the file can't be found, just silently return a zero FILETIME */
		{
			return result;
		}
		KLOG(WARNING, "Failed to get last write time of file '%s'! "
		     "GetLastError=%i", fileName, GetLastError());
		return result;
	}
	result = fileAttributeData.ftLastWriteTime;
	return result;
}
internal GameCode w32LoadGameCode(const char* fileNameDll, 
                                  const char* fileNameDllTemp)
{
	GameCode result = {};
	result.initialize       = gameInitializeStub;
	result.onReloadCode     = gameOnReloadCodeStub;
	result.renderAudio      = gameRenderAudioStub;
	result.updateAndDraw    = gameUpdateAndDrawStub;
	result.onPreUnload      = gameOnPreUnloadStub;
	result.dllLastWriteTime = w32GetLastWriteTime(fileNameDll);
	if(result.dllLastWriteTime.dwHighDateTime == 0 &&
		result.dllLastWriteTime.dwLowDateTime == 0)
	{
		return result;
	}
	if(!CopyFileA(fileNameDll, fileNameDllTemp, false))
	{
		if(GetLastError() == ERROR_SHARING_VIOLATION)
		{
			/* if the file is being used by another program, just silently do 
				nothing since it's very likely the other program is cl.exe */
		}
		else
			KLOG(WARNING, "Failed to copy file '%s' to '%s'! GetLastError=%i", 
			     fileNameDll, fileNameDllTemp, GetLastError());
	}
	else
	{
		result.hLib = LoadLibraryA(fileNameDllTemp);
		if(!result.hLib)
		{
			KLOG(ERROR, "Failed to load library '%s'! GetLastError=%i", 
			     fileNameDllTemp, GetLastError());
		}
	}
	if(result.hLib)
	{
		result.initialize = reinterpret_cast<fnSig_gameInitialize*>(
			GetProcAddress(result.hLib, "gameInitialize"));
		if(!result.initialize)
		{
			KLOG(ERROR, "Failed to get proc address 'gameInitialize'! "
			     "GetLastError=%i", GetLastError());
		}
		result.onReloadCode = reinterpret_cast<fnSig_gameOnReloadCode*>(
			GetProcAddress(result.hLib, "gameOnReloadCode"));
		if(!result.onReloadCode)
		{
			// This is only a warning because the game can optionally just not
			//	implement the hot-reload callback. //
			///TODO: detect if `GameCode` has the ability to hot-reload and 
			///      don't perform hot-reloading if this is not the case.
			KLOG(WARNING, "Failed to get proc address 'gameOnReloadCode'! "
			     "GetLastError=%i", GetLastError());
		}
		result.onPreUnload = reinterpret_cast<fnSig_gameOnPreUnload*>(
			GetProcAddress(result.hLib, "gameOnPreUnload"));
		if(!result.onPreUnload)
		{
			// This is only a warning because the game can optionally just not
			//	implement the hot-reload callback. //
			///TODO: detect if `GameCode` has the ability to hot-reload and 
			///      don't perform hot-reloading if this is not the case.
			KLOG(WARNING, "Failed to get proc address 'gameOnPreUnload'! "
			     "GetLastError=%i", GetLastError());
		}
		result.renderAudio = reinterpret_cast<fnSig_gameRenderAudio*>(
			GetProcAddress(result.hLib, "gameRenderAudio"));
		if(!result.renderAudio)
		{
			KLOG(ERROR, "Failed to get proc address 'gameRenderAudio'! "
			     "GetLastError=%i", GetLastError());
		}
		result.updateAndDraw = reinterpret_cast<fnSig_gameUpdateAndDraw*>(
			GetProcAddress(result.hLib, "gameUpdateAndDraw"));
		if(!result.updateAndDraw)
		{
			KLOG(ERROR, "Failed to get proc address 'gameUpdateAndDraw'! "
			     "GetLastError=%i", GetLastError());
		}
		result.isValid = (result.initialize && result.renderAudio && 
		                  result.updateAndDraw);
	}
	if(!result.isValid)
	{
//		KLOG(WARNING, "Game code is invalid!");
		result.initialize    = gameInitializeStub;
		result.onReloadCode  = gameOnReloadCodeStub;
		result.renderAudio   = gameRenderAudioStub;
		result.updateAndDraw = gameUpdateAndDrawStub;
		result.onPreUnload   = gameOnPreUnloadStub;
	}
	return result;
}
internal void w32UnloadGameCode(GameCode& gameCode)
{
	if(gameCode.hLib)
	{
		if(!FreeLibrary(gameCode.hLib))
		{
			KLOG(ERROR, "Failed to free game code dll! GetLastError=", 
			     GetLastError());
		}
		gameCode.hLib = NULL;
	}
	gameCode.isValid = false;
	gameCode.renderAudio   = gameRenderAudioStub;
	gameCode.updateAndDraw = gameUpdateAndDrawStub;
}
internal v2u32 w32GetWindowDimensions(HWND hwnd)
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
internal ButtonState* w32DecodeVirtualKey(GameMouse* gm, WPARAM vKeyCode)
{
	ButtonState* buttonState = nullptr;
	// Virtual-Key Codes: 
	//	https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	switch(vKeyCode)
	{
		case VK_LBUTTON:  buttonState = &gm->left; break;
		case VK_RBUTTON:  buttonState = &gm->right; break;
		case VK_MBUTTON:  buttonState = &gm->middle; break;
		case VK_XBUTTON1: buttonState = &gm->back; break;
		case VK_XBUTTON2: buttonState = &gm->forward; break;
	}
	return buttonState;
}
internal ButtonState* w32DecodeVirtualKey(GameKeyboard* gk, WPARAM vKeyCode)
{
	ButtonState* buttonState = nullptr;
	// Virtual-Key Codes: 
	//	https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	switch(vKeyCode)
	{
		case VK_OEM_COMMA:  buttonState = &gk->comma; break;
		case VK_OEM_PERIOD: buttonState = &gk->period; break;
		case VK_OEM_2:      buttonState = &gk->slashForward; break;
		case VK_OEM_5:      buttonState = &gk->slashBack; break;
		case VK_OEM_4:      buttonState = &gk->curlyBraceLeft; break;
		case VK_OEM_6:      buttonState = &gk->curlyBraceRight; break;
		case VK_OEM_1:      buttonState = &gk->semicolon; break;
		case VK_OEM_7:      buttonState = &gk->quote; break;
		case VK_OEM_3:      buttonState = &gk->grave; break;
		case VK_OEM_MINUS:  buttonState = &gk->tenkeyless_minus; break;
		case VK_OEM_PLUS:   buttonState = &gk->equals; break;
		case VK_BACK:       buttonState = &gk->backspace; break;
		case VK_ESCAPE:     buttonState = &gk->escape; break;
		case VK_RETURN:     buttonState = &gk->enter; break;
		case VK_SPACE:      buttonState = &gk->space; break;
		case VK_TAB:        buttonState = &gk->tab; break;
		case VK_LSHIFT:     buttonState = &gk->shiftLeft; break;
		case VK_RSHIFT:     buttonState = &gk->shiftRight; break;
		case VK_LCONTROL:   buttonState = &gk->controlLeft; break;
		case VK_RCONTROL:   buttonState = &gk->controlRight; break;
		case VK_LMENU:      buttonState = &gk->altLeft; break;
		case VK_RMENU:      buttonState = &gk->altRight; break;
		case VK_UP:         buttonState = &gk->arrowUp; break;
		case VK_DOWN:       buttonState = &gk->arrowDown; break;
		case VK_LEFT:       buttonState = &gk->arrowLeft; break;
		case VK_RIGHT:      buttonState = &gk->arrowRight; break;
		case VK_INSERT:     buttonState = &gk->insert; break;
		case VK_DELETE:     buttonState = &gk->deleteKey; break;
		case VK_HOME:       buttonState = &gk->home; break;
		case VK_END:        buttonState = &gk->end; break;
		case VK_PRIOR:      buttonState = &gk->pageUp; break;
		case VK_NEXT:       buttonState = &gk->pageDown; break;
		case VK_DECIMAL:    buttonState = &gk->numpad_period; break;
		case VK_DIVIDE:     buttonState = &gk->numpad_divide; break;
		case VK_MULTIPLY:   buttonState = &gk->numpad_multiply; break;
		case VK_SEPARATOR:  buttonState = &gk->numpad_minus; break;
		case VK_ADD:        buttonState = &gk->numpad_add; break;
		default:
		{
			if(vKeyCode >= 0x41 && vKeyCode <= 0x5A)
			{
				buttonState = &gk->a + (vKeyCode - 0x41);
			}
			else if(vKeyCode >= 0x30 && vKeyCode <= 0x39)
			{
				buttonState = &gk->tenkeyless_0 + (vKeyCode - 0x30);
			}
			else if(vKeyCode >= 0x70 && vKeyCode <= 0x7B)
			{
				buttonState = &gk->f1 + (vKeyCode - 0x70);
			}
			else if(vKeyCode >= 0x60 && vKeyCode <= 0x69)
			{
				buttonState = &gk->numpad_0 + (vKeyCode - 0x60);
			}
		} break;
	}
	return buttonState;
}
internal void w32GetKeyboardKeyStates(
	GameKeyboard* gkCurrFrame, GameKeyboard* gkPrevFrame)
{
	for(WPARAM vKeyCode = 0; vKeyCode <= 0xFF; vKeyCode++)
	{
		ButtonState* buttonState = w32DecodeVirtualKey(gkCurrFrame, vKeyCode);
		if(!buttonState)
		continue;
		const SHORT asyncKeyState = 
			GetAsyncKeyState(static_cast<int>(vKeyCode));
		// do NOT use the least-significant bit to determine key state!
		//	Why? See documentation on the GetAsyncKeyState function:
		// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate
		const bool keyDown = (asyncKeyState & ~1) != 0;
		const size_t buttonIndex = buttonState - gkCurrFrame->vKeys;
		*buttonState = keyDown
			? (gkPrevFrame->vKeys[buttonIndex] >= ButtonState::PRESSED
				? ButtonState::HELD
				: ButtonState::PRESSED)
			: ButtonState::NOT_PRESSED;
	}
	// update the game keyboard modifier states //
	gkCurrFrame->modifiers.shift =
		(gkCurrFrame->shiftLeft  > ButtonState::NOT_PRESSED ||
		 gkCurrFrame->shiftRight > ButtonState::NOT_PRESSED);
	gkCurrFrame->modifiers.control =
		(gkCurrFrame->controlLeft  > ButtonState::NOT_PRESSED ||
		 gkCurrFrame->controlRight > ButtonState::NOT_PRESSED);
	gkCurrFrame->modifiers.alt =
		(gkCurrFrame->altLeft  > ButtonState::NOT_PRESSED ||
		 gkCurrFrame->altRight > ButtonState::NOT_PRESSED);
}
internal void w32GetMouseStates(GameMouse* gmCurrent, GameMouse* gmPrevious)
{
	if(g_mouseRelativeMode)
	{
		/* in mouse relative mode, we the mouse should remain in the same 
			position in window-space forever */
		gmCurrent->windowPosition = gmPrevious->windowPosition;
	}
	else
	/* get the mouse cursor position in window-space */
	{
		POINT pointMouseCursor;
		/* first, get the mouse cursor position in screen-space */
		if(!GetCursorPos(&pointMouseCursor))
		{
			KLOG(ERROR, "GetCursorPos failure! GetLastError=%i", 
			     GetLastError());
		}
		/* convert mouse position screen-space => window-space */
		if(!ScreenToClient(g_mainWindow, &pointMouseCursor))
		{
			/* for whatever reason, MSDN doesn't say there is any kind of error 
				code obtainable for this failure condition... ¯\_(ツ)_/¯ */
			KLOG(ERROR, "ScreenToClient failure!");
		}
		gmCurrent->windowPosition.x = pointMouseCursor.x;
		gmCurrent->windowPosition.y = pointMouseCursor.y;
	}
	/* get async mouse button states */
	for(WPARAM vKeyCode = 0; vKeyCode <= 0xFF; vKeyCode++)
	{
		ButtonState* buttonState = w32DecodeVirtualKey(gmCurrent, vKeyCode);
		if(!buttonState)
			continue;
		const SHORT asyncKeyState = 
			GetAsyncKeyState(static_cast<int>(vKeyCode));
		// do NOT use the least-significant bit to determine key state!
		//	Why? See documentation on the GetAsyncKeyState function:
		// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate
		const bool keyDown = (asyncKeyState & ~1) != 0;
		const size_t buttonIndex = buttonState - gmCurrent->vKeys;
		*buttonState = keyDown
			? (gmPrevious->vKeys[buttonIndex] >= ButtonState::PRESSED
				? ButtonState::HELD
				: ButtonState::PRESSED)
			: ButtonState::NOT_PRESSED;
	}
}
internal DWORD w32QueryNearestMonitorRefreshRate(HWND hwnd)
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
	if(!EnumDisplaySettingsA(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, 
	                         &monitorDevMode))
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
internal LRESULT CALLBACK w32MainWindowCallback(HWND hwnd, UINT uMsg, 
                                                WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
	LRESULT result = 0;
	switch(uMsg)
	{
		case WM_SYSCOMMAND:
		{
			KLOG(INFO, "WM_SYSCOMMAND: type=0x%x", wParam);
			if(wParam == SC_KEYMENU)
			// These window messages need to be captured in order to stop 
			//	windows from needlessly beeping when we use ALT+KEY combinations 
			{
				break;
			}
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		}break;
#if 0
		case WM_MENUCHAR:
		{
			KLOG(INFO, "WM_MENUCHAR");
			result = MAKELRESULT(0, MNC_IGNORE);
		}break;
#endif // 0
		case WM_SETCURSOR:
		{
			HCURSOR cursor = NULL;
			switch(LOWORD(lParam))
			{
				case HTBOTTOM:
				case HTTOP:
					cursor = g_cursorSizeVertical;
				break;
				case HTLEFT:
				case HTRIGHT:
					cursor = g_cursorSizeHorizontal;
				break;
				case HTBOTTOMLEFT:
				case HTTOPRIGHT:
					cursor = g_cursorSizeNeSw;
				break;
				case HTBOTTOMRIGHT:
				case HTTOPLEFT:
					cursor = g_cursorSizeNwSe;
				break;
				case HTCLIENT:
				default:
					cursor = g_cursorArrow;
				break;
			}
			SetCursor(cursor);
		} break;
		case WM_SIZE:
		{
			KLOG(INFO, "WM_SIZE: type=%i area={%i,%i}", 
			     wParam, LOWORD(lParam), HIWORD(lParam));
		} break;
		case WM_DESTROY:
		{
			///TODO: handle this error: recreate window?
			g_running = false;
			KLOG(INFO, "WM_DESTROY");
		} break;
		case WM_CLOSE:
		{
			///TODO: ask user first before destroying
			g_running = false;
			KLOG(INFO, "WM_CLOSE");
		} break;
		case WM_ACTIVATEAPP:
		{
			g_isFocused = wParam;
			KLOG(INFO, "WM_ACTIVATEAPP: activated=%s threadId=%i",
			     (wParam ? "TRUE" : "FALSE"), lParam);
		} break;
		case WM_DEVICECHANGE:
		{
			KLOG(INFO, "WM_DEVICECHANGE: event=0x%x", wParam);
			switch(wParam)
			{
				case DBT_DEVNODES_CHANGED:
				{
					KLOG(INFO, "\tA device has been added or removed!");
					g_deviceChangeDetected = true;
				} break;
			}
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
		default:
		{
#if 0
			KLOG(INFO, "Window Message uMsg==0x%x", uMsg);
#endif // 0
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		}break;
	}
	return result;
}
internal LARGE_INTEGER w32QueryPerformanceCounter()
{
	LARGE_INTEGER result;
	if(!QueryPerformanceCounter(&result))
	{
		KLOG(ERROR, "Failed to query performance counter! GetLastError=%i", 
		     GetLastError());
	}
	return result;
}
internal f32 w32ElapsedSeconds(const LARGE_INTEGER& previousPerformanceCount, 
                               const LARGE_INTEGER& performanceCount)
{
	korlAssert(performanceCount.QuadPart > previousPerformanceCount.QuadPart);
	const LONGLONG perfCountDiff = 
		performanceCount.QuadPart - previousPerformanceCount.QuadPart;
	const f32 elapsedSeconds = 
		static_cast<f32>(perfCountDiff) / g_perfCounterHz.QuadPart;
	return elapsedSeconds;
}
internal int w32GenerateDump(PEXCEPTION_POINTERS pExceptionPointers)
{
	// copy-pasta from MSDN basically:
	//	https://docs.microsoft.com/en-us/windows/win32/dxtecharts/crash-dump-analysis
	///TODO: maybe make this a little more robust in the future...
	TCHAR szFileName[MAX_PATH]; 
	SYSTEMTIME stLocalTime;
	GetLocalTime( &stLocalTime );
	// Create a companion folder to store PDB files specifically for this 
	//	dump! //
	TCHAR szPdbDirectory[MAX_PATH];
	StringCchPrintf(szPdbDirectory, MAX_PATH, 
				TEXT("%s\\%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld"), 
				g_pathTemp, APPLICATION_VERSION, 
				stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay, 
				stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond, 
				GetCurrentProcessId(), GetCurrentThreadId());
	if(!CreateDirectory(szPdbDirectory, NULL))
	{
		platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
					"Failed to create dir '%s'!  GetLastError=%i",
					szPdbDirectory, GetLastError());
		return EXCEPTION_EXECUTE_HANDLER;
	}
	// Create the mini dump! //
	StringCchPrintf(szFileName, MAX_PATH, 
	                TEXT("%s\\%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp"), 
	                szPdbDirectory, APPLICATION_VERSION, 
	                stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay, 
	                stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond, 
	                GetCurrentProcessId(), GetCurrentThreadId());
	const HANDLE hDumpFile = CreateFile(szFileName, GENERIC_READ|GENERIC_WRITE, 
	                                    FILE_SHARE_WRITE|FILE_SHARE_READ, 
	                                    0, CREATE_ALWAYS, 0, 0);
	MINIDUMP_EXCEPTION_INFORMATION ExpParam = {
		.ThreadId          = GetCurrentThreadId(),
		.ExceptionPointers = pExceptionPointers,
		.ClientPointers    = TRUE,
	};
	const BOOL bMiniDumpSuccessful = 
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), 
		                  hDumpFile, MiniDumpWithDataSegs, &ExpParam, 
		                  NULL, NULL);
	if(bMiniDumpSuccessful)
	{
		g_hasWrittenCrashDump = true;
#if defined(UNICODE) || defined(_UNICODE)
		char ansiFileName[MAX_PATH];
		size_t convertedCharCount;
		wcstombs_s(&convertedCharCount, ansiFileName, sizeof(ansiFileName), 
		           szFileName, sizeof(szFileName));
		///TODO: U16 platformLog
#else
		platformLog("win32-main", __LINE__, PlatformLogCategory::K_INFO,
		            "Successfully wrote mini dump to: '%s'", szFileName);
#endif
		// Attempt to copy the win32 application's pdb file to the dump 
		//	location //
		TCHAR szFileNameCopySource[MAX_PATH];
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s.pdb"),
		                g_pathToExe, APPLICATION_NAME);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\%s.pdb"), 
		                szPdbDirectory, APPLICATION_NAME);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		/* Attempt to copy the win32 application's VC_*.pdb file to the dump 
			location */
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\VC_%s.pdb"),
		                g_pathToExe, APPLICATION_NAME);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\VC_%s.pdb"), 
		                szPdbDirectory, APPLICATION_NAME);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		// Find the most recent game*.pdb file, then place the filename into 
		//	`szFileNameCopySource` //
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s*.pdb"),
		                g_pathToExe, FILE_NAME_GAME_DLL);
		WIN32_FIND_DATA findFileData;
		HANDLE findHandleGameDll = 
			FindFirstFile(szFileNameCopySource, &findFileData);
		if(findHandleGameDll == INVALID_HANDLE_VALUE)
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to begin search for '%s'!  GetLastError=%i",
			            szFileNameCopySource, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		FILETIME creationTimeLatest = findFileData.ftCreationTime;
		TCHAR fileNameGamePdb[MAX_PATH];
		StringCchPrintf(fileNameGamePdb, MAX_PATH, TEXT("%s"),
		                findFileData.cFileName);
		while(BOOL findNextFileResult = 
			FindNextFile(findHandleGameDll, &findFileData))
		{
			if(!findNextFileResult && GetLastError() != ERROR_NO_MORE_FILES)
			{
				platformLog("win32-main", __LINE__, 
				            PlatformLogCategory::K_ERROR,
				            "Failed to find next for '%s'!  GetLastError=%i",
				            szFileNameCopySource, GetLastError());
				return EXCEPTION_EXECUTE_HANDLER;
			}
			if(CompareFileTime(&findFileData.ftCreationTime, 
			                   &creationTimeLatest) > 0)
			{
				creationTimeLatest = findFileData.ftCreationTime;
				StringCchPrintf(fileNameGamePdb, MAX_PATH, TEXT("%s"),
				                findFileData.cFileName);
			}
		}
		if(!FindClose(findHandleGameDll))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to close search for '%s*.pdb'!  "
			            "GetLastError=%i",
			            FILE_NAME_GAME_DLL, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		// attempt to copy game's pdb file //
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s"),
		                g_pathToExe, fileNameGamePdb);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\%s"), 
		                szPdbDirectory, fileNameGamePdb);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		/* attempt to copy the game's VC_*.pdb file */
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\VC_%s"),
		                g_pathToExe, fileNameGamePdb);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\VC_%s"), 
		                szPdbDirectory, fileNameGamePdb);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		// Attempt to copy the win32 EXE file into the dump location //
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s.exe"),
		                g_pathToExe, APPLICATION_NAME);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\%s.exe"), 
		                szPdbDirectory, APPLICATION_NAME);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
	}
    return EXCEPTION_EXECUTE_HANDLER;
}
internal LONG WINAPI w32VectoredExceptionHandler(
                                             PEXCEPTION_POINTERS pExceptionInfo)
{
	g_hasReceivedException = true;
	if(!g_hasWrittenCrashDump)
	{
		w32GenerateDump(pExceptionInfo);
	}
	// break debugger to give us a chance to figure out what the hell happened
	if(IsDebuggerPresent())
	{
		DebugBreak();
	}
	// I don't use the KLOG macro here because `strrchr` from the 
	//	__FILENAME__ macro seems to just break everything :(
	platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
	            "Vectored Exception! 0x%x", 
	            pExceptionInfo->ExceptionRecord->ExceptionCode);
	w32WriteLogToFile();
	///TODO: cleanup system-wide settings/handles, such as the OS timer 
	///      granularity setting!
	ExitProcess(0xDEADC0DE);
	// return EXCEPTION_CONTINUE_SEARCH;
}
#if 0
internal LONG WINAPI w32TopLevelExceptionHandler(
                                             PEXCEPTION_POINTERS pExceptionInfo)
{
	g_hasReceivedException = true;
	fprintf_s(stderr, "Exception! 0x%x\n", 
	          pExceptionInfo->ExceptionRecord->ExceptionCode);
	fflush(stderr);
	//KLOG(ERROR, "Exception! 0x%x", 
	//     pExceptionInfo->ExceptionRecord->ExceptionCode);
	w32WriteLogToFile();
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
DWORD WINAPI w32WorkThread(_In_ LPVOID lpParameter)
{
	W32ThreadInfo*const threadInfo = 
		reinterpret_cast<W32ThreadInfo*>(lpParameter);
	KLOG(INFO, "thread[%i]: started!", threadInfo->index);
	while(true)
	{
		if(!jobQueuePerformWork(threadInfo->jobQueue, threadInfo->index))
		{
			jobQueueWaitForWork(threadInfo->jobQueue);
		}
	}
	return 0;
}
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
					            "GetLastError=%i",
					            fileQueryString, GetLastError());
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
extern int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, 
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
	WCHAR relativeAssetDir[MAX_PATH];
	relativeAssetDir[0] = '\0';
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
					if(FAILED(StringCchPrintfW(relativeAssetDir, MAX_PATH, 
					                           L"%ws", argv[arg])))
					{
						KLOG(ERROR, "Failed to extract relative asset path!");
						return RETURN_CODE_FAILURE;
					}
				}
				else if(arg == 2)
				{
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
	// Locate what directory we should look in for assets //
	{
		if(FAILED(StringCchPrintf(g_pathToAssets, MAX_PATH, 
		                          TEXT("%s\\%wsassets"), 
		                          g_pathToExe, relativeAssetDir)))
		{
			KLOG(ERROR, "Failed to build g_pathToAssets!  g_pathToExe='%s'", 
			     g_pathToExe);
			return RETURN_CODE_FAILURE;
		}
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
		const u8 allocId = static_cast<u8>(
			StaticMemoryAllocationIndex::IMGUI);
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
	GameCode game = w32LoadGameCode(fullPathToGameDll, 
	                                fullPathToGameDllTemp);
	korlAssert(game.isValid);
	if(!QueryPerformanceFrequency(&g_perfCounterHz))
	{
		KLOG(ERROR, "Failed to query for performance frequency! "
		     "GetLastError=%i", GetLastError());
		return RETURN_CODE_FAILURE;
	}
	g_cursorArrow          = LoadCursorA(NULL, IDC_ARROW);
	g_cursorSizeHorizontal = LoadCursorA(NULL, IDC_SIZEWE);
	g_cursorSizeVertical   = LoadCursorA(NULL, IDC_SIZENS);
	g_cursorSizeNeSw       = LoadCursorA(NULL, IDC_SIZENESW);
	g_cursorSizeNwSe       = LoadCursorA(NULL, IDC_SIZENWSE);
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
	g_mainWindow = CreateWindowExA(
		0,
		wndClass.lpszClassName,
		APPLICATION_NAME,
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
	w32KrbOglInitialize(g_mainWindow);
	w32KrbOglSetVSyncPreference(true);
	///TODO: update the monitorRefreshHz and dependent variable realtime when 
	///      the window gets moved around to another monitor.
	u32 monitorRefreshHz = w32QueryNearestMonitorRefreshRate(g_mainWindow);
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
		if(!ImGui_ImplOpenGL2_Init())
		{
			KLOG(ERROR, "ImGui_ImplOpenGL2_Init failure!");
			return RETURN_CODE_FAILURE;
		}
		if(!ImGui_ImplWin32_Init(g_mainWindow))
		{
			KLOG(ERROR, "ImGui_ImplWin32_Init failure!");
			return RETURN_CODE_FAILURE;
		}
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
	game.onReloadCode(gameMemory, false);
	game.initialize(gameMemory);
	w32InitDSound(g_mainWindow, SOUND_SAMPLE_HZ, SOUND_BUFFER_BYTES, 
	              SOUND_CHANNELS, gameSoundCursorWritePrev);
	const HDC hdc = GetDC(g_mainWindow);
	if(!hdc)
	{
		KLOG(ERROR, "Failed to get main window device context!");
		return RETURN_CODE_FAILURE;
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
					if(game.isValid)
					{
						KLOG(INFO, "Unloading game code...");
						game.onPreUnload();
					}
					game.isValid = false;
				}
				if((CompareFileTime(&gameCodeLastWriteTime, 
				                    &game.dllLastWriteTime) 
				    || !game.isValid) 
				    && !jobQueueHasIncompleteJobs(&g_jobQueue))
				{
					w32UnloadGameCode(game);
					game = w32LoadGameCode(fullPathToGameDll, 
					                       fullPathToGameDllTemp);
					if(game.isValid)
					{
						KLOG(INFO, "Game code hot-reload complete!");
						game.onReloadCode(gameMemory, true);
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
			w32GetKeyboardKeyStates(gameKeyboardCurrentFrame, 
			                        gameKeyboardPreviousFrame);
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
			w32DInputGetGamePadStates(g_gamePadArrayCurrentFrame  + 
			                              XUSER_MAX_COUNT, 
			                          g_gamePadArrayPreviousFrame + 
			                              XUSER_MAX_COUNT);
			w32XInputGetGamePadStates(g_gamePadArrayCurrentFrame,
			                          g_gamePadArrayPreviousFrame);
			ImGui_ImplOpenGL2_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			const v2u32 windowDims = w32GetWindowDimensions(g_mainWindow);
			if(!game.isValid)
			// display a "loading" message while we wait for cl.exe to 
			//	relinquish control of the game binary //
			{
				const ImVec2 displayCenter(
					windowDims.x*0.5f, windowDims.y*0.5f);
				ImGui::SetNextWindowPos(displayCenter, ImGuiCond_Always, 
				                        ImVec2(0.5f, 0.5f));
				ImGui::Begin("Reloading Game Code", nullptr, 
				             ImGuiWindowFlags_NoTitleBar |
				             ImGuiWindowFlags_NoResize |
				             ImGuiWindowFlags_NoMove |
				             ImGuiWindowFlags_NoScrollbar |
				             ImGuiWindowFlags_NoScrollWithMouse |
				             ImGuiWindowFlags_AlwaysAutoResize |
				             ImGuiWindowFlags_NoSavedSettings |
				             ImGuiWindowFlags_NoMouseInputs );
				if(jobQueueHasIncompleteJobs(&g_jobQueue))
				{
					ImGui::Text("Waiting for job queue to finish...%c", 
					            "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
				}
				else
				{
					ImGui::Text("Loading Game Code...%c", 
					            "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
				}
				ImGui::End();
			}
			const f32 deltaSeconds = kmath::min(
				MAX_GAME_DELTA_SECONDS, targetSecondsElapsedPerFrame);
			if(!game.updateAndDraw(
					deltaSeconds, windowDims, 
					*gameMouseFrameCurrent, *gameKeyboardCurrentFrame,
					g_gamePadArrayCurrentFrame, CARRAY_SIZE(g_gamePadArrayA), 
					g_isFocused))
			{
				g_running = false;
			}
			w32WriteDSoundAudio(SOUND_BUFFER_BYTES, SOUND_SAMPLE_HZ, 
			                    SOUND_CHANNELS, gameSoundMemory, 
			                    gameSoundCursorWritePrev, game);
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
			// update window graphics //
			ImGui::Render();
			ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
			if(!SwapBuffers(hdc))
			{
				KLOG(ERROR, "Failed to SwapBuffers! GetLastError=%i", 
				     GetLastError());
			}
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
#pragma warning( push )
// warning C4127: conditional expression is constant
#pragma warning( disable : 4127 )
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_impl_opengl2.cpp"
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
