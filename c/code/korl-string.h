#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_String_CodepointIteratorUtf8
{
    const u8* _currentRawUtf8;
    const u8* _rawUtf8End;
    u32       _codepoint;
    u8        _codepointBytes;
} Korl_String_CodepointIteratorUtf8;
korl_internal Korl_String_CodepointIteratorUtf8 korl_string_codepointIteratorUtf8_initialize(const u8* rawUtf8, u$ rawUtf8Size);
korl_internal bool korl_string_codepointIteratorUtf8_done(const Korl_String_CodepointIteratorUtf8* iterator);
korl_internal void korl_string_codepointIteratorUtf8_next(Korl_String_CodepointIteratorUtf8* iterator);
typedef struct Korl_String_CodepointIteratorUtf16
{
    const u16* _currentRawUtf16;
    const u16* _rawUtf16End;
    u32       _codepoint;
    u8        _codepointSize;
} Korl_String_CodepointIteratorUtf16;
korl_internal Korl_String_CodepointIteratorUtf16 korl_string_codepointIteratorUtf16_initialize(const u16* rawUtf16, u$ rawUtf16Size);
korl_internal bool korl_string_codepointIteratorUtf16_done(const Korl_String_CodepointIteratorUtf16* iterator);
korl_internal void korl_string_codepointIteratorUtf16_next(Korl_String_CodepointIteratorUtf16* iterator);
korl_internal u8 korl_string_codepoint_to_utf8(u32 codepoint, u8* o_buffer);
