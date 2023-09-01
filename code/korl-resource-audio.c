#include "korl-resource-audio.h"
#include "korl-interface-platform.h"
#include "korl-audio.h"
#include "korl-codec-audio.h"
#include "utility/korl-checkCast.h"
typedef struct _Korl_Resource_Audio_Context
{
    Korl_Audio_Format format;// the current format expected by korl-audio; obtained via `korl_audio_format`; we _must_ resample audio resources to match this format in order to properly render
} _Korl_Resource_Audio_Context;
typedef struct _Korl_Resource_Audio
{
    // _all_ dynamic allocations are stored in _transient_ resource memory
    Korl_Audio_Format format;
    void*             data;// decoded; in the form of `format`
    u$                dataBytes;
    Korl_Audio_Format formatResampled;
    void*             dataResampled;// in the form of `_Korl_Resource_Audio_Context::format`; if `format` does not match the context's format, we must resample `data` into here, and use this data instead
    u$                dataResampledBytes;
} _Korl_Resource_Audio;
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructCreate(_korl_resource_audio_descriptorStructCreate)
{
    return korl_allocate(allocatorRuntime, sizeof(_Korl_Resource_Audio));
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_descriptorStructDestroy(_korl_resource_audio_descriptorStructDestroy)
{
    korl_free(allocatorRuntime, resourceDescriptorStruct);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_unload(_korl_resource_audio_unload)
{
    _Korl_Resource_Audio*const audio = resourceDescriptorStruct;
    korl_free(allocatorTransient, audio->data);
    korl_free(allocatorTransient, audio->dataResampled);
    korl_memory_zero(audio, sizeof(*audio));
}
korl_internal bool _korl_resource_audio_formatsAreEqual(const Korl_Audio_Format* a, const Korl_Audio_Format* b)
{
    /* note: we don't care about channel count, since korl-audio can swizzle the 
             audio samples into the mixer stream's channels in a reasonably robust way */
    return a->bytesPerSample == b->bytesPerSample
        && a->sampleFormat   == b->sampleFormat
        && a->frameHz        == b->frameHz;
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_transcode(_korl_resource_audio_transcode)
{
    _Korl_Resource_Audio*const               audio        = resourceDescriptorStruct;
    const _Korl_Resource_Audio_Context*const audioContext = korl_resource_getDescriptorContextStruct(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_AUDIO));
    korl_assert(!audio->dataResampled);
    if(!audio->data)/* if we have not yet decoded the raw audio data, do so now */
        audio->data = korl_codec_audio_decode(data, korl_checkCast_u$_to_u32(dataBytes), allocatorTransient, &audio->dataBytes, &audio->format);
    /* in all cases, we need to conditionally resample the audio->data to match the audioContext->format */
    if(_korl_resource_audio_formatsAreEqual(&audio->format, &audioContext->format))
    {
        /* audio->data satisfies the audioContext's format; we can discard audio->dataResampled */
        korl_free(allocatorTransient, audio->dataResampled);
        audio->dataResampled = NULL;
        return;
    }
    if(audio->dataResampled && _korl_resource_audio_formatsAreEqual(&audio->formatResampled, &audioContext->format))
        return;//if we already have resampled data _and_ that data's format matches the context format, we're done
    audio->formatResampled          = audioContext->format;
    audio->formatResampled.channels = audio->format.channels;// once again, we don't need to change the # of audio channels, since korl-audio can swizzle them into the audio mixer's streams
    const u$  bytesPerFrame    = audio->format.bytesPerSample * audio->format.channels;
    const u$  frames           = audio->dataBytes / bytesPerFrame;
    const f64 seconds          = KORL_C_CAST(f64, frames) / KORL_C_CAST(f64, audio->format.frameHz);
    const u$  newFrames        = audio->format.frameHz == audio->formatResampled.frameHz
                                 ? frames
                                 : KORL_C_CAST(u$, seconds * audio->formatResampled.frameHz);
    const u$  newBytesPerFrame = audio->formatResampled.bytesPerSample * audio->formatResampled.channels;
    audio->dataResampledBytes = newFrames * newBytesPerFrame;
    audio->dataResampled      = korl_allocate(allocatorTransient, audio->dataResampledBytes);
    korl_codec_audio_resample(&audio->format         , audio->data         , audio->dataBytes
                             ,&audio->formatResampled, audio->dataResampled, audio->dataResampledBytes);
}
KORL_EXPORT KORL_FUNCTION_korl_resource_descriptorCallback_clearTransientData(_korl_resource_audio_clearTransientData)
{
    _Korl_Resource_Audio*const audio = resourceDescriptorStruct;
    korl_memory_zero(audio, sizeof(*audio));// _all_ Audio resource data is transient, so we can just clear the whole struct
}
korl_internal void korl_resource_audio_register(void)
{
    KORL_ZERO_STACK(_Korl_Resource_Audio_Context, audioContext);
    KORL_ZERO_STACK(Korl_Resource_DescriptorManifest, descriptorManifest);
    descriptorManifest.utf8DescriptorName                = KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_AUDIO);
    descriptorManifest.descriptorContext                 = &audioContext;
    descriptorManifest.descriptorContextBytes            = sizeof(audioContext);
    descriptorManifest.callbacks.descriptorStructCreate  = korl_functionDynamo_register(_korl_resource_audio_descriptorStructCreate);
    descriptorManifest.callbacks.descriptorStructDestroy = korl_functionDynamo_register(_korl_resource_audio_descriptorStructDestroy);
    descriptorManifest.callbacks.unload                  = korl_functionDynamo_register(_korl_resource_audio_unload);
    descriptorManifest.callbacks.transcode               = korl_functionDynamo_register(_korl_resource_audio_transcode);
    descriptorManifest.callbacks.clearTransientData      = korl_functionDynamo_register(_korl_resource_audio_clearTransientData);
    korl_resource_descriptor_register(&descriptorManifest);
}
korl_internal KORL_RESOURCE_FOR_EACH_CALLBACK(_korl_resource_audio_setFormat_forEach)
{
    const _Korl_Resource_Audio_Context*const audioContext = userData;
    const _Korl_Resource_Audio*const         audio        = resourceDescriptorStruct;
    if(_korl_resource_audio_formatsAreEqual(&audio->format, &audioContext->format))
    {
        /* the audio's format already satisfies the context's format; we don't need resampled data */
        *isTranscoded = (audio->dataResampled == NULL);
        return KORL_RESOURCE_FOR_EACH_RESULT_CONTINUE;
    }
    /* audio's raw data doesn't satisfy the context's format; we need to make sure we resample data at some point */
    if(!audio->dataResampled)/* if no resampled data exists we know for a fact that the resource needs to transcode */
    {
        *isTranscoded = false;
        return KORL_RESOURCE_FOR_EACH_RESULT_CONTINUE;
    }
    /* otherwise, the state of the isTranscoded flag is a result of the 
        satisfaction of our resampled audio format w/ the context's format; 
        note that we don't immediately invalidate/free audio->dataResampled here, 
        because it's entirely possible for the context's format to change again 
        during this frame, which would make resampling efforts meaningless */
    *isTranscoded = _korl_resource_audio_formatsAreEqual(&audio->formatResampled, &audioContext->format);
    return KORL_RESOURCE_FOR_EACH_RESULT_CONTINUE;
}
korl_internal void korl_resource_audio_setFormat(const Korl_Audio_Format* format)
{
    _Korl_Resource_Audio_Context*const audioContext = korl_resource_getDescriptorContextStruct(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_AUDIO));
    if(_korl_resource_audio_formatsAreEqual(&audioContext->format, format))
        return;// do nothing if we aren't changing the audioContext->format
    /* otherwise, loop over all audio resources & set the status of their 
        `isTranscoded` flag to match the `format == audioContext->format` 
        equivalence check; we don't want to _immediately_ invalidate/free their 
        resampled data here, because it's entirely possible that the 
        audioContext->format will be reset to a compatible format before 
        end-of-frame, making audio resampling work unnecessary */
    audioContext->format = *format;
    korl_resource_forEach(KORL_RAW_CONST_UTF8(KORL_RESOURCE_DESCRIPTOR_NAME_AUDIO), _korl_resource_audio_setFormat_forEach, audioContext);
}
korl_internal Korl_Resource_Audio_Data korl_resource_audio_getData(Korl_Resource_Handle resourceHandleAudio)
{
    const _Korl_Resource_Audio*const audio = korl_resource_getDescriptorStruct(resourceHandleAudio);
    if(!audio->data)
        return KORL_STRUCT_INITIALIZE_ZERO(Korl_Resource_Audio_Data);
    if(audio->dataResampled)
        return KORL_STRUCT_INITIALIZE(Korl_Resource_Audio_Data){.format = audio->formatResampled, .raw = KORL_STRUCT_INITIALIZE(acu8){.size = audio->dataResampledBytes, .data = audio->dataResampled}};
    return KORL_STRUCT_INITIALIZE(Korl_Resource_Audio_Data){.format = audio->format, .raw = KORL_STRUCT_INITIALIZE(acu8){.size = audio->dataBytes, .data = audio->data}};
}
