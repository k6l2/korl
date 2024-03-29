#include "korl-crash.h"
#include "korl-windows-globalDefines.h"
#include "korl-file.h"
korl_global_const u32 _KORL_CRASH_MAX_DUMP_COUNT = 8;
korl_global_variable bool _korl_crash_firstAssertLogged;
korl_global_variable bool _korl_crash_hasReceivedException;
korl_global_variable bool _korl_crash_hasWrittenCrashDump;
korl_global_variable bool _korl_crash_pendingFatalException;
korl_global_variable bool _korl_crash_pendingAssert;
korl_internal LONG _korl_crash_fatalException(PEXCEPTION_POINTERS pExceptionPointers, const wchar_t* cStrOrigin)
{
    if(_korl_crash_pendingFatalException)
        return EXCEPTION_CONTINUE_SEARCH;
    _korl_crash_pendingFatalException = true;
    /* when we're debugging, it's annoying having to wait a few seconds for the 
        minidump to write to disk, so let's just break right away */
    if(IsDebuggerPresent())
    {
        OutputDebugString(_T("KORL FATAL EXCEPTION: \""));
        OutputDebugString(cStrOrigin);
        OutputDebugString(_T("\"\n"));
        DebugBreak();
    }
    if(!_korl_crash_hasWrittenCrashDump)
    {
        korl_file_generateMemoryDump(pExceptionPointers, KORL_FILE_PATHTYPE_TEMPORARY_DATA, _KORL_CRASH_MAX_DUMP_COUNT);
        _korl_crash_hasWrittenCrashDump = true;
    }
    korl_log(ASSERT, "Fatal Exception; \"%ws\"! ExceptionCode=0x%x", 
             cStrOrigin, pExceptionPointers->ExceptionRecord->ExceptionCode);
    if(!_korl_crash_hasReceivedException)
    {
        _korl_crash_hasReceivedException = true;
        wchar_t messageBuffer[256];
        korl_string_formatBufferUtf16(messageBuffer, sizeof(messageBuffer)
                                     ,L"Exception code: 0x%X\n"
                                     ,pExceptionPointers->ExceptionRecord->ExceptionCode);
        const int resultMessageBox = MessageBox(NULL/*no owner window*/, 
                                                messageBuffer, cStrOrigin, 
                                                MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        if(resultMessageBox == 0)
            korl_logLastError("MessageBox failed!");
        else
            switch(resultMessageBox)
            {
            case IDOK:{
                /* just do nothing, write a dump & abort */
                break;}
            }
    }
    korl_log_shutDown();
    _korl_crash_pendingFatalException = false;
    ExitProcess(KORL_EXIT_FAIL_EXCEPTION);
    // return EXCEPTION_CONTINUE_SEARCH;// unreachable code
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
        return EXCEPTION_CONTINUE_SEARCH;
    }
    /* if we get to this point, the exception should be considered fatal */
    return _korl_crash_fatalException(pExceptionPointers, cStrError);
}
korl_internal LONG _korl_crash_unhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionPointers)
{
    /* ALL unhandled exceptions should be considered fatal. */
    return _korl_crash_fatalException(pExceptionPointers, L"Unhandled_KORL_Exception");
}
korl_internal void korl_crash_initialize(void)
{
    _korl_crash_firstAssertLogged     = false;
    _korl_crash_hasReceivedException  = false;
    _korl_crash_hasWrittenCrashDump   = false;
    _korl_crash_pendingFatalException = false;
    _korl_crash_pendingAssert         = false;
    /* set up the unhandled exception filter */
    const LPTOP_LEVEL_EXCEPTION_FILTER exceptionFilterPrevious = 
        SetUnhandledExceptionFilter(_korl_crash_unhandledExceptionFilter);
    if(exceptionFilterPrevious != NULL)
        korl_log(INFO, "replaced previous exception filter 0x%X with "
                 "_korl_crash_unhandledExceptionFilter", exceptionFilterPrevious);
    /* set up the vectored exception handler */
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
korl_internal bool korl_crash_pending(void)
{
    return _korl_crash_pendingFatalException || _korl_crash_pendingAssert;
}
korl_internal KORL_FUNCTION__korl_crash_assertConditionFailed(_korl_crash_assertConditionFailed)
{
    if(_korl_crash_pendingAssert)
        return;
    _korl_crash_pendingAssert = true;
    const bool isFirstAssert = !_korl_crash_firstAssertLogged;
    /* when we're debugging, it's annoying having to wait a few seconds for the 
        minidump to write to disk, so let's just break right away */
    if(IsDebuggerPresent())
    {
        OutputDebugString(_T("KORL ASSERTION FAILED: \""));
        OutputDebugString(conditionString);
        OutputDebugString(_T("\"\n"));
        DebugBreak();
    }
    /* write failed condition to standard error stream */
    if(!_korl_crash_firstAssertLogged)
    {
        /* we use a flag here to only log the first assert failure, which is 
            necessary in the case of the assert being fired from inside the log 
            module itself, and really if the program has failed an assert, we 
            probably only care about the first one anyway, since all execution 
            after this is suspect */
        _korl_crash_firstAssertLogged = true;
        /* So for some fucking reason, __try/__except doesn't seem to work when 
            in a debugger.  More specifically, the __except filter expression 
            never seems to execute.  Thus, if we're in a debugger we have no 
            choice at the moment but to forego having exception information in 
            the minidump...  At least until I figure out if such a thing is even 
            possible to do, which I'm sure there has to be a way since that 
            would be just silly if it was not possible to obtain full stack 
            exception information just because we're in a debugger lol... */
        if(IsDebuggerPresent())
            korl_file_generateMemoryDump(NULL/*we don't have any exception data*/, 
                                         KORL_FILE_PATHTYPE_TEMPORARY_DATA, 
                                         _KORL_CRASH_MAX_DUMP_COUNT);
        else
            __try
            {
                RaiseException(STATUS_ASSERTION_FAILURE, 0, 0, NULL);
            }
            __except(korl_file_generateMemoryDump(GetExceptionInformation(), 
                                                  KORL_FILE_PATHTYPE_TEMPORARY_DATA, 
                                                  _KORL_CRASH_MAX_DUMP_COUNT), 
                     EXCEPTION_EXECUTE_HANDLER)
            {
            }
    }
    _korl_log_variadic(1, KORL_LOG_LEVEL_ASSERT, 
                       cStringFileName, cStringFunctionName, lineNumber, 
                       L"ASSERT FAILED: \"%ws\"", conditionString);
    if(isFirstAssert)
    {
        const wchar_t BASE_MESSAGE[] = L"Condition: %ws\n";
        wchar_t messageBuffer[512];
        i$ charactersCopied = korl_string_formatBufferUtf16(messageBuffer, sizeof(messageBuffer)
                                                           ,BASE_MESSAGE
                                                           ,conditionString);
        /* if the entire assert conditionString doesn't fit in our local assert 
            message stack buffer, then let's just truncate the message and 
            display as much as possible */
        if(charactersCopied <= 0)
        {
            wchar_t conditionBuffer[512 - korl_arraySize(BASE_MESSAGE)];
            korl_string_copyUtf16(conditionString, (au16){korl_arraySize(conditionBuffer), conditionBuffer});
            charactersCopied = korl_string_formatBufferUtf16(messageBuffer, sizeof(messageBuffer)
                                                            ,BASE_MESSAGE
                                                            ,conditionBuffer);
        }
        const int resultMessageBox = MessageBox(NULL/*no owner window*/
                                               ,messageBuffer, L"Assertion Failed!"
                                               ,MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        if(resultMessageBox == 0)
            korl_logLastError("MessageBox failed!");
        else
            switch(resultMessageBox)
            {
            case IDOK:{
                /* just do nothing, write a dump & abort */
                break;}
            }
    }
    korl_log_shutDown();
    _korl_crash_pendingAssert = false;
    ExitProcess(KORL_EXIT_FAIL_ASSERT);
}
