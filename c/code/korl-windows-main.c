/** Some resources that I'm using for building an application without 
 * Microsoft's bloated CRT: 
 * https://hero.handmade.network/forums/code-discussion/t/94-guide_-_how_to_avoid_c_c++_runtime_on_windows
 * https://www.codeproject.com/articles/15156/tiny-c-runtime-library */
#include <Windows.h>
#include <stdint.h>
/** We need this symbol to be able to build applications that use float 
 * variables... :| */
int _fltused;
/** MSVC program entry point must use the __stdcall calling convension. */
void __stdcall korl_windows_main()
{
    ExitProcess(0);
}
