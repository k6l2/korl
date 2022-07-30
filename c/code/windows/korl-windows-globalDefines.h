#pragma once
#include "korl-globalDefines.h"
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#pragma warning(push)
    /* warning C4255: "no function prototype given: converting '()' to '(void)'" 
        It seems Windows.h doesn't conform well to C, surprise surprise! */
    #pragma warning(disable : 4255)
    #include <Windows.h>
#pragma warning(pop)
#include <windowsx.h>/* for GET_X_LPARAM, GET_Y_LPARAM, etc... */
#include <tchar.h>/* for _T macro */
#include <shellapi.h>/* for CommandLineToArgv */
#include <ShlObj_core.h>/* for SHGetKnownFolderPath; NOTE: this shitty API uses `near` & `far` keyword defines, which come from Windows.h */
#include <timeapi.h>/* for timeBeginPeriod, timeGetDevCaps, etc... */
#include <mmsyscom.h>/* for MMSYSERR_NOERROR definition */
#include <SetupAPI.h>/* for gamepad module (SetupDiGetClassDevs, etc...) */
#include <Dbt.h>/* for gamepad module (DEV_BROADCAST_DEVICEINTERFACE_*, etc...) */
#ifdef near// defined from somewhere inside Windows.h
    #undef near // fuck you too, Microsoft!
#endif
#ifdef far// defined from somewhere inside Windows.h
    #undef far // fuck you too, Microsoft!
#endif
/* KORL application exit codes */
#define KORL_EXIT_SUCCESS 0
#define KORL_EXIT_FAIL_ASSERT 1
#define KORL_EXIT_FAIL_EXCEPTION 0xDEADC0DE
/* Windows application-wide constants */
korl_global_const TCHAR KORL_APPLICATION_NAME[]         = L""KORL_DEFINE_TO_CSTR(KORL_APP_NAME);
korl_global_const TCHAR KORL_APPLICATION_VERSION[]      = L""KORL_DEFINE_TO_CSTR(KORL_APP_VERSION);
korl_global_const TCHAR KORL_DYNAMIC_APPLICATION_NAME[] = L""KORL_DEFINE_TO_CSTR(KORL_DLL_NAME);
