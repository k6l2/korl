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
/* KORL application exit codes */
#define KORL_EXIT_SUCCESS 0
#define KORL_EXIT_FAIL_ASSERT 1
