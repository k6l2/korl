#pragma once
#include "korl-globalDefines.h"
enum KorlEnumStandardStream
    { KORL_STANDARD_STREAM_OUT
    , KORL_STANDARD_STREAM_ERROR
    , KORL_STANDARD_STREAM_IN };
#define korl_print(korlEnumStandardStream, format, ...) \
    korl_printVariadicArguments(\
        KORL_GET_ARG_COUNT(__VA_ARGS__), korlEnumStandardStream, \
        format, __VA_ARGS__)
enum KorlEnumLogLevel
    { KORL_LOG_LEVEL_ERROR
    , KORL_LOG_LEVEL_WARNING
    , KORL_LOG_LEVEL_INFO
    , KORL_LOG_LEVEL_VERBOSE };
#define korl_log(logLevel, format, ...) \
    korl_logVariadicArguments(\
        KORL_GET_ARG_COUNT(__VA_ARGS__), KORL_LOG_LEVEL_##logLevel, \
        __FILEW__, __FUNCTIONW__, __LINE__, L ## format, ##__VA_ARGS__)
/** \note DO NOT CALL THIS!  Use \c korl_print instead! */
korl_internal void korl_printVariadicArguments(unsigned variadicArgumentCount, 
    enum KorlEnumStandardStream standardStream, wchar_t* format, ...);
/** \note DO NOT CALL THIS!  Use \c korl_log instead! */
korl_internal void korl_logVariadicArguments(
    unsigned variadicArgumentCount, enum KorlEnumLogLevel logLevel, 
    const wchar_t* cStringFileName, const wchar_t* cStringFunctionName, 
    int lineNumber, wchar_t* format, ...);