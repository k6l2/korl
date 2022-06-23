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
#include "korl-interface-platform.h"
typedef u32 Korl_StringPool_StringHandle;// NULL => invalid handle, as usual
/** The user of this module probably shouldn't be touching any of this data 
 * directly; use the provided stringPool API instead. */
typedef struct Korl_StringPool
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool_StringHandle nextStringHandle;
    /* String entries are an opaque type because the user should really be using 
        the provided API to operate on strings. */
    u$ stringsCapacity;                     //KORL-FEATURE-000-000-007: dynamic resizing arrays
    u$ stringsSize;                         //KORL-FEATURE-000-000-007: dynamic resizing arrays
    struct _Korl_StringPool_String* strings;//KORL-FEATURE-000-000-007: dynamic resizing arrays
    /** Having allocation meta data separate from `strings` necessarily means 
     * that we will have data duplication (specifically of the byte offsets of 
     * the actual string data within the pool), but I feel like it's probably a 
     * good idea to have this overhead for the sake of safety, in case raw 
     * string data gets overwritten out of bounds or something crazy like that.  
     * Also, allocation data should obviously be opaque since this will be 
     * internally managed.  */
    u$ allocationsCapacity;                         //KORL-FEATURE-000-000-007: dynamic resizing arrays
    u$ allocationsSize;                             //KORL-FEATURE-000-000-007: dynamic resizing arrays
    struct _Korl_StringPool_Allocation* allocations;//KORL-FEATURE-000-000-007: dynamic resizing arrays
    u$ characterPoolBytes;
    /** Internally we can just treat the character pool as a linked list of 
     * allocations.  This is similar to a general purpose allocator. */
    u8* characterPool;
} Korl_StringPool;
typedef enum Korl_StringPool_CompareResult
    { KORL_STRINGPOOL_COMPARE_RESULT_EQUAL   =  0
    , KORL_STRINGPOOL_COMPARE_RESULT_LESS    = -1// either lexicographically, or by string length
    , KORL_STRINGPOOL_COMPARE_RESULT_GREATER =  1// either lexicographically, or by string length
} Korl_StringPool_CompareResult;
/* Convenience macros specifically designed for minimum typing.  The idea here 
    is that for any given source file, we're almost always going to be using a 
    single string pool context.  So for each source file that the user wants to 
    use these macros in, they would simply redefine _LOCAL_STRING_POOL_POINTER 
    to be whatever string pool context they want. */
#define string_newUtf8(cStringUtf8)                                                                 korl_stringPool_newFromUtf8(_LOCAL_STRING_POOL_POINTER, cStringUtf8, __FILEW__, __LINE__)
#define string_newUtf16(cStringUtf16)                                                               korl_stringPool_newFromUtf16(_LOCAL_STRING_POOL_POINTER, cStringUtf16, __FILEW__, __LINE__)
#define string_newReservedUtf16(reservedSizeExcludingNullTerminator)                                korl_stringPool_newReservedUtf16(_LOCAL_STRING_POOL_POINTER, reservedSizeExcludingNullTerminator, __FILEW__, __LINE__)
#define string_free(stringHandle)                                                                   korl_stringPool_free(_LOCAL_STRING_POOL_POINTER, stringHandle)
#define string_reserveUtf16(stringHandle, reservedSizeExcludingNullTerminator)                      korl_stringPool_reserveUtf16(_LOCAL_STRING_POOL_POINTER, stringHandle, reservedSizeExcludingNullTerminator)
#define string_compareWithUtf16(stringHandle, cStringUtf16)                                         korl_stringPool_compareWithUtf16(_LOCAL_STRING_POOL_POINTER, stringHandle, cStringUtf16)
#define string_equalsUtf16(stringHandle, cStringUtf16)                                              korl_stringPool_equalsUtf16(_LOCAL_STRING_POOL_POINTER, stringHandle, cStringUtf16)
#define string_compareWithUtf8(stringHandle, cStringUtf8)                                           korl_stringPool_compareWithUtf8(_LOCAL_STRING_POOL_POINTER, stringHandle, cStringUtf8)
#define string_equalsUtf8(stringHandle, cStringUtf8)                                                korl_stringPool_equalsUtf8(_LOCAL_STRING_POOL_POINTER, stringHandle, cStringUtf8)
#define string_getRawUtf16(stringHandle)                                                            korl_stringPool_getRawUtf16(_LOCAL_STRING_POOL_POINTER, stringHandle)
#define string_getRawSizeUtf16(stringHandle)                                                        korl_stringPool_getRawSizeUtf16(_LOCAL_STRING_POOL_POINTER, stringHandle)
#define string_getRawWriteableUtf16(stringHandle)                                                   korl_stringPool_getRawWriteableUtf16(_LOCAL_STRING_POOL_POINTER, stringHandle)
#define string_copy(stringHandle)                                                                   korl_stringPool_copy(_LOCAL_STRING_POOL_POINTER, stringHandle)
#define string_append(stringHandle, stringHandleToAppend)                                           korl_stringPool_append(_LOCAL_STRING_POOL_POINTER, stringHandle, stringHandleToAppend)
#define string_appendUtf16(stringHandle, cStringUtf16)                                              korl_stringPool_appendUtf16(_LOCAL_STRING_POOL_POINTER, stringHandle, cStringUtf16)
#define string_appendFormatUtf16(stringHandle, formatUtf16, ...)                                    korl_stringPool_appendFormatUtf16(_LOCAL_STRING_POOL_POINTER, stringHandle, formatUtf16, ##__VA_ARGS__)
#if 0// currently feeling too lazy to implement this API...
#define string_appendUnsignedInteger(stringHandle, uInt, maxFigures, utf8PaddingCharacterString)    korl_stringPool_appendUnsignedInteger(_LOCAL_STRING_POOL_POINTER, stringHandle, uInt, maxFigures, utf8PaddingCharacterString)
#define string_appendUnsignedIntegerHex(stringHandle, uInt, maxFigures, utf8PaddingCharacterString) korl_stringPool_appendUnsignedIntegerHex(_LOCAL_STRING_POOL_POINTER, stringHandle, uInt, maxFigures, utf8PaddingCharacterString)
#endif
/* convenience macros specifically for korl-stringPool module, which 
    automatically inject file/line information */
#define korl_newStringUtf8(stringPoolObject, cString)  korl_stringPool_newFromUtf8 (&stringPoolObject, cString, __FILEW__, __LINE__)
#define korl_newStringUtf16(stringPoolObject, cString) korl_stringPool_newFromUtf16(&stringPoolObject, cString, __FILEW__, __LINE__)
/* string pool API */
korl_internal Korl_StringPool               korl_stringPool_create(Korl_Memory_AllocatorHandle allocatorHandle);
korl_internal void                          korl_stringPool_destroy(Korl_StringPool* context);
korl_internal Korl_StringPool_StringHandle  korl_stringPool_newFromUtf8(Korl_StringPool* context, const i8* cStringUtf8, const wchar_t* file, int line);
korl_internal Korl_StringPool_StringHandle  korl_stringPool_newFromUtf16(Korl_StringPool* context, const u16* cStringUtf16, const wchar_t* file, int line);
korl_internal Korl_StringPool_StringHandle  korl_stringPool_newReservedUtf16(Korl_StringPool* context, u32 reservedSizeExcludingNullTerminator, const wchar_t* file, int line);
korl_internal void                          korl_stringPool_free(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle);
korl_internal void                          korl_stringPool_reserveUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, u32 reservedSizeExcludingNullTerminator);
korl_internal Korl_StringPool_CompareResult korl_stringPool_compareWithUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16);
korl_internal bool                          korl_stringPool_equalsUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16);
korl_internal Korl_StringPool_CompareResult korl_stringPool_compareWithUtf8(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const char* cStringUtf8);
korl_internal bool                          korl_stringPool_equalsUtf8(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const char* cStringUtf8);
korl_internal const wchar_t*                korl_stringPool_getRawUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle);
korl_internal u32                           korl_stringPool_getRawSizeUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle);
korl_internal wchar_t*                      korl_stringPool_getRawWriteableUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle);
korl_internal Korl_StringPool_StringHandle  korl_stringPool_copy(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle);
korl_internal void                          korl_stringPool_append(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, Korl_StringPool_StringHandle stringHandleToAppend);
korl_internal void                          korl_stringPool_appendUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16);
korl_internal void                          korl_stringPool_appendFormatUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const wchar_t* format, ...);
#if 0// currently feeling too lazy to implement this API...
korl_internal void                          korl_stringPool_appendUnsignedInteger(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, u$ x, u32 maxFigures, const i8* utf8PaddingCharacter);
korl_internal void                          korl_stringPool_appendUnsignedIntegerHex(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, u$ x, u32 maxFigures, const i8* utf8PaddingCharacter);
#endif
#if 0/// potential new API:
getRawUtf8
getRawSizeUtf8
getCodepointCount
prepend
find     (not sure whether to use raw offsets, or codepoint offsets; maybe make some user code first to see what we need?)
subString(not sure whether to use raw offsets, or codepoint offsets; maybe make some user code first to see what we need?)
#endif
