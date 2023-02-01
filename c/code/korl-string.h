#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_String_CodepointIteratorUtf8
{
    const u8* _currentRawUtf8;
    const u8* _rawUtf8End;
    u32       _codepoint;// the decoded codepoint of the current iterator position; only valid after calling `initialize`, and after each call to `next` (assuming `done` returns false!)
    u8        _codepointBytes;
    u$        codepointIndex;
} Korl_String_CodepointIteratorUtf8;
korl_internal Korl_String_CodepointIteratorUtf8 korl_string_codepointIteratorUtf8_initialize(const u8* rawUtf8, u$ rawUtf8Size);
korl_internal bool                              korl_string_codepointIteratorUtf8_done(const Korl_String_CodepointIteratorUtf8* iterator);
korl_internal void                              korl_string_codepointIteratorUtf8_next(Korl_String_CodepointIteratorUtf8* iterator);
typedef struct Korl_String_CodepointTokenizerUtf8
{
    Korl_String_CodepointIteratorUtf8 _iterator;
    u32 codepointToken;
    const u8* tokenStart;
    const u8* tokenEnd;
} Korl_String_CodepointTokenizerUtf8;
korl_internal Korl_String_CodepointTokenizerUtf8 korl_string_codepointTokenizerUtf8_initialize(const u8* rawUtf8, u$ rawUtf8Size, u32 codepointToken);
korl_internal bool                               korl_string_codepointTokenizerUtf8_done(const Korl_String_CodepointTokenizerUtf8* tokenizer);
korl_internal void                               korl_string_codepointTokenizerUtf8_next(Korl_String_CodepointTokenizerUtf8* tokenizer);
typedef struct Korl_String_CodepointIteratorUtf16
{
    const u16* _currentRawUtf16;
    const u16* _rawUtf16End;
    u32       _codepoint;// the decoded codepoint of the current iterator position; only valid after calling `initialize`, and after each call to `next` (assuming `done` returns false!)
    u8        _codepointSize;
} Korl_String_CodepointIteratorUtf16;
korl_internal Korl_String_CodepointIteratorUtf16 korl_string_codepointIteratorUtf16_initialize(const u16* rawUtf16, u$ rawUtf16Size);
korl_internal bool                               korl_string_codepointIteratorUtf16_done(const Korl_String_CodepointIteratorUtf16* iterator);
korl_internal void                               korl_string_codepointIteratorUtf16_next(Korl_String_CodepointIteratorUtf16* iterator);
/** \return # of \c u8 elements written to \c o_buffer 
 * \param o_buffer caller is expected to have at _least_ \c 4 elements in this array! */
korl_internal u8 korl_string_codepoint_to_utf8(u32 codepoint, u8* o_buffer);
/** \return # of \c u16 elements written to \c o_buffer 
 * \param o_buffer caller is expected to have at _least_ \c 2 elements in this array! */
korl_internal u8 korl_string_codepoint_to_utf16(u32 codepoint, u16* o_buffer);
korl_internal bool korl_string_isUtf16Surrogate(u16 utf16Unit);
korl_internal bool korl_string_isUtf16SurrogateHigh(u16 utf16Unit);
korl_internal bool korl_string_isUtf16SurrogateLow(u16 utf16Unit);
