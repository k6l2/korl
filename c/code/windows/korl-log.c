#include "korl-log.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-windows-globalDefines.h"
#include "korl-math.h"
#include "korl-assert.h"
#include "korl-checkCast.h"
#include "korl-file.h"
#include <stdio.h>// for freopen_s
/* NOTE:  Windows implementation of %S & %s is NON-STANDARD, so as with here, we 
    need to make sure to _never_ use just %s or %S, and instead opt to use 
    explicit string character width specifiers such as %hs or %ls.
    Source: https://stackoverflow.com/a/10001238
    String format documentation can be found here:
    https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-170#width */
#define _KORL_LOG_META_DATA_STRING L"{%-7ls|%02i:%02i'%02i\"%03i|%5i|%ls|%ls} "
#define _KORL_LOG_FILE_ROTATION_MAX 10
#if 0//@todo: debugging values, delete later
korl_global_const u$ _KORL_LOG_BUFFER_BYTES_MIN = 128*sizeof(wchar_t);
korl_global_const u$ _KORL_LOG_BUFFER_BYTES_MAX = 256*sizeof(wchar_t);
#else
korl_global_const u$ _KORL_LOG_BUFFER_BYTES_MIN = 1024*sizeof(wchar_t);
korl_global_const u$ _KORL_LOG_BUFFER_BYTES_MAX = 1024*1024*sizeof(wchar_t);// about 2 megabytes
#endif
typedef struct _Korl_Log_AsyncWriteDescriptor
{
    Korl_File_AsyncWriteHandle handle;
    void* buffer;
} _Korl_Log_AsyncWriteDescriptor;
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
    u$ bufferedCharacters;//@todo: is this actually useful for anything?  right now we aren't actually using this for any real circular buffer logic or anything, in favor of bufferOffset
    u$ bufferOffset;// the head of the buffer, where we will next write log text
    bool errorAssertionTriggered;
    bool useLogOutputDebugger;
    bool useLogOutputConsole;
    Korl_File_Descriptor fileDescriptor;
    KORL_MEMORY_POOL_DECLARE(_Korl_Log_AsyncWriteDescriptor, asyncWriteDescriptors, 64);
    u$ logFileBytesWritten;
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
    if(logLevel <= KORL_LOG_LEVEL_ERROR && IsDebuggerPresent())
        /* if we ever log an error while a debugger is attached, just break 
            right now so we can figure out what's going on! */
        DebugBreak();
    /* get the current time, used in the log's timestamp metadata */
    KORL_ZERO_STACK(SYSTEMTIME, systemTimeLocal);
    GetLocalTime(&systemTimeLocal);
    /* calculate the buffer size required for the formatted log message & meta data tag */
    const wchar_t* cStringLogLevel = L"???";
    switch(logLevel)
    {
    case KORL_LOG_LEVEL_INFO:   {cStringLogLevel = L"INFO";    break;}
    case KORL_LOG_LEVEL_WARNING:{cStringLogLevel = L"WARNING"; break;}
    case KORL_LOG_LEVEL_ERROR:  {cStringLogLevel = L"ERROR";   break;}
    case KORL_LOG_LEVEL_VERBOSE:{cStringLogLevel = L"VERBOSE"; break;}
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
    /* write the full log line to a transient buffer */
    const u$ logLineSize = bufferSizeMetaTag + bufferSizeFormat;//_excluding_ the null terminator
    EnterCriticalSection(&(context->criticalSection));
    wchar_t*const logLineBuffer = KORL_C_CAST(wchar_t*, 
        korl_allocate(context->allocatorHandle, 
                      (logLineSize + 1/*null terminator*/)*sizeof(wchar_t)));
    LeaveCriticalSection(&(context->criticalSection));
    int charactersWrittenTotal = 0;
    int charactersWritten = swprintf_s(logLineBuffer, bufferSizeMetaTag + 1/*for '\0'*/, _KORL_LOG_META_DATA_STRING, 
                                       cStringLogLevel, systemTimeLocal.wHour, systemTimeLocal.wMinute, 
                                       systemTimeLocal.wSecond, systemTimeLocal.wMilliseconds, lineNumber, 
                                       cStringFileName, cStringFunctionName);
    korl_assert(charactersWritten == bufferSizeMetaTag);
    charactersWrittenTotal += charactersWritten;
    charactersWritten = vswprintf_s(logLineBuffer + charactersWrittenTotal, bufferSizeFormat, format, vaList);
    korl_assert(charactersWritten == bufferSizeFormat - 1/*we haven't written the `\n` yet*/);
    charactersWrittenTotal += charactersWritten;
    charactersWritten = swprintf_s(logLineBuffer + charactersWrittenTotal, 1 + 1/*for '\0'*/, L"\n");
    korl_assert(charactersWritten == 1);
    charactersWrittenTotal += charactersWritten;
    korl_assert(korl_checkCast_i$_to_u$(charactersWrittenTotal) == logLineSize);
    /* allocate string buffer for log meta data tag + formatted message */
    EnterCriticalSection(&(context->criticalSection));
    u$ bufferSize = context->bufferBytes / sizeof(wchar_t);
    //@todo: delete?// const u$ bufferOffset = context->bufferedCharacters % bufferSize;
    wchar_t* buffer = &(context->buffer[context->bufferOffset]);
    // bufferAvailable: the # of characters we have the ability to write to the 
    //                  buffer (NOTE: _only_ the final character must be a '\0')
    // [0][1][2][3][4][5][6][7] bufferSize==8
    //           |           |
    //           |           ┕ reserved '\0'!
    //           ┕ offset == 3
    // - bufferAvailable == 4
    u$ bufferAvailable = bufferSize - 1 - context->bufferOffset;
    // if there's not enough room in the buffer, conditionally expand it //
    if(bufferAvailable < logLineSize)
    {
        /* conditionally increase the context buffer size */
        const u$ newBufferBytes = KORL_MATH_MIN(context->bufferBytes * 2, _KORL_LOG_BUFFER_BYTES_MAX);
        if(newBufferBytes != context->bufferBytes)
        {
            context->buffer = KORL_C_CAST(wchar_t*, korl_reallocate(context->allocatorHandle, context->buffer, newBufferBytes));
            korl_assert(context->buffer);
            /* recalculate the buffer metrics */
            context->bufferBytes = newBufferBytes;
            bufferSize = newBufferBytes / sizeof(wchar_t);
            buffer = &(context->buffer[context->bufferOffset]);
            bufferAvailable = bufferSize - 1 - context->bufferOffset;// see above for details of this calculation
        }
    }
    context->bufferOffset += KORL_MATH_MIN(bufferAvailable, logLineSize);
    wchar_t*const bufferOverflow = (context->bufferOffset == bufferSize - 1) 
        // if the log message wont fit in the remaining buffer, we need to wrap 
        //  around back to the halfway point of the buffer, since we always want 
        //  to retain the very beginning of the logs for the session
        ? &(context->buffer[bufferSize / 2])
        // otherwise, we don't need an overflow address
        : NULL;
    if(bufferOverflow)
        context->bufferOffset = (bufferSize / 2)/*start of the circular portion of the buffer*/ 
            + ((bufferSizeMetaTag + bufferSizeFormat) - bufferAvailable)/*offset by the remaining log text that must be written to bufferOverflow*/;
    korl_assert(context->bufferOffset < bufferSize - 1);
    context->bufferedCharacters += bufferSizeMetaTag + bufferSizeFormat;
    /* ----- write logLineBuffer to buffer/bufferOverflow ----- */
    if(bufferAvailable >= logLineSize)
        // we can fit the entire log line into the buffer //
        korl_memory_copy(buffer, logLineBuffer, logLineSize*sizeof(*logLineBuffer));
    else
    {
        // we must wrap the log line around the circular buffer //
        korl_memory_copy(buffer, logLineBuffer, bufferAvailable*sizeof(*logLineBuffer));
        korl_memory_copy(bufferOverflow, logLineBuffer + bufferAvailable, 
                         (logLineSize - bufferAvailable)*sizeof(*logLineBuffer));
    }
    LeaveCriticalSection(&(context->criticalSection));
    /* write the log meta data string + formatted log message */
    if(_korl_log_context.useLogOutputDebugger)
        // if no debugger is present, system debugger will display the string
        // if system debugger is also not present, OutputDebugString does nothing
        OutputDebugString(logLineBuffer);
    if(_korl_log_context.useLogOutputConsole)
        korl_assert(0 <= fwprintf(stdout, L"%ls", logLineBuffer));
    /* we're done with the transient buffer, so we can free it now */
    EnterCriticalSection(&(context->criticalSection));
    /* ----- write logLineBuffer to log file ----- */
    if((context->fileDescriptor.flags & KORL_FILE_DESCRIPTOR_FLAG_WRITE) 
        && context->logFileBytesWritten < _KORL_LOG_BUFFER_BYTES_MAX / 2)
    {
        /* clean out async write descriptors that have finished 
            until the pool has room for one more operation */
        do
        {
            for(Korl_MemoryPool_Size i = 0; i < KORL_MEMORY_POOL_SIZE(context->asyncWriteDescriptors);)
                /* I have no idea about the performance characteristics of 
                    korl_file_asyncWriteWait yet, but if it's too slow I might 
                    have to change this up...  My hope is that all these calls 
                    will finish super quickly and we'll be on our way, while 
                    simultaneously freeing up space in the pool for new async 
                    write operations */
                if(korl_file_asyncWriteWait(&context->asyncWriteDescriptors[i].handle, 0/*return immediately*/))
                {
                    korl_free(context->allocatorHandle, context->asyncWriteDescriptors[i].buffer);
                    KORL_MEMORY_POOL_REMOVE(context->asyncWriteDescriptors, i);
                }
                else
                    i++;
        } while (KORL_MEMORY_POOL_ISFULL(context->asyncWriteDescriptors));
        _Korl_Log_AsyncWriteDescriptor*const asyncWriteDescriptor = KORL_MEMORY_POOL_ADD(context->asyncWriteDescriptors);
        asyncWriteDescriptor->buffer = logLineBuffer;
        //@todo: we may or may not need to specify the offset to begin writing 
        //       to the file for each async call, but I'm not sure
        asyncWriteDescriptor->handle = korl_file_writeAsync(context->fileDescriptor, logLineBuffer, logLineSize*sizeof(*(logLineBuffer)));
        context->logFileBytesWritten += logLineSize*sizeof(*(logLineBuffer));
    }
    else/* otherwise we can just free the temporary log line buffer now */
        korl_free(context->allocatorHandle, logLineBuffer);
    LeaveCriticalSection(&(context->criticalSection));
    if(logLevel <= KORL_LOG_LEVEL_ERROR && !context->errorAssertionTriggered)
    {
        /* when we're not attached to a debugger (for example, in 
            production), we should still assert that a critical issue has 
            been logged at least for the first error */
        context->errorAssertionTriggered = true;
        korl_assert(!"application has logged an error");
    }
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
korl_internal void korl_log_initialize(bool useLogOutputDebugger, bool useLogOutputConsole)
{
    korl_memory_zero(&_korl_log_context, sizeof(_korl_log_context));
    _korl_log_context.allocatorHandle          = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_megabytes(4));
    _korl_log_context.bufferBytes              = _KORL_LOG_BUFFER_BYTES_MIN;
    _korl_log_context.buffer                   = KORL_C_CAST(wchar_t*, korl_allocate(_korl_log_context.allocatorHandle, _korl_log_context.bufferBytes));
    _korl_log_context.useLogOutputDebugger     = useLogOutputDebugger;
    _korl_log_context.useLogOutputConsole      = useLogOutputConsole;
    InitializeCriticalSection(&_korl_log_context.criticalSection);
    /* if we need to ouptut logs to a console, initialize the console here 
        Sources:
        http://dslweb.nwnexus.com/~ast/dload/guicon.htm
        https://stackoverflow.com/a/30136756
        https://stackoverflow.com/a/57241985 */
    if(useLogOutputConsole)
    {
        // attempt to attach to parent process' console //
        if(!AttachConsole(ATTACH_PARENT_PROCESS) 
            // ERROR_ACCESS_DENIED => we're already attached to a console
            && GetLastError() != ERROR_ACCESS_DENIED)
        {
            // attempt to attach to our own console //
            if(!AttachConsole(GetCurrentProcessId())
                // ERROR_ACCESS_DENIED => we're already attached to a console
                && GetLastError() != ERROR_ACCESS_DENIED)
            {
                // attempt to create a new console //
                if(!AllocConsole())
                {
                    korl_log(WARNING, "AllocConsole failed; cannot output logs to console.  "
                             "GetLastError()==0x%X", GetLastError());
                    _korl_log_context.useLogOutputConsole = false;
                }
            }
        }
        if(_korl_log_context.useLogOutputConsole)
        {
            FILE* fileDummy;
            korl_assert(0 == freopen_s(&fileDummy, "CONOUT$", "w", stdout));
            korl_assert(0 == freopen_s(&fileDummy, "CONOUT$", "w", stderr));
        }
        if(_korl_log_context.useLogOutputConsole)
            korl_log(INFO, "configured to output logs to console");
    }
}
korl_internal void korl_log_initiateFile(void)
{
    _Korl_Log_Context*const context = &_korl_log_context;
    wchar_t logFileName[256];
    korl_assert(0 < korl_memory_stringFormatBuffer(logFileName, sizeof(logFileName), L"%ws.log", KORL_APPLICATION_NAME));
    /* perform log file rotation */
    for(i$ f = _KORL_LOG_FILE_ROTATION_MAX - 2; f >= 0; f--)
    {
        wchar_t logFileNameCurrent[256];
        if(f == 0)
            korl_assert(0 < korl_memory_stringFormatBuffer(logFileNameCurrent, sizeof(logFileNameCurrent), L"%ws.log", KORL_APPLICATION_NAME));
        else
            korl_assert(0 < korl_memory_stringFormatBuffer(logFileNameCurrent, sizeof(logFileNameCurrent), L"%ws.log.%lli", KORL_APPLICATION_NAME, f));
        wchar_t logFileNameNext[256];
        korl_assert(0 < korl_memory_stringFormatBuffer(logFileNameNext, sizeof(logFileNameNext), L"%ws.log.%lli", KORL_APPLICATION_NAME, f + 1));
        korl_file_renameReplace(KORL_FILE_PATHTYPE_LOCAL_DATA, logFileNameCurrent, KORL_FILE_PATHTYPE_LOCAL_DATA, logFileNameNext);
    }
    /**/
    korl_assert(korl_file_openAsync(KORL_FILE_PATHTYPE_LOCAL_DATA, 
                                    logFileName, 
                                    &context->fileDescriptor));
    /* It is highly likely that we have processed logs before initiateFile was 
        called, so we must now asynchronously flush the contents of the log 
        buffer to the log file. */
    EnterCriticalSection(&(context->criticalSection));
    const u$ charactersToWrite = KORL_MATH_MIN(context->bufferedCharacters, 
                                               // we will write a maximum of half of the maximum log buffer 
                                               // size, because the second half of the buffer is a circular 
                                               // buffer whose size we cannot possibly know ahead of time
                                               _KORL_LOG_BUFFER_BYTES_MAX / sizeof(*(context->buffer)) / 2);
    if(charactersToWrite)
    {
        wchar_t* tempBuffer = korl_allocate(context->allocatorHandle, charactersToWrite*sizeof(*(context->buffer)));
        korl_memory_copy(tempBuffer, context->buffer, charactersToWrite*sizeof(*(context->buffer)));
        korl_assert(!KORL_MEMORY_POOL_ISFULL(context->asyncWriteDescriptors));
        _Korl_Log_AsyncWriteDescriptor*const asyncWriteDescriptor = KORL_MEMORY_POOL_ADD(context->asyncWriteDescriptors);
        asyncWriteDescriptor->buffer = tempBuffer;
        asyncWriteDescriptor->handle = korl_file_writeAsync(context->fileDescriptor, tempBuffer, charactersToWrite*sizeof(*(context->buffer)));
        context->logFileBytesWritten += charactersToWrite*sizeof(*(context->buffer));
    }
    LeaveCriticalSection(&(context->criticalSection));
}
korl_internal void korl_log_shutDown(void)
{
    _Korl_Log_Context*const context = &_korl_log_context;
    korl_assert(context->fileDescriptor.flags & KORL_FILE_DESCRIPTOR_FLAG_WRITE);
    //@TODO[1]: dump the circular buffer synchronously
    for(Korl_MemoryPool_Size i = 0; i < KORL_MEMORY_POOL_SIZE(context->asyncWriteDescriptors); i++)
    {
        korl_assert(korl_file_asyncWriteWait(&context->asyncWriteDescriptors[i].handle, KORL_U32_MAX));
        korl_free(context->allocatorHandle, context->asyncWriteDescriptors[i].buffer);
    }
    korl_file_close(&context->fileDescriptor);
}
