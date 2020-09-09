#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
/* prevent windows from including winsock which causes conflict in win32-network 
	Source: https://stackoverflow.com/a/1372836 */
#define _WINSOCKAPI_
#include <windows.h>
struct GameCode
{
	fnSig_gameInitialize* initialize;
	fnSig_gameOnReloadCode* onReloadCode;
	fnSig_gameRenderAudio* renderAudio;
	fnSig_gameUpdateAndDraw* updateAndDraw;
	fnSig_gameOnPreUnload* onPreUnload;
	HMODULE hLib;
	FILETIME dllLastWriteTime;
	bool isValid;
};
internal LARGE_INTEGER w32QueryPerformanceCounter();
internal FILETIME w32GetLastWriteTime(const char* fileName);
global_variable HWND g_mainWindow;