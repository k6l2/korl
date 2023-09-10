#include "utility/korl-jsmn.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-checkCast.h"
#include "jsmn/jsmn.h"
#include "korl-interface-platform.h"
korl_internal i$ korl_jsmn_getString(const u8* json, const jsmntok_t* token, u8* o_bufferRawUtf8)
{
    const acu8 tokenRawUtf8 = {.size = korl_checkCast_i$_to_u$(token->end - token->start), .data = json + token->start};
    korl_assert(token->type == JSMN_STRING);
    if(o_bufferRawUtf8)
    {
        korl_memory_copy(o_bufferRawUtf8, tokenRawUtf8.data, tokenRawUtf8.size);
        o_bufferRawUtf8[tokenRawUtf8.size] = '\0';
    }
    return korl_checkCast_u$_to_i$(tokenRawUtf8.size + 1/*null-terminator*/);
}
korl_internal bool korl_jsmn_getBool(const u8* json, const jsmntok_t* token)
{
    const acu8 tokenRawUtf8 = {.size = korl_checkCast_i$_to_u$(token->end - token->start), .data = json + token->start};
    korl_assert(token->type == JSMN_PRIMITIVE);
    // korl_assert(tokenRawUtf8.data[0] == 't' || tokenRawUtf8.data[0] == 'f');
    if(korl_string_equalsAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("true")))
        return true;
    if(korl_string_equalsAcu8(tokenRawUtf8, KORL_RAW_CONST_UTF8("false")))
        return false;
    korl_log(ERROR, "invalid bool token: \"%.*hs\"", tokenRawUtf8.size, tokenRawUtf8.data);
    return false;
}
korl_internal f32 korl_jsmn_getF32(const u8* json, const jsmntok_t* token)
{
    const acu8 tokenRawUtf8 = {.size = korl_checkCast_i$_to_u$(token->end - token->start), .data = json + token->start};
    korl_assert(token->type == JSMN_PRIMITIVE);
    korl_assert(tokenRawUtf8.data[0] != 'n');// primitive isn't `null`
    korl_assert(tokenRawUtf8.data[0] != 't');// primitive isn't a bool (`true`)
    korl_assert(tokenRawUtf8.data[0] != 'f');// primitive isn't a bool (`false`)
    bool isValid = false;
    const f32 result = korl_string_utf8_to_f32(tokenRawUtf8, NULL, &isValid);
    korl_assert(isValid);
    return result;
}
korl_internal Korl_Math_V3f32 korl_jsmn_getV3f32(const u8* json, const jsmntok_t* token)
{
    KORL_ZERO_STACK(Korl_Math_V3f32, result);
    korl_assert(token->type == JSMN_ARRAY);
    korl_assert(token->size == korl_arraySize(result.elements));
    for(u8 i = 0; i < korl_arraySize(result.elements); i++)
    {
        token++;
        result.elements[i] = korl_jsmn_getF32(json, token);
    }
    return result;
}
korl_internal Korl_Math_V4f32 korl_jsmn_getV4f32(const u8* json, const jsmntok_t* token)
{
    KORL_ZERO_STACK(Korl_Math_V4f32, result);
    korl_assert(token->type == JSMN_ARRAY);
    korl_assert(token->size == korl_arraySize(result.elements));
    for(u8 i = 0; i < korl_arraySize(result.elements); i++)
    {
        token++;
        result.elements[i] = korl_jsmn_getF32(json, token);
    }
    return result;
}
