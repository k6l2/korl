#pragma once
#include "win32-main.h"
struct Korl_Win32_DynamicApplicationModule
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
internal Korl_Win32_DynamicApplicationModule 
	korl_w32_dynApp_load(const char* fileNameDll, const char* fileNameDllTemp);
internal void 
	korl_w32_dynApp_unload(Korl_Win32_DynamicApplicationModule*const dam);