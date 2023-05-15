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
 *   The database is expected to allocate heap memory using the provided 
 *   allocator; the user must call korl_codec_configuration_destroy to release 
 *   this memory.
 * 
 * korl_codec_configuration_destroy
 *   Destroy a configuration database.
 * 
 * set
 *   korl_codec_configuration_setU32
 *   korl_codec_configuration_setI32
 *     Assign a provided rawUtf8Key to contain a value of a given type.  
 *     If the key previously didn't exist in the database, it is created.  
 *     If the key already existed, it is overwritten to contain the given 
 *     value/type.  
 * 
 * get
 *   korl_codec_configuration_get
 *   korl_codec_configuration_getU32
 *   korl_codec_configuration_getI32
 *     Return the value of a provided rawUtf8Key.  
 *     If the key is not found in the database, a value whose type == 
 *     KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_INVALID is returned.
 *     If using an API which specifies the raw data type we expect the value to 
 *     be, an error will be logged if: 
 *     - the database does not contain the key
 *     - the value does not match the expected type _and_ there is no safe cast 
 *       from another similar type (for example, if the user calls _getI32 on a 
 *       u32 value, the call to _getI32 will succeed if the u32 value can safely 
 *       fit in an i32 register; where "safely" means no data is lost)
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
#include "korl-globalDefines.h"
#include "korl-memory.h"
#include "utility/korl-stringPool.h"
typedef struct Korl_Codec_Configuration
{
    Korl_Memory_AllocatorHandle           allocator;
    struct _Korl_Codec_Configuration_Map* stbShDatabase;
    // Korl_StringPool             stringPool;
} Korl_Codec_Configuration;
typedef enum Korl_Codec_Configuration_MapValue_Type
    { KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_INVALID
    , KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_U32
    , KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_I32
} Korl_Codec_Configuration_MapValue_Type;
typedef struct Korl_Codec_Configuration_MapValue
{
    Korl_Codec_Configuration_MapValue_Type type;
    union 
    {
        u32 _u32;
        i32 _i32;
    } subType;
} Korl_Codec_Configuration_MapValue;
korl_internal void korl_codec_configuration_create(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocator);
korl_internal void korl_codec_configuration_destroy(Korl_Codec_Configuration* context);
korl_internal void korl_codec_configuration_collectDefragmentPointers(Korl_Codec_Configuration* context, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers);
korl_internal acu8 korl_codec_configuration_toUtf8(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocatorResult);
korl_internal void korl_codec_configuration_fromUtf8(Korl_Codec_Configuration* context, acu8 fileDataBuffer);
korl_internal void korl_codec_configuration_setU32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, u32 value);
korl_internal void korl_codec_configuration_setI32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, i32 value);
korl_internal Korl_Codec_Configuration_MapValue korl_codec_configuration_get(Korl_Codec_Configuration* context, acu8 rawUtf8Key);
korl_internal u32 korl_codec_configuration_getU32(Korl_Codec_Configuration* context, acu8 rawUtf8Key);
korl_internal i32 korl_codec_configuration_getI32(Korl_Codec_Configuration* context, acu8 rawUtf8Key);
