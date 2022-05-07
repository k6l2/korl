#include "korl-assert.h"
#include "korl-log.h"
#include "korl-windows-globalDefines.h"
#include "korl-interface-platform.h"
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
    /* write failed condition to standard error stream */
    if(!_korl_assert_context.firstAssertLogged)
    {
        /* we use a flag here to only log the first assert failure, which is 
            necessary in the case of the assert being fired from inside the log 
            module itself, and really if the program has failed an assert, we 
            probably only care about the first one anyway, since all execution 
            after this is suspect */
        _korl_assert_context.firstAssertLogged = true;
        korl_log(ERROR, "ASSERT FAILED: \"%ls\" {%ls:%i}", 
                 conditionString, cStringFileName, lineNumber);
    }
    if(IsDebuggerPresent())
    {
        OutputDebugString(_T("KORL ASSERTION FAILED: \""));
        OutputDebugString(conditionString);
        OutputDebugString(_T("\"\n"));
        DebugBreak();
    }
    else
    {
        /* give the user a prompt indicating something went wrong */
        //const int resultMessageBox = 
            MessageBox(
                NULL/*hWnd*/, conditionString, 
                _T("KORL ASSERTION FAILED!"), MB_ICONERROR);
        /* if resultMessageBox is 0, that means an error occurred, which can be 
            obtained via GetLastError(), but we're too deep in critical error 
            handling already for this to be useful */
    }
    //KORL-ISSUE-000-000-028: improve assert fail user experience in production
    ExitProcess(KORL_EXIT_FAIL_ASSERT);
}
