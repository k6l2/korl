#pragma once
#include "korl-interface-platform-resource.h"
typedef struct Korl_Resource_Audio_Data
{
    acu8              raw;
    Korl_Audio_Format format;
} Korl_Resource_Audio_Data;
korl_internal void                     korl_resource_audio_register(void);
korl_internal void                     korl_resource_audio_setFormat(const Korl_Audio_Format* format);
korl_internal Korl_Resource_Audio_Data korl_resource_audio_getData(Korl_Resource_Handle resourceHandleAudio);
