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
#define KORL_W32_VERBOSE_EVENT_LOG 0
global_variable HWND g_mainWindow;
global_variable TCHAR g_pathTemp[MAX_PATH];
global_variable TCHAR g_pathLocalAppData[MAX_PATH];
///TODO: handle file paths longer than MAX_PATH in the future...
global_variable TCHAR g_pathToExe[MAX_PATH];
global_variable HCURSOR g_cursorArrow;
global_variable HCURSOR g_cursorSizeVertical;
global_variable HCURSOR g_cursorSizeHorizontal;
global_variable HCURSOR g_cursorSizeNeSw;
global_variable HCURSOR g_cursorSizeNwSe;
global_variable HCURSOR g_cursorSizeAll;
global_variable HCURSOR g_cursorTextBeam;
global_variable HCURSOR g_cursorHand;
global_variable HCURSOR g_cursorNo;
/* Allow the pre-processor to store compiler definitions as string literals
	Source: https://stackoverflow.com/a/39108392 */
#define _DEFINE_TO_CSTR(define) #define
#define DEFINE_TO_CSTR(d) _DEFINE_TO_CSTR(d)
global_const TCHAR APPLICATION_NAME[] = TEXT(DEFINE_TO_CSTR(KORL_APP_NAME));
global_variable const TCHAR APPLICATION_VERSION[] =	 
	TEXT(DEFINE_TO_CSTR(KORL_APP_VERSION));
global_const TCHAR FILE_NAME_GAME_DLL[] = 
	TEXT(DEFINE_TO_CSTR(KORL_GAME_DLL_FILENAME));