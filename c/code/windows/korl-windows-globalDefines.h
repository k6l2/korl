#pragma once
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
    #ifdef near
        #undef near // fuck you too, Microsoft!
    #endif
    #ifdef far
        #undef far // fuck you too, Microsoft!
    #endif
#pragma warning(pop)
#include <windowsx.h>/* for GET_X_LPARAM, GET_Y_LPARAM, etc... */
#include <tchar.h>/* for _T macro */
#pragma warning(push)
    /* warning C5045: "Compiler will insert Spectre mitigation for memory load 
        if /Qspectre switch specified" I don't care about microshaft's shitty 
        API requiring spectre mitigation, only my own to some extent. */
    #pragma warning(disable : 5045)
    //KORL-ISSUE-000-000-033: remove this header, since this API sucks
    #include <strsafe.h>/* for StringCchVPrintf */
#pragma warning(pop)
/* KORL application exit codes */
#define KORL_EXIT_SUCCESS 0
#define KORL_EXIT_FAIL_ASSERT 1
