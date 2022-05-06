#pragma once
#include "korl-interface-platform.h"
korl_internal void korl_time_initialize(void);
korl_internal KORL_PLATFORM_GET_TIMESTAMP(korl_timeStamp);
korl_internal KORL_PLATFORM_SECONDS_SINCE_TIMESTAMP(korl_time_secondsSinceTimeStamp);
