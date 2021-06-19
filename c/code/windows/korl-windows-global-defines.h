#pragma once
#ifndef NOMINMAX
    #define NOMINMAX
#endif// ndef NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif// ndef WIN32_LEAN_AND_MEAN
#pragma warning(push)
    /* warning C4255: "no function prototype given: converting '()' to '(void)'" 
        It seems Windows.h doesn't conform well to C, surprise surprise! */
    #pragma warning(disable : 4255)
    #include <Windows.h>
#pragma warning(pop)
/* for _T macro */
#include <tchar.h>
#pragma warning(push)
    /* warning C5045: "Compiler will insert Spectre mitigation for memory load 
        if /Qspectre switch specified" I don't care about microshaft's shitty 
        API requiring spectre mitigation, only my own to some extent. */
    #pragma warning(disable: 5045)
    /* for StringCchVPrintf */
    #include <strsafe.h>
#pragma warning(pop)
/* KORL application exit codes */
#define KORL_EXIT_SUCCESS 0
#define KORL_EXIT_FAIL_ASSERT 1
