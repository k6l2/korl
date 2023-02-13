#include "korl-audio.h"
#include "korl-windows-globalDefines.h"
#include "korl-interface-platform.h"
#include <Functiondiscoverykeys_devpkey.h>// for PKEY_Device_FriendlyName
/* Why are these Microsoft constants defined here?  Because as far as I can tell, 
    Microsoft straight up just doesn't support WASAPI in C anymore.  After doing 
    a bunch of searching, all I found was this stackoverflow question which 
    seems to indicate that this is the only solution: https://stackoverflow.com/q/43717147/4526664 */
const CLSID CLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};// values obtained from <mmdeviceapi.h>
const IID   IID_IMMDeviceEnumerator  = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};// values obtained from <mmdeviceapi.h>
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
    /* enumerate audio endpoint devices */
    IMMDeviceEnumerator* mmDeviceEnumerator = NULL;
    IMMDeviceCollection* mmDeviceCollection = NULL;
    KORL_WINDOWS_CHECK_HRESULT(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &mmDeviceEnumerator));
    KORL_WINDOWS_CHECKED_COM(mmDeviceEnumerator, EnumAudioEndpoints, eRender, DEVICE_STATE_ACTIVE, &mmDeviceCollection);
    UINT deviceCount = 0;
    KORL_WINDOWS_CHECKED_COM(mmDeviceCollection, GetCount, &deviceCount);
    korl_log(INFO, "found %u audio endpoint devices:", deviceCount);
    for(UINT d = 0; d < deviceCount; d++)
    {
        IMMDevice* mmDevice = NULL;
        KORL_WINDOWS_CHECKED_COM(mmDeviceCollection, Item, d, &mmDevice);
        LPWSTR pwszDeviceId = NULL;
        KORL_WINDOWS_CHECKED_COM(mmDevice, GetId, &pwszDeviceId);
        IPropertyStore* devicePropertyStore = NULL;
        KORL_WINDOWS_CHECKED_COM(mmDevice, OpenPropertyStore, STGM_READ, &devicePropertyStore);
        KORL_ZERO_STACK(PROPVARIANT, devicePropertyVariantName);
        PropVariantInit(&devicePropertyVariantName);
        KORL_WINDOWS_CHECKED_COM(devicePropertyStore, GetValue, &PKEY_Device_FriendlyName, &devicePropertyVariantName);
        korl_log(INFO, "\tdevice[%u]: name=\"%ws\" id=\"%ws\"", d, devicePropertyVariantName.pwszVal, pwszDeviceId);
        PropVariantClear(&devicePropertyVariantName);
        KORL_WINDOWS_COM_SAFE_TASKMEM_FREE(pwszDeviceId);
        KORL_WINDOWS_COM_SAFE_RELEASE(devicePropertyStore);
        KORL_WINDOWS_COM_SAFE_RELEASE(mmDevice);
    }
    KORL_WINDOWS_COM_SAFE_RELEASE(mmDeviceCollection);
    KORL_WINDOWS_COM_SAFE_RELEASE(mmDeviceEnumerator);
}
korl_internal void korl_audio_shutDown(void)
{
    CoUninitialize();// according to MSDN, this _must_ be called, even if the call to CoInitializeEx failed (due to COM already being initialized)
}
