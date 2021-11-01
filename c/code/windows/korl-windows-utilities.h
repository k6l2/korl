#pragma once
#include "korl-globalDefines.h"
#include "korl-windows-globalDefines.h"
#define korl_logLastError(cStringMessage, ...) \
    korl_logVariadicArguments(\
        1 + KORL_GET_ARG_COUNT(__VA_ARGS__), KORL_LOG_LEVEL_ERROR, \
        __FILEW__, __FUNCTIONW__, __LINE__, \
        cStringMessage L"  GetLastError=0x%x", ##__VA_ARGS__, GetLastError())
korl_internal DWORD korl_windows_sizet_to_dword(size_t value);
korl_internal i32 korl_windows_dword_to_i32(DWORD value);
