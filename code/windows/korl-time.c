#include "korl-time.h"
#include "korl-windows-globalDefines.h"
#include "korl-memory.h"
#include "korl-log.h"
#include "korl-interface-platform-time.h"
#include "korl-stb-ds.h"
#include "korl-checkCast.h"
#include "korl-stringPool.h"
#define _KORL_TIME_DESIRED_OS_TIMER_GRANULARITY_MS 1
typedef struct _Korl_Time_Probe
{
    PlatformTimeStamp timeStampStart;
    PlatformTimeStamp timeStampEnd;
    const wchar_t* file;
    const wchar_t* function;
    const wchar_t* label;
    int line;
    Korl_Time_ProbeHandle parent;// 0 (invalid handle) if there is no parent
} _Korl_Time_Probe;
typedef enum _Korl_Time_ProbeData_Type
    { _KORL_TIME_PROBE_DATA_TYPE_U32
} _Korl_Time_ProbeData_Type;
typedef struct _Korl_Time_ProbeData
{
    Korl_Time_ProbeHandle probeHandle;
    _Korl_Time_ProbeData_Type type;
    u32 bufferOffsetLabel;
    u32 bufferOffsetData;
} _Korl_Time_ProbeData;
typedef struct _Korl_Time_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    _Korl_Time_Probe*      stbDaTimeProbes;
    Korl_Time_ProbeHandle* stbDaTimeProbeStack;
    u8*                    stbDaTimeProbeDataRaw;
    _Korl_Time_ProbeData*  stbDaTimeProbeData;
    bool systemSupportsDesiredTimerGranularity;
    bool sleepIsGranular;
} _Korl_Time_Context;
korl_global_variable _Korl_Time_Context _korl_time_context;
korl_global_variable LARGE_INTEGER _korl_time_perfCounterHz;
typedef union _Korl_Time_TimeStampUnion
{
    LARGE_INTEGER largeInt;
    PlatformTimeStamp timeStamp;
} _Korl_Time_TimeStampUnion;
KORL_STATIC_ASSERT(sizeof(PlatformTimeStamp) <= sizeof(LARGE_INTEGER));
korl_internal void _korl_time_timeStampExtractDifferenceCounts(Korl_Time_Counts countDiff, 
                                                               u$* out_minutes, u8* out_seconds, u16* out_milliseconds, u16* out_microseconds)
{
    const Korl_Time_Counts perfCountPerMinute      = _korl_time_perfCounterHz.QuadPart * 60;
    const Korl_Time_Counts perfCountPerMilliSecond = _korl_time_perfCounterHz.QuadPart / 1000;
    const Korl_Time_Counts perfCountPerMicroSecond = _korl_time_perfCounterHz.QuadPart / 1000000;
    *out_minutes = countDiff / perfCountPerMinute;
    countDiff %= perfCountPerMinute;
    *out_seconds = korl_checkCast_i$_to_u8(countDiff / _korl_time_perfCounterHz.QuadPart);
    countDiff %= _korl_time_perfCounterHz.QuadPart;
    *out_milliseconds = korl_checkCast_i$_to_u16(countDiff / perfCountPerMilliSecond);
    countDiff %= perfCountPerMilliSecond;
    *out_microseconds = korl_checkCast_i$_to_u16(countDiff / perfCountPerMicroSecond);
}
korl_internal void _korl_time_timeStampExtractDifference(PlatformTimeStamp ptsA, PlatformTimeStamp ptsB, 
                                                         u$* out_minutes, u8* out_seconds, u16* out_milliseconds, u16* out_microseconds)
{
    Korl_Time_Counts countDiff = korl_time_timeStampCountDifference(ptsA, ptsB);
    _korl_time_timeStampExtractDifferenceCounts(countDiff, out_minutes, out_seconds, out_milliseconds, out_microseconds);
}
korl_internal void korl_time_initialize(void)
{
    korl_memory_zero(&_korl_time_context, sizeof(_korl_time_context));
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(16);
    _korl_time_context.allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, L"korl-time", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbes      , 1024/*1024 comes out to ~48kB*/);
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbeStack  , 128);
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbeDataRaw, korl_math_kilobytes(16));
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbeData   , 1024);
    /* According to MSDN, this function is _guaranteed_ to succeed on all 
        versions of Windows >= XP, so we really should ensure that it does 
        succeed since high-performance timing capabilitites are critical */
    const BOOL successQueryPerformanceFrequency = 
        QueryPerformanceFrequency(&_korl_time_perfCounterHz);
    if(!successQueryPerformanceFrequency)
        korl_logLastError("QueryPerformanceFrequency failed");
    korl_assert(successQueryPerformanceFrequency);
    // Determine if the system is capable of our desired timer granularity //
    TIMECAPS timingCapabilities;
    if(timeGetDevCaps(&timingCapabilities, sizeof(TIMECAPS)) == MMSYSERR_NOERROR)
        _korl_time_context.systemSupportsDesiredTimerGranularity = 
            (timingCapabilities.wPeriodMin <= _KORL_TIME_DESIRED_OS_TIMER_GRANULARITY_MS);
    else
        korl_log(WARNING, "System doesn't support custom scheduler granularity."
                          "  Sleep will not be called!");
    // set scheduler granularity using timeBeginPeriod //
    /* MSDN notes:  prior to Windows 10 version 2004, this used to be a _GLOBAL_ 
        system setting, and greater care must be taken to ensure that we 
        "clean up" this value (which we do in korl_time_shutDown), but I really 
        don't care about that level of "support" for older Windows versions, 
        since it seems like most people use Win10+ nowadays anyway.  At least 
        Steam survey shows like < 5% of users use a version of Windows prior to 
        Windows 10, for example.  Of course, I will go ahead and at least clean 
        up this value when the time module shuts down for normal execution. */
    _korl_time_context.sleepIsGranular = _korl_time_context.systemSupportsDesiredTimerGranularity &&
        timeBeginPeriod(_KORL_TIME_DESIRED_OS_TIMER_GRANULARITY_MS) == TIMERR_NOERROR;
    if(!_korl_time_context.sleepIsGranular)
        korl_log(WARNING, "System supports custom scheduler granularity, but "
                          "setting this value has failed!  Sleep will not be "
                          "called!");
}
korl_internal void korl_time_shutDown(void)
{
    if(   _korl_time_context.sleepIsGranular 
       && timeEndPeriod(_KORL_TIME_DESIRED_OS_TIMER_GRANULARITY_MS) != TIMERR_NOERROR)
        korl_log(ERROR, "timeEndPeriod failed");
}
korl_internal KORL_FUNCTION_korl_timeStamp(korl_timeStamp)
{
    KORL_ZERO_STACK(_Korl_Time_TimeStampUnion, timeStampUnion);
    if(!QueryPerformanceCounter(&timeStampUnion.largeInt))
        korl_logLastError("QueryPerformanceCounter failed");
    return timeStampUnion.timeStamp;
}
korl_internal KORL_FUNCTION_korl_time_secondsSinceTimeStamp(korl_time_secondsSinceTimeStamp)
{
    _Korl_Time_TimeStampUnion timeStampUnionPrevious;
    timeStampUnionPrevious.timeStamp = pts;
    KORL_ZERO_STACK(_Korl_Time_TimeStampUnion, timeStampUnion);
    if(!QueryPerformanceCounter(&timeStampUnion.largeInt))
        korl_logLastError("QueryPerformanceCounter failed");
    /* calculate the # of seconds between the previous timestamp and the current timestamp */
    korl_assert(timeStampUnion.largeInt.QuadPart >= timeStampUnionPrevious.largeInt.QuadPart);
    const LONGLONG perfCountDiff = timeStampUnion.largeInt.QuadPart - timeStampUnionPrevious.largeInt.QuadPart;
    return KORL_C_CAST(f32, perfCountDiff) / KORL_C_CAST(f32, _korl_time_perfCounterHz.QuadPart);
}
korl_internal Korl_Time_Counts korl_time_countsFromHz(u16 hz)
{
    return _korl_time_perfCounterHz.QuadPart / hz;
}
korl_internal Korl_Time_Counts korl_time_timeStampCountDifference(PlatformTimeStamp ptsA, PlatformTimeStamp ptsB)
{
    _Korl_Time_TimeStampUnion tsuA, tsuB;
    tsuA.timeStamp = ptsA;
    tsuB.timeStamp = ptsB;
    /* make sure that tsuA is always the _smaller_ time stamp */
    if(tsuA.largeInt.QuadPart > tsuB.largeInt.QuadPart)
    {
        const LARGE_INTEGER temp = tsuA.largeInt;
        tsuA.largeInt = tsuB.largeInt;
        tsuB.largeInt = temp;
    }
    return tsuB.largeInt.QuadPart - tsuA.largeInt.QuadPart;
}
korl_internal void korl_time_sleep(Korl_Time_Counts counts)
{
    const PlatformTimeStamp sleepStart = korl_timeStamp();
    u$ timeDiffMinutes;
    u8 timeDiffSeconds;
    u16 timeDiffMilliseconds, timeDiffMicroseconds;
    _korl_time_timeStampExtractDifferenceCounts(counts, &timeDiffMinutes, &timeDiffSeconds, &timeDiffMilliseconds, &timeDiffMicroseconds);
    /* we use this value to spin the CPU for a small amount of time each sleep 
        attempt because Sleep is extremely inaccurate, and experimentally this 
        improves Sleep accuracy _tramendously_, with likely very small CPU cost */
    korl_shared_const DWORD SLEEP_SLACK_MILLISECONDS = 1;
    if(timeDiffMilliseconds > SLEEP_SLACK_MILLISECONDS && _korl_time_context.sleepIsGranular)
        Sleep(timeDiffMilliseconds - SLEEP_SLACK_MILLISECONDS);
    while(korl_time_timeStampCountDifference(sleepStart, korl_timeStamp()) < counts)
        if(_korl_time_context.sleepIsGranular)
            Sleep(0);
}
korl_internal i$ korl_time_countsFormatBuffer(Korl_Time_Counts counts, wchar_t* buffer, u$ bufferBytes)
{
    u$ timeDiffMinutes;
    u8 timeDiffSeconds;
    u16 timeDiffMilliseconds, timeDiffMicroseconds;
    _korl_time_timeStampExtractDifferenceCounts(counts, &timeDiffMinutes, &timeDiffSeconds, &timeDiffMilliseconds, &timeDiffMicroseconds);
    return korl_string_formatBufferUtf16(buffer, bufferBytes
                                        ,L"% 2llu:% 2hhu'% 3hu\"% 3hu"
                                        ,timeDiffMinutes, timeDiffSeconds, timeDiffMilliseconds, timeDiffMicroseconds);
}
korl_internal Korl_Time_DateStamp_Compare_Result korl_time_dateStampCompare(KorlPlatformDateStamp dsA, KorlPlatformDateStamp dsB)
{
    union
    {
        FILETIME fileTime;
        KorlPlatformDateStamp dateStamp;
    } dsuA, dsuB;
    dsuA.dateStamp = dsA;
    dsuB.dateStamp = dsB;
    const LONG resultCompareFileTime = CompareFileTime(&dsuA.fileTime, &dsuB.fileTime);
    switch(resultCompareFileTime)
    {
    case -1:{
        return KORL_TIME_DATESTAMP_COMPARE_RESULT_FIRST_TIME_EARLIER;}
    case 0:{
        return KORL_TIME_DATESTAMP_COMPARE_RESULT_EQUAL;}
    case 1:{
        return KORL_TIME_DATESTAMP_COMPARE_RESULT_FIRST_TIME_LATER;}
    default:{
        korl_log(ERROR, "invalid CompareFileTime result: %li", resultCompareFileTime);
        return KORL_TIME_DATESTAMP_COMPARE_RESULT_FIRST_TIME_EARLIER;}
    }
}
korl_internal Korl_Time_ProbeHandle korl_time_probeBegin(const wchar_t* file, const wchar_t* function, int line, const wchar_t* label)
{
    const PlatformTimeStamp timeStampProbeStart = korl_timeStamp();// obtain the time stamp as fast as possible!
    const Korl_Time_ProbeHandle timeProbeIndex = korl_checkCast_u$_to_u32(arrlenu(_korl_time_context.stbDaTimeProbes));
    mcarrpush(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbes, (_Korl_Time_Probe){0});
    korl_assert(timeProbeIndex < ~KORL_C_CAST(Korl_Time_ProbeHandle, 0));// we cannot index into the maximum unsigned integer value, since handle values must be representable in the same bit size & be non-zero!
    _Korl_Time_Probe*const timeProbe = &_korl_time_context.stbDaTimeProbes[timeProbeIndex];
    korl_memory_zero(timeProbe, sizeof(*timeProbe));
    timeProbe->timeStampStart = timeStampProbeStart;
    timeProbe->file           = file;
    timeProbe->function       = function;
    timeProbe->line           = line;
    timeProbe->label          = label;
    if(arrlenu(_korl_time_context.stbDaTimeProbeStack))
        timeProbe->parent = arrlast(_korl_time_context.stbDaTimeProbeStack);
    return mcarrpush(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbeStack, timeProbeIndex + 1/*a handle value of 0 should be considered invalid*/);
}
korl_internal Korl_Time_Counts korl_time_probeEnd(Korl_Time_ProbeHandle timeProbeHandle)
{
    const PlatformTimeStamp timeStampProbeEnd = korl_timeStamp();// obtain the time stamp as fast as possible!
    korl_assert(timeProbeHandle);
    korl_assert(arrlenu(_korl_time_context.stbDaTimeProbeStack) > 0);
    korl_assert(arrpop(_korl_time_context.stbDaTimeProbeStack) == timeProbeHandle);
    timeProbeHandle--;// transform the handle back into an array index
    korl_assert(timeProbeHandle < arrlenu(_korl_time_context.stbDaTimeProbes));
    _Korl_Time_Probe*const timeProbe = &_korl_time_context.stbDaTimeProbes[timeProbeHandle];
    timeProbe->timeStampEnd = timeStampProbeEnd;
    /* convenience: return the platform-specific time counts between the probe 
        start/stop points (if we never utilize this anywhere, we can probably 
        just get rid of this later I guess?  But it should be an extremely cheap 
        operation) */
    korl_assert(timeProbe->timeStampStart <= timeProbe->timeStampEnd);
    return timeProbe->timeStampEnd - timeProbe->timeStampStart;
}
korl_internal void korl_time_probeDataU32(const wchar_t* label, u32 data)
{
    korl_assert(arrlenu(_korl_time_context.stbDaTimeProbeStack) > 0);
    const u$ labelSize  = korl_string_sizeUtf16(label);
    const u$ labelBytes = (labelSize + 1/*null-terminator*/) * sizeof(*label);
    const u32 bufferOffsetLabel = korl_checkCast_u$_to_u32(arrlenu(_korl_time_context.stbDaTimeProbeDataRaw));
    const u32 bufferOffsetData  = korl_checkCast_u$_to_u32(arrlenu(_korl_time_context.stbDaTimeProbeDataRaw) + labelBytes);
    mcarrsetlen(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbeDataRaw, 
                arrlenu(_korl_time_context.stbDaTimeProbeDataRaw) + labelBytes + sizeof(data));
    korl_memory_copy(_korl_time_context.stbDaTimeProbeDataRaw + bufferOffsetLabel, label, labelBytes);
    korl_memory_copy(_korl_time_context.stbDaTimeProbeDataRaw + bufferOffsetData , &data, sizeof(data));
    mcarrpush(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbeData, (_Korl_Time_ProbeData){0});
    arrlast(_korl_time_context.stbDaTimeProbeData).probeHandle       = arrlast(_korl_time_context.stbDaTimeProbeStack);
    arrlast(_korl_time_context.stbDaTimeProbeData).type              = _KORL_TIME_PROBE_DATA_TYPE_U32;
    arrlast(_korl_time_context.stbDaTimeProbeData).bufferOffsetLabel = bufferOffsetLabel;
    arrlast(_korl_time_context.stbDaTimeProbeData).bufferOffsetData  = bufferOffsetData;
}
korl_internal void korl_time_probeLogReport(i32 maxProbeDepth)
{
    const PlatformTimeStamp timeStampStart = korl_timeStamp();
    u$ timeDiffMinutes;
    u8 timeDiffSeconds;
    u16 timeDiffMilliseconds, timeDiffMicroseconds;
    /** 
     * Desired time duration format:
     *  Minutes:Seconds'Milliseconds"Microseconds
     *  mm:ss'mmm"uuu
     * 
     * Desired output example:
     *  ║ mm:ss'mmm"uuu ----[probe_label] function_name; file_name.c:line (dataLabel0=data0, ...)
     *  ║ ├ mm:ss'mmm"uuu --[probe_label] function_name; file_name.c:line (dataLabel0=data0, ...)
     *  ║ │ ├ mm:ss'mmm"uuu [probe_label] function_name; file_name.c:line (dataLabel0=data0, ...)
     *  ║ │ └ mm:ss'mmm"uuu [probe_label] function_name; file_name.c:line (dataLabel0=data0, ...)
     *  ║ └ mm:ss'mmm"uuu --[probe_label] function_name; file_name.c:line (dataLabel0=data0, ...)
     *  ║   └ mm:ss'mmm"uuu [probe_label] function_name; file_name.c:line (dataLabel0=data0, ...)
     *  ║ mm:ss'mmm"uuu ----[probe_label] function_name; file_name.c:line (dataLabel0=data0, ...)
     */
    korl_assert(arrlenu(_korl_time_context.stbDaTimeProbeStack) == 0);
    korl_log_noMeta(INFO, "╔════ ⏲ Time Probes Report ═════ Duration Format: MM:ss'mmm\"uuu ═══════════╗");
    /* Before logging the actual report, let's do a pass to gather some metrics 
        to improve the appearance of the final report. */
    u$ longestProbeLabel    = 0;
    u$ longestProbeFunction = 0;
    u$ deepestProbeDepth    = 0;
    Korl_Time_ProbeHandle* stbDaTimeProbeStack = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), stbDaTimeProbeStack, arrlenu(_korl_time_context.stbDaTimeProbeStack));
    u$*               timeProbeDirectChildren = korl_allocate(_korl_time_context.allocatorHandle, arrlenu(_korl_time_context.stbDaTimeProbes) * sizeof(*timeProbeDirectChildren));
    Korl_Time_Counts* timeProbeCoverageCounts = korl_allocate(_korl_time_context.allocatorHandle, arrlenu(_korl_time_context.stbDaTimeProbes) * sizeof(*timeProbeCoverageCounts));
    for(Korl_Time_ProbeHandle tpi = 0; tpi < arrlenu(_korl_time_context.stbDaTimeProbes); tpi++)
    {
        const _Korl_Time_Probe* timeProbe = &_korl_time_context.stbDaTimeProbes[tpi];
        if(timeProbe->parent)
        {
            timeProbeDirectChildren[timeProbe->parent - 1]++;
            timeProbeCoverageCounts[timeProbe->parent - 1] += korl_time_timeStampCountDifference(timeProbe->timeStampStart, timeProbe->timeStampEnd);
        }
        if(timeProbe->label)
            longestProbeLabel = KORL_MATH_MAX(longestProbeLabel, korl_string_sizeUtf16(timeProbe->label));
        longestProbeFunction = KORL_MATH_MAX(longestProbeFunction, korl_string_sizeUtf16(timeProbe->function));
        while(arrlenu(stbDaTimeProbeStack) && (!timeProbe->parent || (timeProbe->parent && arrlast(stbDaTimeProbeStack) != timeProbe->parent)))
            arrpop(stbDaTimeProbeStack);
        deepestProbeDepth = KORL_MATH_MAX(deepestProbeDepth, arrlenu(stbDaTimeProbeStack));
        mcarrpush(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), stbDaTimeProbeStack, tpi + 1);
    }
    /* okay, now we can print the full report, including flex juice 😎 */
    const u$ bufferDurationSize = 13/*duration time as described*/ + 2*deepestProbeDepth/*spacing required for probe depth tabbing style*/ + 1/*null terminator*/;
    wchar_t* bufferDuration = korl_allocate(_korl_time_context.allocatorHandle, bufferDurationSize * sizeof(*bufferDuration));
    korl_assert(bufferDuration);
    // print help labels at the top so the reader can understand the report //
    korl_log_noMeta(INFO, "║ %*ws┌probe duration", 
                    (bufferDurationSize/2) - 1/*null terminator*/, L"");
    korl_log_noMeta(INFO, "║ %*ws│%*ws ┌coverage%%: what percentage of the duration is accounted for by child probes", 
                    (bufferDurationSize/2) - 1/*null terminator*/                       , L"", 
                    bufferDurationSize - ((bufferDurationSize/2) - 1/*null terminator*/), L"");
    korl_log_noMeta(INFO, "║ %*ws│%*ws │   %*ws┌probe label", 
                    (bufferDurationSize/2) - 1/*null terminator*/                       , L"", 
                    bufferDurationSize - ((bufferDurationSize/2) - 1/*null terminator*/), L"", 
                    longestProbeLabel/2                                                 , L"");
    // print the actual report for each time probe //
    mcarrsetlen(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), stbDaTimeProbeStack, 0);
    Korl_StringPool stringPool        = korl_stringPool_create(_korl_time_context.allocatorHandle);
    Korl_StringPool_String stringData = korl_stringNewEmptyUtf16(&stringPool, 0);
    u$ currentProbeData = 0;
    for(Korl_Time_ProbeHandle tpi = 0; tpi < arrlenu(_korl_time_context.stbDaTimeProbes); tpi++)
    {
        const _Korl_Time_Probe*const timeProbe = &_korl_time_context.stbDaTimeProbes[tpi];
        /* pop probes off the stack until we encounter our parent */
        while(arrlenu(stbDaTimeProbeStack) && (!timeProbe->parent || (timeProbe->parent && arrlast(stbDaTimeProbeStack) != timeProbe->parent)))
            arrpop(stbDaTimeProbeStack);
        if(maxProbeDepth >= 0 && arrlen(stbDaTimeProbeStack) > maxProbeDepth)
            goto printProbesLoop_cleanUp;
        /**/
        const wchar_t* file = timeProbe->file;
        for(const wchar_t* fileNameCursor = timeProbe->file; *fileNameCursor; fileNameCursor++)
            if(*fileNameCursor == '\\' || *fileNameCursor == '/')
                file = fileNameCursor + 1;
        /* assemble the formatted duration string for this probe */
        _korl_time_timeStampExtractDifference(timeProbe->timeStampStart, timeProbe->timeStampEnd, 
                                              &timeDiffMinutes, &timeDiffSeconds, &timeDiffMilliseconds, &timeDiffMicroseconds);
        timeDiffMinutes = KORL_MATH_CLAMP(timeDiffMinutes, 0, 99);
        const i$ durationBufferSize = 
            korl_string_formatBufferUtf16(bufferDuration, bufferDurationSize*sizeof(*bufferDuration)
                                         ,L"%*ws% 2llu:% 2hhu'% 3hu\"% 3hu%*ws"
                                         ,2*arrlenu(stbDaTimeProbeStack), L""
                                         ,timeDiffMinutes, timeDiffSeconds, timeDiffMilliseconds, timeDiffMicroseconds
                                         ,2*(deepestProbeDepth - arrlenu(stbDaTimeProbeStack)), L"");
        korl_assert(durationBufferSize > 0);
        /* now we can look at the contents of timeProbeDirectChildren for each 
            of the handles in the stack to see if we need to print a tabbing 
            character for that level of the stack; then we can decrement our 
            parent's direct children count to keep later probes updated */
        for(u$ s = 0; s < arrlenu(stbDaTimeProbeStack); s++)
        {
            if(timeProbeDirectChildren[stbDaTimeProbeStack[s] - 1] == 1)
                if(s == arrlenu(stbDaTimeProbeStack) - 1)
                    bufferDuration[s*2] = L'└';
                else
                    bufferDuration[s*2] = L'│';
            else if(timeProbeDirectChildren[stbDaTimeProbeStack[s] - 1] > 0)
                if(s == arrlenu(stbDaTimeProbeStack) - 1)
                    bufferDuration[s*2] = L'├';
                else
                    bufferDuration[s*2] = L'│';
        }
        if(timeProbe->parent)
            timeProbeDirectChildren[timeProbe->parent - 1]--;
        /* accumulate a string containing any data associated with this probe; 
            here we take advantage of the fact that the probe data entries are 
            "in-line" with the probe entries */
        while(   currentProbeData < arrlenu(_korl_time_context.stbDaTimeProbeData) 
              && _korl_time_context.stbDaTimeProbeData[currentProbeData].probeHandle == tpi + 1)
        {
            const u16*const dataLabel = KORL_C_CAST(u16*, _korl_time_context.stbDaTimeProbeDataRaw + _korl_time_context.stbDaTimeProbeData[currentProbeData].bufferOffsetLabel);
            if(string_getRawSizeUtf16(&stringData) == 0)
                string_appendUtf16(&stringData, L"(");
            else
                string_appendUtf16(&stringData, L", ");
            string_appendFormatUtf16(&stringData, L"%ws: ", dataLabel);
            switch(_korl_time_context.stbDaTimeProbeData[currentProbeData].type)
            {
            case _KORL_TIME_PROBE_DATA_TYPE_U32:{
                const u32*const data = KORL_C_CAST(u32*, _korl_time_context.stbDaTimeProbeDataRaw + _korl_time_context.stbDaTimeProbeData[currentProbeData].bufferOffsetData);
                string_appendFormatUtf16(&stringData, L"%u", *data);
                break;}
            }
            currentProbeData++;
        }
        if(string_getRawSizeUtf16(&stringData))
            string_appendUtf16(&stringData, L")");
        /**/
        const f64 probeCoverageRatio = KORL_C_CAST(f64, timeProbeCoverageCounts[tpi]) 
                                     / KORL_C_CAST(f64, korl_time_timeStampCountDifference(timeProbe->timeStampStart, timeProbe->timeStampEnd));
        wchar_t bufferCoverageRatio[] = L"   ∞";
        if(timeProbeDirectChildren[tpi])
            korl_assert(0 < korl_string_formatBufferUtf16(bufferCoverageRatio, sizeof(bufferCoverageRatio)
                                                         ,L"%3hhu%%", KORL_C_CAST(u8, probeCoverageRatio * 100)));
        korl_log_noMeta(INFO, "║ %ws %ws [%-*ws] %-*ws; %ws:%i %ws",
                        bufferDuration, bufferCoverageRatio, 
                        longestProbeLabel, timeProbe->label ? timeProbe->label : L"", 
                        longestProbeFunction, timeProbe->function, file, timeProbe->line, 
                        string_getRawUtf16(&stringData));
        printProbesLoop_cleanUp:
            mcarrpush(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), stbDaTimeProbeStack, tpi + 1);
            string_reserveUtf16(&stringData, 0);// clear the data string buffer for re-use in future probes
    }
    korl_free(_korl_time_context.allocatorHandle, bufferDuration);
    korl_free(_korl_time_context.allocatorHandle, timeProbeCoverageCounts);
    korl_free(_korl_time_context.allocatorHandle, timeProbeDirectChildren);
    mcarrfree(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), stbDaTimeProbeStack);
    korl_stringPool_destroy(&stringPool);// no need to free individual strings, since we're just going to destroy the entire pool
    const PlatformTimeStamp timeStampEnd = korl_timeStamp();
    _korl_time_timeStampExtractDifference(timeStampStart, timeStampEnd, 
                                          &timeDiffMinutes, &timeDiffSeconds, &timeDiffMilliseconds, &timeDiffMicroseconds);
    korl_log_noMeta(INFO, "╚═════ END of time probe report!  Report time taken: %02llu:%02hhu'%03hu\"%03hu | Total Probes: %u ═════════╝",
                    timeDiffMinutes, timeDiffSeconds, timeDiffMilliseconds, timeDiffMicroseconds, 
                    arrlenu(_korl_time_context.stbDaTimeProbes));
}
korl_internal void korl_time_probeReset(void)
{
    mcarrsetlen(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbes      , 0);
    mcarrsetlen(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbeStack  , 0);
    mcarrsetlen(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbeDataRaw, 0);
    mcarrsetlen(KORL_STB_DS_MC_CAST(_korl_time_context.allocatorHandle), _korl_time_context.stbDaTimeProbeData   , 0);
}
