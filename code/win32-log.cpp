#include "win32-log.h"
#include "win32-main.h"
#include "strsafe.h"
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
	platformLog("win32-log", __LINE__, PlatformLogCategory::K_INFO,
	            "Log successfully wrote to '%s'!", fullPathToLog);
	// Once the log file has been finalized, we can delete the temporary file //
	if(!DeleteFileA(fullPathToNewLog))
	{
		platformLog("win32-log", __LINE__, PlatformLogCategory::K_WARNING,
		            "Failed to delete '%s'!", fullPathToNewLog);
	}
}