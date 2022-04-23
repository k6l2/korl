#include "korl-log.h"
#include "korl-memory.h"
#include "korl-windows-globalDefines.h"
#include "korl-math.h"
#include "korl-assert.h"
#define _KORL_LOG_META_DATA_STRING L"{%-7S|%02i:%02i'%02i\"%03i|%5i|%s|%s} "
typedef struct _Korl_Log_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    CRITICAL_SECTION criticalSection;
    u$ bufferBytes;
    /** we will split the log buffer into two parts:
     * - the first part will be fixed size
     * - the second part will be treated as a circular buffer
     * why? because we want to truncate huge logs, and the most important data 
     * in a log is most likely at the beginning & end */
    wchar_t* buffer;
} _Korl_Log_Context;
korl_global_variable _Korl_Log_Context _korl_log_context;
korl_internal unsigned _korl_log_countFormatSubstitutions(const wchar_t* format)
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
        {
            /* if the current character is a '%', and there is a next 
                character, and the next character is NOT a '%', that 
                means this must be a variable substitution */
            formatSubstitutions++;
            korl_assert(formatSubstitutions != 0);//check for overflow
        }
    }
    return formatSubstitutions;
}
korl_internal void _korl_log_vaList(
    unsigned variadicArgumentCount, enum KorlEnumLogLevel logLevel, 
    const wchar_t* cStringFileName, const wchar_t* cStringFunctionName, 
    int lineNumber, const wchar_t* format, va_list vaList)
{
    _Korl_Log_Context*const context = &_korl_log_context;
    /* if we ever log an error while a debugger is attached, just break right 
        now so we can figure out what's going on! */
    if(IsDebuggerPresent() && logLevel <= KORL_LOG_LEVEL_ERROR)
        DebugBreak();
    /* get the current time, used in the log's timestamp metadata */
    KORL_ZERO_STACK(SYSTEMTIME, systemTimeLocal);
    GetLocalTime(&systemTimeLocal);
    /* calculate the buffer size required for the formatted log message & meta data tag */
    const char* cStringLogLevel = "???";
    switch(logLevel)
    {
    case KORL_LOG_LEVEL_INFO:   {cStringLogLevel = "INFO";    break;}
    case KORL_LOG_LEVEL_WARNING:{cStringLogLevel = "WARNING"; break;}
    case KORL_LOG_LEVEL_ERROR:  {cStringLogLevel = "ERROR";   break;}
    case KORL_LOG_LEVEL_VERBOSE:{cStringLogLevel = "VERBOSE"; break;}
    }
    // only print the file name, not the full path!
    for(const wchar_t* fileNameCursor = cStringFileName; *fileNameCursor; 
    fileNameCursor++)
        if(*fileNameCursor == '\\' || *fileNameCursor == '/')
            cStringFileName = fileNameCursor + 1;
    //  //
    const int bufferSizeFormat  = _vscwprintf(format, vaList) + 1/*newline character*/;//excluding the null terminator
    const int bufferSizeMetaTag = _scwprintf(_KORL_LOG_META_DATA_STRING, 
                                             cStringLogLevel, systemTimeLocal.wHour, systemTimeLocal.wMinute, 
                                             systemTimeLocal.wSecond, systemTimeLocal.wMilliseconds, lineNumber, 
                                             cStringFileName, cStringFunctionName);//excluding the null terminator
    korl_assert(bufferSizeFormat  > 0);
    korl_assert(bufferSizeMetaTag > 0);
    /* allocate string buffer for log meta data tag + formatted message */
    // the only critical section to make printing logs thread-safe is the buffer
    //  allocation itself - after that, each thread can just write to their 
    //  allocation at their own pace
    // IMPORTANT: this will only work if we are guaranteed to always reallocate 
    //            the buffer in-place every allocation!
    EnterCriticalSection(&(context->criticalSection));
    LeaveCriticalSection(&(context->criticalSection));
    /* write the log meta data string + formatted log message */
#if 0//@TODO[1]: recycle
    wchar_t*const result = (wchar_t*)allocator.callbackAllocate(allocator.userData, bufferSize * sizeof(wchar_t), __FILEW__, __LINE__);
    const int charactersWritten = vswprintf_s(result, bufferSize, format, vaList);
    korl_assert(charactersWritten == bufferSize - 1);
#endif
#if 0//@TODO[1]: recycle
    // print out the the log entry alone side the meta data //
    korl_print(printStream, L"{%-7s|%02i:%02i'%02i\"%03i|%5i|%S|%S} %S\n", 
        cStringLogLevel, systemTimeLocal.wHour, systemTimeLocal.wMinute, 
        systemTimeLocal.wSecond, systemTimeLocal.wMilliseconds, lineNumber, 
        cStringFileName, cStringFunctionName, stackStringBuffer);
#endif
}
korl_internal KORL_PLATFORM_LOG(_korl_log_variadic)
{
    const unsigned formatSubstitutions = _korl_log_countFormatSubstitutions(format);
    korl_assert(variadicArgumentCount == formatSubstitutions);
    va_list vaList;
    va_start(vaList, format);
    _korl_log_vaList(variadicArgumentCount, logLevel, cStringFileName, 
                     cStringFunctionName, lineNumber, format, vaList);
    va_end(vaList);
}
korl_internal void korl_log_initialize(void)
{
    korl_memory_zero(&_korl_log_context, sizeof(_korl_log_context));
    _korl_log_context.allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_megabytes(1));
    _korl_log_context.bufferBytes     = 1024*sizeof(wchar_t);
    _korl_log_context.buffer          = KORL_C_CAST(wchar_t*, korl_allocate(_korl_log_context.allocatorHandle, _korl_log_context.bufferBytes));
    InitializeCriticalSection(&_korl_log_context.criticalSection);
}
korl_internal void korl_log_shutDown(void)
{
    //@TODO[1]: get the file path to save the log file in
    //@TODO[1]: perform log file rotation
    //@TODO[1]: write buffer out to log file
}
