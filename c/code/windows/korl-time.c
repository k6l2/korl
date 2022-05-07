#include "korl-time.h"
#include "korl-windows-globalDefines.h"
#include "korl-memory.h"
#include "korl-log.h"
#include "korl-interface-platform.h"
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
#define _KORL_TIME_PROBE_STACK_DEPTH_MAX 128
typedef struct _Korl_Time_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    _Korl_Time_Probe* timeProbes;//KORL-FEATURE-000-000-007: dynamic resizing arrays
    Korl_Time_ProbeHandle timeProbesCapacity;
    Korl_Time_ProbeHandle timeProbesCount;
    Korl_Time_ProbeHandle timeProbeStack[_KORL_TIME_PROBE_STACK_DEPTH_MAX];
    u16 timeProbeStackDepth;
} _Korl_Time_Context;
korl_global_variable _Korl_Time_Context _korl_time_context;
korl_global_variable LARGE_INTEGER _korl_time_perfCounterHz;
typedef union _Korl_Time_TimeStampUnion
{
    LARGE_INTEGER largeInt;
    PlatformTimeStamp timeStamp;
} _Korl_Time_TimeStampUnion;
_STATIC_ASSERT(sizeof(PlatformTimeStamp) <= sizeof(LARGE_INTEGER));
korl_internal void _korl_time_timeStampExtractDifference(PlatformTimeStamp ptsA, PlatformTimeStamp ptsB, 
                                                         u$* out_minutes, u8* out_seconds, u16* out_milliseconds, u16* out_microseconds)
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
    LONGLONG diff = tsuB.largeInt.QuadPart - tsuA.largeInt.QuadPart;
    const LONGLONG perfCountPerMinute      = _korl_time_perfCounterHz.QuadPart * 60;
    const LONGLONG perfCountPerMilliSecond = _korl_time_perfCounterHz.QuadPart / 1000;
    const LONGLONG perfCountPerMicroSecond = _korl_time_perfCounterHz.QuadPart / 1000000;
    *out_minutes = diff / perfCountPerMinute;
    diff %= perfCountPerMinute;
    *out_seconds = korl_checkCast_i$_to_u8(diff / _korl_time_perfCounterHz.QuadPart);
    diff %= _korl_time_perfCounterHz.QuadPart;
    *out_milliseconds = korl_checkCast_i$_to_u16(diff / perfCountPerMilliSecond);
    diff %= perfCountPerMilliSecond;
    *out_microseconds = korl_checkCast_i$_to_u16(diff / perfCountPerMicroSecond);
}
korl_internal void korl_time_initialize(void)
{
    korl_memory_zero(&_korl_time_context, sizeof(_korl_time_context));
    _korl_time_context.allocatorHandle    = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_kilobytes(64));
    _korl_time_context.timeProbesCapacity = 128;
    _korl_time_context.timeProbes         = korl_allocate(_korl_time_context.allocatorHandle, _korl_time_context.timeProbesCapacity * sizeof(*_korl_time_context.timeProbes));
    /* According to MSDN, this function is _guaranteed_ to succeed on all 
        versions of Windows >= XP, so we really should ensure that it does 
        succeed since high-performance timing capabilitites are critical */
    const BOOL successQueryPerformanceFrequency = 
        QueryPerformanceFrequency(&_korl_time_perfCounterHz);
    if(!successQueryPerformanceFrequency)
        korl_logLastError("QueryPerformanceFrequency failed");
    korl_assert(successQueryPerformanceFrequency);
}
korl_internal KORL_PLATFORM_GET_TIMESTAMP(korl_timeStamp)
{
    KORL_ZERO_STACK(_Korl_Time_TimeStampUnion, timeStampUnion);
    if(!QueryPerformanceCounter(&timeStampUnion.largeInt))
        korl_logLastError("QueryPerformanceCounter failed");
    return timeStampUnion.timeStamp;
}
korl_internal KORL_PLATFORM_SECONDS_SINCE_TIMESTAMP(korl_time_secondsSinceTimeStamp)
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
korl_internal Korl_Time_ProbeHandle korl_time_probeBegin(const wchar_t* file, const wchar_t* function, int line, const wchar_t* label)
{
    const PlatformTimeStamp timeStampProbeStart = korl_timeStamp();// obtain the time stamp as fast as possible!
    if(_korl_time_context.timeProbesCount >= _korl_time_context.timeProbesCapacity)
    {
        const Korl_Time_ProbeHandle newCapacity = _korl_time_context.timeProbesCapacity * 2;
        _korl_time_context.timeProbes = korl_reallocate(_korl_time_context.allocatorHandle, _korl_time_context.timeProbes, newCapacity * sizeof(*_korl_time_context.timeProbes));
        korl_assert(_korl_time_context.timeProbes);
        _korl_time_context.timeProbesCapacity = newCapacity;
    }
    const Korl_Time_ProbeHandle timeProbeIndex = _korl_time_context.timeProbesCount++;
    korl_assert(timeProbeIndex < ~KORL_C_CAST(Korl_Time_ProbeHandle, 0));// we cannot index into the maximum unsigned integer value, since handle values must be representable in the same bit size & be non-zero!
    _Korl_Time_Probe*const timeProbe = &_korl_time_context.timeProbes[timeProbeIndex];
    korl_memory_zero(timeProbe, sizeof(*timeProbe));
    timeProbe->timeStampStart = timeStampProbeStart;
    timeProbe->file           = file;
    timeProbe->function       = function;
    timeProbe->line           = line;
    timeProbe->label          = label;
    if(_korl_time_context.timeProbeStackDepth > 0)
        timeProbe->parent = _korl_time_context.timeProbeStack[_korl_time_context.timeProbeStackDepth - 1];
    korl_assert(_korl_time_context.timeProbeStackDepth < korl_arraySize(_korl_time_context.timeProbeStack));
    _korl_time_context.timeProbeStack[_korl_time_context.timeProbeStackDepth++] = timeProbeIndex + 1;
    return timeProbeIndex + 1;// a handle value of 0 should be considered invalid
}
korl_internal void korl_time_probeEnd(Korl_Time_ProbeHandle timeProbeHandle)
{
    const PlatformTimeStamp timeStampProbeEnd = korl_timeStamp();// obtain the time stamp as fast as possible!
    korl_assert(timeProbeHandle);
    korl_assert(_korl_time_context.timeProbeStackDepth > 0);
    korl_assert(_korl_time_context.timeProbeStack[_korl_time_context.timeProbeStackDepth - 1] == timeProbeHandle);
    _korl_time_context.timeProbeStackDepth--;
    timeProbeHandle--;// transform the handle back into an array index
    korl_assert(timeProbeHandle < _korl_time_context.timeProbesCount);
    _Korl_Time_Probe*const timeProbe = &_korl_time_context.timeProbes[timeProbeHandle];
    timeProbe->timeStampEnd = timeStampProbeEnd;
}
korl_internal void korl_time_probeLogReport(void)
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
     *  ║ mm:ss'mmm"uuu ----[probe_label] function_name; file_name.c:line
     *  ║ ├ mm:ss'mmm"uuu --[probe_label] function_name; file_name.c:line
     *  ║ │ ├ mm:ss'mmm"uuu [probe_label] function_name; file_name.c:line
     *  ║ │ └ mm:ss'mmm"uuu [probe_label] function_name; file_name.c:line
     *  ║ └ mm:ss'mmm"uuu --[probe_label] function_name; file_name.c:line
     *  ║   └ mm:ss'mmm"uuu [probe_label] function_name; file_name.c:line
     *  ║ mm:ss'mmm"uuu ----[probe_label] function_name; file_name.c:line
     */
    korl_assert(_korl_time_context.timeProbeStackDepth == 0);
    korl_log(INFO, "╔════ Time probes report ═════ Duration format: mm:ss'mmm\"uuu ═════════════╗");
    /* Before logging the actual report, let's do a pass to gather some metrics 
        to improve the appearance of the final report. */
    u$ longestProbeLabel = 0;
    u$ longestProbeFunction = 0;
    u$ deepestProbeDepth = 0;
    Korl_Time_ProbeHandle timeProbeStack[_KORL_TIME_PROBE_STACK_DEPTH_MAX];
    u$ timeProbeStackDepth = 0;
    u$* timeProbeDirectChildren = korl_allocate(_korl_time_context.allocatorHandle, _korl_time_context.timeProbesCount * sizeof(*timeProbeDirectChildren));
    korl_assert(timeProbeDirectChildren);
    for(Korl_Time_ProbeHandle tpi = 0; tpi < _korl_time_context.timeProbesCount; tpi++)
    {
        const _Korl_Time_Probe* timeProbe = &_korl_time_context.timeProbes[tpi];
        if(timeProbe->parent)
            timeProbeDirectChildren[timeProbe->parent - 1]++;
        if(timeProbe->label)
            longestProbeLabel = KORL_MATH_MAX(longestProbeLabel, korl_memory_stringSize(timeProbe->label));
        longestProbeFunction = KORL_MATH_MAX(longestProbeFunction, korl_memory_stringSize(timeProbe->function));
        while(timeProbeStackDepth && (!timeProbe->parent || (timeProbe->parent && timeProbeStack[timeProbeStackDepth - 1] != timeProbe->parent)))
            timeProbeStackDepth--;
        deepestProbeDepth = KORL_MATH_MAX(deepestProbeDepth, timeProbeStackDepth);
        timeProbeStack[timeProbeStackDepth++] = tpi + 1;
    }
    /* okay, now we can print the full report, including flex juice 😎 */
    const u$ bufferDurationSize = 13/*duration time as described*/ + 2*deepestProbeDepth/*spacing required for probe depth tabbing style*/ + 1/*null terminator*/;
    wchar_t* bufferDuration = korl_allocate(_korl_time_context.allocatorHandle, bufferDurationSize * sizeof(wchar_t));
    korl_assert(bufferDuration);
    timeProbeStackDepth = 0;
    for(Korl_Time_ProbeHandle tpi = 0; tpi < _korl_time_context.timeProbesCount; tpi++)
    {
        const _Korl_Time_Probe*const timeProbe = &_korl_time_context.timeProbes[tpi];
        _korl_time_timeStampExtractDifference(timeProbe->timeStampStart, timeProbe->timeStampEnd, 
                                              &timeDiffMinutes, &timeDiffSeconds, &timeDiffMilliseconds, &timeDiffMicroseconds);
        timeDiffMinutes = KORL_MATH_CLAMP(timeDiffMinutes, 0, 99);
        const wchar_t* file = timeProbe->file;
        for(const wchar_t* fileNameCursor = timeProbe->file; *fileNameCursor; fileNameCursor++)
            if(*fileNameCursor == '\\' || *fileNameCursor == '/')
                file = fileNameCursor + 1;
        while(timeProbeStackDepth && (!timeProbe->parent || (timeProbe->parent && timeProbeStack[timeProbeStackDepth - 1] != timeProbe->parent)))
            timeProbeStackDepth--;
        const i$ durationBufferSize = 
            korl_memory_stringFormatBuffer(bufferDuration, bufferDurationSize*sizeof(*bufferDuration), L"%*ws%02llu:%02hhu'%03hu\"%03hu%*ws", 
                                           2*timeProbeStackDepth, L"", 
                                           timeDiffMinutes, timeDiffSeconds, timeDiffMilliseconds, timeDiffMicroseconds, 
                                           2*(deepestProbeDepth - timeProbeStackDepth), L"");
        korl_assert(durationBufferSize > 0);
        /* now we can look at the contents of timeProbeDirectChildren for each 
            of the handles in the stack to see if we need to print a tabbing 
            character for that level of the stack; then we can decrement our 
            parent's direct children count to keep later probes updated */
        for(u$ s = 0; s < timeProbeStackDepth; s++)
        {
            if(timeProbeDirectChildren[timeProbeStack[s] - 1] == 1)
                if(s == timeProbeStackDepth - 1)
                    bufferDuration[s*2] = L'└';
                else
                    bufferDuration[s*2] = L'│';
            else if(timeProbeDirectChildren[timeProbeStack[s] - 1] > 0)
                if(s == timeProbeStackDepth - 1)
                    bufferDuration[s*2] = L'├';
                else
                    bufferDuration[s*2] = L'│';
        }
        if(timeProbe->parent)
            timeProbeDirectChildren[timeProbe->parent - 1]--;
        korl_log(INFO, "║ %ws [%-*ws] %-*ws; %ws:%i",
                 bufferDuration, 
                 longestProbeLabel, timeProbe->label ? timeProbe->label : L"",
                 longestProbeFunction, timeProbe->function, file, timeProbe->line);
        timeProbeStack[timeProbeStackDepth++] = tpi + 1;
    }
    korl_free(_korl_time_context.allocatorHandle, bufferDuration);
    korl_free(_korl_time_context.allocatorHandle, timeProbeDirectChildren);
    const PlatformTimeStamp timeStampEnd = korl_timeStamp();
    _korl_time_timeStampExtractDifference(timeStampStart, timeStampEnd, 
                                          &timeDiffMinutes, &timeDiffSeconds, &timeDiffMilliseconds, &timeDiffMicroseconds);
    korl_log(INFO, "╚════ END of time probe report!  report time taken: %02llu:%02hhu'%03hu\"%03hu ═════════╝",
             timeDiffMinutes, timeDiffSeconds, timeDiffMilliseconds, timeDiffMicroseconds);
}
korl_internal void korl_time_probeReset(void)
{
    _korl_time_context.timeProbesCount     = 0;
    _korl_time_context.timeProbeStackDepth = 0;
}
