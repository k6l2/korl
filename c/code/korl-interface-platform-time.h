#pragma once
#include "korl-globalDefines.h"
/** Do not use this value directly, since the meaning of this data is 
 * platform-dependent.  Instead, use the platform APIs which uses this type.  
 * This value should represent the highest possible time resolution 
 * (microseconds) of the platform, but it should only be valid for the current 
 * session of the underlying hardware.  (Do NOT save this value to disk and then 
 * expect it to be correct when you load it from disk on another 
 * session/machine!) */
typedef u64 PlatformTimeStamp;
#define KORL_PLATFORM_GET_TIMESTAMP(name) PlatformTimeStamp name(void)
#define KORL_PLATFORM_SECONDS_SINCE_TIMESTAMP(name) f32 name(PlatformTimeStamp pts)
/** Do not use this value directly, since the meaning of this data is 
 * platform-dependent.  Instead, use the platform APIs which uses this type.  
 * This value should be considered lower resolution (milliseconds) than 
 * PlatformTimeStamp, but is synchronized to a machine-independent time 
 * reference (UTC), so it should remain valid between application runs. */
typedef u64 KorlPlatformDateStamp;
