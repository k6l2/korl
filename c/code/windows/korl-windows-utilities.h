#pragma once
#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#define korl_logLastError(cStringMessage, ...) \
    korl_logVariadicArguments(\
        KORL_GET_ARG_COUNT(__VA_ARGS__), KORL_LOG_LEVEL_ERROR, \
        __FILEW__, __FUNCTIONW__, __LINE__, L"%S  GetLastError=0x%x", \
        cStringMessage, GetLastError())
#define korl_makeZeroStackVariable(type, name) \
    type name; ZeroMemory(&(name), sizeof(name));
korl_internal DWORD korl_windows_sizet_to_dword(size_t value);
korl_internal i32 korl_windows_dword_to_i32(DWORD value);
