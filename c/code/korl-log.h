#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_log_initialize(void);
korl_internal void korl_log_configure(bool useLogOutputDebugger, bool useLogOutputConsole, bool useLogFileBig, bool disableMetaTags);
/** \param logFileEnabled Required to tell the log system that for the rest of 
 * the application's runtime, we do or do not need to worry about log file I/O.  
 * Otherwise, in the case that all log outputs are disabled, we would be wasting 
 * tons of time generating log strings that will never be read!  In addition, 
 * the log system _must_ initially assume that we will use a log file at some 
 * point when the log module is initialized so that we can store logs that 
 * happen in between those two API calls until the log file is actually opened. */
korl_internal void korl_log_initiateFile(bool logFileEnabled);
korl_internal void korl_log_clearAsyncIo(void);
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
korl_internal KORL_PLATFORM_LOG_GET_BUFFER(korl_log_getBuffer);
