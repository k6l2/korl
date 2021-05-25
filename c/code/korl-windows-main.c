/** Some resources that I'm using for building an application without 
 * Microsoft's bloated CRT: 
 * https://hero.handmade.network/forums/code-discussion/t/94-guide_-_how_to_avoid_c_c++_runtime_on_windows
 * https://www.codeproject.com/articles/15156/tiny-c-runtime-library */
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#pragma warning(push)
/* warning C4255: "no function prototype given: converting '()' to '(void)'" 
    It seems Windows.h doesn't conform well to C, surprise surprise! */
#pragma warning(disable : 4255)
#include <Windows.h>
#pragma warning(pop)
/** We need this symbol to be able to build applications that use float 
 * variables... :| */
void* _fltused;
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main(void)
{
    ExitProcess(0);
}
