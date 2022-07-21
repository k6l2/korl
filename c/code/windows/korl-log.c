#include "korl-log.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-windows-globalDefines.h"
#include "korl-math.h"
#include "korl-checkCast.h"
#include "korl-file.h"
#include <stdio.h>// for freopen_s
/* NOTE:  Windows implementation of %S & %s is NON-STANDARD, so as with here, we 
    need to make sure to _never_ use just %s or %S, and instead opt to use 
    explicit string character width specifiers such as %hs or %ls.
    Source: https://stackoverflow.com/a/10001238
    String format documentation can be found here:
    https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-170#width */
#define _KORL_LOG_META_DATA_STRING L"╟%-7ls┆%02i:%02i'%02i\"%03i┆%5i┆%ls┆%ls╢ "
#define _KORL_LOG_FILE_ROTATION_MAX 10
korl_global_const u$ _KORL_LOG_BUFFER_BYTES_MIN = 1024*sizeof(wchar_t);
korl_global_const u$ _KORL_LOG_BUFFER_BYTES_MAX = 1024*1024*sizeof(wchar_t);// about 2 megabytes
typedef struct _Korl_Log_AsyncWriteDescriptor
{
    Korl_File_AsyncIoHandle asyncIoHandle;
} _Korl_Log_AsyncWriteDescriptor;
typedef struct _Korl_Log_Context
{
    Korl_Memory_AllocatorHandle allocatorHandlePersistent;
    Korl_Memory_AllocatorHandle allocatorHandleTransient;//KORL-ISSUE-000-000-052: log: potentially unnecessary allocator
    wchar_t* transientBuffer;
    u$ transientBufferCharacters;
    CRITICAL_SECTION criticalSection;
    u$ bufferBytes;
    /** we will split the log buffer into two parts:
     * - the first part will be fixed size
     * - the second part will be treated as a circular buffer
     * why? because we want to truncate huge logs, and the most important data 
     * in a log is most likely at the beginning & end */
    wchar_t* buffer;
    u$ bufferedCharacters;
    u$ bufferOffset;// the head of the buffer, where we will next write log text
    bool errorAssertionTriggered;
    bool useLogOutputDebugger;
    bool useLogOutputConsole;
    bool useLogFileBig;
    bool logFileEnabled;
    bool disableMetaTags;
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
        wchar_t nextChar = *(f+1);
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
            /* check if the next character is a flag */
            if(nextChar == '#' || nextChar == '0' || nextChar == '-' || nextChar == '+' || nextChar == ' ')
            {
                // consume the flag character //
                f++;
                nextChar = *(f+1);
                korl_assert(nextChar);
            }
            /* check if next character is a width specifier */
            if(nextChar == '*')
            {
                formatSubstitutions++;// parameter needed for width specifier
                f++;
                nextChar = *(f+1);
                korl_assert(nextChar);
            }
            else
            {
                // consume width number characters //
                while(nextChar && nextChar >= '0' && nextChar <= '9')
                {
                    f++;
                    nextChar = *(f+1);
                    korl_assert(nextChar);
                }
            }
            if(nextChar == '.')
            {
                /* check if next character is a precision specifier */
                f++;
                nextChar = *(f+1);
                korl_assert(nextChar);
                if(nextChar == '*')
                    formatSubstitutions++;// parameter needed for precision specifier
            }
            formatSubstitutions++;// parameter needed for the format specifier
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
    const bool errorAssertionTriggered = context->errorAssertionTriggered;
    /* get the current time, used in the log's timestamp metadata */
    KORL_ZERO_STACK(SYSTEMTIME, systemTimeLocal);
    GetLocalTime(&systemTimeLocal);
    /**/
    if(logLevel <= KORL_LOG_LEVEL_ERROR)
        context->errorAssertionTriggered = true;// trigger as soon as possible, so that if another error is logged in error-handling code, we don't trigger the assertion failure again
    /* we can skip most of this if there are no log outputs enabled */
    if(!context->logFileEnabled && !context->useLogOutputDebugger && !context->useLogOutputConsole)
        goto logOutputDone;
    /* calculate the buffer size required for the formatted log message & meta data tag */
    const wchar_t* cStringLogLevel = L"???";
    switch(logLevel)
    {
    case KORL_LOG_LEVEL_ASSERT: {cStringLogLevel = L"ASSERT";  break;}
    case KORL_LOG_LEVEL_ERROR:  {cStringLogLevel = L"ERROR";   break;}
    case KORL_LOG_LEVEL_WARNING:{cStringLogLevel = L"WARNING"; break;}
    case KORL_LOG_LEVEL_INFO:   {cStringLogLevel = L"INFO";    break;}
    case KORL_LOG_LEVEL_VERBOSE:{cStringLogLevel = L"VERBOSE"; break;}
    }
    // only print the file name, not the full path!
    for(const wchar_t* fileNameCursor = cStringFileName; fileNameCursor && *fileNameCursor; fileNameCursor++)
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
    const bool prependMetaTag = !context->disableMetaTags && cStringFileName && cStringFunctionName && lineNumber > 0;
    const u$ logLineSize = prependMetaTag 
        ? bufferSizeMetaTag + bufferSizeFormat//_excluding_ the null terminator
        : bufferSizeFormat;
    EnterCriticalSection(&(context->criticalSection));
    /* before allocating more from the transient buffer, let's see if we can 
        clean up any pending async buffers so that we can prevent the linear 
        allocator from overflowing */
    //KORL-ISSUE-000-000-052: this code might not need to even need to exist with better allocation strategy!
    for(Korl_MemoryPool_Size i = 0; i < KORL_MEMORY_POOL_SIZE(context->asyncWriteDescriptors);)
    {
        const Korl_File_GetAsyncIoResult resultAsyncIo = 
            korl_file_getAsyncIoResult(&(context->asyncWriteDescriptors[i].asyncIoHandle), false/*don't block*/);
        if(resultAsyncIo == KORL_FILE_GET_ASYNC_IO_RESULT_DONE)
            KORL_MEMORY_POOL_REMOVE(context->asyncWriteDescriptors, i);
        else
        {
            korl_assert(resultAsyncIo == KORL_FILE_GET_ASYNC_IO_RESULT_PENDING);
            i++;
        }
    }
    if(context->transientBufferCharacters < logLineSize + 1/*null terminator*/)
    {
        context->transientBufferCharacters = 2*(logLineSize + 1/*null terminator*/);
        context->transientBuffer = korl_reallocate(context->allocatorHandleTransient, context->transientBuffer, context->transientBufferCharacters*sizeof(*(context->transientBuffer)));
    }
    int charactersWrittenTotal = 0;
    int charactersWritten;
    if(prependMetaTag)
    {
        charactersWritten = swprintf_s(context->transientBuffer, bufferSizeMetaTag + 1/*for '\0'*/, _KORL_LOG_META_DATA_STRING, 
                                       cStringLogLevel, systemTimeLocal.wHour, systemTimeLocal.wMinute, 
                                       systemTimeLocal.wSecond, systemTimeLocal.wMilliseconds, lineNumber, 
                                       cStringFileName, cStringFunctionName);
        korl_assert(charactersWritten == bufferSizeMetaTag);
        charactersWrittenTotal += charactersWritten;
    }
    charactersWritten = vswprintf_s(context->transientBuffer + charactersWrittenTotal, bufferSizeFormat, format, vaList);
    korl_assert(charactersWritten == bufferSizeFormat - 1/*we haven't written the `\n` yet*/);
    charactersWrittenTotal += charactersWritten;
    charactersWritten = swprintf_s(context->transientBuffer + charactersWrittenTotal, 1 + 1/*for '\0'*/, L"\n");
    korl_assert(charactersWritten == 1);
    charactersWrittenTotal += charactersWritten;
    korl_assert(korl_checkCast_i$_to_u$(charactersWrittenTotal) == logLineSize);
    /* allocate string buffer for log meta data tag + formatted message */
    u$ bufferSize = context->bufferBytes / sizeof(*context->buffer);
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
            context->buffer = KORL_C_CAST(wchar_t*, korl_reallocate(context->allocatorHandlePersistent, context->buffer, newBufferBytes));
            korl_assert(context->buffer);
            /* recalculate the buffer metrics */
            context->bufferBytes = newBufferBytes;
            bufferSize = newBufferBytes / sizeof(*context->buffer);
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
            + (logLineSize - bufferAvailable)/*offset by the remaining log text that must be written to bufferOverflow*/;
    korl_assert(context->bufferOffset < bufferSize - 1);
    context->bufferedCharacters += logLineSize;
    /* ----- write logLineBuffer to buffer/bufferOverflow ----- */
    if(bufferAvailable >= logLineSize)
        // we can fit the entire log line into the buffer //
        korl_memory_copy(buffer, context->transientBuffer, logLineSize*sizeof(*context->transientBuffer));
    else
    {
        // we must wrap the log line around the circular buffer //
        korl_memory_copy(buffer, context->transientBuffer, bufferAvailable*sizeof(*context->transientBuffer));
        korl_memory_copy(bufferOverflow, context->transientBuffer + bufferAvailable, 
                         (logLineSize - bufferAvailable)*sizeof(*context->transientBuffer));
    }
    /* write the log meta data string + formatted log message */
    if(_korl_log_context.useLogOutputDebugger)
        // if no debugger is present, system debugger will display the string
        // if system debugger is also not present, OutputDebugString does nothing
        OutputDebugString(context->transientBuffer);
    if(_korl_log_context.useLogOutputConsole)
    {
        const HANDLE handleConsole = GetStdHandle(logLevel > KORL_LOG_LEVEL_ERROR 
                                                  ? STD_OUTPUT_HANDLE 
                                                  : STD_ERROR_HANDLE);
        korl_assert(handleConsole != INVALID_HANDLE_VALUE);
        korl_assert(0 != WriteConsole(handleConsole, context->transientBuffer, 
                                      korl_checkCast_u$_to_u32(logLineSize), 
                                      NULL/*out_charsWritten; I don't care*/, 
                                      NULL/*reserved; _must_ be NULL*/));
    }
    if(logLevel == KORL_LOG_LEVEL_ERROR && !errorAssertionTriggered)
    {
        /* when we're not attached to a debugger (for example, in 
            production), we should still assert that a critical issue has 
            been logged at least for the first error */
        /* we need to copy the transient buffer because if we don't, the buffer 
            contents will get clobbered if a call to log happens during 
            korl-crash assert handling routines */
        const u$ lineSize = korl_memory_stringSize(context->transientBuffer + bufferSizeMetaTag/*exclude the meta tag in the assert message*/);
        wchar_t* tempBuffer = korl_allocate(context->allocatorHandleTransient, (lineSize + 1/*null-terminator*/)*sizeof(*tempBuffer));
        // no need to check if tempBuffer got allocated, since we'd just write to 0x0 anyway
        korl_assert(0 < korl_memory_stringCopy(context->transientBuffer + bufferSizeMetaTag/*exclude the meta tag in the assert message*/, tempBuffer, lineSize + 1/*null-terminator*/));
        tempBuffer[lineSize - 1] = L'\0';// remove the '\n'
        /* we don't call the korl_assert macro here because we want to propagate 
            the meta data of the CALLER of this error log entry to the user; the 
            fact that the assert condition fails in the log module is irrelevant */
        _korl_crash_assertConditionFailed(tempBuffer, cStringFileName, cStringFunctionName, lineNumber);
        korl_free(context->allocatorHandleTransient, tempBuffer);
    }
    /* we're done with the transient buffer, so we can reuse it in future log 
        calls; the data has all been written to the circular log buffer, with 
        _very_ little chance of it being over-written by the time we attempt to 
        do any async writes to disk, so it should be fine to just read from the 
        locations where the log line was written to the circular buffer for now */
    //KORL-ISSUE-000-000-065: log: although this is generally safe for current workloads, this operation is unsafe for async log file writes
    /* ----- write logLineBuffer to log file ----- */
    if((context->fileDescriptor.flags & KORL_FILE_DESCRIPTOR_FLAG_WRITE) 
        && (   context->logFileBytesWritten < _KORL_LOG_BUFFER_BYTES_MAX / 2
            || context->useLogFileBig))
    {
        /* clean out async write descriptors that have finished 
            until the pool has room for one more operation */
        while (KORL_MEMORY_POOL_ISFULL(context->asyncWriteDescriptors))
        {
            for(Korl_MemoryPool_Size i = 0; i < KORL_MEMORY_POOL_SIZE(context->asyncWriteDescriptors);)
            {
                const Korl_File_GetAsyncIoResult resultAsyncIo = 
                    korl_file_getAsyncIoResult(&(context->asyncWriteDescriptors[i].asyncIoHandle), false/*don't block*/);
                if(resultAsyncIo == KORL_FILE_GET_ASYNC_IO_RESULT_DONE)
                    KORL_MEMORY_POOL_REMOVE(context->asyncWriteDescriptors, i);
                else
                {
                    korl_assert(resultAsyncIo == KORL_FILE_GET_ASYNC_IO_RESULT_PENDING);
                    i++;
                }
            }
        }
        _Korl_Log_AsyncWriteDescriptor*const asyncWriteDescriptor = KORL_MEMORY_POOL_ADD(context->asyncWriteDescriptors);
        korl_memory_zero(asyncWriteDescriptor, sizeof(*asyncWriteDescriptor));
        //KORL-ISSUE-000-000-046: log: test to validate order of async log writes
        const u$ reliableBufferBytesRemaining = context->useLogFileBig 
            ? logLineSize * sizeof(*buffer)
            : (_KORL_LOG_BUFFER_BYTES_MAX / 2) - context->logFileBytesWritten;
        /* if the logFileBytesWritten is going to exceed _KORL_LOG_BUFFER_BYTES_MAX / 2,
            we need to clamp this write operation to that value, so that 
            when the circular buffer is written on log shutdown, we either 
            just write the rest of the buffer, or we cut the file log & 
            do two separate circular buffer writes if necessary */
        u$ logFileLineBytes = KORL_MATH_MIN(logLineSize * sizeof(*buffer), reliableBufferBytesRemaining);
        asyncWriteDescriptor->asyncIoHandle = korl_file_writeAsync(context->fileDescriptor, buffer, logFileLineBytes);
        context->logFileBytesWritten += logFileLineBytes;
    }
    LeaveCriticalSection(&(context->criticalSection));
logOutputDone:
    return;
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
    _korl_log_context.allocatorHandlePersistent = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_megabytes(4), L"korl-log-persistent", KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS, NULL/*let platform choose address*/);
    _korl_log_context.allocatorHandleTransient  = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_megabytes(1), L"korl-log-transient" , KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS, NULL/*let platform choose address*/);
    _korl_log_context.bufferBytes               = _KORL_LOG_BUFFER_BYTES_MIN;
    _korl_log_context.buffer                    = KORL_C_CAST(wchar_t*, korl_allocate(_korl_log_context.allocatorHandlePersistent, _korl_log_context.bufferBytes));
    _korl_log_context.logFileEnabled            = true;// assume we will use a log file eventually, until the user specifies we wont
    InitializeCriticalSection(&_korl_log_context.criticalSection);
    /* there's no reason to have to assert that the buffer was created here, 
        since an assertion will fire the moment we try to log something if it 
        did, and the resulting error will likely be easy to catch, so I'm just 
        going to leave this commented out for now */
    //korl_assert(_korl_log_context.buffer);
}
korl_internal void korl_log_configure(bool useLogOutputDebugger, bool useLogOutputConsole, bool useLogFileBig, bool disableMetaTags)
{
    _korl_log_context.useLogOutputDebugger = useLogOutputDebugger;
    _korl_log_context.useLogOutputConsole  = useLogOutputConsole;
    _korl_log_context.useLogFileBig        = useLogFileBig;
    _korl_log_context.disableMetaTags      = disableMetaTags;
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
    //KORL-ISSUE-000-000-075: log: dump log buffer to debugger if configured to output logs to debugger
}
korl_internal void korl_log_initiateFile(bool logFileEnabled)
{
    _Korl_Log_Context*const context = &_korl_log_context;
    context->logFileEnabled = logFileEnabled;
    if(!logFileEnabled)
        return;
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
    korl_assert(korl_file_open(KORL_FILE_PATHTYPE_LOCAL_DATA, 
                               logFileName, 
                               &context->fileDescriptor, 
                               true/*async*/));
    /* info about BOMs: https://unicode.org/faq/utf_bom.html */
    korl_shared_const u8 BOM_UTF16_LITTLE_ENDIAN[] = { 0xFF,0xFE };// Byte Order Marker to indicate UTF-16LE; must be written to the beginnning of the file!
    korl_file_write(context->fileDescriptor, BOM_UTF16_LITTLE_ENDIAN, sizeof(BOM_UTF16_LITTLE_ENDIAN));
    /* It is highly likely that we have processed logs before initiateFile was 
        called, so we must now asynchronously flush the contents of the log 
        buffer to the log file. */
    EnterCriticalSection(&(context->criticalSection));
    const u$ maxInitCharacters = context->useLogFileBig 
        ? context->bufferedCharacters 
        : // we will write a maximum of half of the maximum log buffer 
          // size, because the second half of the buffer is a circular 
          // buffer whose size we cannot possibly know ahead of time
          _KORL_LOG_BUFFER_BYTES_MAX / sizeof(*(context->buffer)) / 2;
    const u$ charactersToWrite = KORL_MATH_MIN(context->bufferedCharacters, maxInitCharacters);
    if(charactersToWrite)
    {
        korl_assert(!KORL_MEMORY_POOL_ISFULL(context->asyncWriteDescriptors));
        _Korl_Log_AsyncWriteDescriptor*const asyncWriteDescriptor = KORL_MEMORY_POOL_ADD(context->asyncWriteDescriptors);
        korl_memory_zero(asyncWriteDescriptor, sizeof(*asyncWriteDescriptor));
        asyncWriteDescriptor->asyncIoHandle = korl_file_writeAsync(context->fileDescriptor, context->buffer, charactersToWrite*sizeof(*(context->buffer)));
        context->logFileBytesWritten += charactersToWrite*sizeof(*(context->buffer));
    }
    LeaveCriticalSection(&(context->criticalSection));
}
korl_internal void korl_log_clearAsyncIo(void)
{
    _Korl_Log_Context*const context = &_korl_log_context;
    for(Korl_MemoryPool_Size i = 0; i < KORL_MEMORY_POOL_SIZE(context->asyncWriteDescriptors); i++)
        korl_assert(KORL_FILE_GET_ASYNC_IO_RESULT_DONE == korl_file_getAsyncIoResult(&context->asyncWriteDescriptors[i].asyncIoHandle, true/*block until complete*/));
    KORL_MEMORY_POOL_EMPTY(context->asyncWriteDescriptors);
}
korl_internal void korl_log_shutDown(void)
{
    _Korl_Log_Context*const context = &_korl_log_context;
    if(!(context->fileDescriptor.flags & KORL_FILE_DESCRIPTOR_FLAG_WRITE))
        goto skipFileCleanup;
    korl_log_clearAsyncIo();
    korl_memory_allocator_empty(context->allocatorHandleTransient);
    if(context->useLogFileBig)
    {
        /* we don't have to do any sync writing here with the circular buffer, 
            since we would have already submitted write jobs for every line up 
            until this point anyway; do nothing */
    }
    else if(context->bufferedCharacters > _KORL_LOG_BUFFER_BYTES_MAX / sizeof(*(context->buffer)))
    {
        /* the log file has to be cut, since we lost log data during writes to 
            the circular buffer which wrapped to the beginning */
        korl_shared_const wchar_t LOG_MESSAGE_CUT[] = L"\n\n----- LOG FILE CUT -----\n\n\n";
        korl_file_write(context->fileDescriptor, LOG_MESSAGE_CUT, sizeof(LOG_MESSAGE_CUT));
        const wchar_t*const bufferEnd = context->buffer + (_KORL_LOG_BUFFER_BYTES_MAX / sizeof(*(context->buffer)));
        const wchar_t*const bufferA = context->buffer + context->bufferOffset;
        const u$ bufferSizeA = bufferEnd - 1/*null terminator at the last position*/ - bufferA;
        korl_file_write(context->fileDescriptor, bufferA, bufferSizeA*sizeof(*(context->buffer)));
        if(context->bufferOffset > (_KORL_LOG_BUFFER_BYTES_MAX / sizeof(*(context->buffer))) / 2)
        {
            const wchar_t*const bufferB = context->buffer + (_KORL_LOG_BUFFER_BYTES_MAX / sizeof(*(context->buffer))) / 2;
            const u$ bufferSizeB = bufferA - bufferB;
            korl_file_write(context->fileDescriptor, bufferB, bufferSizeB*sizeof(*(context->buffer)));
        }
    }
    else if (context->bufferedCharacters > _KORL_LOG_BUFFER_BYTES_MAX / sizeof(*(context->buffer)) / 2)
    {
        /* we can just write the remaining buffer directly to the log file */
        const u$ remainingCharacters  = context->bufferedCharacters - (_KORL_LOG_BUFFER_BYTES_MAX / sizeof(*(context->buffer)) / 2);
        const void*const bufferOffset = context->buffer             + (_KORL_LOG_BUFFER_BYTES_MAX / sizeof(*(context->buffer)) / 2);
        korl_file_write(context->fileDescriptor, bufferOffset, remainingCharacters*sizeof(*(context->buffer)));
    }
    korl_file_close(&context->fileDescriptor);
skipFileCleanup:
    return;
}
