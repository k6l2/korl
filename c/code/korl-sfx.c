#include "korl-sfx.h"
#include "korl-audio.h"
#include "korl-resource.h"
#include "korl-memoryPool.h"
#include "korl-interface-platform.h"
#define _KORL_SFX_APPLY_MASTER_FILTER 0
#define _KORL_SFX_MIX(name) void name(void* resultSample, const void* sourceSample, f64 sourceFactor)
typedef _KORL_SFX_MIX(_fnSig_korl_sfx_mix);
#if _KORL_SFX_APPLY_MASTER_FILTER
    #define _KORL_SFX_SAMPLE_DECODE(name) f64 name(const void* sample)
    typedef _KORL_SFX_SAMPLE_DECODE(_fnSig_korl_sfx_sampleDecode);
    #define _KORL_SFX_SAMPLE_ENCODE(name) void name(void* o_sample, f64 sampleRatio)
    typedef _KORL_SFX_SAMPLE_ENCODE(_fnSig_korl_sfx_sampleEncode);
#endif
typedef struct _Korl_Sfx_TapeDeck
{
    Korl_Resource_Handle     resource;
    u$                       frame;// the next frame of `resource` which will be written to the audio renderer
    u16                      salt;
    Korl_Sfx_TapeDeckControl control;
} _Korl_Sfx_TapeDeck;
typedef struct _Korl_Sfx_Context
{
    KORL_MEMORY_POOL_DECLARE(_Korl_Sfx_TapeDeck, tapeDecks, 32);
    Korl_Sfx_TapeCategoryControl tapeDeckControls[32];
    f32 masterVolumeRatio;
    struct
    {
        Korl_Math_V3f32 worldPosition;
        Korl_Math_V3f32 worldNormalUp;
        Korl_Math_V3f32 worldNormalForward;
    } listener;// these values only affect TapeDecks which are configured to 
} _Korl_Sfx_Context;
korl_global_variable _Korl_Sfx_Context _korl_sfx_context;
korl_internal _KORL_SFX_MIX(_korl_sfx_mix_i8)
{
    *KORL_C_CAST(i8*, resultSample) += KORL_C_CAST(i8, korl_math_round_f64_to_i64(sourceFactor * *KORL_C_CAST(const i8*, sourceSample)));
}
korl_internal _KORL_SFX_MIX(_korl_sfx_mix_i16)
{
    *KORL_C_CAST(i16*, resultSample) += KORL_C_CAST(i16, korl_math_round_f64_to_i64(sourceFactor * *KORL_C_CAST(const i16*, sourceSample)));
}
korl_internal _KORL_SFX_MIX(_korl_sfx_mix_i32)
{
    *KORL_C_CAST(i32*, resultSample) += KORL_C_CAST(i32, korl_math_round_f64_to_i64(sourceFactor * *KORL_C_CAST(const i32*, sourceSample)));
}
korl_internal _KORL_SFX_MIX(_korl_sfx_mix_f32)
{
    *KORL_C_CAST(f32*, resultSample) += KORL_C_CAST(f32, sourceFactor * *KORL_C_CAST(const f32*, sourceSample));
}
korl_internal _KORL_SFX_MIX(_korl_sfx_mix_f64)
{
    *KORL_C_CAST(f64*, resultSample) += sourceFactor * *KORL_C_CAST(const f64*, sourceSample);
}
#if _KORL_SFX_APPLY_MASTER_FILTER
korl_internal _KORL_SFX_SAMPLE_DECODE(_korl_sfx_sampleDecode_i8)
{
    return KORL_C_CAST(f64, *KORL_C_CAST(i8*, sample)) / KORL_C_CAST(f64, KORL_I8_MAX);
}
korl_internal _KORL_SFX_SAMPLE_DECODE(_korl_sfx_sampleDecode_i16)
{
    return KORL_C_CAST(f64, *KORL_C_CAST(i16*, sample)) / KORL_C_CAST(f64, KORL_I16_MAX);
}
korl_internal _KORL_SFX_SAMPLE_DECODE(_korl_sfx_sampleDecode_i32)
{
    return KORL_C_CAST(f64, *KORL_C_CAST(i32*, sample)) / KORL_C_CAST(f64, KORL_I32_MAX);
}
korl_internal _KORL_SFX_SAMPLE_DECODE(_korl_sfx_sampleDecode_f32)
{
    return KORL_C_CAST(f64, *KORL_C_CAST(f32*, sample));
}
korl_internal _KORL_SFX_SAMPLE_DECODE(_korl_sfx_sampleDecode_f64)
{
    return *KORL_C_CAST(f64*, sample);
}
korl_internal _KORL_SFX_SAMPLE_ENCODE(_korl_sfx_sampleEncode_i8)
{
    *KORL_C_CAST(i8*, o_sample) = KORL_C_CAST(i8, korl_math_round_f64_to_i64(sampleRatio * KORL_I8_MAX));
}
korl_internal _KORL_SFX_SAMPLE_ENCODE(_korl_sfx_sampleEncode_i16)
{
    *KORL_C_CAST(i16*, o_sample) = KORL_C_CAST(i16, korl_math_round_f64_to_i64(sampleRatio * KORL_I16_MAX));
}
korl_internal _KORL_SFX_SAMPLE_ENCODE(_korl_sfx_sampleEncode_i32)
{
    *KORL_C_CAST(i32*, o_sample) = KORL_C_CAST(i32, korl_math_round_f64_to_i64(sampleRatio * KORL_I32_MAX));
}
korl_internal _KORL_SFX_SAMPLE_ENCODE(_korl_sfx_sampleEncode_f32)
{
    *KORL_C_CAST(f32*, o_sample) = KORL_C_CAST(f32, sampleRatio);
}
korl_internal _KORL_SFX_SAMPLE_ENCODE(_korl_sfx_sampleEncode_f64)
{
    *KORL_C_CAST(f64*, o_sample) = sampleRatio;
}
#endif
korl_internal void korl_sfx_initialize(void)
{
    korl_memory_zero(&_korl_sfx_context, sizeof(_korl_sfx_context));
    KORL_MEMORY_POOL_RESIZE(_korl_sfx_context.tapeDecks, 8);
    _korl_sfx_context.masterVolumeRatio = 1.f;
    for(u$ tcc = 0; tcc < korl_arraySize(_korl_sfx_context.tapeDeckControls); tcc++)
    {
        Korl_Sfx_TapeCategoryControl*const tapeCategoryControl = &_korl_sfx_context.tapeDeckControls[tcc];
        tapeCategoryControl->volumeRatio = 1.f;
    }
}
korl_internal void korl_sfx_mix(void)
{
    const Korl_Audio_Format       audioFormat        = korl_audio_format();
    const u32                     audioBytesPerFrame = audioFormat.channels * audioFormat.bytesPerSample;
    _fnSig_korl_sfx_mix*          mix                = NULL;
#if _KORL_SFX_APPLY_MASTER_FILTER
    _fnSig_korl_sfx_sampleDecode* sampleDecode       = NULL;
    _fnSig_korl_sfx_sampleEncode* sampleEncode       = NULL;
#endif
    switch(audioFormat.sampleFormat)
    {
    case KORL_AUDIO_SAMPLE_FORMAT_PCM_SIGNED:{
        switch(audioFormat.bytesPerSample)
        {
#if _KORL_SFX_APPLY_MASTER_FILTER
        case 1: mix = _korl_sfx_mix_i8;  sampleDecode = _korl_sfx_sampleDecode_i8;  sampleEncode = _korl_sfx_sampleEncode_i8;  break;
        case 2: mix = _korl_sfx_mix_i16; sampleDecode = _korl_sfx_sampleDecode_i16; sampleEncode = _korl_sfx_sampleEncode_i16; break;
        case 4: mix = _korl_sfx_mix_i32; sampleDecode = _korl_sfx_sampleDecode_i32; sampleEncode = _korl_sfx_sampleEncode_i32; break;
#else
        case 1: mix = _korl_sfx_mix_i8;  break;
        case 2: mix = _korl_sfx_mix_i16; break;
        case 4: mix = _korl_sfx_mix_i32; break;
#endif
        default: break;
        }
        break;}
    case KORL_AUDIO_SAMPLE_FORMAT_FLOAT:{
        switch(audioFormat.bytesPerSample)
        {
#if _KORL_SFX_APPLY_MASTER_FILTER
        case 4: mix = _korl_sfx_mix_f32; sampleDecode = _korl_sfx_sampleDecode_f32; sampleEncode = _korl_sfx_sampleEncode_f32; break;
        case 8: mix = _korl_sfx_mix_f64; sampleDecode = _korl_sfx_sampleDecode_f64; sampleEncode = _korl_sfx_sampleEncode_f64; break;
#else
        case 4: mix = _korl_sfx_mix_f32; break;
        case 8: mix = _korl_sfx_mix_f64; break;
#endif
        default: break;
        }
        break;}
    default: break;
    }
    if(!mix)
        korl_log(ERROR, "sample format %u bytesPerSample %hhu invalid", audioFormat.sampleFormat, audioFormat.bytesPerSample);
    korl_resource_setAudioFormat(&audioFormat);// we need to make sure all our audio resources are resampled to a format that is compatible with our renderer
    Korl_Audio_WriteBuffer audioBuffer = korl_audio_writeBufferGet();
    u32 framesWritten = 0;
    /* mix Tapes into audioBuffer */
    for(u8 d = 0; d < KORL_MEMORY_POOL_SIZE(_korl_sfx_context.tapeDecks); d++)
    {
        _Korl_Sfx_TapeDeck*const tapeDeck = &_korl_sfx_context.tapeDecks[d];
        if(!tapeDeck->resource)
            continue;
        korl_assert(korl_arraySize(tapeDeck->control.channelVolumeRatios) <= audioFormat.channels);// if this ever gets hit, we just have to do an extra modulus inside the channel mix loop below I guess, or something...
        korl_assert(tapeDeck->control.category < korl_arraySize(_korl_sfx_context.tapeDeckControls));
        if(tapeDeck->control.spatialization.enabled)
        {
            const Korl_Math_V3f32 listenerToDeck         = korl_math_v3f32_subtract(tapeDeck->control.spatialization.worldPosition, _korl_sfx_context.listener.worldPosition);
            const f32             listenerToDeckDistance = korl_math_v3f32_magnitude(&listenerToDeck);
            const f32             attenuationFactor      = korl_math_exponential(-tapeDeck->control.spatialization.attenuation * listenerToDeckDistance);
            tapeDeck->control.channelVolumeRatios[0] = attenuationFactor;
            tapeDeck->control.channelVolumeRatios[1] = attenuationFactor;
        }
        Korl_Sfx_TapeCategoryControl*const tapeCategoryControl = &_korl_sfx_context.tapeDeckControls[tapeDeck->control.category];
        const f32 uniformVolumeRatio = _korl_sfx_context.masterVolumeRatio * tapeCategoryControl->volumeRatio * tapeDeck->control.volumeRatio;
        korl_assert(uniformVolumeRatio >= 0.f && uniformVolumeRatio <= 1.f);
        KORL_ZERO_STACK(Korl_Audio_Format, tapeAudioFormat);
        acu8 tapeAudio = korl_resource_getAudio(tapeDeck->resource, &tapeAudioFormat);
        if(!tapeAudio.data)
            continue;
        korl_assert(tapeAudioFormat.bytesPerSample == audioFormat.bytesPerSample);
        korl_assert(tapeAudioFormat.sampleFormat   == audioFormat.sampleFormat);
        korl_assert(tapeAudioFormat.frameHz        == audioFormat.frameHz);
        const u32 tapeBytesPerFrame   = tapeAudioFormat.channels * tapeAudioFormat.bytesPerSample;
        const u32 tapeFrames          = korl_checkCast_u$_to_u32(tapeAudio.size) / tapeBytesPerFrame;
        const u32 tapeFramesRemaining = tapeDeck->frame < tapeFrames ? tapeFrames - korl_checkCast_u$_to_u32(tapeDeck->frame) : 0;
        const u32 framesToMix         = KORL_MATH_MIN(tapeFramesRemaining, audioBuffer.framesSize);
        if(framesToMix > framesWritten)
            korl_memory_zero(KORL_C_CAST(u8*, audioBuffer.frames) + (framesWritten * audioBytesPerFrame)
                            ,(framesToMix - framesWritten) * audioBytesPerFrame);
        for(u32 frame = 0; frame < framesToMix; frame++)
        {
            u8*const       audioBufferFrame = KORL_C_CAST(u8*, audioBuffer.frames) + (frame * audioBytesPerFrame);
            const u8*const tapeAudioFrame   = tapeAudio.data + ((tapeDeck->frame + frame) * tapeBytesPerFrame);
            for(u8 channel = 0; channel < audioFormat.channels; channel++)// no matter what, we want to mix audio from this tape into all the channels of audioBuffer
            {
                const u8 tapeChannel = (channel % tapeAudioFormat.channels);// allow tapes to play, even if they have fewer channels than audioBuffer
                mix(audioBufferFrame + (channel     *     audioFormat.bytesPerSample)
                   ,tapeAudioFrame   + (tapeChannel * tapeAudioFormat.bytesPerSample), uniformVolumeRatio * tapeDeck->control.channelVolumeRatios[channel]);
            }
        }
        KORL_MATH_ASSIGN_CLAMP_MIN(framesWritten, framesToMix);
        tapeDeck->frame += framesToMix;
        if(tapeDeck->frame >= tapeFrames)
            tapeDeck->resource = 0;
    }
#if _KORL_SFX_APPLY_MASTER_FILTER
    /* apply master filters over all Tape outputs */
    for(u32 frame = 0; frame < framesWritten; frame++)
    {
        u8*const audioBufferFrame = KORL_C_CAST(u8*, audioBuffer.frames) + (frame * audioBytesPerFrame);
        for(u8 channel = 0; channel < audioFormat.channels; channel++)
        {
            u8*const audioBufferSample = audioBufferFrame + (channel * audioFormat.bytesPerSample);
            f64 sampleRatio = sampleDecode(audioBufferSample);
            sampleRatio *= _korl_sfx_context.masterVolumeRatio;
            KORL_MATH_ASSIGN_CLAMP(sampleRatio, -1.0, 1.0);
            sampleEncode(audioBufferSample, sampleRatio);
        }
    }
#endif
    korl_audio_writeBufferRelease(framesWritten);
}
korl_internal KORL_FUNCTION_korl_sfx_playResource(korl_sfx_playResource)
{
    u8 d = 0;
    for(; d < KORL_MEMORY_POOL_SIZE(_korl_sfx_context.tapeDecks); d++)
        if(!_korl_sfx_context.tapeDecks[d].resource)
            break;
    if(d >= KORL_MEMORY_POOL_SIZE(_korl_sfx_context.tapeDecks))
        /* all tape decks are playing a tape; silently do nothing & return an invalid handle */
        return KORL_STRUCT_INITIALIZE_ZERO(Korl_Sfx_TapeHandle);
    _korl_sfx_context.tapeDecks[d].resource = resourceHandleAudio;
    _korl_sfx_context.tapeDecks[d].frame    = 0;
    _korl_sfx_context.tapeDecks[d].control  = tapeDeckControl;
    _korl_sfx_context.tapeDecks[d].salt++;
    KORL_ZERO_STACK(Korl_Sfx_TapeHandle, tapeHandle);
    tapeHandle.deckIndex = d;
    tapeHandle.resource  = resourceHandleAudio;
    tapeHandle.salt      = _korl_sfx_context.tapeDecks[d].salt;
    return tapeHandle;
}
korl_internal KORL_FUNCTION_korl_sfx_setVolume(korl_sfx_setVolume)
{
    _korl_sfx_context.masterVolumeRatio = volumeRatio;
}
korl_internal KORL_FUNCTION_korl_sfx_category_set(korl_sfx_category_set)
{
    korl_assert(category < korl_arraySize(_korl_sfx_context.tapeDeckControls));
    _korl_sfx_context.tapeDeckControls[category] = tapeCategoryControl;
}
korl_internal KORL_FUNCTION_korl_sfx_setListener(korl_sfx_setListener)
{
    _korl_sfx_context.listener.worldPosition      = worldPosition;
    _korl_sfx_context.listener.worldNormalUp      = worldNormalUp;
    _korl_sfx_context.listener.worldNormalForward = worldNormalForward;
}
