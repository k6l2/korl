/**
 * Here, we will utilize the WinMM & WASAPI APIs to perform software audio 
 * rendering specifically for the Windows platform.
 */
#include "korl-audio.h"
#include "korl-memory.h"
#include "korl-windows-globalDefines.h"
#include "korl-interface-platform.h"
#include <Functiondiscoverykeys_devpkey.h>// for PKEY_Device_FriendlyName
#define _KORL_AUDIO_BUFFER_HECTO_NANO_SECONDS 10'000'000 // 10'000'000 => 1 second
/* Why are these Microsoft constants defined here?  Because as far as I can tell, 
    Microsoft straight up just doesn't support WASAPI in C anymore.  After doing 
    a bunch of searching, all I found was this stackoverflow question which 
    seems to indicate that this is the only solution: https://stackoverflow.com/q/43717147/4526664 */
const CLSID CLSID_MMDeviceEnumerator        = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};// obtained from <mmdeviceapi.h>
const IID   IID_IMMDeviceEnumerator         = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};// obtained from <mmdeviceapi.h>
const IID   IID_IAudioClient                = {0x1CB9AD4C, 0xDBFA, 0x4c32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};// obtained from <Audioclient.h>
const IID   IID_IAudioRenderClient          = {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};// obtained from <Audioclient.h>
const GUID  KSDATAFORMAT_SUBTYPE_PCM        = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};// obtained from <mmreg.h>
const GUID  KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};// obtained from <mmreg.h>
typedef enum _Korl_Audio_SampleFormat
    {_KORL_AUDIO_SAMPLE_FORMAT_UNKNOWN
    ,_KORL_AUDIO_SAMPLE_FORMAT_PCM
    ,_KORL_AUDIO_SAMPLE_FORMAT_FLOAT
} _Korl_Audio_SampleFormat;
typedef struct _Korl_Audio_Context
{
    IMMDeviceEnumerator* mmDeviceEnumerator;// my assumption here is that we only ever have to create one device enumerator for the entire duration of the application, as nothing in the WinMM API seems to indicate otherwise from what I've seen
    struct
    {
        bool                     isOpen;
        IMMDevice*               mmDevice;
        IPropertyStore*          propertyStore;
        PROPVARIANT              propertyVariantName;
        IAudioClient*            audioClient;
        WAVEFORMATEX*            waveFormat;
        _Korl_Audio_SampleFormat sampleFormat;
        UINT32                   bufferFramesSize;// bytes/frame == waveFormat->nChannels * waveFormat->wBitsPerSample/8
        IAudioRenderClient*      audioRenderClient;
    } output;
} _Korl_Audio_Context;
korl_global_variable _Korl_Audio_Context _korl_audio_context;
#define _KORL_AUDIO_CHECK_COM(pUnknown,api,...) _korl_audio_check((pUnknown)->lpVtbl->api(pUnknown, ## __VA_ARGS__), #api)
korl_internal bool _korl_audio_check(HRESULT hResult, const char* cStringApi)
{
    switch(hResult)
    {
    case S_OK:{
        break;}
    case AUDCLNT_E_DEVICE_INVALIDATED:{
        korl_log(WARNING, "%hs failed; HRESULT==AUDCLNT_E_DEVICE_INVALIDATED; audio endpoint or adapter device disconnected", cStringApi);
        break;}
    case AUDCLNT_E_DEVICE_IN_USE:{
        korl_log(WARNING, "%hs failed; HRESULT==AUDCLNT_E_DEVICE_IN_USE; share mode mismatch", cStringApi);
        break;}
    case AUDCLNT_E_ENDPOINT_CREATE_FAILED:{
        korl_log(WARNING, "%hs failed; HRESULT==AUDCLNT_E_ENDPOINT_CREATE_FAILED; audio endpoint is unplugged/reconfigured/unavailable", cStringApi);
        break;}
    case AUDCLNT_E_SERVICE_NOT_RUNNING:{
        korl_log(WARNING, "%hs failed; HRESULT==AUDCLNT_E_SERVICE_NOT_RUNNING; Windows audio service not running", cStringApi);
        break;}
    case ERROR_NOT_FOUND:{
        korl_log(WARNING, "%hs failed; HRESULT==ERROR_NOT_FOUND; no audio endpoints found", cStringApi);
        break;}
    default:{
        korl_log(ERROR, "%hs failed; HRESULT==0x%X", cStringApi, hResult);
        break;}
    }
    return hResult == S_OK;
}
korl_internal void _korl_audio_output_close(void)
{
    _Korl_Audio_Context*const context = &_korl_audio_context;
    KORL_WINDOWS_COM_SAFE_TASKMEM_FREE(context->output.waveFormat);
    KORL_WINDOWS_CHECK_HRESULT(PropVariantClear(&context->output.propertyVariantName));
    KORL_WINDOWS_COM_SAFE_RELEASE(context->output.propertyStore);
    KORL_WINDOWS_COM_SAFE_RELEASE(context->output.mmDevice);
}
korl_internal void _korl_audio_output_openDefault(void)
{
    _Korl_Audio_Context*const context = &_korl_audio_context;
    korl_assert(!context->output.isOpen);
    /* select & activate the default audio endpoint device */
    if(!_KORL_AUDIO_CHECK_COM(context->mmDeviceEnumerator, GetDefaultAudioEndpoint, eRender, eMultimedia, &context->output.mmDevice))
        goto cleanUp;
    /* we have an audio endpoint; we can get its information now */
    KORL_WINDOWS_CHECKED_COM(context->output.mmDevice, OpenPropertyStore, STGM_READ, &context->output.propertyStore);
    KORL_WINDOWS_CHECKED_COM(context->output.propertyStore, GetValue, &PKEY_Device_FriendlyName, &context->output.propertyVariantName);
    korl_log(INFO, "opening audio endpoint device: \"%ws\"...", context->output.propertyVariantName.pwszVal);
    if(!_KORL_AUDIO_CHECK_COM(context->output.mmDevice, Activate, &IID_IAudioClient, CLSCTX_ALL, NULL/*activationParams; IAudioClient=>NULL*/, &context->output.audioClient))
        goto cleanUp;
    if(!_KORL_AUDIO_CHECK_COM(context->output.audioClient, GetMixFormat, &context->output.waveFormat))
        goto cleanUp;
    korl_log(INFO, "AudioClient wave format: channels=%hu hz=%u bitsPerSample=%hu", context->output.waveFormat->nChannels, context->output.waveFormat->nSamplesPerSec, context->output.waveFormat->wBitsPerSample);
    context->output.sampleFormat = _KORL_AUDIO_SAMPLE_FORMAT_UNKNOWN;
    if(context->output.waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        const WAVEFORMATEXTENSIBLE*const waveFormatExtensible = KORL_C_CAST(WAVEFORMATEXTENSIBLE*, context->output.waveFormat);
        if     (IsEqualGUID(&waveFormatExtensible->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM))
            context->output.sampleFormat = _KORL_AUDIO_SAMPLE_FORMAT_PCM;
        else if(IsEqualGUID(&waveFormatExtensible->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
            context->output.sampleFormat = _KORL_AUDIO_SAMPLE_FORMAT_FLOAT;
        const char* sampleFormatCString = NULL;
        switch(context->output.sampleFormat)
        {
        case _KORL_AUDIO_SAMPLE_FORMAT_PCM:  {sampleFormatCString = "PCM";   break;}
        case _KORL_AUDIO_SAMPLE_FORMAT_FLOAT:{sampleFormatCString = "Float"; break;}
        default:                             {sampleFormatCString = "Unknown"; break;}
        }
        korl_log(INFO, "\textensible wave format: subFormat=%hs validBits/sample=%hu channelMask=0x%X", sampleFormatCString, waveFormatExtensible->Samples.wValidBitsPerSample, waveFormatExtensible->dwChannelMask);
    }
    if(!_KORL_AUDIO_CHECK_COM(context->output.audioClient, Initialize, AUDCLNT_SHAREMODE_SHARED, 0/*streamFlags*/, _KORL_AUDIO_BUFFER_HECTO_NANO_SECONDS, 0/*devicePeriod; _must_ == 0 in SHARED mode*/, context->output.waveFormat, NULL/*AudioSessionGuid; NULL=>create new session*/))
        goto cleanUp;
    if(!_KORL_AUDIO_CHECK_COM(context->output.audioClient, GetBufferSize, &context->output.bufferFramesSize))
        goto cleanUp;
    korl_log(INFO, "AudioClient bufferFramesSize=%u", context->output.bufferFramesSize);
    if(!_KORL_AUDIO_CHECK_COM(context->output.audioClient, GetService, &IID_IAudioRenderClient, &context->output.audioRenderClient))
        goto cleanUp;
    BYTE* audioBuffer = NULL;
    if(!_KORL_AUDIO_CHECK_COM(context->output.audioRenderClient, GetBuffer, context->output.bufferFramesSize, &audioBuffer))
        goto cleanUp;
    // @TODO: remove this code after testing is done
    {
        korl_assert(context->output.sampleFormat == _KORL_AUDIO_SAMPLE_FORMAT_FLOAT);
        korl_assert(context->output.waveFormat->wBitsPerSample == 32);
        f32* sampleBuffer = KORL_C_CAST(f32*, audioBuffer);
        const f32 sineHz = 493.883f;// middle-B (B4)
        const f32 samplesPerSine = KORL_C_CAST(f32, context->output.waveFormat->nSamplesPerSec) / sineHz;
        for(u32 f = 0; f < context->output.bufferFramesSize; f++)
        {
            const f32 currentSineSample = korl_math_fmod(KORL_C_CAST(f32, f), samplesPerSine);
            const f32 currentSineRatio  = currentSineSample / samplesPerSine;
            for(u16 c = 0; c < context->output.waveFormat->nChannels; c++)
            {
                // (sampleBuffer + f*context->output.waveFormat->nChannels)[c] = 0.f;
                (sampleBuffer + f*context->output.waveFormat->nChannels)[c] = korl_math_sin(currentSineRatio*KORL_TAU32);
            }
        }
    }
    // @TODO: pass the AUDCLNT_BUFFERFLAGS_SILENT flag to ReleaseBuffer after the above code is removed
    if(!_KORL_AUDIO_CHECK_COM(context->output.audioRenderClient, ReleaseBuffer, context->output.bufferFramesSize, 0/*flags*/))
        goto cleanUp;
    if(!_KORL_AUDIO_CHECK_COM(context->output.audioClient, Start))
        goto cleanUp;
    context->output.isOpen = true;
    korl_log(INFO, "...audio stream open!");
    cleanUp:
        if(!context->output.isOpen)
            /* finish releasing the context's output dynamic resources if we did not complete the opening process */
            _korl_audio_output_close();
}
korl_internal void korl_audio_initialize(void)
{
    _Korl_Audio_Context*const context = &_korl_audio_context;
    korl_memory_zero(context, sizeof(*context));
    PropVariantInit(&context->output.propertyVariantName);
    /* we're using WinMM/WASAPI; we need to make sure COM is initialized */
    const HRESULT hResultCoInitialize = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY);
    switch(hResultCoInitialize)
    {
    case S_OK:{
        korl_log(INFO, "COM initialized; may the gods help us all...");
        break;}
    case S_FALSE:{/* COM already initialized; this isn't really an error */
        korl_log(INFO, "COM already initialized; may the gods help us all...");
        break;}
    case RPC_E_CHANGED_MODE:{/* this also doesn't really indicate an error, but I guess it's worth logging this case... */
        korl_log(INFO, "CoInitializeEx: RPC_E_CHANGED_MODE; proceeding execution will assume this is okay");
        break;}
    default:{
        korl_log(ERROR, "CoInitializeEx failed; HRESULT==0x%X", hResultCoInitialize);
        break;}
    }
    KORL_WINDOWS_CHECK_HRESULT(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &context->mmDeviceEnumerator));
    /* enumerate audio endpoint devices */
    IMMDeviceCollection* mmDeviceCollection = NULL;
    UINT                 deviceCount        = 0;
    KORL_WINDOWS_CHECKED_COM(context->mmDeviceEnumerator, EnumAudioEndpoints, eRender, DEVICE_STATE_ACTIVE, &mmDeviceCollection);
    KORL_WINDOWS_CHECKED_COM(mmDeviceCollection, GetCount, &deviceCount);
    korl_log(INFO, "found %u audio endpoint devices:", deviceCount);
    for(UINT d = 0; d < deviceCount; d++)
    {
        IMMDevice*      mmDevice            = NULL;
        IPropertyStore* devicePropertyStore = NULL;
        LPWSTR          pwszDeviceId        = NULL;
        KORL_ZERO_STACK(PROPVARIANT, devicePropertyVariantName);
        /* obtain metadata of the device */
        KORL_WINDOWS_CHECKED_COM(mmDeviceCollection, Item, d, &mmDevice);
        KORL_WINDOWS_CHECKED_COM(mmDevice, GetId, &pwszDeviceId);
        KORL_WINDOWS_CHECKED_COM(mmDevice, OpenPropertyStore, STGM_READ, &devicePropertyStore);
        PropVariantInit(&devicePropertyVariantName);
        KORL_WINDOWS_CHECKED_COM(devicePropertyStore, GetValue, &PKEY_Device_FriendlyName, &devicePropertyVariantName);
        /**/
        korl_log(INFO, "\tdevice[%u]: name=\"%ws\" id=\"%ws\"", d, devicePropertyVariantName.pwszVal, pwszDeviceId);
        /* clean up dynamic memory */
        KORL_WINDOWS_CHECK_HRESULT(PropVariantClear(&devicePropertyVariantName));
        KORL_WINDOWS_COM_SAFE_TASKMEM_FREE(pwszDeviceId);
        KORL_WINDOWS_COM_SAFE_RELEASE(devicePropertyStore);
        KORL_WINDOWS_COM_SAFE_RELEASE(mmDevice);
    }
    KORL_WINDOWS_COM_SAFE_RELEASE(mmDeviceCollection);
    /**/
    _korl_audio_output_openDefault();// when korl-audio is initialized, start by attempting to open the default audio rendering device
}
korl_internal void korl_audio_shutDown(void)
{
    _Korl_Audio_Context*const context = &_korl_audio_context;
    _korl_audio_output_close();
    KORL_WINDOWS_COM_SAFE_RELEASE(context->mmDeviceEnumerator);
    CoUninitialize();// according to MSDN, this _must_ be called, even if the call to CoInitializeEx failed due to COM already being initialized
}
