#pragma once
#define STBDS_REALLOC(context, pointer, bytes, file, line) _korl_stb_ds_reallocate(context, pointer, bytes, file, line)
#define STBDS_FREE(context, pointer)                       _korl_stb_ds_free(context, pointer)
#include "stb/stb_ds.h"
#include "korl-globalDefines.h"
korl_internal void korl_stb_ds_initialize(void);
