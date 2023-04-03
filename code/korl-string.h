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
korl_internal bool korl_string_isWhitespace(u$ codePoint);
korl_internal bool korl_string_isNumeric(u$ codePoint);
korl_internal i64 korl_string_utf8_to_i64(acu8 utf8, bool* out_resultIsValid);
korl_internal f32 korl_string_utf8_to_f32(acu8 utf8, u8** out_utf8F32End, bool* out_resultIsValid);
/**
 * \return the number of characters copied from \c source into \c destination , 
 * INCLUDING the null terminator.  If the \c source cannot be copied into the 
 * \c destination then the size of \c source INCLUDING the null terminator and 
 * multiplied by -1 is returned, and \c destination is filled with the maximum 
 * number of characters that can be copied, including a null-terminator.
 */
korl_internal i$ korl_string_copyUtf16(const wchar_t* source, au16 destination);
korl_internal char* korl_string_formatUtf8(Korl_Memory_AllocatorHandle allocatorHandle, const char* format, ...);
/**
 * \return the number of characters copied from \c format into \c buffer , 
 * INCLUDING the null terminator.  If the \c format cannot be copied into the 
 * \c buffer then the size of \c format , INCLUDING the null terminator, 
 * multiplied by -1, is returned.
 */
korl_internal i$ korl_string_formatBufferUtf16(wchar_t* buffer, u$ bufferBytes, const wchar_t* format, ...);
korl_internal wchar_t* korl_string_formatUtf16(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* format, ...);
/**
 * \return \c 0 if the two strings are equal
 */
korl_internal int korl_string_compareUtf8(const char* a, const char* b);
korl_internal int korl_string_compareAcu8(acu8 a, acu8 b);
/**
 * \return \c 0 if the two strings are equal
 */
korl_internal int korl_string_compareUtf16(const wchar_t* a, const wchar_t* b);
/**
 * \return The size of string \c s _excluding_ the null terminator.  "Size" is 
 * defined as the total number of characters; _not_ the total number of 
 * codepoints!
 */
korl_internal u$  korl_string_sizeUtf8(const char* s);
/**
 * \return the size of string \c s _excluding_ the null terminator
 */
korl_internal u$  korl_string_sizeUtf16(const wchar_t* s);
//KORL-ISSUE-000-000-120: interface-platform: remove KORL_DEFINED_INTERFACE_PLATFORM_API; this it just messy imo; if there is clear separation of code that should only exist in the platform layer, then we should probably just physically separate it out into separate source file(s)
#if !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)// these API defined in the platform layer because they contain platform-specific code
    korl_internal KORL_FUNCTION_korl_string_formatVaListUtf8(korl_string_formatVaListUtf8);
    korl_internal KORL_FUNCTION_korl_string_formatVaListUtf16(korl_string_formatVaListUtf16);
    korl_internal KORL_FUNCTION_korl_string_formatBufferVaListUtf16(korl_string_formatBufferVaListUtf16);
#endif
