#include "korl-codec-audio.h"
#include "korl-string.h"
#include "korl-checkCast.h"
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
korl_internal void* korl_codec_audio_decode(const void* rawFileData, u32 rawFileDataBytes, Korl_Memory_AllocatorHandle resultAllocator, u$* o_resultBytes, Korl_Audio_Format* o_resultFormat)
{
    KORL_ZERO_STACK(_Korl_Codec_Audio_WaveMeta, waveMeta);
    if(_korl_codec_audio_isWave(rawFileData, rawFileDataBytes, &waveMeta))
        return _korl_codec_audio_decodeWave(&waveMeta, resultAllocator, o_resultBytes, o_resultFormat);
    return NULL;// if we could not find a decoder, we just return nothing
}
