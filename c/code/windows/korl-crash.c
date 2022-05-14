#include "korl-crash.h"
#include "korl-windows-globalDefines.h"
#include "korl-file.h"
#include <minidumpapiset.h>
#define _KORL_CRASH_MAX_DUMP_COUNT 16
korl_global_variable bool _korl_crash_hasReceivedException;
korl_global_variable bool _korl_crash_hasWrittenCrashDump;
/** \return the # of matched files */
korl_internal u$ _korl_crash_findOldestFile(const wchar_t* findFileDirectory, const wchar_t* findFileNamePattern, wchar_t* o_filePathOldest, u$ filePathOldestBytes, i$* o_filePathOldestSize)
{
    wchar_t findFilePattern[MAX_PATH];
    if(0 > korl_memory_stringFormatBuffer(findFilePattern, sizeof(findFilePattern),
                                          L"%ws\\%ws", findFileDirectory, findFileNamePattern))
    {
        korl_logLastError("format findFilePattern failed");
        return 0;
    }
    WIN32_FIND_DATA findFileData;
    HANDLE findHandle = FindFirstFile(findFilePattern, &findFileData);
    if(findHandle == INVALID_HANDLE_VALUE)
    {
        if(GetLastError() != ERROR_FILE_NOT_FOUND)
            korl_logLastError("FindFirstFile(%ws) failed", findFilePattern);
        return 0;
    }
    u$ findFileCount = 1;
    FILETIME creationTimeOldest = findFileData.ftCreationTime;
    *o_filePathOldestSize = 
        korl_memory_stringFormatBuffer(o_filePathOldest, filePathOldestBytes,
                                       L"%ws\\%ws", findFileDirectory, findFileData.cFileName);
    korl_assert(*o_filePathOldestSize > 0);
    for(;;)
    {
        const BOOL resultFindNextFile = FindNextFile(findHandle, &findFileData);
        if(!resultFindNextFile)
        {
            korl_assert(GetLastError() == ERROR_NO_MORE_FILES);
            break;
        }
        findFileCount++;
        if(CompareFileTime(&findFileData.ftCreationTime, &creationTimeOldest) < 0)
        {
            creationTimeOldest = findFileData.ftCreationTime;
            *o_filePathOldestSize = 
                korl_memory_stringFormatBuffer(o_filePathOldest, filePathOldestBytes,
                                               L"%ws\\%ws", findFileDirectory, findFileData.cFileName);
            korl_assert(*o_filePathOldestSize > 0);
        }
    }
    if(!FindClose(findHandle))
        korl_logLastError("FindClose failed");
    return findFileCount;
}
/*@TODO: move the crash dump file generation stuff into korl-file & remove the korl_file_getPath API, 
         if it's important to contain _all_ filesystem operations inside the korl-file module (I would prefer this)
    void _korl_file_generateMemoryDump(void* exceptionData, Korl_File_PathType type) */
korl_internal void _korl_crash_generateDump(PEXCEPTION_POINTERS pExceptionPointers)
{
    // derived from MSDN sample code:
    //	https://docs.microsoft.com/en-us/windows/win32/dxtecharts/crash-dump-analysis
    SYSTEMTIME localTime;
    GetLocalTime( &localTime );
    /* create a memory dump directory in the temporary data directory */
    wchar_t dumpDirectory[MAX_PATH];
    if(0 > korl_memory_stringFormatBuffer(dumpDirectory, sizeof(dumpDirectory), 
                                          L"%ws\\%ws", 
                                          korl_file_getPath(KORL_FILE_PATHTYPE_TEMPORARY_DATA), 
                                          L"memory-dumps"))
    {
        korl_log(ERROR, "dumpDirectory failed");
        return;
    }
    if(!CreateDirectory(dumpDirectory, NULL/*default security*/))
        switch(GetLastError())
        {
        case ERROR_ALREADY_EXISTS:
            break;
        case ERROR_PATH_NOT_FOUND:
            korl_log(ERROR, "CreateDirectory(%ws) failed: path not found", dumpDirectory);
            return;
        }
    /* delete the oldest dump folder after we reach some maximum dump count */
    wchar_t filePathOldest[MAX_PATH];
    i$ filePathOldestSize = 0;
    korl_assert(_KORL_CRASH_MAX_DUMP_COUNT > 0);
    while(_KORL_CRASH_MAX_DUMP_COUNT <= _korl_crash_findOldestFile(dumpDirectory, 
                                                                   L"*-*-*-*-*", 
                                                                   filePathOldest, 
                                                                   sizeof(filePathOldest), 
                                                                   &filePathOldestSize))
    {
        /* apparently pFrom needs to be double null-terminated */
        korl_assert(filePathOldestSize < korl_arraySize(filePathOldest) - 1);
        filePathOldest[filePathOldestSize] = L'\0';
        /**/
        korl_log(INFO, "deleting oldest dump folder: %ws", filePathOldest);
        KORL_ZERO_STACK(SHFILEOPSTRUCT, fileOpStruct);
        fileOpStruct.wFunc  = FO_DELETE;
        fileOpStruct.pFrom  = filePathOldest;
        fileOpStruct.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION;
        const int resultFileOpDeleteRecursive = SHFileOperation(&fileOpStruct);
        if(resultFileOpDeleteRecursive != 0)
            korl_log(WARNING, "recursive delete of \"%ws\" failed; result=%i", 
                     filePathOldest, resultFileOpDeleteRecursive);
    }
    // Create a companion folder to store PDB files specifically for this dump! //
    wchar_t pdbDirectory[MAX_PATH];
    if(0 > korl_memory_stringFormatBuffer(pdbDirectory, sizeof(pdbDirectory), 
                                          L"%ws\\%ws-%04d%02d%02d-%02d%02d%02d-%ld-%ld", 
                                          dumpDirectory, KORL_APPLICATION_VERSION, 
                                          localTime.wYear, localTime.wMonth, localTime.wDay, 
                                          localTime.wHour, localTime.wMinute, localTime.wSecond, 
                                          GetCurrentProcessId(), GetCurrentThreadId()))
    {
        korl_log(ERROR, "pdbDirectory failed");
        return;
    }
    if(!CreateDirectory(pdbDirectory, NULL))
    {
        korl_logLastError("CreateDirectory(%ws) failed!", pdbDirectory);
        return;
    }
    // Create the mini dump! //
    wchar_t fileNameMinidump[MAX_PATH];
    if(0 > korl_memory_stringFormatBuffer(fileNameMinidump, sizeof(fileNameMinidump),
                                          L"%s\\%s-%04d%02d%02d-%02d%02d%02d-0x%X-0x%X.dmp", 
                                          pdbDirectory, KORL_APPLICATION_VERSION, 
                                          localTime.wYear, localTime.wMonth, localTime.wDay, 
                                          localTime.wHour, localTime.wMinute, localTime.wSecond, 
                                          GetCurrentProcessId(), GetCurrentThreadId()))
    {
        korl_log(ERROR, "fileNameMinidump failed");
        return;
    }
    const HANDLE hDumpFile = CreateFile(fileNameMinidump, GENERIC_READ|GENERIC_WRITE, 
                                        FILE_SHARE_WRITE|FILE_SHARE_READ, 
                                        0, CREATE_ALWAYS, 0, 0);
    if(INVALID_HANDLE_VALUE == hDumpFile)
    {
        korl_logLastError("CreateFile(%ws) failed!", fileNameMinidump);
        return;
    }
    MINIDUMP_EXCEPTION_INFORMATION ExpParam = 
        { .ThreadId          = GetCurrentThreadId()
        , .ExceptionPointers = pExceptionPointers
        , .ClientPointers    = TRUE };
    if(!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), 
                          hDumpFile, MiniDumpWithDataSegs, &ExpParam, 
                          NULL, NULL))
    {
        korl_logLastError("MiniDumpWriteDump failed!");
        return;
    }
    _korl_crash_hasWrittenCrashDump = true;
    korl_log(INFO, "Crash dump written to: %ws", fileNameMinidump);
    // Attempt to copy the win32 application's pdb file to the dump location //
#if 0//@TODO: do we need these???  Future Kyle here: yeah, this might be useful for development.  A likely scenario: to crash while building something, then make iterations which destroy the files that are in crypto-sync with the .dmp file, leaving the programmer with less data to refer back to when attempting to fix the crash
        TCHAR szFileNameCopySource[MAX_PATH];
        StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s.pdb"),
                        g_pathToExe, APPLICATION_NAME);
        StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\%s.pdb"), 
                        szPdbDirectory, APPLICATION_NAME);
        if(!CopyFile(szFileNameCopySource, szFileName, false))
        {
            platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
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
            platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
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
            platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
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
                platformLog("win32-crash", __LINE__, 
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
            platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
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
            platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
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
            platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
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
            platformLog("win32-crash", __LINE__, PlatformLogCategory::K_ERROR,
                        "Failed to copy '%s' to '%s'!  GetLastError=%i",
                        szFileNameCopySource, szFileName, GetLastError());
            return EXCEPTION_EXECUTE_HANDLER;
        }
#endif
}
korl_internal void _korl_crash_fatalException(PEXCEPTION_POINTERS pExceptionPointers, const wchar_t* cStrOrigin)
{
    if(!_korl_crash_hasWrittenCrashDump)
        _korl_crash_generateDump(pExceptionPointers);
    if(IsDebuggerPresent())
        DebugBreak();
    else
    {
        if(_korl_crash_hasReceivedException)
            return;
        else
        {
            _korl_crash_hasReceivedException = true;
            wchar_t messageBuffer[256];
            korl_memory_stringFormatBuffer(messageBuffer, sizeof(messageBuffer), 
                                           L"Exception code: 0x%X\n"
                                           L"Would you like to ignore it?", 
                                           pExceptionPointers->ExceptionRecord->ExceptionCode);
            const int resultMessageBox = MessageBox(NULL/*no owner window*/, 
                                                    messageBuffer, cStrOrigin, 
                                                    MB_YESNO | MB_ICONERROR | MB_SYSTEMMODAL);
            if(resultMessageBox == 0)
                korl_logLastError("MessageBox failed!");
            else
                switch(resultMessageBox)
                {
                case IDYES:{
                    return;
                    break;}
                case IDNO:{
                    /* just do nothing, write a dump & abort */
                    break;}
                }
        }
    }
    korl_log(ERROR, "Fatal Exception; \"%ws\"! ExceptionCode=0x%x", 
             cStrOrigin, pExceptionPointers->ExceptionRecord->ExceptionCode);
    korl_log_shutDown();
    ExitProcess(KORL_EXIT_FAIL_EXCEPTION);
}
korl_internal LONG _korl_crash_vectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionPointers)
{
    /* The vectored exception handler is the first line of defence; we're going 
        to be getting ALL exceptions immediately, and some of which are being 
        issued by code we consume (such as Vulkan) which uses try/catch logic to 
        throw exceptions and handle their own internal control flow based on the 
        caught exceptions.  This is unfortunate, because we _want_ to be able to 
        just capture _all_ exceptions thrown and treat them all as fatal errors, 
        in favor of just using classic error handling mechanisms since they have 
        significaly less overhead than modern "exception handling" control flow.  
        Ergo, we must first choose to process a limited subset of exception 
        codes that are _known_ to be critical errors, and _hope_ that the 
        unhandled exception filter can catch the rest... 🤡  Source that shows 
        an example of why this is important: https://stackoverflow.com/q/19656946 */
    const wchar_t* cStrError = NULL;
    switch(pExceptionPointers->ExceptionRecord->ExceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:{         cStrError = L"EXCEPTION_ACCESS_VIOLATION";         break;}
    case EXCEPTION_DATATYPE_MISALIGNMENT:{    cStrError = L"EXCEPTION_DATATYPE_MISALIGNMENT";    break;}
    case EXCEPTION_BREAKPOINT:{               cStrError = L"EXCEPTION_BREAKPOINT";               break;}
    case EXCEPTION_SINGLE_STEP:{              cStrError = L"EXCEPTION_SINGLE_STEP";              break;}
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:{    cStrError = L"EXCEPTION_ARRAY_BOUNDS_EXCEEDED";    break;}
    case EXCEPTION_FLT_DENORMAL_OPERAND:{     cStrError = L"EXCEPTION_FLT_DENORMAL_OPERAND";     break;}
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:{       cStrError = L"EXCEPTION_FLT_DIVIDE_BY_ZERO";       break;}
    case EXCEPTION_FLT_INEXACT_RESULT:{       cStrError = L"EXCEPTION_FLT_INEXACT_RESULT";       break;}
    case EXCEPTION_FLT_INVALID_OPERATION:{    cStrError = L"EXCEPTION_FLT_INVALID_OPERATION";    break;}
    case EXCEPTION_FLT_OVERFLOW:{             cStrError = L"EXCEPTION_FLT_OVERFLOW";             break;}
    case EXCEPTION_FLT_STACK_CHECK:{          cStrError = L"EXCEPTION_FLT_STACK_CHECK";          break;}
    case EXCEPTION_FLT_UNDERFLOW:{            cStrError = L"EXCEPTION_FLT_UNDERFLOW";            break;}
    case EXCEPTION_INT_DIVIDE_BY_ZERO:{       cStrError = L"EXCEPTION_INT_DIVIDE_BY_ZERO";       break;}
    case EXCEPTION_INT_OVERFLOW:{             cStrError = L"EXCEPTION_INT_OVERFLOW";             break;}
    case EXCEPTION_PRIV_INSTRUCTION:{         cStrError = L"EXCEPTION_PRIV_INSTRUCTION";         break;}
    case EXCEPTION_IN_PAGE_ERROR:{            cStrError = L"EXCEPTION_IN_PAGE_ERROR";            break;}
    case EXCEPTION_ILLEGAL_INSTRUCTION:{      cStrError = L"EXCEPTION_ILLEGAL_INSTRUCTION";      break;}
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:{ cStrError = L"EXCEPTION_NONCONTINUABLE_EXCEPTION"; break;}
    case EXCEPTION_STACK_OVERFLOW:{           cStrError = L"EXCEPTION_STACK_OVERFLOW";           break;}
    case EXCEPTION_INVALID_DISPOSITION:{      cStrError = L"EXCEPTION_INVALID_DISPOSITION";      break;}
    case EXCEPTION_GUARD_PAGE:{               cStrError = L"EXCEPTION_GUARD_PAGE";               break;}
    case EXCEPTION_INVALID_HANDLE:{           cStrError = L"EXCEPTION_INVALID_HANDLE";           break;}
  //case EXCEPTION_POSSIBLE_DEADLOCK:{        cStrError = L"EXCEPTION_POSSIBLE_DEADLOCK";        break;}
    case STATUS_HEAP_CORRUPTION:{             cStrError = L"STATUS_HEAP_CORRUPTION";             break;}
    default:
#if 1
        return EXCEPTION_CONTINUE_SEARCH;
#else//@TODO: development scaffolding; delete before merging into master
        break;
#endif
    }
    /* if we get to this point, the exception should be considered fatal */
    _korl_crash_fatalException(pExceptionPointers, cStrError);
    return EXCEPTION_CONTINUE_SEARCH;
}
korl_internal LONG _korl_crash_unhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionPointers)
{
    /* ALL unhandled exceptions should be considered fatal. */
    _korl_crash_fatalException(pExceptionPointers, L"Unhandled_KORL_Exception");
    return EXCEPTION_CONTINUE_SEARCH;
}
korl_internal void korl_crash_initialize(void)
{
    _korl_crash_hasReceivedException = false;
    /* set up the unhandled exception filter */
    const LPTOP_LEVEL_EXCEPTION_FILTER exceptionFilterPrevious = 
        SetUnhandledExceptionFilter(_korl_crash_unhandledExceptionFilter);
    if(exceptionFilterPrevious != NULL)
        korl_log(INFO, "replaced previous exception filter 0x%X with "
                 "_korl_crash_unhandledExceptionFilter", exceptionFilterPrevious);
    korl_assert(NULL != AddVectoredExceptionHandler(TRUE/*we want this exception handler to fire before all others*/, 
                                                    _korl_crash_vectoredExceptionHandler));
    /* Reserve stack space for stack overflow exceptions */
    // Experimentally, this is about the minimum # of reserved stack bytes
    //	required to carry out my debug dump/log routines when a stack 
    //	overflow exception occurs.
    const ULONG reservedStackBytesDesired = korl_checkCast_u$_to_u32(korl_math_kilobytes(32) + 1);
    ULONG stackSizeBytes = reservedStackBytesDesired;
    if(SetThreadStackGuarantee(&stackSizeBytes))
        korl_log(INFO, "Previous reserved stack=%ld, new reserved stack=%ld", 
                 stackSizeBytes, reservedStackBytesDesired);
    else
        korl_log(WARNING, "Failed to set & retrieve reserved stack size!  "
                          "The system probably won't be able to log stack "
                          "overflow exceptions.");
}
