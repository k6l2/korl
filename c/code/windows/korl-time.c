#include "korl-time.h"
#include "korl-windows-globalDefines.h"
korl_global_variable LARGE_INTEGER _korl_time_perfCounterHz;
typedef union _Korl_Time_TimeStampUnion
{
    LARGE_INTEGER largeInt;
    PlatformTimeStamp timeStamp;
} _Korl_Time_TimeStampUnion;
_STATIC_ASSERT(sizeof(PlatformTimeStamp) <= sizeof(LARGE_INTEGER));
korl_internal void korl_time_initialize(void)
{
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