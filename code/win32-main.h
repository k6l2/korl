#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
/* prevent windows from including winsock which causes conflict in win32-network 
	Source: https://stackoverflow.com/a/1372836 */
#ifndef _WINSOCKAPI_
	#define _WINSOCKAPI_
#endif
/*#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif*/
#ifndef NOMINMAX
	#define NOMINMAX
#endif
#include <windows.h>
global_variable HWND g_mainWindow;
global_variable TCHAR g_pathTemp[MAX_PATH];
global_variable TCHAR g_pathLocalAppData[MAX_PATH];
/* Allow the pre-processor to store compiler definitions as string literals
	Source: https://stackoverflow.com/a/39108392 */
#define _DEFINE_TO_CSTR(define) #define
#define DEFINE_TO_CSTR(d) _DEFINE_TO_CSTR(d)
global_variable const TCHAR APPLICATION_VERSION[] =	 
	TEXT(DEFINE_TO_CSTR(KORL_APP_VERSION));