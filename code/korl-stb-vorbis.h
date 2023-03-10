#pragma once
#include "korl-globalDefines.h"
#define STB_VORBIS_NO_CRT
#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY
korl_internal void korl_stb_vorbis_initialize(void);
