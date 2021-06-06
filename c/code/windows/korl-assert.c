#include "korl-assert.h"
#include "korl-io.h"
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
        korl_log(ERROR, "ASSERT FAILED: \"%S\" {%i|%s}", 
            conditionString, lineNumber, cStringFileName);
    /** @todo: In the case that we're deploying this application (not running in 
     * a debugger) maybe do one or more of the following things:
     * - give the user a prompt indicating something went wrong
     * - write a memory dump to disk
     * - write failed condition to standard error stream
     * - end the program right now */
}
