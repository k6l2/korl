#include "win32-dynApp.h"
#include "win32-time.h"
internal GAME_INITIALIZE(gameInitializeStub)
{
}
internal GAME_ON_RELOAD_CODE(gameOnReloadCodeStub)
{
}
internal GAME_RENDER_AUDIO(gameRenderAudioStub)
{
}
internal GAME_UPDATE_AND_DRAW(gameUpdateAndDrawStub)
{
	// continue running the application to keep attempting to reload game code!
	return true;
}
internal GAME_ON_PRE_UNLOAD(gameOnPreUnloadStub)
{
}
internal Korl_Win32_DynamicApplicationModule 
	korl_w32_dynApp_load(const char* fileNameDll, const char* fileNameDllTemp)
{
	Korl_Win32_DynamicApplicationModule result = {};
	result.initialize       = gameInitializeStub;
	result.onReloadCode     = gameOnReloadCodeStub;
	result.renderAudio      = gameRenderAudioStub;
	result.updateAndDraw    = gameUpdateAndDrawStub;
	result.onPreUnload      = gameOnPreUnloadStub;
	result.dllLastWriteTime = w32GetLastWriteTime(fileNameDll);
	if(result.dllLastWriteTime.dwHighDateTime == 0 &&
		result.dllLastWriteTime.dwLowDateTime == 0)
	{
		return result;
	}
	if(!CopyFileA(fileNameDll, fileNameDllTemp, false))
	{
		if(GetLastError() == ERROR_SHARING_VIOLATION)
		{
			/* if the file is being used by another program, just silently do 
				nothing since it's very likely the other program is cl.exe */
		}
		else
			KLOG(WARNING, "Failed to copy file '%s' to '%s'! GetLastError=%i", 
			     fileNameDll, fileNameDllTemp, GetLastError());
	}
	else
	{
		result.hLib = LoadLibraryA(fileNameDllTemp);
		if(!result.hLib)
		{
			KLOG(ERROR, "Failed to load library '%s'! GetLastError=%i", 
			     fileNameDllTemp, GetLastError());
		}
	}
	if(result.hLib)
	{
		result.initialize = reinterpret_cast<fnSig_gameInitialize*>(
			GetProcAddress(result.hLib, "gameInitialize"));
		if(!result.initialize)
		{
			KLOG(ERROR, "Failed to get proc address 'gameInitialize'! "
			     "GetLastError=%i", GetLastError());
		}
		result.onReloadCode = reinterpret_cast<fnSig_gameOnReloadCode*>(
			GetProcAddress(result.hLib, "gameOnReloadCode"));
		if(!result.onReloadCode)
		{
			// This is only a warning because the game can optionally just not
			//	implement the hot-reload callback. //
			///TODO: detect if `GameCode` has the ability to hot-reload and 
			///      don't perform hot-reloading if this is not the case.
			KLOG(WARNING, "Failed to get proc address 'gameOnReloadCode'! "
			     "GetLastError=%i", GetLastError());
		}
		result.onPreUnload = reinterpret_cast<fnSig_gameOnPreUnload*>(
			GetProcAddress(result.hLib, "gameOnPreUnload"));
		if(!result.onPreUnload)
		{
			// This is only a warning because the game can optionally just not
			//	implement the hot-reload callback. //
			///TODO: detect if `GameCode` has the ability to hot-reload and 
			///      don't perform hot-reloading if this is not the case.
			KLOG(WARNING, "Failed to get proc address 'gameOnPreUnload'! "
			     "GetLastError=%i", GetLastError());
		}
		result.renderAudio = reinterpret_cast<fnSig_gameRenderAudio*>(
			GetProcAddress(result.hLib, "gameRenderAudio"));
		if(!result.renderAudio)
		{
			KLOG(ERROR, "Failed to get proc address 'gameRenderAudio'! "
			     "GetLastError=%i", GetLastError());
		}
		result.updateAndDraw = reinterpret_cast<fnSig_gameUpdateAndDraw*>(
			GetProcAddress(result.hLib, "gameUpdateAndDraw"));
		if(!result.updateAndDraw)
		{
			KLOG(ERROR, "Failed to get proc address 'gameUpdateAndDraw'! "
			     "GetLastError=%i", GetLastError());
		}
		result.isValid = (result.initialize && result.renderAudio && 
		                  result.updateAndDraw);
	}
	if(!result.isValid)
	{
		result.initialize    = gameInitializeStub;
		result.onReloadCode  = gameOnReloadCodeStub;
		result.renderAudio   = gameRenderAudioStub;
		result.updateAndDraw = gameUpdateAndDrawStub;
		result.onPreUnload   = gameOnPreUnloadStub;
	}
	return result;
}
internal void 
	korl_w32_dynApp_unload(Korl_Win32_DynamicApplicationModule*const dam)
{
	if(dam->hLib)
	{
		if(!FreeLibrary(dam->hLib))
		{
			KLOG(ERROR, "Failed to free game code dll! GetLastError=", 
			     GetLastError());
		}
		dam->hLib = NULL;
	}
	dam->isValid       = false;
	dam->renderAudio   = gameRenderAudioStub;
	dam->updateAndDraw = gameUpdateAndDrawStub;
}