/**
 * Used to convert between a raw UTF-8 configuration text file & a decoded 
 * database, which can be queried using a Key=>Value relationship.  
 * Configuration text files use a subset of the "initialization" file format 
 * from the Windows ecosystem (.ini files).
 * 
 * # API
 * 
 * ## Database Management
 * 
 * korl_codec_configuration_create
 *   Initialize an empty configuration database.
 * 
 * korl_codec_configuration_destroy
 *   Destroy a configuration database.
 * 
 * korl_codec_configuration_setU32
 * korl_codec_configuration_setI32
 *   Assign a provided rawUtf8Key to contain a value of a given type.  
 *   If the key previously didn't exist in the database, it is created.  
 *   If the key already existed, it is overwritten to contain the given value/type.  
 * 
 * korl_codec_configuration_getU32
 * korl_codec_configuration_getI32
 *   Return the value of a provided rawUtf8Key.  
 *   The key _must_ exist in the database.  
 *   The value of the key _must_ be the given type.
 * 
 * ## Encode/Decode
 * 
 * korl_codec_configuration_toUtf8
 *   Encode the database into UTF-8 text, which should be compatible with any 
 *   text editor if dumped to a file.
 * 
 * korl_codec_configuration_fromUtf8
 *   Decode the database from a block of UTF-8 text data.  It should be safe to 
 *   assume that the contents of the database are entirely overwritten after 
 *   this function completes.
 */
#pragma once
#include "korl-memory.h"
typedef struct Korl_Codec_Configuration
{
    Korl_Memory_AllocatorHandle allocator;
} Korl_Codec_Configuration;
korl_internal void korl_codec_configuration_create(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocator);
korl_internal void korl_codec_configuration_destroy(Korl_Codec_Configuration* context);
korl_internal acu8 korl_codec_configuration_toUtf8(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocatorResult);
korl_internal void korl_codec_configuration_fromUtf8(Korl_Codec_Configuration* context, acu8 fileDataBuffer);
korl_internal void korl_codec_configuration_setU32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, u32 value);
korl_internal void korl_codec_configuration_setI32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, i32 value);
korl_internal u32  korl_codec_configuration_getU32(Korl_Codec_Configuration* context, acu8 rawUtf8Key);
korl_internal i32  korl_codec_configuration_getI32(Korl_Codec_Configuration* context, acu8 rawUtf8Key);
