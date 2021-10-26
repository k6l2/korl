#include "korl-assert.h"
#include "korl-io.h"
#include "korl-windows-globalDefines.h"
korl_internal void korl_assertConditionFailed(
    wchar_t* conditionString, const char* cStringFileName, int lineNumber)
{
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
        /* write failed condition to standard error stream */
        korl_log(ERROR, "ASSERT FAILED: \"%S\" {%i|%s}", 
            conditionString, lineNumber, cStringFileName);
    }
    /** @todo: In the case that we're deploying this application (not running in 
     * a debugger) maybe do one or more of the following things:
     * - offer a choice to continue execution
     * - offer a choice to write a memory dump to disk */
    ExitProcess(KORL_EXIT_FAIL_ASSERT);
}
