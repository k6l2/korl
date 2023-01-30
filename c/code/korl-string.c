#include "korl-string.h"
korl_internal bool _korl_string_isBigEndian(void)
{
    korl_shared_const i32 I = 1;
    return KORL_C_CAST(const u8*const, &I)[0] == 0;
}
korl_internal void _korl_string_codepointIteratorUtf8_decode(Korl_String_CodepointIteratorUtf8* iterator)
{
    // this could probably look a lot cleaner by doing some kind of loop over the high bits of the first raw u8, but whatever //
    if(iterator->_currentRawUtf8 >= iterator->_rawUtf8End)
        iterator->_codepoint = iterator->_codepointBytes = 0;
    else if((*iterator->_currentRawUtf8 >> 4) == 0b1111)
    {
        korl_assert(iterator->_currentRawUtf8 + 4 <= iterator->_rawUtf8End);
        korl_assert(((*(iterator->_currentRawUtf8 + 1)) >> 6) == 0b10);
        korl_assert(((*(iterator->_currentRawUtf8 + 2)) >> 6) == 0b10);
        korl_assert(((*(iterator->_currentRawUtf8 + 3)) >> 6) == 0b10);
        iterator->_codepointBytes = 4;
        iterator->_codepoint      = ((*iterator->_currentRawUtf8       & 0x3u ) << 18)
                                  | ((*(iterator->_currentRawUtf8 + 1) & 0x3Fu) << 12)
                                  | ((*(iterator->_currentRawUtf8 + 2) & 0x3Fu) <<  6)
                                  |  (*(iterator->_currentRawUtf8 + 3) & 0x3Fu);
    }
    else if((*iterator->_currentRawUtf8 >> 5) == 0b111)
    {
        korl_assert(iterator->_currentRawUtf8 + 3 <= iterator->_rawUtf8End);
        korl_assert(((*(iterator->_currentRawUtf8 + 1)) >> 6) == 0b10);
        korl_assert(((*(iterator->_currentRawUtf8 + 2)) >> 6) == 0b10);
        iterator->_codepointBytes = 3;
        iterator->_codepoint      = ((*iterator->_currentRawUtf8       & 0xFu ) << 12)
                                  | ((*(iterator->_currentRawUtf8 + 1) & 0x3Fu) << 6)
                                  |  (*(iterator->_currentRawUtf8 + 2) & 0x3Fu);
    }
    else if((*iterator->_currentRawUtf8 >> 6) == 0b11)
    {
        korl_assert(iterator->_currentRawUtf8 + 2 <= iterator->_rawUtf8End);
        korl_assert(((*(iterator->_currentRawUtf8 + 1)) >> 6) == 0b10);
        iterator->_codepointBytes = 2;
        iterator->_codepoint      = ((*iterator->_currentRawUtf8       & 0x1Fu) << 6)
                                  |  (*(iterator->_currentRawUtf8 + 1) & 0x3Fu);
    }
    else
    {
        korl_assert(((*iterator->_currentRawUtf8) >> 7) == 0);// for 1-byte, the high bit _must not_ be set
        iterator->_codepointBytes = 1;
        iterator->_codepoint      = *iterator->_currentRawUtf8;
    }
}
korl_internal Korl_String_CodepointIteratorUtf8 korl_string_codepointIteratorUtf8_initialize(const u8* rawUtf8, u$ rawUtf8Size)
{
    KORL_ZERO_STACK(Korl_String_CodepointIteratorUtf8, iterator);
    iterator._currentRawUtf8 = rawUtf8;
    iterator._rawUtf8End     = rawUtf8 + rawUtf8Size;
    _korl_string_codepointIteratorUtf8_decode(&iterator);
    return iterator;
}
korl_internal bool korl_string_codepointIteratorUtf8_done(const Korl_String_CodepointIteratorUtf8* iterator)
{
    const bool done = iterator->_codepointBytes <= 0;
    if(done)
        korl_assert(iterator->_currentRawUtf8 == iterator->_rawUtf8End);
    return done;
}
korl_internal void korl_string_codepointIteratorUtf8_next(Korl_String_CodepointIteratorUtf8* iterator)
{
    iterator->_currentRawUtf8 += iterator->_codepointBytes;
    _korl_string_codepointIteratorUtf8_decode(iterator);
    iterator->codepointIndex++;
}
korl_internal void _korl_string_codepointIteratorUtf16_decode(Korl_String_CodepointIteratorUtf16* iterator)
{
    if(iterator->_currentRawUtf16 >= iterator->_rawUtf16End)
    {
        korl_assert(iterator->_currentRawUtf16 == iterator->_rawUtf16End);
        iterator->_codepoint = iterator->_codepointSize = 0;
    }
    else if((*iterator->_currentRawUtf16 >> 10) == 0b110110)// this is the high-surrogate for a codepoint which spans 2 raw elements
    {
        korl_assert(iterator->_currentRawUtf16 + 1 < iterator->_rawUtf16End);// the next element must exist
        korl_assert(*(iterator->_currentRawUtf16 + 1) >> 10 == 0b110111);    // the next element must be the low-surrogate
        iterator->_codepointSize = 2;
        iterator->_codepoint     = 0x10000
                                 + ( (( *iterator->_currentRawUtf16      & 0x3FFu) << 10)
                                   |  (*(iterator->_currentRawUtf16 + 1) & 0x3FFu));
    }
    else
    {
        korl_assert(   *iterator->_currentRawUtf16 <  0xD800 
                    || *iterator->_currentRawUtf16 >= 0xE000);// we _must_ be in the valid range of Basic Multilingual Plane values (we _must not_ be in the surrogate range)
        iterator->_codepointSize = 1;
        iterator->_codepoint     = *iterator->_currentRawUtf16;
    }
}
korl_internal Korl_String_CodepointIteratorUtf16 korl_string_codepointIteratorUtf16_initialize(const u16* rawUtf16, u$ rawUtf16Size)
{
    korl_assert(!_korl_string_isBigEndian());// big-endian not supported; I don't want to do byte-swapping nonsense
    KORL_ZERO_STACK(Korl_String_CodepointIteratorUtf16, iterator);
    iterator._currentRawUtf16 = rawUtf16;
    iterator._rawUtf16End     = rawUtf16 + rawUtf16Size;
    _korl_string_codepointIteratorUtf16_decode(&iterator);
    return iterator;
}
korl_internal bool korl_string_codepointIteratorUtf16_done(const Korl_String_CodepointIteratorUtf16* iterator)
{
    const bool done = iterator->_codepointSize <= 0;
    if(done)
        korl_assert(iterator->_currentRawUtf16 == iterator->_rawUtf16End);
    return done;
}
korl_internal void korl_string_codepointIteratorUtf16_next(Korl_String_CodepointIteratorUtf16* iterator)
{
    iterator->_currentRawUtf16 += iterator->_codepointSize;
    _korl_string_codepointIteratorUtf16_decode(iterator);
}
korl_internal u8 korl_string_codepoint_to_utf8(u32 codepoint, u8* o_buffer)
{
    /* derived just from wikipedia: https://en.wikipedia.org/wiki/UTF-8 */
    u8 bufferSize = 0;
    if(codepoint <= 0x7F)
    {
        bufferSize = 1;
        o_buffer[0] = KORL_C_CAST(u8, codepoint);
    }
    else if(codepoint <= 0x7FF)
    {
        bufferSize = 2;
        o_buffer[0] = 0b11000000u | (KORL_C_CAST(u8, codepoint) >> 6);
        o_buffer[1] = 0b10000000u | (KORL_C_CAST(u8, codepoint) & 0x3F);
    }
    else if(codepoint <= 0xFFFF)
    {
        korl_assert(codepoint < 0xD800 || codepoint >= 0xE000);// ensure that codepoint is a valid Basic Multilingual Plane value
        bufferSize = 3;
        o_buffer[0] = 0b11100000u |  (KORL_C_CAST(u8, codepoint >> 12));
        o_buffer[1] = 0b10000000u | ((KORL_C_CAST(u8, codepoint >>  6)) & 0x3F);
        o_buffer[2] = 0b10000000u | ( KORL_C_CAST(u8, codepoint)        & 0x3F);
    }
    else
    {
        korl_assert(codepoint < 0x10FFFF);// RFC 3629 §3 limits UTF-8 encoding to code point U+10FFFF, to match the limits of UTF-16
        bufferSize = 4;
        o_buffer[0] = 0b11110000u |  (KORL_C_CAST(u8, codepoint >> 18));
        o_buffer[1] = 0b10000000u | ((KORL_C_CAST(u8, codepoint >> 12)) & 0x3F);
        o_buffer[2] = 0b10000000u | ((KORL_C_CAST(u8, codepoint >>  6)) & 0x3F);
        o_buffer[3] = 0b10000000u | ( KORL_C_CAST(u8, codepoint)        & 0x3F);
    }
    return bufferSize;
}
korl_internal u8 korl_string_codepoint_to_utf16(u32 codepoint, u16* o_buffer)
{
    /* derived just from wikipedia: https://en.wikipedia.org/wiki/UTF-16 */
    u8 bufferSize = 0;
    if(codepoint < 0x10000)
    {
        korl_assert(codepoint < 0xD800 || codepoint >= 0xE000);// ensure that codepoint is a valid Basic Multilingual Plane value
        bufferSize = 1;
        o_buffer[0] = KORL_C_CAST(u16, codepoint);
    }
    else
    {
        korl_assert(codepoint < 0x10FFFF);// RFC 3629 §3 limits UTF-8 encoding to code point U+10FFFF, to match the limits of UTF-16
        codepoint -= 0x10000;
        o_buffer[0] = KORL_C_CAST(u16, 0xD800 + (codepoint >> 10));  // high surrogate
        o_buffer[1] =                  0xDC00 + (codepoint & 0x3FFu);// low  surrogate
    }
    return bufferSize;
}
korl_internal bool korl_string_isUtf16Surrogate(u16 utf16Unit)
{
    return korl_string_isUtf16SurrogateHigh(utf16Unit)
        || korl_string_isUtf16SurrogateLow(utf16Unit);
}
korl_internal bool korl_string_isUtf16SurrogateHigh(u16 utf16Unit)
{
    return utf16Unit >= 0xD800 && utf16Unit < 0xDC00;
}
korl_internal bool korl_string_isUtf16SurrogateLow(u16 utf16Unit)
{
    return utf16Unit >= 0xDC00 && utf16Unit < 0xE000;
}
