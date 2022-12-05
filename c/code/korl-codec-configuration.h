/**
 * Used to convert between a raw UTF-8 configuration text file & a decoded 
 * database, which can be queried using a Key=>Value relationship.  
 * Configuration text files use a subset of the "initialization" file format 
 * from the Windows ecosystem (.ini files).
 */
#pragma once
#include "korl-memory.h"
typedef struct Korl_Codec_Configuration
{
    Korl_Memory_AllocatorHandle allocator;
} Korl_Codec_Configuration;
korl_internal void korl_codec_configuration_create(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocator);
korl_internal void korl_codec_configuration_destroy(Korl_Codec_Configuration* context);
korl_internal void korl_codec_configuration_setU32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, u32 value);
korl_internal void korl_codec_configuration_setI32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, i32 value);
korl_internal acu8 korl_codec_configuration_toUtf8(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocatorResult);
