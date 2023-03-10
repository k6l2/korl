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
#include <initguid.h>/* needed in order to use the DEFINE_GUID macro; supposedly, you're supposed to include this _before_ including headers that use GUIDs defined by things like COM APIs, but I've actually found that if you move this include further down, it doesn't cause any build errors or anything...  I'll leave it above those headers just in case though. ¯\_(ツ)_/¯ */
#include <windowsx.h>/* for GET_X_LPARAM, GET_Y_LPARAM, etc... */
#include <tchar.h>/* for _T macro */
#include <shellapi.h>/* for CommandLineToArgv */
#include <mmdeviceapi.h>/* apparently this _must_ be included _before_ ShlGuid.h, or we get a bunch of build errors lol; for device enumeration support for WASAPI */
#include <ShlGuid.h>/* for FOLDERID_LocalAppData; normally ShlObj_core.h would include this at some point automatically, but it just doesn't if INITGUID is defined, soooo..... ¯\_(ツ)_/¯ */
#include <ShlObj_core.h>/* for SHGetKnownFolderPath; NOTE: this shitty API uses `near` & `far` keyword defines, which come from Windows.h */
#include <timeapi.h>/* for timeBeginPeriod, timeGetDevCaps, etc... */
#include <mmsyscom.h>/* for MMSYSERR_NOERROR definition */
#include <SetupAPI.h>/* for gamepad module (SetupDiGetClassDevs, etc...) */
#include <Dbt.h>/* for gamepad module (DEV_BROADCAST_DEVICEINTERFACE_*, etc...) */
#include <devioctl.h>/* needed in order to use the CTL_CODE macro */
#include <WinSock2.h>/* for bluetooth module */
#include <ws2bth.h>/* for bluetooth module; specifically the SOCKADDR_BTH struct */
#include <Audioclient.h>/* for WASAPI; this needs to be here since it _requires_ the near/far macros*/
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
/* Convenience macros for Windows API */
/** \return \c true if operation \c x succeeds */
#define KORL_WINDOWS_CHECK(x) (!(x) ? (korl_logLastError(#x" failed"), false) : true)
#define KORL_WINDOWS_CHECK_HRESULT(x) \
    do\
    {\
        const HRESULT _korl_windows_check_hResult = (x);\
        if(FAILED(_korl_windows_check_hResult))\
            korl_log(ERROR, #x" failed; HRESULT==0x%X", _korl_windows_check_hResult);\
    } while(0);
#define KORL_WINDOWS_COM(pUnknown, api, ...) (pUnknown)->lpVtbl->api(pUnknown, ##__VA_ARGS__)
#define KORL_WINDOWS_CHECKED_COM(pUnknown, api, ...) KORL_WINDOWS_CHECK_HRESULT(KORL_WINDOWS_COM(pUnknown, api, ##__VA_ARGS__))
#define KORL_WINDOWS_COM_SAFE_RELEASE(pUnknown) \
    if(pUnknown)\
    {\
        KORL_WINDOWS_COM(pUnknown, Release);\
        (pUnknown) = NULL;\
    }
#define KORL_WINDOWS_COM_SAFE_TASKMEM_FREE(address) \
    if(address)\
    {\
        CoTaskMemFree(address);\
        (address) = NULL;\
    }
/* Windows application-wide constants */
korl_global_const TCHAR KORL_APPLICATION_NAME[]         = L""KORL_DEFINE_TO_CSTR(KORL_APP_NAME);
korl_global_const TCHAR KORL_APPLICATION_VERSION[]      = L""KORL_DEFINE_TO_CSTR(KORL_APP_VERSION);
korl_global_const TCHAR KORL_DYNAMIC_APPLICATION_NAME[] = L""KORL_DEFINE_TO_CSTR(KORL_DLL_NAME);
