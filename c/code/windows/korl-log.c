#include "korl-log.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-windows-globalDefines.h"
#include "korl-math.h"
#include "korl-checkCast.h"
#include "korl-file.h"
#include "korl-stb-ds.h"///@TODO: WARNING: the stb-ds module is initialized _after_ the log module; this is okay for now as far as I can tell since the stb-ds code can still function without the module being loaded, as all it does right now is just run unit tests, but this obviously not clean
#include <stdio.h>// for freopen_s
/* NOTE:  Windows implementation of %S & %s is NON-STANDARD, so as with here, we 
    need to make sure to _never_ use just %s or %S, and instead opt to use 
    explicit string character width specifiers such as %hs or %ls.
    Source: https://stackoverflow.com/a/10001238
    String format documentation can be found here:
    https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-170#width */
#define _KORL_LOG_META_DATA_STRING L"╟%-7ls┆%02i:%02i'%02i\"%03i┆%5i┆%ls┆%ls╢ "
#define _KORL_LOG_FILE_ROTATION_MAX 10
korl_global_const u$ _KORL_LOG_BUFFER_REGION_BYTES = 2*1024*1024*sizeof(wchar_t);// 4 mebibytes;
typedef struct _Korl_Log_AsyncWriteDescriptor
{
    Korl_File_AsyncIoHandle asyncIoHandle;
} _Korl_Log_AsyncWriteDescriptor;
typedef struct _Korl_Log_Context
{
    CRITICAL_SECTION criticalSection;
    /** This virtual address will point to the beginning of a region of memory 
     * which is mapped in the following way: [A][B][B], where [A] is mapped to a 
     * physical region in memory which contains the persistent chunk of the very 
     * beginning of the log, and [B] is mapped to a physical region in memory 
     * which can be used as a ring buffer. */
    void* rawLog;
    u$ rawLogChunkBytes;// how many bytes the system decided to use for each region of physical memory; this is not necessarily going to be == to the value we requested because the system needs to obey memory alignment requirements
    u$ loggedBytes;// from the entire lifetime of the application
    bool errorAssertionTriggered;
    bool useLogOutputDebugger;
    bool useLogOutputConsole;
    bool useLogFileBig;
    bool logFileEnabled;
    bool disableMetaTags;
    /// @TODO: add a flag to _enable_ async log file writes; stop doing async log file writes by default since that code path is likely _super_ slow
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
korl_internal void _korl_log_vaList(unsigned variadicArgumentCount
                                   ,enum KorlEnumLogLevel logLevel
                                   ,const wchar_t* cStringFileName
                                   ,const wchar_t* cStringFunctionName
                                   ,int lineNumber
                                   ,const wchar_t* format
                                   ,va_list vaList)
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
        goto _korl_log_vaList_done;
    /**/
    EnterCriticalSection(&(context->criticalSection));
    /* figure out where we are in the raw log buffer */
    const u$ rawLogOffset = context->loggedBytes < context->rawLogChunkBytes
        ? context->loggedBytes
        : context->rawLogChunkBytes + ((context->loggedBytes - context->rawLogChunkBytes) % context->rawLogChunkBytes);
    wchar_t*const logBuffer = KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, context->rawLog) + rawLogOffset);
    /* print the log message to the raw log buffer */
    int charactersWrittenTotal = 0;
    int charactersWritten;
    // write the log meta tag //
    if(!context->disableMetaTags && cStringFileName && cStringFunctionName && lineNumber > 0)
    {
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
        charactersWritten = swprintf(logBuffer + charactersWrittenTotal
                                    ,(context->rawLogChunkBytes / sizeof(*logBuffer)) - charactersWrittenTotal
                                    ,_KORL_LOG_META_DATA_STRING
                                    ,cStringLogLevel, systemTimeLocal.wHour, systemTimeLocal.wMinute
                                    ,systemTimeLocal.wSecond, systemTimeLocal.wMilliseconds, lineNumber
                                    ,cStringFileName, cStringFunctionName);
        korl_assert(charactersWritten > 0);
        charactersWrittenTotal += charactersWritten;
    }
    // write the log message //
    charactersWritten = vswprintf(logBuffer + charactersWrittenTotal
                                 ,(context->rawLogChunkBytes / sizeof(*logBuffer)) - charactersWrittenTotal
                                 ,format, vaList);
    korl_assert(charactersWritten > 0);
    charactersWrittenTotal += charactersWritten;
    // write a newline character //
    logBuffer[charactersWrittenTotal++] = L'\n';
    logBuffer[charactersWrittenTotal  ] = L'\0';
    korl_assert(charactersWrittenTotal*sizeof(logBuffer) < _KORL_LOG_BUFFER_REGION_BYTES);
    // finally, we can update the log context book keeping for buffered logs //
    context->loggedBytes += charactersWrittenTotal*sizeof(*logBuffer);
    /* optionally write the buffered log entry to the debugger */
    if(_korl_log_context.useLogOutputDebugger)
        // if no debugger is present, system debugger will display the string
        // if system debugger is also not present, OutputDebugString does nothing
        OutputDebugString(logBuffer);
    /* optionally write the buffered log entry to the console */
    if(_korl_log_context.useLogOutputConsole)
    {
        const HANDLE handleConsole = GetStdHandle(logLevel > KORL_LOG_LEVEL_ERROR 
                                                  ? STD_OUTPUT_HANDLE 
                                                  : STD_ERROR_HANDLE);
        korl_assert(handleConsole != INVALID_HANDLE_VALUE);
        korl_assert(0 != WriteConsole(handleConsole, logBuffer
                                     ,korl_checkCast_u$_to_u32(charactersWrittenTotal)
                                     ,NULL/*out_charsWritten; I don't care*/
                                     ,NULL/*reserved; _must_ be NULL*/));
    }
    if(logLevel == KORL_LOG_LEVEL_ERROR && !errorAssertionTriggered)
        /* when we're not attached to a debugger (for example, in 
            production), we should still assert that a critical issue has 
            been logged at least for the first error; 
            we don't call the korl_assert macro here because we want to propagate 
            the meta data of the CALLER of this error log entry to the user; the 
            fact that the assert condition fails in the log module is irrelevant */
        _korl_crash_assertConditionFailed(logBuffer, cStringFileName, cStringFunctionName, lineNumber);
    /* ----- write the buffered log entry to log file ----- */
    if(   (context->fileDescriptor.flags & KORL_FILE_DESCRIPTOR_FLAG_WRITE) 
       && (   context->logFileBytesWritten < context->rawLogChunkBytes
           || context->useLogFileBig))
    {
        /* clean out async write descriptors that have finished 
            until the pool has room for one more operation */
        while(KORL_MEMORY_POOL_ISFULL(context->asyncWriteDescriptors))
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
        _Korl_Log_AsyncWriteDescriptor*const asyncWriteDescriptor = KORL_MEMORY_POOL_ADD(context->asyncWriteDescriptors);
        korl_memory_zero(asyncWriteDescriptor, sizeof(*asyncWriteDescriptor));
        //KORL-ISSUE-000-000-046: log: test to validate order of async log writes
        const u$ reliableBufferBytesRemaining = context->useLogFileBig 
            ? charactersWrittenTotal * sizeof(*logBuffer)
            : context->rawLogChunkBytes - context->logFileBytesWritten;
        /* if the logFileBytesWritten is going to exceed _KORL_LOG_BUFFER_BYTES_MAX / 2,
            we need to clamp this write operation to that value, so that 
            when the circular buffer is written on log shutdown, we either 
            just write the rest of the buffer, or we cut the file log & 
            do two separate circular buffer writes if necessary */
        u$ logFileLineBytes = KORL_MATH_MIN(charactersWrittenTotal * sizeof(*logBuffer), reliableBufferBytesRemaining);
        asyncWriteDescriptor->asyncIoHandle = korl_file_writeAsync(context->fileDescriptor, logBuffer, logFileLineBytes);
        context->logFileBytesWritten += logFileLineBytes;
    }
    /**/
    LeaveCriticalSection(&(context->criticalSection));
_korl_log_vaList_done:
    return;
}
korl_internal KORL_PLATFORM_LOG(_korl_log_variadic)
{
    const unsigned formatSubstitutions = _korl_log_countFormatSubstitutions(format);
    korl_assert(variadicArgumentCount == formatSubstitutions);
    va_list vaList;
    va_start(vaList, format);
    _korl_log_vaList(variadicArgumentCount, logLevel, cStringFileName
                    ,cStringFunctionName, lineNumber, format, vaList);
    va_end(vaList);
}
korl_internal void korl_log_initialize(void)
{
    KORL_ZERO_STACK_ARRAY(u16, virtualRegionMapRawLog, 3);
    virtualRegionMapRawLog[0] = 0;// map to the persistent log buffer region
    virtualRegionMapRawLog[1] = 1;// map to the ring log buffer region
    virtualRegionMapRawLog[2] = 1;// map to the ring log buffer region
    KORL_ZERO_STACK(Korl_Memory_FileMapAllocation_CreateInfo, createInfoRawLog);
    createInfoRawLog.physicalMemoryChunkBytes = _KORL_LOG_BUFFER_REGION_BYTES;
    createInfoRawLog.physicalRegionCount      = 2;// one region for the persistent log buffer, & one for the ring buffer
    createInfoRawLog.virtualRegionCount       = korl_arraySize(virtualRegionMapRawLog);
    createInfoRawLog.virtualRegionMap         = virtualRegionMapRawLog;
    korl_memory_zero(&_korl_log_context, sizeof(_korl_log_context));
    _korl_log_context.logFileEnabled            = true;// assume we will use a log file eventually, until the user specifies we wont
    _korl_log_context.rawLog                    = korl_memory_fileMapAllocation_create(&createInfoRawLog, &_korl_log_context.rawLogChunkBytes);
    InitializeCriticalSection(&_korl_log_context.criticalSection);
    /* there's no reason to have to assert that the buffer was created here, 
        since an assertion will fire the moment we try to log something if it 
        did, and the resulting error will likely be easy to catch, so I'm just 
        going to leave this commented out for now */
    //korl_assert(_korl_log_context.buffer);
#if KORL_DEBUG
    /* test the rawLog file mapped allocation */
    const wchar_t testString0[] = L"Persistent Log Buffer Region";
    const wchar_t testString1[] = L"Ring Log Buffer Region";
    korl_memory_stringCopy(testString0, _korl_log_context.rawLog, korl_arraySize(testString0));
    korl_memory_stringCopy(testString1, KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, _korl_log_context.rawLog) + _korl_log_context.rawLogChunkBytes), korl_arraySize(testString0));
    korl_assert(0 == korl_memory_stringCompare(testString0, _korl_log_context.rawLog));
    korl_assert(0 == korl_memory_stringCompare(testString1, KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, _korl_log_context.rawLog) +   _korl_log_context.rawLogChunkBytes)));
    korl_assert(0 == korl_memory_stringCompare(testString1, KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, _korl_log_context.rawLog) + 2*_korl_log_context.rawLogChunkBytes)));
    korl_memory_zero(_korl_log_context.rawLog, 2*_korl_log_context.rawLogChunkBytes);
#endif
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
        const Korl_File_ResultRenameReplace resultRenameReplace = 
            korl_file_renameReplace(KORL_FILE_PATHTYPE_LOCAL_DATA, logFileNameCurrent, KORL_FILE_PATHTYPE_LOCAL_DATA, logFileNameNext);
        korl_assert(   resultRenameReplace == KORL_FILE_RESULT_RENAME_REPLACE_SUCCESS 
                    || resultRenameReplace == KORL_FILE_RESULT_RENAME_REPLACE_SOURCE_FILE_DOES_NOT_EXIST);
    }
    /**/
    korl_assert(korl_file_create(KORL_FILE_PATHTYPE_LOCAL_DATA, 
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
    const u$ loggedCharacters = context->loggedBytes / sizeof(wchar_t);
    const u$ maxInitCharacters = context->useLogFileBig 
        ? loggedCharacters
        : // we will write a maximum of half of the maximum log buffer 
          // size, because the second half of the buffer is a circular 
          // buffer whose size we cannot possibly know ahead of time
          context->rawLogChunkBytes / sizeof(wchar_t);
    const u$ charactersToWrite = KORL_MATH_MIN(loggedCharacters, maxInitCharacters);
    if(charactersToWrite)
    {
        korl_assert(!KORL_MEMORY_POOL_ISFULL(context->asyncWriteDescriptors));
        _Korl_Log_AsyncWriteDescriptor*const asyncWriteDescriptor = KORL_MEMORY_POOL_ADD(context->asyncWriteDescriptors);
        korl_memory_zero(asyncWriteDescriptor, sizeof(*asyncWriteDescriptor));
        asyncWriteDescriptor->asyncIoHandle = korl_file_writeAsync(context->fileDescriptor, context->rawLog, charactersToWrite*sizeof(wchar_t));
        context->logFileBytesWritten += charactersToWrite*sizeof(wchar_t);
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
    if(context->useLogFileBig)
    {
        /* we don't have to do any sync writing here with the circular buffer, 
            since we would have already submitted write jobs for every line up 
            until this point anyway; do nothing */
    }
    else if(context->loggedBytes > 2*context->rawLogChunkBytes)
    {
        /* the log file has to be cut, since we lost log data during writes to 
            the circular buffer which wrapped to the beginning */
        korl_shared_const wchar_t LOG_MESSAGE_CUT[] = L"\n\n----- LOG FILE CUT -----\n\n\n";
        korl_file_write(context->fileDescriptor, LOG_MESSAGE_CUT, sizeof(LOG_MESSAGE_CUT));
        const u$ rawLogOffset = context->rawLogChunkBytes + ((context->loggedBytes - context->rawLogChunkBytes) % context->rawLogChunkBytes);
        wchar_t*const logBuffer = KORL_C_CAST(wchar_t*, KORL_C_CAST(u8*, context->rawLog) + rawLogOffset);
        korl_file_write(context->fileDescriptor
                       ,logBuffer + 1/*skip \0 terminator to reach the "head" of the ring buffer*/
                       ,context->rawLogChunkBytes);
    }
    else if(context->loggedBytes > context->rawLogChunkBytes)
    {
        /* we can just write the remaining buffer directly to the log file */
        const u$ remainingBytes       = context->loggedBytes              - context->rawLogChunkBytes;
        const void*const bufferOffset = KORL_C_CAST(u8*, context->rawLog) + context->rawLogChunkBytes;
        korl_file_write(context->fileDescriptor, bufferOffset, remainingBytes);
    }
    korl_file_close(&context->fileDescriptor);
    korl_memory_fileMapAllocation_destroy(context->rawLog, context->rawLogChunkBytes, 3);
skipFileCleanup:
    return;
}
korl_internal KORL_PLATFORM_LOG_GET_BUFFER(korl_log_getBuffer)
{
    _Korl_Log_Context*const context = &_korl_log_context;
    if(out_loggedBytes)
        *out_loggedBytes = context->loggedBytes;
    if(context->loggedBytes < 2*context->rawLogChunkBytes)
        return (acu16){.data = context->rawLog
                      ,.size = context->loggedBytes / sizeof(u16)};
    const u$ rawLogOffset = context->rawLogChunkBytes + ((context->loggedBytes - context->rawLogChunkBytes) % context->rawLogChunkBytes);
    return (acu16){.data = KORL_C_CAST(u16*, KORL_C_CAST(u8*, context->rawLog) + rawLogOffset) + 1
                  ,.size = context->rawLogChunkBytes / sizeof(u16)};
}
