#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_log_initialize(void);
/** Logs will not get saved to disk until this API is called!  We should not 
 * care about reading logs in real-time from a file (using the tail command for 
 * example).  Even though such a feature would be neat to have, it is completely 
 * pointless since we can just render log output in real time within the user 
 * application GUI itself.  In addition, real-time file io is slow & complex, so 
 * we should do our best to not rely on it for real-time interactive programs. */
korl_internal void korl_log_shutDown(void);
/** \note DO NOT CALL THIS!  Use \c korl_log instead!  *************************
 * This symbol is only exposed for the compiler! */
korl_internal KORL_PLATFORM_LOG(_korl_log_variadic);
