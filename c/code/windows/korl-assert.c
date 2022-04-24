#include "korl-assert.h"
#include "korl-log.h"
#include "korl-windows-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal KORL_PLATFORM_ASSERT_FAILURE(korl_assertConditionFailed)
{
    /* write failed condition to standard error stream */
    korl_log(ERROR, "ASSERT FAILED: \"%ls\" {%i|%ls}", 
        conditionString, lineNumber, cStringFileName);
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
