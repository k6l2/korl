/**
 * An API for a super-minimalist JSON parser.
 * Complete API reference of `jsmn`:
 * JSMN_API void jsmn_init(jsmn_parser *parser);
 * JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
 *                         jsmntok_t *tokens, const unsigned int num_tokens);
 */
#pragma once
#define JSMN_STATIC
#define JSMN_HEADER
    #include "jsmn/jsmn.h"
#undef JSMN_HEADER
