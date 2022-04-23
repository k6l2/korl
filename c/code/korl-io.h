#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
enum KorlEnumStandardStream
    { KORL_STANDARD_STREAM_OUT
    , KORL_STANDARD_STREAM_ERROR
    , KORL_STANDARD_STREAM_IN };
#define korl_print(korlEnumStandardStream, format, ...) \
    korl_printVariadicArguments(\
        KORL_GET_ARG_COUNT(__VA_ARGS__), korlEnumStandardStream, \
        format, __VA_ARGS__)
korl_internal void korl_io_initialize(void);
/** \note DO NOT CALL THIS!  Use \c korl_print instead! */
korl_internal void korl_printVariadicArguments(unsigned variadicArgumentCount, 
    enum KorlEnumStandardStream standardStream, const wchar_t* format, ...);
/** \note DO NOT CALL THIS!  Use \c korl_log instead! */
korl_internal KORL_PLATFORM_LOG(korl_logVariadicArguments);
