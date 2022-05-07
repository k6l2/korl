#pragma once
#include "korl-interface-platform.h"
typedef u32 Korl_Time_ProbeHandle;
/** Creates a const \c Korl_Time_ProbeHandle with an internal name derived from 
 * the provided \c label paremeter.  You want to pass the exact same \c label 
 * you provided here to the \c korl_time_probeStop macro. */
#define korl_time_probeStart(label) \
    const Korl_Time_ProbeHandle _korl_time_probeHandle_##label = \
        korl_time_probeBegin(__FILEW__, __FUNCTIONW__, __LINE__, L""#label)
/** convenience macro to use internally "name-mangled" time probe handle 
 * identifiers to end a time probe period */
#define korl_time_probeStop(label) \
    korl_time_probeEnd(_korl_time_probeHandle_##label)
korl_internal void korl_time_initialize(void);
korl_internal KORL_PLATFORM_GET_TIMESTAMP(korl_timeStamp);
korl_internal KORL_PLATFORM_SECONDS_SINCE_TIMESTAMP(korl_time_secondsSinceTimeStamp);
#if 0// not yet sure whether I want this API or not, so I'll leave it commented out for now...
/** order of arguments does not matter, the result is always >= 0.f */
korl_internal f32 korl_time_secondsBetweenTimeStamps(PlatformTimeStamp ptsA, PlatformTimeStamp ptsB);
#endif
/** You probably want to use \c korl_time_probeStart instead of this function! */
korl_internal Korl_Time_ProbeHandle korl_time_probeBegin(const wchar_t* file, const wchar_t* function, int line, const wchar_t* label);
/** You probably want to use \c korl_time_probeStop instead of this function! */
korl_internal void korl_time_probeEnd(Korl_Time_ProbeHandle timeProbeHandle);
korl_internal void korl_time_probeLogReport(void);
korl_internal void korl_time_probeReset(void);
