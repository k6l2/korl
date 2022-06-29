#pragma once
#include "korl-interface-platform.h"
typedef enum Korl_Time_DateStamp_Compare_Result
    { KORL_TIME_DATESTAMP_COMPARE_RESULT_FIRST_TIME_EARLIER
    , KORL_TIME_DATESTAMP_COMPARE_RESULT_EQUAL
    , KORL_TIME_DATESTAMP_COMPARE_RESULT_FIRST_TIME_LATER
} Korl_Time_DateStamp_Compare_Result;
/** Opaque datatype for for highest possible integral measurement of time.  
 * Likely _not_ a standard measurement of time, but an internal representation 
 * of some platform-specific concept of "cycles", so don't try to use this value 
 * directly, and instead use API that consumes this data to produce useful 
 * behavior/information. */
typedef i$ Korl_Time_Counts;
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
korl_internal void korl_time_shutDown(void);
korl_internal KORL_PLATFORM_GET_TIMESTAMP(korl_timeStamp);
korl_internal KORL_PLATFORM_SECONDS_SINCE_TIMESTAMP(korl_time_secondsSinceTimeStamp);
korl_internal Korl_Time_Counts korl_time_countsFromHz(u16 hz);
korl_internal Korl_Time_Counts korl_time_timeStampCountDifference(PlatformTimeStamp ptsA, PlatformTimeStamp ptsB);
korl_internal void korl_time_sleep(Korl_Time_Counts counts);
/** \return the # of characters written to \c buffer .  If \c bufferBytes is not 
 * large enough, \c buffer is not modified and the value returned is the number 
 * of characters that _would_ have been written, including the null terminator, 
 * multiplied by -1 */
korl_internal i$ korl_time_countsFormatBuffer(Korl_Time_Counts counts, wchar_t* buffer, u$ bufferBytes);
#if 0// not yet sure whether I want this API or not, so I'll leave it commented out for now...
/** order of arguments does not matter, the result is always >= 0.f */
korl_internal f32 korl_time_secondsBetweenTimeStamps(PlatformTimeStamp ptsA, PlatformTimeStamp ptsB);
#endif
korl_internal Korl_Time_DateStamp_Compare_Result korl_time_dateStampCompare(KorlPlatformDateStamp dsA, KorlPlatformDateStamp dsB);
/** You probably want to use \c korl_time_probeStart instead of this function! */
korl_internal Korl_Time_ProbeHandle korl_time_probeBegin(const wchar_t* file, const wchar_t* function, int line, const wchar_t* label);
/** You probably want to use \c korl_time_probeStop instead of this function! 
 * \return the platform-specific integral representation of the time elapsed 
 * between the calls to \c probeBegin and \c probeEnd. */
korl_internal Korl_Time_Counts korl_time_probeEnd(Korl_Time_ProbeHandle timeProbeHandle);
korl_internal void korl_time_probeLogReport(void);
korl_internal void korl_time_probeReset(void);
