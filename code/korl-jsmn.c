#include "korl-jsmn.h"
#include "jsmn/jsmn.h"
korl_internal void korl_jsmn_getString(const u8* json, const jsmntok_t* token, u8* o_bufferRawUtf8, u$ bufferRawUtf8Size)
{
    const acu8 tokenRawUtf8 = {.data = json + token->start, .size = token->end - token->start};
    korl_assert(token->type == JSMN_STRING);
    korl_assert(tokenRawUtf8.size < bufferRawUtf8Size);
    korl_memory_copy(o_bufferRawUtf8, tokenRawUtf8.data, tokenRawUtf8.size);
    o_bufferRawUtf8[tokenRawUtf8.size] = '\0';
}
korl_internal f32 korl_jsmn_getF32(const u8* json, const jsmntok_t* token)
{
    const acu8 tokenRawUtf8 = {.data = json + token->start, .size = token->end - token->start};
    korl_assert(token->type == JSMN_PRIMITIVE);
    korl_assert(tokenRawUtf8.data[0] != 'n');
    korl_assert(tokenRawUtf8.data[0] != 't');
    korl_assert(tokenRawUtf8.data[0] != 'f');
    bool isValid = false;
    const f32 result = korl_string_utf8_to_f32(tokenRawUtf8, NULL, &isValid);
    korl_assert(isValid);
    return result;
}
