/** Some resources that I'm using for building an application without 
 * Microsoft's bloated CRT : 
 * https://hero.handmade.network/forums/code-discussion/t/94-guide_-_how_to_avoid_c_c++_runtime_on_windows
 * https://www.codeproject.com/articles/15156/tiny-c-runtime-library */
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
/** disambiguations of the \c static key word to improve project 
 * searchability */
#define korl_internal static
/** calculate the size of an array 
 * (NOTE: does NOT work for dynamic array sizes!) */
#define korl_arraySize(array) (sizeof(array) / sizeof(array[0]))
#define korl_assert(condition) \
    if(!(condition)) { korl_assertConditionFailed(L#condition); }
/** derived from https://stackoverflow.com/a/36015150 */
#if 1
#define KORL_GET_ARG_COUNT(...) \
    KORL_INTERNAL_EXPAND_ARGS_PRIVATE(KORL_INTERNAL_ARGS_AUGMENTER(__VA_ARGS__))
#define KORL_INTERNAL_ARGS_AUGMENTER(...) unused, __VA_ARGS__
#define KORL_INTERNAL_EXPAND(x) x
#define KORL_INTERNAL_EXPAND_ARGS_PRIVATE(...) \
    KORL_INTERNAL_EXPAND(KORL_INTERNAL_GET_ARG_COUNT_PRIVATE(__VA_ARGS__, \
    69, 68, 67, 66, 65, \
    64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, \
    48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, \
    32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
    16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1, 0))
#define KORL_INTERNAL_GET_ARG_COUNT_PRIVATE(\
    _1_, _2_, _3_, _4_, _5_, _6_, _7_, _8_, _9_, _10, \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
    _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
    _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
    _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
    _61, _62, _63, _64, _65, _66, _67, _68, _69, _70, count, ...) count
#else /* this technique does not work w/ MSVC for 0 arguments :( */
#define KORL_GET_ARG_COUNT(...) \
    KORL_INTERNAL_GET_ARG_COUNT_PRIVATE(0, ## __VA_ARGS__, \
        70, 69, 68, 67, 66, 65, \
        64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, \
        48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, \
        32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
        16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1, 0)
#define KORL_INTERNAL_GET_ARG_COUNT_PRIVATE(_0_, \
    _1_, _2_, _3_, _4_, _5_, _6_, _7_, _8_, _9_, _10, \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
    _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
    _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
    _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
    _61, _62, _63, _64, _65, _66, _67, _68, _69, _70, count, ...) count
#endif//0
korl_internal void korl_assertConditionFailed(wchar_t* conditionString)
{
    if(IsDebuggerPresent())
    {
        OutputDebugString(_T("KORL ASSERTION FAILED: \""));
        OutputDebugString(conditionString);
        OutputDebugString(_T("\"\n"));
        DebugBreak();
    }
    /** @todo: maybe do one or more of the following things:
     * - give the user a prompt indicating something went wrong
     * - write a memory dump to disk
     * - write failed condition to standard error stream
     * - end the program right now */
}
korl_internal DWORD korl_windows_cast_sizetToDword(size_t value)
{
    _STATIC_ASSERT(sizeof(DWORD) == 4);
    korl_assert(value <= 0xFFFFFFFF);
    return (DWORD)value;
}
enum KorlEnumStandardStream
    { KORL_STANDARD_STREAM_OUT
    , KORL_STANDARD_STREAM_ERROR
    , KORL_STANDARD_STREAM_IN };
/** \return \c NULL if the specified stream has not been redirected.  
 * \c INVALID_HANDLE_VALUE if an invalid value was passed to 
 * \c KorlEnumStandardStream */
korl_internal HANDLE korl_windows_handleFromEnumStandardStream(
    int KorlEnumStandardStream)
{
    HANDLE result = INVALID_HANDLE_VALUE;
    switch(KorlEnumStandardStream)
    {
    case KORL_STANDARD_STREAM_OUT: {
        result = GetStdHandle(STD_OUTPUT_HANDLE);
        } break;
    case KORL_STANDARD_STREAM_ERROR: {
        result = GetStdHandle(STD_ERROR_HANDLE);
        } break;
    case KORL_STANDARD_STREAM_IN: {
        result = GetStdHandle(STD_INPUT_HANDLE);
        } break;
    }
    korl_assert(result != INVALID_HANDLE_VALUE);
    /** @todo: do something with lastError? */
#if 0
    if(result == INVALID_HANDLE_VALUE)
    {
        const DWORD lastError = GetLastError();
    }
#endif//0
    return result;
}
korl_internal void korl_printVariableArgumentList(
    int KorlEnumStandardStream, wchar_t* format, va_list vaList)
{
    /* attempt to write the formatted string to a buffer on the stack */
    wchar_t stackStringBuffer[512];
    wchar_t* finalBuffer = stackStringBuffer;
    size_t finalBufferMaxSize = korl_arraySize(stackStringBuffer);
    korl_assert(finalBufferMaxSize <= STRSAFE_MAX_CCH);
    const HRESULT hResultPrintToStackBuffer = 
        StringCchVPrintf(stackStringBuffer, finalBufferMaxSize, format, vaList);
    korl_assert(hResultPrintToStackBuffer != STRSAFE_E_INVALID_PARAMETER);
    if(hResultPrintToStackBuffer == S_OK)
        goto WRITE_FINAL_BUFFER;
    /* if that fails, allocate a dynamic buffer twice that size */
    korl_assert(!"TODO: also pass in an allocator callback");
    /* attempt to write the formatted string to the dynamic buffer */
    korl_assert(!"TODO");
    /* if that fails, double the size of the buffer & repeat until we 
        succeed */
    korl_assert(!"TODO");
    /* write the final buffer to the desired KorlEnumStandardStream */
    WRITE_FINAL_BUFFER:
    const HANDLE hStream = 
        korl_windows_handleFromEnumStandardStream(KorlEnumStandardStream);
    korl_assert(hStream != NULL);
    korl_assert(hStream != INVALID_HANDLE_VALUE);
    korl_assert(finalBufferMaxSize <= STRSAFE_MAX_CCH);
    size_t finalBufferSize = 0;
    const HRESULT resultGetFinalBufferSize = 
        StringCchLength(finalBuffer, finalBufferMaxSize, &finalBufferSize);
    korl_assert(resultGetFinalBufferSize == S_OK);
    // check whether or not hStream is a "console handle" //
    DWORD consoleMode = 0;
    const BOOL successGetConsoleMode = GetConsoleMode(hStream, &consoleMode);
    /** @hack: this shouldn't be here; we're lying to the user and not actually 
     * printing to the file stream they asked for just for the sake of 
     * displaying things correctly in visual studio debug output window! */
    if(IsDebuggerPresent())
    {
        OutputDebugString(finalBuffer);
    }
    else if(successGetConsoleMode)
    {
        DWORD numCharsWritten = 0;
        const BOOL successWriteConsole = 
            WriteConsole(
                hStream, finalBuffer, 
                korl_windows_cast_sizetToDword(finalBufferSize), 
                &numCharsWritten, NULL/*reserved; always NULL*/);
        /** @todo: do something with lastError? */
#if 0
        if(!successWriteConsole)
        {
            const DWORD lastError = GetLastError();
        }
#endif//0
        korl_assert(successWriteConsole);
    }
    else
    {
        DWORD bytesWritten = 0;
        const BOOL successWriteFile = 
            WriteFile(
                hStream, finalBuffer, 
                korl_windows_cast_sizetToDword(
                    finalBufferSize*sizeof(finalBuffer[0])), 
                &bytesWritten, 
                NULL/* OVERLAPPED*: NULL == we're not doing async I/O here */);
        /** @todo: do something with lastError? */
#if 0
        if(!resultWriteFile)
        {
            const DWORD lastError = GetLastError();
        }
#endif//0
        korl_assert(successWriteFile);
    }
    /* free dynamic buffer if we had to allocate one */
    if(finalBuffer != stackStringBuffer)
    {
        korl_assert(!"TODO");
    }
}
/** this macro definition of the \c korl_print function allows us to 
 * automatically pass in the number of arguments passed to the print function */
#define korl_print(...) \
    korl_print(KORL_GET_ARG_COUNT(__VA_ARGS__) - 2, __VA_ARGS__)
/** in order to use the above macro to automatically determine the number of 
 * arguments, we need to wrap this function identifier in parenthesis
 * @note MAKE SURE to not call this directly!  Use the macro version instead! */
korl_internal void (korl_print)(unsigned variadicArgumentCount, 
    int KorlEnumStandardStream, wchar_t* format, ...)
{
    /* find the number of variable substitutions in the format string */
    unsigned formatSubstitutions = 0;
    for(wchar_t* f = format; *f; f++)
    {
        if(*f != '%')
            continue;
        const wchar_t nextChar = *(f+1);
        /* for now, let's just make it illegal to have a trailing '%' at the end 
            of the format string */
        korl_assert(nextChar);
        if(nextChar == '%')
            /* f is currently pointing to an escape character sequence 
                for the '%' character, so we should skip both 
                characters */
            f++;
        else
            /* if the current character is a '%', and there is a next 
                character, and the next character is NOT a '%', that 
                means this must be a variable substitution */
            formatSubstitutions++;
    }
    /* check to see if this number doesn't match variadicArgumentCount */
    korl_assert(variadicArgumentCount == formatSubstitutions);
    /* print the formatted text to the desired standard handle */
    va_list vaList;
    va_start(vaList, format);
    korl_printVariableArgumentList(KorlEnumStandardStream, format, vaList);
    va_end(vaList);
}
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    /*const BOOL successAttachConsole = */AttachConsole(ATTACH_PARENT_PROCESS);
#if 0
    if(!successAttachConsole)
    {
        //const BOOL successFreeConsole = FreeConsole();
        //korl_assert(successFreeConsole);
        const BOOL successAllocConsole = AllocConsole();
        korl_assert(successAllocConsole);
        /* after allocating a new console, we have to re-open the I/O streams!
            source: https://stackoverflow.com/a/57241985 */
        FILE *fileDummy;
        const errno_t resultReOpenStdIn = 
            freopen_s(&fileDummy, "CONIN$", "r", stdin);
        const errno_t resultReOpenStdErr = 
            freopen_s(&fileDummy, "CONOUT$", "w", stderr);
        const errno_t resultReOpenStdOut = 
            freopen_s(&fileDummy, "CONOUT$", "w", stdout);
        korl_assert(resultReOpenStdIn == 0);
        korl_assert(resultReOpenStdErr == 0);
        korl_assert(resultReOpenStdOut == 0);
        const HANDLE hConOut = 
            CreateFile(_T("CONOUT$"), GENERIC_READ | GENERIC_WRITE, 
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
                FILE_ATTRIBUTE_NORMAL, NULL);
        const HANDLE hConIn = 
            CreateFile(_T("CONIN$"), GENERIC_READ | GENERIC_WRITE, 
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
                FILE_ATTRIBUTE_NORMAL, NULL);
        korl_assert(SetStdHandle(STD_OUTPUT_HANDLE, hConOut));
        korl_assert(SetStdHandle(STD_ERROR_HANDLE, hConOut));
        korl_assert(SetStdHandle(STD_INPUT_HANDLE, hConIn));
    }
#endif//0
    SYSTEM_INFO systemInfo;
    ZeroMemory(&systemInfo, sizeof(systemInfo));
    GetSystemInfo(&systemInfo);
    korl_print(KORL_STANDARD_STREAM_OUT, L"dwPageSize=%lu PI=%f\n", 
        systemInfo.dwPageSize, 3.14159f);
#if 0
    if(!IsDebuggerPresent())
    {
        korl_print(KORL_STANDARD_STREAM_OUT, L"Press [Enter] to exit:\n");
        getchar();
    }
#endif//0
    ExitProcess(0);
}
