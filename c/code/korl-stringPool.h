/** Objectives of this module:
 * - string creation/destruction with minimal overhead
 * - store "unlimited" number of characters per string
 * - store character strings of different encodings (thanks, Microsoft! 😡)
 * - perform operations internally between any strings, taking into account encoding conversions
 *   - comparisons
 *   - substring extraction
 *   - concatenations
 *   - copying
 *   - find (substrings)
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-memory.h"
typedef u32 Korl_StringPool_StringHandle;// NULL => invalid handle, as usual
/** The user of this module probably shouldn't be touching any of this data 
 * directly; use the provided stringPool API instead. */
typedef struct Korl_StringPool
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool_StringHandle nextStringHandle;
    /* String entries are an opaque type because the user should really be using 
        the provided API to operate on strings. */
    struct _Korl_StringPool_String* stbDaStrings;
    /** Having allocation meta data separate from `strings` necessarily means 
     * that we will have data duplication (specifically of the byte offsets of 
     * the actual string data within the pool), but I feel like it's probably a 
     * good idea to have this overhead for the sake of safety, in case raw 
     * string data gets overwritten out of bounds or something crazy like that.  
     * Also, allocation data should obviously be opaque since this will be 
     * internally managed.  */
    struct _Korl_StringPool_Allocation* stbDaAllocations;
    u$ characterPoolBytes;
    /** Internally we can just treat the character pool as a linked list of 
     * allocations.  This is similar to a general purpose allocator. */
    u8* characterPool;
} Korl_StringPool;
typedef enum Korl_StringPool_CompareResult
    { KORL_STRINGPOOL_COMPARE_RESULT_EQUAL   =  0
    , KORL_STRINGPOOL_COMPARE_RESULT_LESS    = -1// lexicographically if string length equal, otherwise by string length
    , KORL_STRINGPOOL_COMPARE_RESULT_GREATER =  1// lexicographically if string length equal, otherwise by string length
} Korl_StringPool_CompareResult;
typedef struct Korl_StringPool_String
{
    Korl_StringPool_StringHandle handle;
    Korl_StringPool*             pool;
#if defined(__cplusplus)
    bool operator==(const Korl_StringPool_String& other) const
    {
        return (handle == other.handle) && (pool == other.pool);
    }
    operator bool() const
    {
        return (handle != 0);
    }
#endif
} Korl_StringPool_String;
#if defined(__cplusplus)
/* if we're using C++, we can have convenience macros for function overloads, 
    which are defined later in this header file */
#define string_new(...)                                korl_stringPool_new(_LOCAL_STRING_POOL_POINTER, ##__VA_ARGS__, __FILEW__, __LINE__)
//      string_append(stringHandle, ...)               Should already work with `const char*`, `const wchar_t*`, and StringHandle as second parameter!
#define string_appendFormat(stringHandle, format, ...) korl_stringPool_appendFormat(stringHandle, __FILEW__, __LINE__, format, ##__VA_ARGS__)
#endif// defined(__cplusplus)
/* Convenience macros specifically designed for minimum typing.  The idea here 
    is that for any given source file, we're almost always going to be using a 
    single string pool context.  So for each source file that the user wants to 
    use these macros in, they would simply redefine _LOCAL_STRING_POOL_POINTER 
    to be whatever string pool context they want. */
#define string_newUtf8(cStringUtf8)                                            korl_stringPool_newFromUtf8(_LOCAL_STRING_POOL_POINTER, cStringUtf8, __FILEW__, __LINE__)
#define string_newUtf16(cStringUtf16)                                          korl_stringPool_newFromUtf16(_LOCAL_STRING_POOL_POINTER, cStringUtf16, __FILEW__, __LINE__)
#define string_newAci8(constArrayCi8)                                          korl_stringPool_newFromAci8(_LOCAL_STRING_POOL_POINTER, constArrayCi8, __FILEW__, __LINE__)
#define string_newAcu16(constArrayCu16)                                        korl_stringPool_newFromAcu16(_LOCAL_STRING_POOL_POINTER, constArrayCu16, __FILEW__, __LINE__)
#define string_newEmptyUtf8(reservedSizeExcludingNullTerminator)               korl_stringPool_newEmptyUtf8(_LOCAL_STRING_POOL_POINTER, reservedSizeExcludingNullTerminator, __FILEW__, __LINE__)
#define string_newEmptyUtf16(reservedSizeExcludingNullTerminator)              korl_stringPool_newEmptyUtf16(_LOCAL_STRING_POOL_POINTER, reservedSizeExcludingNullTerminator, __FILEW__, __LINE__)
#define string_newFormatUtf8(formatUtf8, ...)                                  korl_stringPool_newFormatUtf8(_LOCAL_STRING_POOL_POINTER, __FILEW__, __LINE__, formatUtf8, ##__VA_ARGS__)
#define string_newFormatUtf16(formatUtf16, ...)                                korl_stringPool_newFormatUtf16(_LOCAL_STRING_POOL_POINTER, __FILEW__, __LINE__, formatUtf16, ##__VA_ARGS__)
#define string_free(stringHandle)                                              korl_stringPool_free(stringHandle)
#define string_reserveUtf8(stringHandle, reservedSizeExcludingNullTerminator)  korl_stringPool_reserveUtf8(stringHandle, reservedSizeExcludingNullTerminator, __FILEW__, __LINE__)
#define string_reserveUtf16(stringHandle, reservedSizeExcludingNullTerminator) korl_stringPool_reserveUtf16(stringHandle, reservedSizeExcludingNullTerminator, __FILEW__, __LINE__)
#define string_compare(stringHandleA, stringHandleB)                           korl_stringPool_compare(stringHandleA, stringHandleB)
#define string_compareWithUtf8(stringHandle, cStringUtf8)                      korl_stringPool_compareWithUtf8(stringHandle, cStringUtf8)
#define string_compareWithUtf16(stringHandle, cStringUtf16)                    korl_stringPool_compareWithUtf16(stringHandle, cStringUtf16)
#define string_equals(stringHandleA, stringHandleB)                            korl_stringPool_equals(stringHandleA, stringHandleB)
#define string_equalsUtf8(stringHandle, cStringUtf8)                           korl_stringPool_equalsUtf8(stringHandle, cStringUtf8)
#define string_equalsUtf16(stringHandle, cStringUtf16)                         korl_stringPool_equalsUtf16(stringHandle, cStringUtf16)
#define string_getRawUtf8(stringHandle)                                        korl_stringPool_getRawUtf8(stringHandle)
#define string_getRawUtf16(stringHandle)                                       korl_stringPool_getRawUtf16(stringHandle)
#define string_getRawSizeUtf8(stringHandle)                                    korl_stringPool_getRawSizeUtf8(stringHandle)
#define string_getRawSizeUtf16(stringHandle)                                   korl_stringPool_getRawSizeUtf16(stringHandle)
#define string_getRawWriteableUtf8(stringHandle)                               korl_stringPool_getRawWriteableUtf8(stringHandle)
#define string_getRawWriteableUtf16(stringHandle)                              korl_stringPool_getRawWriteableUtf16(stringHandle)
#define string_getRawAcu16(stringHandle)                                       korl_stringPool_getRawAcu16(stringHandle)
#define string_copy(stringHandle)                                              korl_stringPool_copy(stringHandle, __FILEW__, __LINE__)
#define string_copyToStringPool(stringPool,stringHandle)                       korl_stringPool_copyToStringPool(stringPool, stringHandle, __FILEW__, __LINE__)
#define string_append(stringHandle, stringHandleToAppend)                      korl_stringPool_append(stringHandle, stringHandleToAppend, __FILEW__, __LINE__)
#define string_appendUtf8(stringHandle, cStringUtf8)                           korl_stringPool_appendUtf8(stringHandle, cStringUtf8, __FILEW__, __LINE__)
#define string_appendUtf16(stringHandle, cStringUtf16)                         korl_stringPool_appendUtf16(stringHandle, cStringUtf16, __FILEW__, __LINE__)
#define string_appendFormatUtf8(stringHandle, formatUtf8, ...)                 korl_stringPool_appendFormatUtf8(stringHandle, __FILEW__, __LINE__, formatUtf8, ##__VA_ARGS__)
#define string_appendFormatUtf16(stringHandle, formatUtf16, ...)               korl_stringPool_appendFormatUtf16(stringHandle, __FILEW__, __LINE__, formatUtf16, ##__VA_ARGS__)
#define string_prependUtf8(stringHandle, cStringUtf8)                          korl_stringPool_prependUtf8(stringHandle, cStringUtf8, __FILEW__, __LINE__)
#define string_prependUtf16(stringHandle, cStringUtf16)                        korl_stringPool_prependUtf16(stringHandle, cStringUtf16, __FILEW__, __LINE__)
#define string_toUpper(stringHandle)                                           korl_stringPool_toUpper(stringHandle)
#define string_findUtf8(stringHandle, cStringUtf8, u32CharacterOffsetStart)    korl_stringPool_findUtf8(stringHandle, cStringUtf8, u32CharacterOffsetStart)
#define string_findUtf16(stringHandle, cStringUtf16, u32CharacterOffsetStart)  korl_stringPool_findUtf16(stringHandle, cStringUtf16, u32CharacterOffsetStart)
#if 0// currently feeling too lazy to implement this API...
#define string_appendUnsignedInteger(stringHandle, uInt, maxFigures, utf8PaddingCharacterString)    korl_stringPool_appendUnsignedInteger(_LOCAL_STRING_POOL_POINTER, stringHandle, uInt, maxFigures, utf8PaddingCharacterString)
#define string_appendUnsignedIntegerHex(stringHandle, uInt, maxFigures, utf8PaddingCharacterString) korl_stringPool_appendUnsignedIntegerHex(_LOCAL_STRING_POOL_POINTER, stringHandle, uInt, maxFigures, utf8PaddingCharacterString)
#endif
/* convenience macros specifically for korl-stringPool module, which 
    automatically inject file/line information */
#define korl_stringNewUtf8(stringPoolPointer, cString)                                                korl_stringPool_newFromUtf8(stringPoolPointer, cString, __FILEW__, __LINE__)
#define korl_stringNewUtf16(stringPoolPointer, cString)                                               korl_stringPool_newFromUtf16(stringPoolPointer, cString, __FILEW__, __LINE__)
#define korl_stringNewAci8(stringPoolPointer, constArrayCi8)                                          korl_stringPool_newFromAci8(stringPoolPointer, constArrayCi8, __FILEW__, __LINE__)
#define korl_stringNewAcu16(stringPoolPointer, constArrayCu16)                                        korl_stringPool_newFromAcu16(stringPoolPointer, constArrayCu16, __FILEW__, __LINE__)
#define korl_stringNewEmptyUtf8(stringPoolPointer, reservedSizeExcludingNullTerminator)               korl_stringPool_newEmptyUtf8(stringPoolPointer, reservedSizeExcludingNullTerminator, __FILEW__, __LINE__)
#define korl_stringNewEmptyUtf16(stringPoolPointer, reservedSizeExcludingNullTerminator)              korl_stringPool_newEmptyUtf16(stringPoolPointer, reservedSizeExcludingNullTerminator, __FILEW__, __LINE__)
#define korl_stringNewFormatUtf8(stringPoolPointer, formatUtf8, ...)                                  korl_stringPool_newFormatUtf8(stringPoolPointer, __FILEW__, __LINE__, formatUtf8, ##__VA_ARGS__)
#define korl_stringNewFormatUtf16(stringPoolPointer, formatUtf16, ...)                                korl_stringPool_newFormatUtf16(stringPoolPointer, __FILEW__, __LINE__, formatUtf16, ##__VA_ARGS__)
#define korl_stringReserveUtf8(stringPoolPointer, stringHandle, reservedSizeExcludingNullTerminator)  korl_stringPool_reserveUtf8(stringHandle, reservedSizeExcludingNullTerminator, __FILEW__, __LINE__)
#define korl_stringReserveUtf16(stringPoolPointer, stringHandle, reservedSizeExcludingNullTerminator) korl_stringPool_reserveUtf16(stringHandle, reservedSizeExcludingNullTerminator, __FILEW__, __LINE__)
#define korl_stringCopy(stringHandle)                                                                 korl_stringPool_copy(stringHandle, __FILEW__, __LINE__);
#define korl_stringAppend(stringHandle, stringHandleToAppend)                                         korl_stringPool_append(stringHandle, stringHandleToAppend, __FILEW__, __LINE__)
#define korl_stringAppendUtf8(stringHandle, cStringUtf8)                                              korl_stringPool_appendUtf8(stringHandle, cStringUtf8, __FILEW__, __LINE__)
#define korl_stringAppendUtf16(stringHandle, cStringUtf16)                                            korl_stringPool_appendUtf16(stringHandle, cStringUtf16, __FILEW__, __LINE__)
#define korl_stringAppendFormatUtf8(stringHandle, formatUtf8, ...)                                    korl_stringPool_appendFormatUtf8(stringHandle, __FILEW__, __LINE__, formatUtf8, ##__VA_ARGS__)
#define korl_stringAppendFormatUtf16(stringHandle, formatUtf16, ...)                                  korl_stringPool_appendFormatUtf16(stringHandle, __FILEW__, __LINE__, formatUtf16, ##__VA_ARGS__)
#define korl_stringPrependUtf8(stringHandle, cStringUtf8)                                             korl_stringPool_prependUtf8(stringHandle, cStringUtf8, __FILEW__, __LINE__)
#define korl_stringPrependUtf16(stringHandle, cStringUtf16)                                           korl_stringPool_prependUtf16(stringHandle, cStringUtf16, __FILEW__, __LINE__)
#define korl_string_subString(string, graphemeOffset, graphemeSize)                                   korl_stringPool_subString(string, graphemeOffset, graphemeSize, __FILEW__, __LINE__)
/* string pool API */
/** Because of the fact that korl-stringPool now deals with string handles that 
 * contain the StringPool pointer itself, we can no longer store StringPools in 
 * data segments which are serialized to memory states, since it is impossible 
 * to keep this address stable after loading a memory state!  We must either: 
 * - choose to use a StringPool* & dynamically allocate the StringPool struct on 
 *   the heap using allocatorHandle, or 
 * - we can revert back to using the old StringPool API that requires passing 
 *   the StringPool* for each API call
 * The user of this code module is advised to be careful about how they manage 
 * the Korl_StringPool returned by this function given the behavior described 
 * above! @korl-string-pool-no-data-segment-storage */
korl_internal Korl_StringPool               korl_stringPool_create(Korl_Memory_AllocatorHandle allocatorHandle);
korl_internal void                          korl_stringPool_destroy(Korl_StringPool* context);
korl_internal Korl_StringPool_String        korl_stringPool_newFromUtf8(Korl_StringPool* context, const i8* cStringUtf8, const wchar_t* file, int line);
korl_internal Korl_StringPool_String        korl_stringPool_newFromUtf16(Korl_StringPool* context, const u16* cStringUtf16, const wchar_t* file, int line);
korl_internal Korl_StringPool_String        korl_stringPool_newFromAci8(Korl_StringPool* context, aci8 constArrayCi8, const wchar_t* file, int line);
korl_internal Korl_StringPool_String        korl_stringPool_newFromAcu16(Korl_StringPool* context, acu16 constArrayCu16, const wchar_t* file, int line);
korl_internal Korl_StringPool_String        korl_stringPool_newEmptyUtf8(Korl_StringPool* context, u32 reservedSizeExcludingNullTerminator, const wchar_t* file, int line);
korl_internal Korl_StringPool_String        korl_stringPool_newEmptyUtf16(Korl_StringPool* context, u32 reservedSizeExcludingNullTerminator, const wchar_t* file, int line);
korl_internal Korl_StringPool_String        korl_stringPool_newFormatUtf8(Korl_StringPool* context, const wchar_t* file, int line, const char* formatUtf8, ...);
korl_internal Korl_StringPool_String        korl_stringPool_newFormatUtf16(Korl_StringPool* context, const wchar_t* file, int line, const wchar_t* formatUtf16, ...);
korl_internal void                          korl_stringPool_free(Korl_StringPool_String string);
korl_internal void                          korl_stringPool_reserveUtf8(Korl_StringPool_String string, u32 reservedSizeExcludingNullTerminator, const wchar_t* file, int line);
korl_internal void                          korl_stringPool_reserveUtf16(Korl_StringPool_String string, u32 reservedSizeExcludingNullTerminator, const wchar_t* file, int line);
korl_internal Korl_StringPool_CompareResult korl_stringPool_compare(Korl_StringPool_String stringA, Korl_StringPool_String stringB);
korl_internal Korl_StringPool_CompareResult korl_stringPool_compareWithUtf8(Korl_StringPool_String string, const char* cStringUtf8);
korl_internal Korl_StringPool_CompareResult korl_stringPool_compareWithUtf16(Korl_StringPool_String string, const u16* cStringUtf16);
korl_internal bool                          korl_stringPool_equals(Korl_StringPool_String stringA, Korl_StringPool_String stringB);
korl_internal bool                          korl_stringPool_equalsUtf8(Korl_StringPool_String string, const char* cStringUtf8);
korl_internal bool                          korl_stringPool_equalsUtf16(Korl_StringPool_String string, const u16* cStringUtf16);
korl_internal const char*                   korl_stringPool_getRawUtf8(Korl_StringPool_String string);
korl_internal const wchar_t*                korl_stringPool_getRawUtf16(Korl_StringPool_String string);
korl_internal u32                           korl_stringPool_getRawSizeUtf8(Korl_StringPool_String string);
korl_internal u32                           korl_stringPool_getRawSizeUtf16(Korl_StringPool_String string);
korl_internal char*                         korl_stringPool_getRawWriteableUtf8(Korl_StringPool_String string);
korl_internal wchar_t*                      korl_stringPool_getRawWriteableUtf16(Korl_StringPool_String string);
korl_internal acu16                         korl_stringPool_getRawAcu16(Korl_StringPool_String string);
korl_internal u32                           korl_stringPool_getGraphemeSize(Korl_StringPool_String string);
korl_internal Korl_StringPool_String        korl_stringPool_copyToStringPool(Korl_StringPool* destContext, Korl_StringPool_String string, const wchar_t* file, int line);
korl_internal Korl_StringPool_String        korl_stringPool_copy(Korl_StringPool_String string, const wchar_t* file, int line);
korl_internal Korl_StringPool_String        korl_stringPool_subString(Korl_StringPool_String string, u$ graphemeOffset, u$ graphemeSize, const wchar_t* file, int line);
korl_internal void                          korl_stringPool_append(Korl_StringPool_String string, Korl_StringPool_String stringToAppend, const wchar_t* file, int line);
korl_internal void                          korl_stringPool_appendUtf8(Korl_StringPool_String string, const i8* cStringUtf8, const wchar_t* file, int line);
korl_internal void                          korl_stringPool_appendUtf16(Korl_StringPool_String string, const u16* cStringUtf16, const wchar_t* file, int line);
korl_internal void                          korl_stringPool_appendFormatUtf8(Korl_StringPool_String string, const wchar_t* file, int line, const char* format, ...);
korl_internal void                          korl_stringPool_appendFormatUtf16(Korl_StringPool_String string, const wchar_t* file, int line, const wchar_t* format, ...);
korl_internal void                          korl_stringPool_prependUtf8(Korl_StringPool_String string, const i8* cStringUtf8, const wchar_t* file, int line);
korl_internal void                          korl_stringPool_prependUtf16(Korl_StringPool_String string, const u16* cStringUtf16, const wchar_t* file, int line);
korl_internal void                          korl_stringPool_toUpper(Korl_StringPool_String string);
/** \return The raw character offset of the first location of \c cStringUtf8 , 
 * starting from \c characterOffsetStart and advancing the search forward 
 * towards the end of the raw string.  If \c cStringUtf8 is not found, the 
 * value of \c korl_stringPool_getRawSizeUtf8 is returned. */
korl_internal u32                           korl_stringPool_findUtf8(Korl_StringPool_String string, const i8* cStringUtf8, u32 characterOffsetStart);
/** \return The raw character offset of the first location of \c cStringUtf16 , 
 * starting from \c characterOffsetStart and advancing the search forward 
 * towards the end of the raw string.  If \c cStringUtf16 is not found, the 
 * value of \c korl_stringPool_getRawSizeUtf16 is returned. */
korl_internal u32                           korl_stringPool_findUtf16(Korl_StringPool_String string, const u16* cStringUtf16, u32 characterOffsetStart);
/* if we're using C++, we can support function overloads, which should reduce 
    typing & hopefully cognitive load */
#if defined(__cplusplus)
korl_internal Korl_StringPool_String       korl_stringPool_new(Korl_StringPool* context, const wchar_t* file, int line);
korl_internal Korl_StringPool_String       korl_stringPool_new(Korl_StringPool* context, const i8* cStringUtf8, const wchar_t* file, int line);
korl_internal Korl_StringPool_String       korl_stringPool_new(Korl_StringPool* context, const u16* cStringUtf16, const wchar_t* file, int line);
korl_internal Korl_StringPool_String       korl_stringPool_new(Korl_StringPool* context, aci8 constArrayCi8, const wchar_t* file, int line);
korl_internal void                         korl_stringPool_append(Korl_StringPool_String string, const i8* cStringUtf8, const wchar_t* file, int line);
korl_internal void                         korl_stringPool_append(Korl_StringPool_String string, const u16* cStringUtf16, const wchar_t* file, int line);
korl_internal void                         korl_stringPool_appendFormat(Korl_StringPool_String string, const wchar_t* file, int line, const char* format, ...);
korl_internal void                         korl_stringPool_appendFormat(Korl_StringPool_String string, const wchar_t* file, int line, const wchar_t* format, ...);
#if 0
typedef struct Korl_StringPool_String
{
    Korl_StringPool_String handle;
#if 0/* In order to do a lot of the operator overloads that I would consider to 
        be "useful", we would have to do something like this and create a string 
        handle wrapper class of some kind, and I am really really _not_ keen on 
        implementing something like this, as it would falsely convince the user 
        of such an API that this string implementation somehow covers all the 
        expected "RAII" scenarios that a naive user would be accustomed to 
        (things like string object lifetimes being tied to the scope of the 
        string object itself, for example).  I don't subscribe to this nonsense 
        anymore, so I am currently not considering doing any of this at the 
        moment. */
    Korl_StringPool_String();
    Korl_StringPool_String(const char* cStringUtf8);
    Korl_StringPool_String(const wchar_t* cStringUtf16);
    Korl_StringPool_String(const Korl_StringPool_String& other);
    ~Korl_StringPool_String();
    Korl_StringPool_String getHandle() const;
#endif// if 0
    bool operator==(const Korl_StringPool_String& other) const;
    bool operator==(const char* cStringUtf8) const;
    bool operator==(const wchar_t* cStringUtf16) const;
    Korl_StringPool_String& operator+=(const Korl_StringPool_String& other);
    Korl_StringPool_String& operator+=(const char* cStringUtf8);
    Korl_StringPool_String& operator+=(const wchar_t* cStringUtf16);
} Korl_StringPool_String;
#endif

/// operator+(string, string)         => append(string, string)
/// operator+(string, const char*)    => appendUtf8
/// operator+(string, const wchar_t*) => appendUtf16
/// operator+(const char*, string)    => prependUtf8
/// operator+(const wchar_t*, string) => prependUtf16
/// operator+=(string, string)         => append(string, string)
/// operator+=(string, const char*)    => appendUtf8
/// operator+=(string, const wchar_t*) => appendUtf16
/// operator+=(const char*, string)    => prependUtf8
/// operator+=(const wchar_t*, string) => prependUtf16
#endif// defined(__cplusplus)
#if 0/// potential new API:
korl_internal void                          korl_stringPool_appendUnsignedInteger(Korl_StringPool* context, Korl_StringPool_String stringHandle, u$ x, u32 maxFigures, const i8* utf8PaddingCharacter);
korl_internal void                          korl_stringPool_appendUnsignedIntegerHex(Korl_StringPool* context, Korl_StringPool_String stringHandle, u$ x, u32 maxFigures, const i8* utf8PaddingCharacter);
/** \param io_rawCharacterIterator The user specifies a positive character 
 * offset in the raw UTF-16 string.  The user should initialize the value at 
 * this address to \c 0 to properly iterate over all codepoints.  If there are 
 * no more unicode codepoints, a negative number is returned to this address.
 * \return The next unicode codepoint starting at \c *io_rawCharacterIterator . */
korl_internal u32                           korl_stringPool_getNextCodepointUtf16(Korl_StringPool* context, Korl_StringPool_String stringHandle, i64* io_rawCharacterIterator);
getCodepointCount
setCodepoint
#endif// if 0
