#include "korl-sfx.h"
#include "korl-audio.h"
#include "korl-resource.h"
korl_internal void korl_sfx_initialize(void)
{
}
korl_internal void korl_sfx_mix(void)
{
    const Korl_Audio_Format audioFormat = korl_audio_format();
    korl_resource_setAudioFormat(&audioFormat);// we need to make sure all our audio resources are resampled to a format that is compatible with our renderer
    Korl_Audio_WriteBuffer audioBuffer = korl_audio_writeBufferGet();
    //@TODO: write audio to buffer
    // korl_log(VERBOSE, "audioBuffer.size=%llu", audioBuffer.size);//@TODO: delete
    korl_audio_writeBufferRelease(audioBuffer.framesSize);
}
korl_internal KORL_FUNCTION_korl_sfx_playResource(korl_sfx_playResource)
{
    return 0;//@TODO
}
