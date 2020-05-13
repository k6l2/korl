#include "game.h"
#include "global-defines.h"
// crt io
#include <stdio.h>
#include <ctime>
#include "win32-main.h"
#include "win32-dsound.h"
#include "win32-xinput.h"
#include "win32-krb-opengl.h"
#include "generalAllocator.h"
global_variable bool g_running;
global_variable bool g_displayCursor;
global_variable HCURSOR g_cursor;
global_variable W32OffscreenBuffer g_backBuffer;
global_variable LARGE_INTEGER g_perfCounterHz;
global_variable KgaHandle g_genAllocStbImage;
global_variable const size_t MAX_LOG_BUFFER_SIZE = 128;
global_variable char g_logBeginning[MAX_LOG_BUFFER_SIZE];
global_variable char g_logCircularBuffer[MAX_LOG_BUFFER_SIZE] = {};
global_variable size_t g_logCurrentBeginningChar;
global_variable size_t g_logCurrentCircularBufferChar;
// This represents the total # of character sent to the circular buffer, 
//	including characters that are overwritten!
global_variable size_t g_logCircularBufferRunningTotal;
internal PLATFORM_LOG(platformLog)
{
	// Get a timestamp (Source: https://stackoverflow.com/a/35260146) //
	time_t now = time(nullptr);
	struct tm newTime;
	localtime_s(&newTime, &now);
	char timeBuffer[80];
	strftime(timeBuffer, sizeof(timeBuffer), "%H:%M.%S", &newTime);
	// First, we build the new text to add to the log using a local buffer. //
	local_persist const size_t MAX_LOG_LINE_SIZE = 128;
	char logLineBuffer[MAX_LOG_LINE_SIZE];
	const char*const strCategory = 
		   logCategory == PlatformLogCategory::K_INFO    ? "INFO"
		: (logCategory == PlatformLogCategory::K_WARNING ? "WARNING"
		: (logCategory == PlatformLogCategory::K_ERROR   ? "ERROR"
		: "UNKNOWN-CATEGORY"));
	// Print out the leading meta-data tag for this log entry //
	const int logLineMetaCharsWritten = 
		_snprintf_s(logLineBuffer, MAX_LOG_LINE_SIZE, _TRUNCATE, 
		            "[%s | %s | %s:%i] ", strCategory, timeBuffer, 
		            sourceFileName, sourceFileLineNumber);
	kassert(logLineMetaCharsWritten > 0);
	const size_t remainingLogLineSize = (logLineMetaCharsWritten < 0 ||
		logLineMetaCharsWritten >= MAX_LOG_LINE_SIZE) ? 0
		: MAX_LOG_LINE_SIZE - logLineMetaCharsWritten;
	int logLineCharsWritten = 0;
	if(remainingLogLineSize > 0)// should never fail, but I'm paranoid.
	{
		va_list args;
		va_start(args, formattedString);
		// Print out the actual log entry into our temp line buffer //
		logLineCharsWritten = 
			vsnprintf_s(logLineBuffer + logLineMetaCharsWritten, 
			            remainingLogLineSize, _TRUNCATE, formattedString, args);
		va_end(args);
		if(logLineCharsWritten < 0)
		{
			///TODO: handle overflow case for max characters per log line limit!
#if INTERNAL_BUILD
			OutputDebugStringA("LOG LINE OVERFLOW!\n");
#endif
		}
	}
	// Now that we know what needs to be added to the log, we need to dump this
	//	line buffer into the appropriate log memory location.  First, 
	//	logBeginning gets filled up until it can no longer contain any more 
	//	lines.  For all lines afterwards, we use the logCircularBuffer. //
#if 0
	OutputDebugStringA(logLineBuffer);
#endif
	const size_t totalLogLineSize = 
		logLineMetaCharsWritten + logLineCharsWritten;
	const size_t remainingLogBeginning = 
		MAX_LOG_BUFFER_SIZE - g_logCurrentBeginningChar;
	// check for one extra character here because of the implicit "\n\0"
	if(remainingLogBeginning >= totalLogLineSize + 2)
	// append line to the log beginning //
	{
		const int charsWritten = 
			_snprintf_s(g_logBeginning + g_logCurrentBeginningChar,
			            remainingLogBeginning, _TRUNCATE, 
			            "%s\n", logLineBuffer);
		// we should have written the total log line + "\n". \0 isn't counted!
		kassert(charsWritten == totalLogLineSize + 1);
		g_logCurrentBeginningChar += charsWritten;
	}
	else
	// append line to the log circular buffer //
	{
		// Ensure no more lines can be added to the log beginning buffer:
		g_logCurrentBeginningChar = MAX_LOG_BUFFER_SIZE;
		const size_t remainingLogCircularBuffer = 
			MAX_LOG_BUFFER_SIZE - g_logCurrentCircularBufferChar;
		kassert(remainingLogCircularBuffer > 0);
		// I use `min` here as the second parameter to prevent _snprintf_s from 
		//	preemptively destroying earlier buffer data!
		int charsWritten = 
			_snprintf_s(g_logCircularBuffer + g_logCurrentCircularBufferChar,
			            // +2 here to account for the \n\0 at the end !!!
			            min(remainingLogCircularBuffer, totalLogLineSize + 2), 
			            _TRUNCATE, "%s\n", logLineBuffer);
		if(errno)
		{
			kassert(!"log circular buffer part 1 string print error!");
			///TODO: handle error
		}
		else if(charsWritten < 0)
		// If no errors have occurred, it means we have truncated the 
		//	string (circular buffer has been wrapped) //
		{
			// subtract one here because of the \0 that must be written!
			charsWritten = static_cast<int>(remainingLogCircularBuffer - 1);
		}
		g_logCurrentCircularBufferChar += charsWritten;
		// Attempt to wrap around the circular buffer if we reach the end //
		//@ASSUMPTION: the log circular buffer is >= the local log line buffer
		if(charsWritten == remainingLogCircularBuffer - 1)
		{
			kassert(g_logCurrentCircularBufferChar == MAX_LOG_BUFFER_SIZE - 1);
			g_logCurrentCircularBufferChar = 0;
			// +2 here because we need to make sure to write the \n\0 at the end
			const size_t remainingLogLineBytes = 
				totalLogLineSize + 2 - charsWritten;
			if(remainingLogLineBytes == 1)
			{
				// if we only need to write a null-terminator, we shouldn't make
				//	another call to _snprintf_s requesting a `\n` write!
				*g_logCircularBuffer = '\0';
			}
			const int charsWrittenClipped = remainingLogLineBytes < 2 
				? static_cast<int>(remainingLogLineBytes)
				: _snprintf_s(g_logCircularBuffer, remainingLogLineBytes, 
				              _TRUNCATE, "%s\n", logLineBuffer + charsWritten);
			if(errno)
			{
				kassert(!"log circular buffer part 2 string print error!");
			}
			kassert(charsWrittenClipped >= 0);
			g_logCurrentCircularBufferChar += charsWrittenClipped;
#if INTERNAL_BUILD && 0
			// There should only ever be ONE \0 char in the circular buffer, 
			//	excluding the \0 which should always be present in the last 
			//	character position.  If this isn't true, then something has 
			//	seriously fucked up.
			u32 nullCharCount = 0;
			for(size_t c = 0; c < MAX_LOG_BUFFER_SIZE - 1; c++)
			{
				if(g_logCircularBuffer[c] == '\0')
					nullCharCount++;
				kassert(nullCharCount < 2);
			}
#endif
		}
		// +1 here because of the '\n' at the end of our log line!
		g_logCircularBufferRunningTotal += totalLogLineSize + 1;
	}
}
#if INTERNAL_BUILD
internal void w32DebugPrintLog()
{
	OutputDebugStringA("=========== PRINTING LOG =================\n");
	OutputDebugStringA(g_logBeginning);
	if(g_logCurrentBeginningChar == MAX_LOG_BUFFER_SIZE)
	{
		if(g_logCircularBufferRunningTotal >= MAX_LOG_BUFFER_SIZE - 1)
		{
			OutputDebugStringA("- - - - - - LOG CUT - - - - - - - -\n");
			if(g_logCurrentCircularBufferChar < MAX_LOG_BUFFER_SIZE)
			{
				OutputDebugStringA(g_logCircularBuffer + 
				                   g_logCurrentCircularBufferChar + 1);
			}
		}
		OutputDebugStringA(g_logCircularBuffer);
	}
}
PLATFORM_PRINT_DEBUG_STRING(platformPrintDebugString)
{
	OutputDebugStringA(string);
}
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
		fileSize32 = kmath::safeTruncateU32(fileSize.QuadPart);
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
GAME_RENDER_AUDIO(gameRenderAudioStub)
{
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDrawStub)
{
	///TODO: log error?
	// continue running the application to keep attempting to reload game code!
	return true;
}
internal FILETIME w32GetLastWriteTime(const char* fileName)
{
	FILETIME result = {};
	WIN32_FILE_ATTRIBUTE_DATA fileAttributeData;
	if(!GetFileAttributesExA(fileName, GetFileExInfoStandard, 
	                         &fileAttributeData))
	{
		OutputDebugStringA("w32GetLastWriteTime failed!\n");
		///TODO: handle GetLastError
		return result;
	}
	result = fileAttributeData.ftLastWriteTime;
	return result;
}
internal GameCode w32LoadGameCode(const char* fileNameDll, 
                                  const char* fileNameDllTemp)
{
	GameCode result = {};
	result.renderAudio      = gameRenderAudioStub;
	result.updateAndDraw    = gameUpdateAndDrawStub;
	result.dllLastWriteTime = w32GetLastWriteTime(fileNameDll);
	if(result.dllLastWriteTime.dwHighDateTime == 0 &&
		result.dllLastWriteTime.dwLowDateTime == 0)
	{
		return result;
	}
	if(!CopyFileA(fileNameDll, fileNameDllTemp, false))
	{
		const DWORD copyError = GetLastError();
		switch(copyError)
		{
			case ERROR_FILE_NOT_FOUND:
			{
				OutputDebugStringA("w32LoadGameCode ERROR_FILE_NOT_FOUND\n");
			} break;
			case ERROR_ACCESS_DENIED:
			{
				OutputDebugStringA("w32LoadGameCode ERROR_ACCESS_DENIED\n");
			} break;
			case ERROR_ENCRYPTION_FAILED:
			{
				OutputDebugStringA("w32LoadGameCode ERROR_ENCRYPTION_FAILED\n");
			} break;
			case ERROR_SHARING_VIOLATION:
			{
				OutputDebugStringA("w32LoadGameCode ERROR_SHARING_VIOLATION\n");
			} break;
			default:
			{
				char outputBuffer[256] = {};
				snprintf(outputBuffer, sizeof(outputBuffer), "copyError=%i\n", 
				         copyError);
				OutputDebugStringA(outputBuffer);
			} break;
		}
		///TODO: log GetLastError
	}
	else
	{
		result.hLib = LoadLibraryA(fileNameDllTemp);
	}
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
		OutputDebugStringA("load game dll failed!\n");
		//kassert(!"load game dll failed!");
		///TODO: handle GetLastError
	}
	if(!result.isValid)
	{
		OutputDebugStringA("game dll invalid!\n");
		//kassert(!"game dll invalid!");
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
			kassert(!"free game dll failed!");
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
internal DWORD w32QueryNearestMonitorRefreshRate(HWND hwnd)
{
	local_persist const DWORD DEFAULT_RESULT = 60;
	const HMONITOR nearestMonitor = 
		MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEXA monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFOEXA);
	if(!GetMonitorInfoA(nearestMonitor, &monitorInfo))
	{
		///TODO: log error
		return DEFAULT_RESULT;
	}
	DEVMODEA monitorDevMode;
	monitorDevMode.dmSize = sizeof(DEVMODEA);
	monitorDevMode.dmDriverExtra = 0;
	if(!EnumDisplaySettingsA(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, 
	                         &monitorDevMode))
	{
		///TODO: log error
		return DEFAULT_RESULT;
	}
	if(monitorDevMode.dmDisplayFrequency < 2)
	{
		///TODO: log warning: unknown hardware-defined refresh rate!!!
		return DEFAULT_RESULT;
	}
	return monitorDevMode.dmDisplayFrequency;
}
internal LRESULT CALLBACK w32MainWindowCallback(HWND hwnd, UINT uMsg, 
                                                WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	switch(uMsg)
	{
		case WM_SETCURSOR:
		{
			if(g_displayCursor)
			{
				SetCursor(g_cursor);
			}
			else
			{
				SetCursor(NULL);
			}
		} break;
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
#if 0
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
#endif
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
#if INTERNAL_BUILD && 0
	// how the fuck does _snprintf_s even work???
	{
		local_persist const size_t BUFFER_SIZE = 10;
		char buffer[BUFFER_SIZE];
		int logLineMetaCharsWritten;
		buffer[9] = 69;
		// This will just put a \0 at the end of the buffer.  None of the 
		//	provided formatted string will be written!
		logLineMetaCharsWritten = _snprintf_s(buffer+9, 1, _TRUNCATE, "0123");
		// This will cause "012\0" to be written to the end of buffer, and 
		//	logLineMetaCharsWritten will be -1.  You can infer that param2 - 1
		//	bytes of the provided formatted string have been written!
		logLineMetaCharsWritten = _snprintf_s(buffer+6, 4, _TRUNCATE, "0123");
		// This command will write a null-terminator, and destroy the following
		//	4 characters in the specified range //
		logLineMetaCharsWritten = _snprintf_s(buffer, 5, _TRUNCATE, "");
		// _TRUNCATE doesn't seem to affect the rest of the buffer...
		logLineMetaCharsWritten = _snprintf_s(buffer, 5, _TRUNCATE, "0123");
		// maximum # of bytes we can write to buffer = BUFFER_SIZE - 1.
		// ALWAYS set the second param to either the total byte size of the 
		//	first param, or the size of the fourth param INCLUDING the null-
		//	terminating char at the end. (whichever is smaller!)
		logLineMetaCharsWritten = 
			_snprintf_s(buffer, BUFFER_SIZE, _TRUNCATE, "012345678");
		// logLineMetaCharsWritten == 9
		// buffer == "012345678\0"
		KLOG_INFO("logLineMetaCharsWritten=%i", logLineMetaCharsWritten);
		logLineMetaCharsWritten = _snprintf_s(buffer, 6, _TRUNCATE, "hello");
		// logLineMetaCharsWritten == 5
		// buffer == "hello\0678\0"
		KLOG_INFO("logLineMetaCharsWritten=%i", logLineMetaCharsWritten);
	}
#endif
	KLOG_INFO("START!");
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
	// allocate a dynamic memory arena for STB_IMAGE
	{
		local_persist const SIZE_T STB_IMAGE_MEMORY_BYTES = kmath::megabytes(1);
		VOID*const stbImageMemory = VirtualAlloc(NULL, STB_IMAGE_MEMORY_BYTES, 
		                                         MEM_RESERVE | MEM_COMMIT, 
		                                         PAGE_READWRITE);
		if(!stbImageMemory)
		{
			///TODO: handle GetLastError
			return -1;
		}
		g_genAllocStbImage = kgaInit(stbImageMemory, STB_IMAGE_MEMORY_BYTES);
	}
	GameCode game = w32LoadGameCode(fullPathToGameDll, 
	                                fullPathToGameDllTemp);
	if(!QueryPerformanceFrequency(&g_perfCounterHz))
	{
		///TODO: log GetLastError
		return -1;
	}
	w32LoadXInput();
#if 0
	w32ResizeDibSection(g_backBuffer, 1280, 720);
#endif
#if INTERNAL_BUILD
	g_displayCursor = true;
#endif
	g_cursor = LoadCursorA(NULL, IDC_ARROW);
	const WNDCLASS wndClass = {
		.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
		.lpfnWndProc   = w32MainWindowCallback,
		.hInstance     = hInstance,
		.hCursor       = g_cursor,
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
	w32KrbOglInitialize(mainWindow);
	w32KrbOglSetVSyncPreference(true);
	///TODO: update the monitorRefreshHz and dependent variable realtime when 
	///      the window gets moved around to another monitor.
	u32 monitorRefreshHz = w32QueryNearestMonitorRefreshRate(mainWindow);
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
	gameMemory.krbBeginFrame            = krbBeginFrame;
	gameMemory.krbSetProjectionOrtho    = krbSetProjectionOrtho;
	gameMemory.krbDrawLine              = krbDrawLine;
	gameMemory.krbDrawTri               = krbDrawTri;
	gameMemory.krbDrawTriTextured       = krbDrawTriTextured;
	gameMemory.krbViewTranslate         = krbViewTranslate;
	gameMemory.krbSetModelXform         = krbSetModelXform;
	gameMemory.krbLoadImageZ85          = krbLoadImageZ85;
	gameMemory.krbUseTexture            = krbUseTexture;
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
	///TODO: delete LIMIT_FRAMERATE_USING_TIMER stuff because it doesn't seem to 
	///      work the way I want it to...
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
		LARGE_INTEGER perfCountFrameLimiterPrev = w32QueryPerformanceCounter();
		LARGE_INTEGER perfCountPrev = w32QueryPerformanceCounter();
		u64 clockCyclesPrev = __rdtsc();
		while(g_running)
		{
			// dynamically hot-reload game code!!! //
			{
				const FILETIME gameCodeLastWriteTime = 
					w32GetLastWriteTime(fullPathToGameDll);
				if((CompareFileTime(&gameCodeLastWriteTime, 
				                    &game.dllLastWriteTime) &&
					!(gameCodeLastWriteTime.dwHighDateTime == 0 &&
					gameCodeLastWriteTime.dwLowDateTime    == 0)) ||
					!game.isValid)
				{
					w32UnloadGameCode(game);
					game = w32LoadGameCode(fullPathToGameDll, 
					                       fullPathToGameDllTemp);
					if(game.isValid)
					{
						OutputDebugStringA("Game code hot-reload complete!\n");
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
			const W32Dimension2d windowDims = 
				w32GetWindowDimensions(mainWindow);
			if(!game.updateAndDraw(gameMemory, 
			                       {windowDims.width, windowDims.height}, 
			                       gameKeyboard,
			                       gamePadArrayCurrentFrame, numGamePads))
			{
				g_running = false;
			}
			w32WriteDSoundAudio(SOUND_BUFFER_BYTES, SOUND_SAMPLE_HZ, 
			                    SOUND_CHANNELS, SOUND_LATENCY_SAMPLES, 
			                    runningSoundSample, gameSoundMemory, 
			                    gameMemory, game);
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
			// update window graphics //
			if(!SwapBuffers(hdc))
			{
				///TODO: handle GetLastError
			}
			// enforce targetSecondsElapsedPerFrame //
			///TODO: we still have to Sleep/wait when VSync is on if SwapBuffers
			///      completes too early!!! (like when the double-buffer is not
			///      yet filled up at beginning of execution)
			///TODO: maybe just don't sleep/wait the first frame at all because
			///      the windows message queue needs to be unclogged and game
			///      state memory needs to be initialized.
			if(!w32KrbOglGetVSync())
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
#if SLOW_BUILD
				// send performance measurement to debugger as a string //
				KLOG_INFO("%.2f ms/f | %.2f Mc/f", 
				          elapsedSeconds*1000, clockCycleDiff/1000000.f);
				w32DebugPrintLog();
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
	kassert(kgaUsedBytes(g_genAllocStbImage) == 0);
	return 0;
}
#include "win32-dsound.cpp"
#include "win32-xinput.cpp"
#include "win32-krb-opengl.cpp"
#include "krb-opengl.cpp"
#include "z85.cpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ASSERT(x) kassert(x)
#define STBI_MALLOC(sz)       kgaAlloc(g_genAllocStbImage,sz)
#define STBI_REALLOC(p,newsz) kgaRealloc(g_genAllocStbImage,p,newsz)
#define STBI_FREE(p)          kgaFree(g_genAllocStbImage,p)
#include "stb/stb_image.h"
#include "generalAllocator.cpp"