#include "korl-audio.h"
#include "korl-windows-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_audio_initialize(void)
{
    /* we're using WinMM/WASAPI; we need to make sure COM is initialized */
    const HRESULT hResultCoInitialize = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY);
    switch(hResultCoInitialize)
    {
    case S_OK:{
        korl_log(INFO, "COM initialized; may the gods help us all...");
        break;}
    case S_FALSE:{
        /* COM already initialized; this isn't really an error */
        korl_log(INFO, "COM already initialized; may the gods help us all...");
        break;}
    case RPC_E_CHANGED_MODE:{
        /* this also doesn't really indicate an error, but I guess it's worth logging this case... */
        korl_log(INFO, "CoInitializeEx: RPC_E_CHANGED_MODE; proceeding execution will assume this is okay");
        break;}
    default:{
        korl_log(ERROR, "CoInitializeEx failed; HRESULT==0x%X", hResultCoInitialize);
        break;}
    }
}
korl_internal void korl_audio_shutDown(void)
{
    CoUninitialize();// according to MSDN, this _must_ be called, even if the call to CoInitializeEx failed (due to COM already being initialized)
}
