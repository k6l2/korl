#pragma once
#include "korl-globalDefines.h"
typedef enum Korl_Audio_SampleFormat
    {KORL_AUDIO_SAMPLE_FORMAT_UNKNOWN
    ,KORL_AUDIO_SAMPLE_FORMAT_PCM_SIGNED
    ,KORL_AUDIO_SAMPLE_FORMAT_FLOAT
} Korl_Audio_SampleFormat;
typedef struct Korl_Audio_Format
{
    u32                     frameHz;// bytesPerFrame == channels * bytesPerSample
    Korl_Audio_SampleFormat sampleFormat;
    u8                      channels;
    u8                      bytesPerSample;
} Korl_Audio_Format;
typedef struct Korl_Audio_WriteBuffer
{
    void* frames;
    u32   framesSize;// NOTE: user must use data obtained from `korl_audio_format` to calculate how many bytes are contained in each frame
} Korl_Audio_WriteBuffer;
korl_internal void                   korl_audio_initialize(void);
korl_internal void                   korl_audio_shutDown(void);
korl_internal Korl_Audio_Format      korl_audio_format(void);
korl_internal Korl_Audio_WriteBuffer korl_audio_writeBufferGet(void);
korl_internal void                   korl_audio_writeBufferRelease(u32 framesWritten);
