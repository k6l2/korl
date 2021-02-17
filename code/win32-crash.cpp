#include "win32-crash.h"
#include "win32-log.h"
#include "strsafe.h"
#include <dbghelp.h>
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
	StringCchPrintf(
		szPdbDirectory, MAX_PATH, 
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
internal LONG WINAPI 
	w32VectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
	g_hasReceivedException = true;
	// break debugger to give us a chance to figure out what the hell happened
	if(IsDebuggerPresent())
		DebugBreak();
	if(!g_hasWrittenCrashDump)
		w32GenerateDump(pExceptionInfo);
	// I don't use the KLOG macro here because `strrchr` from the 
	//	__FILENAME__ macro seems to just break everything :(
	platformLog(
		"win32-main", __LINE__, PlatformLogCategory::K_ERROR,
		"Vectored Exception! 0x%x", 
		pExceptionInfo->ExceptionRecord->ExceptionCode);
	w32WriteLogToFile();
	///@todo: cleanup system-wide settings/handles
	///	- OS timer granularity setting
	ExitProcess(0xDEADC0DE);
	// return EXCEPTION_CONTINUE_SEARCH;
}
#if 0
internal LONG WINAPI 
	w32TopLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
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