#include "korl-codec-configuration.h"
#include "korl-stb-ds.h"
#include "korl-checkCast.h"
#include "korl-log.h"
#include "korl-string.h"
typedef struct _Korl_Codec_Configuration_Map
{
    char* key;
    Korl_Codec_Configuration_MapValue value;
} _Korl_Codec_Configuration_Map;
korl_internal void _korl_codec_configuration_appendLine(const char* newLine, u$ newLineSize
                                                       ,Korl_Memory_AllocatorHandle allocatorBuffer
                                                       ,u8** buffer, u$* bufferBytes, u$* bufferBytesUsed)
{
    const u$ bufferBytesRemaining = *bufferBytes - *bufferBytesUsed;
    if(bufferBytesRemaining < newLineSize)
    {
        *bufferBytes = KORL_MATH_MAX(2 * *bufferBytes, *bufferBytesUsed + newLineSize);
        *buffer      = korl_reallocate(allocatorBuffer, *buffer, *bufferBytes);
        korl_assert(*buffer);
    }
    korl_memory_copy(*buffer + *bufferBytesUsed, newLine, newLineSize);
    *bufferBytesUsed += newLineSize;
}
korl_internal void korl_codec_configuration_create(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocator)
{
    korl_assert(korl_memory_isNull(context, sizeof(*context)));
    context->allocator  = allocator;
    mcsh_new_arena(KORL_STB_DS_MC_CAST(context->allocator), context->stbShDatabase);
}
korl_internal void korl_codec_configuration_destroy(Korl_Codec_Configuration* context)
{
    mcshfree(KORL_STB_DS_MC_CAST(context->allocator), context->stbShDatabase);
    korl_memory_zero(context, sizeof(*context));
}
korl_internal acu8 korl_codec_configuration_toUtf8(Korl_Codec_Configuration* context, Korl_Memory_AllocatorHandle allocatorResult)
{

    u$  bufferBytes     = 1024;
    u8* buffer          = korl_allocate(allocatorResult, bufferBytes);
    u$  bufferBytesUsed = 0;
    const _Korl_Codec_Configuration_Map*const databaseEnd = context->stbShDatabase + shlen(context->stbShDatabase);
    for(const _Korl_Codec_Configuration_Map* m = context->stbShDatabase; m < databaseEnd; m++)
    {
        switch(m->value.type)
        {
        case KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_U32:{
            char*const newLine   = korl_string_formatUtf8(allocatorResult, "%hs = %u\n", m->key, m->value.subType._u32);
            const u$ newLineSize = korl_string_sizeUtf8(newLine);
            _korl_codec_configuration_appendLine(newLine, newLineSize, allocatorResult, &buffer, &bufferBytes, &bufferBytesUsed);
            korl_free(allocatorResult, newLine);
            break;}
        case KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_I32:{
            char*const newLine   = korl_string_formatUtf8(allocatorResult, "%hs = %i\n", m->key, m->value.subType._i32);
            const u$ newLineSize = korl_string_sizeUtf8(newLine);
            _korl_codec_configuration_appendLine(newLine, newLineSize, allocatorResult, &buffer, &bufferBytes, &bufferBytesUsed);
            korl_free(allocatorResult, newLine);
            break;}
        default:
            korl_log(ERROR, "type not implemented: %u", m->value.type);
        }
    }
    return (acu8){.data = buffer, .size = bufferBytesUsed};
}
korl_internal void korl_codec_configuration_fromUtf8(Korl_Codec_Configuration* context, acu8 fileDataBuffer)
{
    u$    keyTempBufferSize = 128;
    char* keyTempBuffer     = korl_allocate(context->allocator, keyTempBufferSize);
    /* clear the database context before decoding the file data buffer */
    if(shlenu(context->stbShDatabase))// only reallocate the string hash map if it is actually dirty
    {
        mcshfree      (KORL_STB_DS_MC_CAST(context->allocator), context->stbShDatabase);
        mcsh_new_arena(KORL_STB_DS_MC_CAST(context->allocator), context->stbShDatabase);
    }
    /**/
    //KORL-ISSUE-000-000-119: codec-configuration: rewrite UTF-8 decoder to use Utf8 iterators from korl-string
    i$ keyStartOffset          = -1;
    u$ keySize                 =  0;
    i$ keyValueDelimiterOffset = -1;
    i$ valueStartOffset        = -1;
    for(u$ i = 0; i < fileDataBuffer.size; i++)
    {
        const bool isWhiteSpace = korl_string_isWhitespace(fileDataBuffer.data[i]);
        if(fileDataBuffer.data[i] == '\n')
        {
            if(keyStartOffset >= 0 && valueStartOffset >= 0)
            {
                korl_assert(keyStartOffset < keyValueDelimiterOffset);  // parsing a value offset before a key offset should be invalid
                korl_assert(keyValueDelimiterOffset < valueStartOffset);// parsing a value offset before a key offset should be invalid
                korl_assert(keySize);
                /* we parsed a valid key/value pair; add the encoded pair to the database */
                // determine what data type the value is & decode as necessary //
                Korl_Codec_Configuration_MapValue value = {.type = KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_INVALID};
                bool isValidI64 = false;
                const i64 valueI64 = korl_string_utf8_to_i64((acu8){.data = fileDataBuffer.data + valueStartOffset
                                                                   ,.size = i - valueStartOffset}
                                                            ,&isValidI64);
                if(isValidI64)
                {
                    if(valueI64 >= 0 && valueI64 <= KORL_U32_MAX)
                    {
                        value.type         = KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_U32;
                        value.subType._u32 = KORL_C_CAST(u32, valueI64);
                    }
                    else if(valueI64 >= KORL_I32_MIN && valueI64 < KORL_I32_MAX)
                    {
                        value.type         = KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_I32;
                        value.subType._i32 = KORL_C_CAST(i32, valueI64);
                    }
                }
                // add the key/value to the database (if it is valid) //
                if(value.type != KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_INVALID)
                {
                    if(keyTempBufferSize < keySize + 1/*null-terminator*/)
                    {
                        keyTempBufferSize = KORL_MATH_MAX(2*keyTempBufferSize, keySize + 1/*null-terminator*/);
                        keyTempBuffer     = korl_reallocate(context->allocator, keyTempBuffer, keyTempBufferSize);
                    }
                    korl_memory_copy(keyTempBuffer, fileDataBuffer.data + keyStartOffset, keySize);
                    keyTempBuffer[keySize] = '\0';// null-terminate the temporary key buffer so we can use CRT-like API exposed by stb_ds 🤢
                    mcshput(KORL_STB_DS_MC_CAST(context->allocator), context->stbShDatabase, keyTempBuffer, value);
                }
                else
                    korl_log(ERROR, "failed to decode value for key: %.*hs", keySize, fileDataBuffer.data + keyStartOffset);
            }
            else
                korl_assert(keyStartOffset < 0 && valueStartOffset < 0);// it should be invalid to have a lone key or value; they _must_ come in pairs!
            keyStartOffset          = -1;
            keySize                 =  0;
            keyValueDelimiterOffset = -1;
            valueStartOffset        = -1;
        }
        else if(fileDataBuffer.data[i] == '=')
        {
            korl_assert(keyStartOffset >= 0);
            korl_assert(keySize);
            keyValueDelimiterOffset = korl_checkCast_u$_to_i$(i + 1);
        }
        else if(isWhiteSpace)
        {
            if(!keySize && keyStartOffset >= 0)
            {
                korl_assert(keyValueDelimiterOffset < 0);
                keySize = i - keyStartOffset;
            }
        }
        else// if(!isWhiteSpace)
        {
            if(keyStartOffset < 0)
                keyStartOffset = korl_checkCast_u$_to_i$(i);
            else if(valueStartOffset < 0 && keyValueDelimiterOffset >= 0)
                valueStartOffset = korl_checkCast_u$_to_i$(i);
        }
    }
    korl_free(context->allocator, keyTempBuffer);
}
korl_internal void korl_codec_configuration_setU32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, u32 value)
{
    const Korl_Codec_Configuration_MapValue mapValue = {.type = KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_U32, .subType = {._u32 = value}};
    mcshput(KORL_STB_DS_MC_CAST(context->allocator), context->stbShDatabase, rawUtf8Key.data, mapValue);
}
korl_internal void korl_codec_configuration_setI32(Korl_Codec_Configuration* context, acu8 rawUtf8Key, i32 value)
{
    const Korl_Codec_Configuration_MapValue mapValue = {.type = KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_I32, .subType = {._i32 = value}};
    mcshput(KORL_STB_DS_MC_CAST(context->allocator), context->stbShDatabase, rawUtf8Key.data, mapValue);
}
korl_internal Korl_Codec_Configuration_MapValue korl_codec_configuration_get(Korl_Codec_Configuration* context, acu8 rawUtf8Key)
{
    const i$ mapIndex = mcshgeti(KORL_STB_DS_MC_CAST(context->allocator), context->stbShDatabase, rawUtf8Key.data);
    if(mapIndex < 0)
        return (Korl_Codec_Configuration_MapValue){.type = KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_INVALID};
    const _Korl_Codec_Configuration_Map*const mapEntry = context->stbShDatabase + mapIndex;
    return mapEntry->value;
}
korl_internal u32 korl_codec_configuration_getU32(Korl_Codec_Configuration* context, acu8 rawUtf8Key)
{
    const Korl_Codec_Configuration_MapValue value = korl_codec_configuration_get(context, rawUtf8Key);
    switch(value.type)
    {
    case KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_U32:{
        return value.subType._u32;}
    case KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_I32:{
        return korl_checkCast_i$_to_u32(value.subType._i32);}
    default:{
        korl_log(ERROR, "incompatible type %i for key: \"%hs\"", value.type, rawUtf8Key.data);
        return KORL_U32_MAX;}
    }
}
korl_internal i32 korl_codec_configuration_getI32(Korl_Codec_Configuration* context, acu8 rawUtf8Key)
{
    const Korl_Codec_Configuration_MapValue value = korl_codec_configuration_get(context, rawUtf8Key);
    switch(value.type)
    {
    case KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_U32:{
        return korl_checkCast_u$_to_i32(value.subType._u32);}
    case KORL_CODEC_CONFIGURATION_MAP_VALUE_TYPE_I32:{
        return value.subType._i32;}
    default:{
        korl_log(ERROR, "incompatible type %i for key: \"%hs\"", value.type, rawUtf8Key.data);
        return KORL_U32_MAX;}
    }
}
