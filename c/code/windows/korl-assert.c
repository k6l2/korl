#include "korl-assert.h"
#include "korl-log.h"
#include "korl-windows-globalDefines.h"
#include "korl-interface-platform.h"
#include "korl-file.h"
#include "korl-crash.h"
#include "korl-windows-utilities.h"
typedef struct _Korl_Assert_Context
{
    bool firstAssertLogged;
} _Korl_Assert_Context;
korl_global_variable _Korl_Assert_Context _korl_assert_context;
korl_internal void korl_assert_initialize(void)
{
    korl_memory_zero(&_korl_assert_context, sizeof(_korl_assert_context));
}
korl_internal KORL_PLATFORM_ASSERT_FAILURE(korl_assertConditionFailed)
{
    const bool isFirstAssert = !_korl_assert_context.firstAssertLogged;
    /* write failed condition to standard error stream */
    if(!_korl_assert_context.firstAssertLogged)
    {
        /* we use a flag here to only log the first assert failure, which is 
            necessary in the case of the assert being fired from inside the log 
            module itself, and really if the program has failed an assert, we 
            probably only care about the first one anyway, since all execution 
            after this is suspect */
        _korl_assert_context.firstAssertLogged = true;
        __try
        {
            RaiseException(STATUS_ASSERTION_FAILURE, 0, 0, NULL);
        }
        __except(korl_file_generateMemoryDump(GetExceptionInformation(), 
                                              KORL_FILE_PATHTYPE_TEMPORARY_DATA, 
                                              KORL_CRASH_MAX_DUMP_COUNT), 
                 EXCEPTION_EXECUTE_HANDLER)
        {
            _korl_log_variadic(1, KORL_LOG_LEVEL_ASSERT, 
                               cStringFileName, cStringFunctionName, lineNumber, 
                               L"ASSERT FAILED: \"%ws\"", conditionString);
        }
    }
    if(IsDebuggerPresent())
    {
        OutputDebugString(_T("KORL ASSERTION FAILED: \""));
        OutputDebugString(conditionString);
        OutputDebugString(_T("\"\n"));
        DebugBreak();
    }
    else if(isFirstAssert)
    {
        wchar_t messageBuffer[256];
        korl_memory_stringFormatBuffer(messageBuffer, sizeof(messageBuffer), 
                                       L"Condition: %ws\n"
                                       L"Would you like to ignore it?", 
                                       conditionString);
        const int resultMessageBox = MessageBox(NULL/*no owner window*/, 
                                                messageBuffer, L"Assertion Failed!", 
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
    korl_log_shutDown();
    ExitProcess(KORL_EXIT_FAIL_ASSERT);
    //KORL-ISSUE-000-000-056: assert: this code is extremely similar to code inside of korl-crash; maybe merge korl-assert into korl-crash?
}
