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
korl_internal Korl_String_CodepointTokenizerUtf8 korl_string_codepointTokenizerUtf8_initialize(const u8* rawUtf8, u$ rawUtf8Size, u32 codepointToken)
{
    KORL_ZERO_STACK(Korl_String_CodepointTokenizerUtf8, tokenizer);
    tokenizer._iterator      = korl_string_codepointIteratorUtf8_initialize(rawUtf8, rawUtf8Size);
    tokenizer.codepointToken = codepointToken;
    // tokenizer.tokenStart     = 
    // tokenizer.tokenEnd       = tokenizer._iterator._currentRawUtf8;
    korl_string_codepointTokenizerUtf8_next(&tokenizer);
    return tokenizer;
}
korl_internal bool korl_string_codepointTokenizerUtf8_done(const Korl_String_CodepointTokenizerUtf8* tokenizer)
{
    return tokenizer->tokenStart >= tokenizer->tokenEnd;
}
korl_internal void korl_string_codepointTokenizerUtf8_next(Korl_String_CodepointTokenizerUtf8* tokenizer)
{
    for(;!korl_string_codepointIteratorUtf8_done(&tokenizer->_iterator)
        ; korl_string_codepointIteratorUtf8_next(&tokenizer->_iterator))
        if(tokenizer->_iterator._codepoint != tokenizer->codepointToken)
            break;
    tokenizer->tokenStart = tokenizer->_iterator._currentRawUtf8;
    for(;!korl_string_codepointIteratorUtf8_done(&tokenizer->_iterator)
        ; korl_string_codepointIteratorUtf8_next(&tokenizer->_iterator))
        if(tokenizer->_iterator._codepoint == tokenizer->codepointToken)
            break;
    tokenizer->tokenEnd = tokenizer->_iterator._currentRawUtf8;
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
korl_internal bool korl_string_isWhitespace(u$ codePoint)
{
    korl_shared_const unsigned char WHITESPACE_CHARS[] = {' ', '\t', '\n', '\v', '\f', '\r'};
    for(u$ w = 0; w < korl_arraySize(WHITESPACE_CHARS); w++)
        if(codePoint == WHITESPACE_CHARS[w])
            return true;
    return false;
}
korl_internal bool korl_string_isNumeric(u$ codePoint)
{
    return codePoint >= '0' && codePoint <= '9';
}
korl_internal i64 korl_string_utf8_to_i64(acu8 utf8, bool* out_resultIsValid)
{
    korl_assert(out_resultIsValid != NULL);
    i$   byteOffsetNumberStart = -1;
    bool isNegative            = false;
    i64  result                = 0;
    //KORL-ISSUE-000-000-104: unicode, UTF-8, UTF-16: likely related to KORL-ISSUE-000-000-076; UTF-8 string codepoints can _not_ be randomly accessed via an array index (and for that matter, I think UTF-16 also has this issue?); the only way to properly access UTF-8 string characters is to use an iterator; I have been doing this incorrectly in many places, so my only hope is probably to just search for "utf8" in the code and meticulously fix everything...  Sorry, future me! 😶
    for(u$ i = 0; i < utf8.size; i++)
    {
        const bool isWhiteSpace = korl_string_isWhitespace(utf8.data[i]);
        if(isWhiteSpace)
        {
            if(byteOffsetNumberStart >= 0)
            {
                // if(out_parsedBytes)
                //     *out_parsedBytes = i + 1;
                break;
            }
        }
        else// if(!isWhiteSpace)
        {
            const bool isNumeric = korl_string_isNumeric(utf8.data[i]);
            if(byteOffsetNumberStart < 0)
                byteOffsetNumberStart = korl_checkCast_u$_to_i$(i);
            if(isNumeric)
            {
                const i64 digit = isNegative
                                ? -(utf8.data[i] - '0')
                                :   utf8.data[i] - '0';
                const i64 resultPrevious = result;
                result *= 10;
                result += digit;
                if((result - digit) / 10 != resultPrevious)// the UTF-8 number is too large to fit in result
                {
                    *out_resultIsValid = false;
                    return KORL_I64_MAX;
                }
            }
            else
            {
                if(korl_checkCast_i$_to_u$(byteOffsetNumberStart) == i && utf8.data[i] == '-')
                    isNegative = true;
                else// invalid non-numeric codepoint
                {
                    *out_resultIsValid = false;
                    return KORL_I64_MAX;
                }
            }
        }
    }
    *out_resultIsValid = true;
    return result;
}
korl_internal i$ korl_string_copyUtf16(const wchar_t* source, au16 destination)
{
    const u16*const destinationEnd = destination.data + destination.size;
    i$ charsCopied = 0;
    for(; *source; ++source, ++(destination.data), ++charsCopied)
        if((destination.data) < destinationEnd - 1)
            *(destination.data) = *source;
    *(destination.data) = L'\0';
    ++charsCopied;// +1 for the null terminator
    if(korl_checkCast_i$_to_u$(charsCopied) > destination.size)
        charsCopied *= -1;
    return charsCopied;
}
korl_internal char* korl_string_formatUtf8(Korl_Memory_AllocatorHandle allocatorHandle, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char*const result = korl_string_formatVaListUtf8(allocatorHandle, format, args);
    va_end(args);
    return result;
}
korl_internal i$ korl_string_formatBufferUtf16(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    i$ result = korl_string_formatBufferVaListUtf16(buffer, bufferBytes, format, args);
    va_end(args);
    return result;
}
#if 0//currently unused, but I'll keep it around for now...
korl_internal wchar_t* korl_string_formatUtf16(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    wchar_t*const result = korl_string_formatVaListUtf16(allocatorHandle, format, args);
    va_end(args);
    return result;
}
#endif
korl_internal int korl_string_compareUtf8(const char* a, const char* b)
{
    for(; *a && *b; ++a, ++b)
    {
        if(*a < *b)
            return -1;
        else if(*a > *b)
            return 1;
    }
    if(*a)
        return 1;
    else if(*b)
        return -1;
    return 0;
}
korl_internal int korl_string_compareUtf16(const wchar_t* a, const wchar_t* b)
{
    for(; *a && *b; ++a, ++b)
    {
        if(*a < *b)
            return -1;
        else if(*a > *b)
            return 1;
    }
    if(*a)
        return 1;
    else if(*b)
        return -1;
    return 0;
}
korl_internal u$  korl_string_sizeUtf8(const char* s)
{
    if(!s)
        return 0;
    /*  [ t][ e][ s][ t][\0]
        [ 0][ 1][ 2][ 3][ 4]
        [sB]            [s ] */
    const char* sBegin = s;
    for(; *s; ++s) {}
    return korl_checkCast_i$_to_u$(s - sBegin);
}
korl_internal u$  korl_string_sizeUtf16(const wchar_t* s)
{
    if(!s)
        return 0;
    /*  [ t][ e][ s][ t][\0]
        [ 0][ 1][ 2][ 3][ 4]
        [sB]            [s ] */
    const wchar_t* sBegin = s;
    for(; *s; ++s) {}
    return korl_checkCast_i$_to_u$(s - sBegin);
}
#if !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)
korl_internal KORL_FUNCTION_korl_string_formatVaListUtf8(korl_string_formatVaListUtf8)
{
    const int bufferSize = _vscprintf(format, vaList) + 1/*for the null terminator*/;
    korl_assert(bufferSize > 0);
    char*const result = (char*)korl_allocate(allocatorHandle, bufferSize * sizeof(*result));
    korl_assert(result);
    const int charactersWritten = vsprintf_s(result, bufferSize, format, vaList);
    korl_assert(charactersWritten == bufferSize - 1);
    return result;
}
korl_internal KORL_FUNCTION_korl_string_formatVaListUtf16(korl_string_formatVaListUtf16)
{
    const int bufferSize = _vscwprintf(format, vaList) + 1/*for the null terminator*/;
    korl_assert(bufferSize > 0);
    wchar_t*const result = (wchar_t*)korl_allocate(allocatorHandle, bufferSize * sizeof(*result));
    korl_assert(result);
    const int charactersWritten = vswprintf_s(result, bufferSize, format, vaList);
    korl_assert(charactersWritten == bufferSize - 1);
    return result;
}
korl_internal KORL_FUNCTION_korl_string_formatBufferVaListUtf16(korl_string_formatBufferVaListUtf16)
{
    const int bufferSize = _vscwprintf(format, vaList);// excludes null terminator
    korl_assert(bufferSize >= 0);
    if(korl_checkCast_i$_to_u$(bufferSize + 1/*for null terminator*/) > bufferBytes / sizeof(*buffer))
        return -(bufferSize + 1/*for null terminator*/);
    const int charactersWritten = vswprintf_s(buffer, bufferBytes/sizeof(*buffer), format, vaList);// excludes null terminator
    korl_assert(charactersWritten == bufferSize);
    return charactersWritten + 1/*for null terminator*/;
}
#endif
