#pragma once
#define STBDS_REALLOC(context, pointer, bytes, file, line) _korl_stb_ds_reallocate(context, pointer, bytes, file, line)
#define STBDS_FREE(context, pointer)                       _korl_stb_ds_free(context, pointer)
#define STBDS_API static
#include "stb/stb_ds.h"
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
#define KORL_STB_DS_MC_CAST(x) ((void*)x)
korl_internal void korl_stb_ds_initialize(void);
korl_internal void korl_stb_ds_arrayAppendU8(void* memoryContext, u8** pStbDsArray, const void* data, u$ bytes);
#if !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)
korl_internal KORL_PLATFORM_STB_DS_REALLOCATE(_korl_stb_ds_reallocate);
korl_internal KORL_PLATFORM_STB_DS_FREE(_korl_stb_ds_free);
#endif// !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)
