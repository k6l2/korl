#include "korl-codec-audio.h"
#include "korl-string.h"
#include "korl-checkCast.h"
#include "korl-interface-platform.h"
#include "korl-stb-vorbis.h"
typedef struct _Korl_Codec_Audio_WaveMeta
{
    u32       riffChunkSize;
    u32       riffFmtChunkSize;
    u16       riffFmtAudioFormat;
    u16       riffFmtNumChannels;
    u32       riffFmtSampleRate;
    u32       riffFmtByteRate;
    u16       riffFmtBlockAlign;
    u16       riffFmtBitsPerSample;
    u32       riffDataBytes;
    const u8* dataStart;
} _Korl_Codec_Audio_WaveMeta;
korl_internal bool _korl_codec_audio_isWave(const void* rawFileData, u32 rawFileDataBytes, _Korl_Codec_Audio_WaveMeta* o_waveMeta)
{
    const u8*      currByte = rawFileData;
    const u8*const lastByte = currByte + rawFileDataBytes;
    /* "RIFF" */
    const char CSTRING_RIFF[] = "RIFF";
    if(   currByte + korl_arraySize(CSTRING_RIFF) > lastByte 
       || 0 != korl_memory_compare(KORL_C_CAST(const char*, currByte), CSTRING_RIFF, korl_arraySize(CSTRING_RIFF) - 1))
        return false;
    currByte += korl_arraySize(CSTRING_RIFF) - 1;
    /* riff chunk size */
    o_waveMeta->riffChunkSize = *KORL_C_CAST(const u32*, currByte);
    currByte += sizeof(o_waveMeta->riffChunkSize);
    /* "WAVE" */
    const char CSTRING_WAVE[] = "WAVE";
    if(   currByte + korl_arraySize(CSTRING_WAVE) > lastByte 
       || 0 != korl_memory_compare(KORL_C_CAST(const char*, currByte), CSTRING_WAVE, korl_arraySize(CSTRING_WAVE) - 1))
        return false;
    currByte += korl_arraySize(CSTRING_WAVE) - 1;
    /* "fmt " */
    const char CSTRING_FMT[] = "fmt ";
    if(   currByte + korl_arraySize(CSTRING_FMT) > lastByte 
       || 0 != korl_memory_compare(KORL_C_CAST(const char*, currByte), CSTRING_FMT, korl_arraySize(CSTRING_FMT) - 1))
        return false;
    currByte += korl_arraySize(CSTRING_FMT) - 1;
    /* RIFF fmt chunk size */
    o_waveMeta->riffFmtChunkSize = *KORL_C_CAST(const u32*, currByte);
    if(o_waveMeta->riffFmtChunkSize != currByte - KORL_C_CAST(const u8*, rawFileData))
        return false;
    currByte += sizeof(o_waveMeta->riffFmtChunkSize);
    /* audio format */
    o_waveMeta->riffFmtAudioFormat = *KORL_C_CAST(const u16*, currByte);
    if(o_waveMeta->riffFmtAudioFormat != 1/*PCM*/)
        return false;
    currByte += sizeof(o_waveMeta->riffFmtAudioFormat);
    /* channels */
    o_waveMeta->riffFmtNumChannels = *KORL_C_CAST(const u16*, currByte);
    if(o_waveMeta->riffFmtNumChannels > KORL_U8_MAX || o_waveMeta->riffFmtNumChannels == 0)
        return false;
    currByte += sizeof(o_waveMeta->riffFmtNumChannels);
    /* sample rate */
    o_waveMeta->riffFmtSampleRate = *KORL_C_CAST(const u32*, currByte);
    currByte += sizeof(o_waveMeta->riffFmtSampleRate);
    /* byte rate */
    o_waveMeta->riffFmtByteRate = *KORL_C_CAST(const u32*, currByte);
    currByte += sizeof(o_waveMeta->riffFmtByteRate);
    /* block alignment */
    o_waveMeta->riffFmtBlockAlign = *KORL_C_CAST(const u16*, currByte);
    currByte += sizeof(o_waveMeta->riffFmtBlockAlign);
    /* bits per sample */
    o_waveMeta->riffFmtBitsPerSample = *KORL_C_CAST(const u16*, currByte);
    currByte += sizeof(o_waveMeta->riffFmtBitsPerSample);
    /* "data" */
    const char CSTRING_DATA[] = "data";
    if(   currByte + korl_arraySize(CSTRING_DATA) > lastByte 
       || 0 != korl_memory_compare(KORL_C_CAST(const char*, currByte), CSTRING_DATA, korl_arraySize(CSTRING_DATA) - 1))
        return false;
    currByte += korl_arraySize(CSTRING_DATA) - 1;
    /* riff data size */
    o_waveMeta->riffDataBytes = *KORL_C_CAST(const u32*, currByte);
    currByte += sizeof(o_waveMeta->riffDataBytes);
    if(currByte + o_waveMeta->riffDataBytes > lastByte)
        return false;
    o_waveMeta->dataStart = currByte;
    return true;
}
korl_internal void* _korl_codec_audio_decodeWave(const _Korl_Codec_Audio_WaveMeta* waveMeta, Korl_Memory_AllocatorHandle resultAllocator, u$* o_resultBytes, Korl_Audio_Format* o_resultFormat)
{
    void*const result = korl_allocate(resultAllocator, waveMeta->riffDataBytes);
    korl_memory_copy(result, waveMeta->dataStart, waveMeta->riffDataBytes);
    *o_resultBytes                 = waveMeta->riffDataBytes;
    o_resultFormat->frameHz        = waveMeta->riffFmtSampleRate;
    o_resultFormat->sampleFormat   = KORL_AUDIO_SAMPLE_FORMAT_PCM_SIGNED;
    o_resultFormat->channels       = korl_checkCast_u$_to_u8(waveMeta->riffFmtNumChannels);
    o_resultFormat->bytesPerSample = korl_checkCast_u$_to_u8(waveMeta->riffFmtBitsPerSample/8u);
    return result;
}
korl_internal void* _korl_codec_audio_decodeVorbis(const void* rawFileData, u32 rawFileDataBytes, Korl_Memory_AllocatorHandle resultAllocator, u$* o_resultBytes, Korl_Audio_Format* o_resultFormat)
{
    void* result = NULL;
    int vorbisError = VORBIS__no_error;
    stb_vorbis*const vorbis = stb_vorbis_open_memory(rawFileData, rawFileDataBytes, &vorbisError, NULL/*alloc_buffer; NULL=>use malloc/free; custom memory arena to perform all allocations from*/);
    if(vorbisError != VORBIS__no_error)
    {
        korl_log(WARNING, "stb_vorbis_open_memory failed; error=%i", vorbisError);
        goto cleanUp_returnResult;
    }
    const stb_vorbis_info vorbisInfo = stb_vorbis_get_info(vorbis);
    const i32 vorbisFrames  = stb_vorbis_stream_length_in_samples(vorbis);
    const u$  vorbisSamples = vorbisFrames * vorbisInfo.channels;
    *o_resultBytes = vorbisSamples * sizeof(f32);
    result = korl_allocate(resultAllocator, *o_resultBytes);
    if(!result)
    {
        korl_log(ERROR, "vorbis sample buffer allocate failed");
        goto cleanUp_returnResult;
    }
    const int framesDecoded = stb_vorbis_get_samples_float_interleaved(vorbis, vorbisInfo.channels, result, korl_checkCast_u$_to_i32(vorbisSamples));
    if(framesDecoded != vorbisFrames)
    {
        korl_log(ERROR, "stb_vorbis_get_samples_float_interleaved failed; framesDecoded==%i", framesDecoded);
        vorbisError = VORBIS_outofmem;
        goto cleanUp_returnResult;
    }
    o_resultFormat->sampleFormat   = KORL_AUDIO_SAMPLE_FORMAT_FLOAT;
    o_resultFormat->bytesPerSample = sizeof(f32);
    o_resultFormat->channels       = korl_checkCast_i$_to_u8(vorbisInfo.channels);
    o_resultFormat->frameHz        = vorbisInfo.sample_rate;
    cleanUp_returnResult:
        if(vorbisError != VORBIS__no_error)
        {
            korl_free(resultAllocator, result);
            result = NULL;
        }
        stb_vorbis_close(vorbis);
        return result;
}
korl_internal void* korl_codec_audio_decode(const void* rawFileData, u32 rawFileDataBytes, Korl_Memory_AllocatorHandle resultAllocator, u$* o_resultBytes, Korl_Audio_Format* o_resultFormat)
{
    void* resultData = NULL;
    KORL_ZERO_STACK(_Korl_Codec_Audio_WaveMeta, waveMeta);
    if(_korl_codec_audio_isWave(rawFileData, rawFileDataBytes, &waveMeta))
        return _korl_codec_audio_decodeWave(&waveMeta, resultAllocator, o_resultBytes, o_resultFormat);
    else if(resultData = _korl_codec_audio_decodeVorbis(rawFileData, rawFileDataBytes, resultAllocator, o_resultBytes, o_resultFormat))
        return resultData;
    korl_log(ERROR, "failed to decode audio file");
    return resultData;// if we could not find a decoder, we just return nothing
}
/** used to extract the highest possible resolution normalized float 
 * representing a single audio sample at a given format */
korl_internal f64 _korl_codec_audio_sampleRatio(const Korl_Audio_Format* format, const void* sampleData)
{
    f64 result = 0;
    switch(format->sampleFormat)
    {
    case KORL_AUDIO_SAMPLE_FORMAT_PCM_SIGNED:{
        switch(format->bytesPerSample)
        {
        case 1:
            result = *KORL_C_CAST(const i8*, sampleData) / KORL_C_CAST(f64, KORL_I8_MAX);
            break;
        case 2:
            result = *KORL_C_CAST(const i16*, sampleData) / KORL_C_CAST(f64, KORL_I16_MAX);
            break;
        case 4:
            result = *KORL_C_CAST(const i32*, sampleData) / KORL_C_CAST(f64, KORL_I32_MAX);
            break;
        default:
            korl_log(ERROR, "sample format %u bytesPerSample %hhu invalid", format->sampleFormat, format->bytesPerSample);
            break;
        }
        break;}
    case KORL_AUDIO_SAMPLE_FORMAT_FLOAT:{
        switch(format->bytesPerSample)
        {
        case 4:
            result = *KORL_C_CAST(const f32*, sampleData);
            break;
        case 8:
            result = *KORL_C_CAST(const f64*, sampleData);
            break;
        default:{
            korl_log(ERROR, "sample format %u bytesPerSample %hhu invalid", format->sampleFormat, format->bytesPerSample);
            break;}
        }
        break;}
    default:{
        korl_log(ERROR, "sample format %u invalid", format->sampleFormat);
        break;}
    }
    return result;
}
/** encode a sample ratio into a raw sample buffer at a specified format */
korl_internal void _korl_codec_audio_encodeSample(const f64 sampleRatio, const Korl_Audio_Format* format, void* o_sampleData)
{
    switch(format->sampleFormat)
    {
    case KORL_AUDIO_SAMPLE_FORMAT_PCM_SIGNED:{
        switch(format->bytesPerSample)
        {
        case 1:
            *KORL_C_CAST(i8*, o_sampleData) = KORL_C_CAST(i8, sampleRatio * KORL_I8_MAX);
            break;
        case 2:
            *KORL_C_CAST(i16*, o_sampleData) = KORL_C_CAST(i16, sampleRatio * KORL_I16_MAX);
            break;
        case 4:
            *KORL_C_CAST(i32*, o_sampleData) = KORL_C_CAST(i32, sampleRatio * KORL_I32_MAX);
            break;
        default:{
            korl_log(ERROR, "sample format %u bytesPerSample %hhu invalid", format->sampleFormat, format->bytesPerSample);
            break;}
        }
        break;}
    case KORL_AUDIO_SAMPLE_FORMAT_FLOAT:{
        switch(format->bytesPerSample)
        {
        case 4:
            *KORL_C_CAST(f32*, o_sampleData) = KORL_C_CAST(f32, sampleRatio);
            break;
        case 8:
            *KORL_C_CAST(f64*, o_sampleData) = KORL_C_CAST(f64, sampleRatio);
            break;
        default:{
            korl_log(ERROR, "sample format %u bytesPerSample %hhu invalid", format->sampleFormat, format->bytesPerSample);
            break;}
        }
        break;}
    default:{
        korl_log(ERROR, "sample format %u invalid", format->sampleFormat);
        break;}
    }
}
korl_internal void korl_codec_audio_resample(const Korl_Audio_Format* sourceFormat, const void* sourceData   , u$ sourceDataBytes
                                            ,const Korl_Audio_Format* resultFormat, void*       io_resultData, u$ resultDataBytes)
{
    korl_assert(sourceFormat->channels == resultFormat->channels);
    const u$  sourceBytesPerFrame = sourceFormat->bytesPerSample * sourceFormat->channels;
    const u$  sourceFrames        = sourceDataBytes / sourceBytesPerFrame;
    const u8* sourceBytes         = KORL_C_CAST(const u8*, sourceData);
    const u$  resultBytesPerFrame = resultFormat->bytesPerSample * resultFormat->channels;
    const u$  resultFrames        = resultDataBytes / resultBytesPerFrame;
    u8*       resultBytes         = KORL_C_CAST(u8*, io_resultData);
    const f64 resampleRatio       = KORL_C_CAST(f64, sourceFormat->frameHz) / KORL_C_CAST(f64, resultFormat->frameHz);// examples: convert 44.1khz => 48khz, ratio=1.088...; convert 88.2khz => 48khz, ratio=0.544...
    /* just for now, let's do naive LERP to resample audio */
    for(u$ resultFrame = 0; resultFrame < resultFrames; resultFrame++)
    {
        const f64 sourceFrameF32  = KORL_C_CAST(f64, resultFrame) * resampleRatio;
        const u$  sourceFrameLow  = KORL_C_CAST(u$, sourceFrameF32);// truncate; round down
        const u$  sourceFrameHigh = sourceFrameLow < sourceFrames - 1 ? sourceFrameLow + 1 : sourceFrames - 1;
        korl_assert(sourceFrameLow  < sourceFrames);
        korl_assert(sourceFrameHigh < sourceFrames);
        const f64 sourceFrameRatio = sourceFrameF32 - sourceFrameLow;// extract fraction component
        for(u8 channel = 0; channel < resultFormat->channels; channel++)
        {
            const f64 sourceSampleRatioLow  = _korl_codec_audio_sampleRatio(sourceFormat, sourceBytes + (sourceFrameLow  * sourceBytesPerFrame) + (channel * sourceFormat->bytesPerSample));
            const f64 sourceSampleRatioHigh = _korl_codec_audio_sampleRatio(sourceFormat, sourceBytes + (sourceFrameHigh * sourceBytesPerFrame) + (channel * sourceFormat->bytesPerSample));
            const f64 resultSampleRatio = (sourceFrameRatio * sourceSampleRatioLow) + ((1.0 - sourceFrameRatio) * sourceSampleRatioHigh);
            _korl_codec_audio_encodeSample(resultSampleRatio, resultFormat, resultBytes + (resultFrame * resultBytesPerFrame) + (channel * resultFormat->bytesPerSample));
        }
    }
}
