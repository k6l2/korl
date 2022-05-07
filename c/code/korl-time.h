#pragma once
#include "korl-interface-platform.h"
typedef u32 Korl_Time_ProbeHandle;
/** \param label _optional_; if no label is desired, pass NULL */
#define korl_time_probe(label, x) \
    {\
        Korl_Time_ProbeHandle timeProbeHande = korl_time_probeBegin(__FILEW__, __FUNCTIONW__, __LINE__, L""label);\
        x\
        korl_time_probeEnd(timeProbeHande);\
    }
/** \param label _optional_; if no label is desired, pass NULL */
#define korl_timeProbeBegin(label) korl_time_probeBegin(__FILEW__, __FUNCTIONW__, __LINE__, L""label)
korl_internal void korl_time_initialize(void);
korl_internal KORL_PLATFORM_GET_TIMESTAMP(korl_timeStamp);
korl_internal KORL_PLATFORM_SECONDS_SINCE_TIMESTAMP(korl_time_secondsSinceTimeStamp);
#if 0
/** order of arguments does not matter, the result is always >= 0.f */
korl_internal f32 korl_time_secondsBetweenTimeStamps(PlatformTimeStamp ptsA, PlatformTimeStamp ptsB);
#endif
/** You probably want to use \c korl_timeProbeBegin instead of this function! */
korl_internal Korl_Time_ProbeHandle korl_time_probeBegin(const wchar_t* file, const wchar_t* function, int line, const wchar_t* label);
korl_internal void korl_time_probeEnd(Korl_Time_ProbeHandle timeProbeHandle);
korl_internal void korl_time_probeLogReport(void);
korl_internal void korl_time_probeReset(void);
