#include "korl-io.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-memory.h"
korl_internal unsigned _korl_countFormatSubstitutions(const wchar_t* format)
{
    /* find the number of variable substitutions in the format string */
    unsigned formatSubstitutions = 0;
    for(const wchar_t* f = format; *f; f++)
    {
        if(*f != '%')
            continue;
        const wchar_t nextChar = *(f+1);
        /* for now, let's just make it illegal to have a trailing '%' at the end 
            of the format string */
        korl_assert(nextChar);
        if(nextChar == '%')
            /* f is currently pointing to an escape character sequence 
                for the '%' character, so we should skip both 
                characters */
            f++;
        else
            /* if the current character is a '%', and there is a next 
                character, and the next character is NOT a '%', that 
                means this must be a variable substitution */
            formatSubstitutions++;
    }
    return formatSubstitutions;
}
/** \return \c NULL if the specified stream has not been redirected.  
 * \c INVALID_HANDLE_VALUE if an invalid value was passed to 
 * \c KorlEnumStandardStream */
korl_internal HANDLE _korl_windows_handleFromEnumStandardStream(
    enum KorlEnumStandardStream standardStream)
{
    DWORD lastError;
    HANDLE result = INVALID_HANDLE_VALUE;
    switch(standardStream)
    {
    case KORL_STANDARD_STREAM_OUT: {
        result = GetStdHandle(STD_OUTPUT_HANDLE);
        } break;
    case KORL_STANDARD_STREAM_ERROR: {
        result = GetStdHandle(STD_ERROR_HANDLE);
        } break;
    case KORL_STANDARD_STREAM_IN: {
        result = GetStdHandle(STD_INPUT_HANDLE);
        } break;
    }
    /* save the result of GetLastError to the stack in case we need to examine a 
        memory dump that has a stack trace or somethin ;) */
    if(result == INVALID_HANDLE_VALUE)
        lastError = GetLastError();
    korl_assert(result != INVALID_HANDLE_VALUE);
    return result;
}
korl_internal bool _korl_printVaList_variableLengthStackString(
    unsigned stackStringBufferSize, enum KorlEnumStandardStream standardStream, 
    unsigned variadicArgumentCount, const wchar_t* format, va_list vaList)
{
    const unsigned formatSubstitutions = _korl_countFormatSubstitutions(format);
    korl_assert(variadicArgumentCount == formatSubstitutions);
    korl_assert(stackStringBufferSize > 0);
    wchar_t*const stackStringBuffer = 
        _alloca(stackStringBufferSize * sizeof(wchar_t));
    /* attempt to write the formatted string to a buffer on the stack */
    korl_assert(stackStringBufferSize <= STRSAFE_MAX_CCH);
    const HRESULT hResultPrintToStackBuffer = 
        StringCchVPrintf(
            stackStringBuffer, stackStringBufferSize, format, vaList);
    korl_assert(hResultPrintToStackBuffer != STRSAFE_E_INVALID_PARAMETER);
    if(hResultPrintToStackBuffer == STRSAFE_E_INSUFFICIENT_BUFFER)
        return false;
    korl_assert(hResultPrintToStackBuffer == S_OK);
    /* write the final buffer to the desired KorlEnumStandardStream */
    const HANDLE hStream = 
        _korl_windows_handleFromEnumStandardStream(standardStream);
    korl_assert(hStream != NULL);
    korl_assert(hStream != INVALID_HANDLE_VALUE);
    size_t finalBufferSize = 0;
    const HRESULT resultGetFinalBufferSize = 
        StringCchLength(
            stackStringBuffer, stackStringBufferSize, &finalBufferSize);
    korl_assert(resultGetFinalBufferSize == S_OK);
    // check whether or not hStream is a "console handle" //
    DWORD consoleMode = 0;
    const BOOL successGetConsoleMode = GetConsoleMode(hStream, &consoleMode);
    if(successGetConsoleMode)
    {
        DWORD numCharsWritten = 0;
        const BOOL successWriteConsole = 
            WriteConsole(
                hStream, stackStringBuffer, 
                korl_windows_sizet_to_dword(finalBufferSize), 
                &numCharsWritten, NULL/*reserved; always NULL*/);
        /* save the result of GetLastError to the stack in case we need to 
            examine a memory dump that has a stack trace or somethin ;) */
        DWORD lastError;
        if(!successWriteConsole)
            lastError = GetLastError();
        korl_assert(successWriteConsole);
    }
#if 0
    /* we're in the debugger, and the output stream isn't a console, which most 
        likely means we're printing to the visual studio debug output window, 
        which has special encoding requirements :( */
    else if(IsDebuggerPresent())
    {
        OutputDebugString(stackStringBuffer);
    }
#endif//0
    else
    {
        DWORD bytesWritten = 0;
        const BOOL successWriteFile = 
            WriteFile(
                hStream, stackStringBuffer, 
                korl_windows_sizet_to_dword(
                    finalBufferSize*sizeof(stackStringBuffer[0])), 
                &bytesWritten, 
                NULL/* OVERLAPPED*: NULL == we're not doing async I/O here */);
        /* save the result of GetLastError to the stack in case we need to 
            examine a memory dump that has a stack trace or somethin ;) */
        DWORD lastError;
        if(!successWriteFile)
            lastError = GetLastError();
        korl_assert(successWriteFile);
    }
    return true;
}
korl_internal void _korl_printVaList(
    enum KorlEnumStandardStream standardStream, unsigned variadicArgumentCount, 
    const wchar_t* format, va_list vaList)
{
    const unsigned formatSubstitutions = _korl_countFormatSubstitutions(format);
    korl_assert(variadicArgumentCount == formatSubstitutions);
    /* Now, we're going to do some stupid shit here.  Because Windows API is 
        insanely dumb and doesn't allow us to calculate how many characters a 
        buffer will need to hold a formatted string, all we can do is 
        continuously attempt to format the string using larger and larger 
        buffers until the operation actually succeeds. */
    unsigned stackStringSize = 64;
    bool success = false;
    while(!success)
    {
        va_list vaListCopy;
        va_copy(vaListCopy, vaList);
        success = 
            _korl_printVaList_variableLengthStackString(
                stackStringSize, standardStream, variadicArgumentCount, 
                format, vaList);
        va_end(vaListCopy);
        stackStringSize *= 2;
    }
}
korl_internal void korl_printVariadicArguments(unsigned variadicArgumentCount, 
    enum KorlEnumStandardStream standardStream, const wchar_t* format, ...)
{
    const unsigned formatSubstitutions = _korl_countFormatSubstitutions(format);
    korl_assert(variadicArgumentCount == formatSubstitutions);
    /* print the formatted text to the desired standard handle */
    va_list vaList;
    va_start(vaList, format);
    _korl_printVaList(standardStream, variadicArgumentCount, format, vaList);
    va_end(vaList);
}
/** \return \c true if the formatted string could be stored within a buffer of 
 * size \c stackStringBufferSize and \c false otherwise.
 */
korl_internal bool _korl_logVaList_variableLengthStackString(
    unsigned stackStringBufferSize, unsigned variadicArgumentCount, 
    enum KorlEnumLogLevel logLevel, const wchar_t* cStringFileName, 
    const wchar_t* cStringFunctionName, int lineNumber, const wchar_t* format, 
    va_list vaList)
{
    const unsigned formatSubstitutions = _korl_countFormatSubstitutions(format);
    korl_assert(variadicArgumentCount == formatSubstitutions);
    korl_assert(stackStringBufferSize > 0);
    wchar_t*const stackStringBuffer = 
        _alloca(stackStringBufferSize * sizeof(wchar_t));
    /* attempt to write the formatted string to the local buffer */
    korl_assert(stackStringBufferSize <= STRSAFE_MAX_CCH);
    const HRESULT hResultPrintToStackBuffer = 
        StringCchVPrintf(
            stackStringBuffer, stackStringBufferSize, format, vaList);
    korl_assert(hResultPrintToStackBuffer != STRSAFE_E_INVALID_PARAMETER);
    if(hResultPrintToStackBuffer == STRSAFE_E_INSUFFICIENT_BUFFER)
        return false;
    korl_assert(hResultPrintToStackBuffer == S_OK);
    /* print the final formatted string to the appropriate output, as well as a 
        tag containing all the provided meta data about the log message */
    const char* cStringLogLevel = "???";
    switch(logLevel)
    {
    case KORL_LOG_LEVEL_INFO:    { cStringLogLevel = "INFO";    }break;
    case KORL_LOG_LEVEL_WARNING: { cStringLogLevel = "WARNING"; }break;
    case KORL_LOG_LEVEL_ERROR:   { cStringLogLevel = "ERROR";   }break;
    case KORL_LOG_LEVEL_VERBOSE: { cStringLogLevel = "VERBOSE"; }break;
    }
    // only print the file name, not the full path!
    for(const wchar_t* fileNameCursor = cStringFileName; *fileNameCursor; 
    fileNameCursor++)
        if(*fileNameCursor == '\\' || *fileNameCursor == '/')
            cStringFileName = fileNameCursor + 1;
    // determine which standard stream we need to print to based on log level
    const enum KorlEnumStandardStream printStream = 
        logLevel <= KORL_LOG_LEVEL_ERROR 
            ? KORL_STANDARD_STREAM_ERROR
            : KORL_STANDARD_STREAM_OUT;
    // get the current time //
    KORL_ZERO_STACK(SYSTEMTIME, systemTimeLocal);
    GetLocalTime(&systemTimeLocal);
    // print out the the log entry alone side the meta data //
    korl_print(printStream, L"{%-7s|%02i:%02i'%02i\"%03i|%5i|%S|%S} %S\n", 
        cStringLogLevel, systemTimeLocal.wHour, systemTimeLocal.wMinute, 
        systemTimeLocal.wSecond, systemTimeLocal.wMilliseconds, lineNumber, 
        cStringFileName, cStringFunctionName, stackStringBuffer);
    /* if we ever log an error while a debugger is attached, just break right 
        now so we can figure out what's going on! */
    if(IsDebuggerPresent() && logLevel <= KORL_LOG_LEVEL_ERROR)
        DebugBreak();
    return true;
}
korl_internal void _korl_logVaList(
    unsigned variadicArgumentCount, enum KorlEnumLogLevel logLevel, 
    const wchar_t* cStringFileName, const wchar_t* cStringFunctionName, 
    int lineNumber, const wchar_t* format, va_list vaList)
{
    const unsigned formatSubstitutions = _korl_countFormatSubstitutions(format);
    korl_assert(variadicArgumentCount == formatSubstitutions);
    /* Now, we're going to do some stupid shit here.  Because Windows API is 
        insanely dumb and doesn't allow us to calculate how many characters a 
        buffer will need to hold a formatted string, all we can do is 
        continuously attempt to format the string using larger and larger 
        buffers until the operation actually succeeds. */
    unsigned stackStringSize = 64;
    bool success = false;
    while(!success)
    {
        va_list vaListCopy;
        va_copy(vaListCopy, vaList);
        success = 
            _korl_logVaList_variableLengthStackString(
                stackStringSize, variadicArgumentCount, logLevel, 
                cStringFileName, cStringFunctionName, lineNumber, format, 
                vaList);
        va_end(vaListCopy);
        stackStringSize *= 2;
    }
}
korl_internal void korl_logVariadicArguments(
    unsigned variadicArgumentCount, enum KorlEnumLogLevel logLevel, 
    const wchar_t* cStringFileName, const wchar_t* cStringFunctionName, 
    int lineNumber, const wchar_t* format, ...)
{
    const unsigned formatSubstitutions = _korl_countFormatSubstitutions(format);
    korl_assert(variadicArgumentCount == formatSubstitutions);
    /* now we can log the using a va_list instead of variadic arguments */
    va_list vaList;
    va_start(vaList, format);
    _korl_logVaList(
        variadicArgumentCount, logLevel, cStringFileName, cStringFunctionName, 
        lineNumber, format, vaList);
    va_end(vaList);
}
