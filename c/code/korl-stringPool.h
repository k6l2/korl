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
typedef struct Korl_StringPool
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool_StringHandle nextStringHandle;
    /* String entries are an opaque type because the user should really be using 
        the provided API to operate on strings. */
    u$ entriesCapacity;                    //KORL-FEATURE-000-000-007: dynamic resizing arrays
    u$ entriesSize;                        //KORL-FEATURE-000-000-007: dynamic resizing arrays
    struct _Korl_StringPool_Entry* entries;//KORL-FEATURE-000-000-007: dynamic resizing arrays
    u$ characterPoolBytes;
    u8* characterPool;
} Korl_StringPool;
typedef enum Korl_StringPool_CompareResult
    { KORL_STRINGPOOL_COMPARE_RESULT_EQUAL   =  0
    , KORL_STRINGPOOL_COMPARE_RESULT_LESS    = -1// either lexicographically, or by string length
    , KORL_STRINGPOOL_COMPARE_RESULT_GREATER =  1// either lexicographically, or by string length
} Korl_StringPool_CompareResult;
korl_internal Korl_StringPool               korl_stringPool_create(Korl_Memory_AllocatorHandle allocatorHandle);
korl_internal void                          korl_stringPool_destroy(Korl_StringPool* context);
korl_internal Korl_StringPool_StringHandle  korl_stringPool_addFromUtf16(Korl_StringPool* context, const wchar_t* cString, const wchar_t* file, int line);
korl_internal void                          korl_stringPool_remove(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle);
korl_internal Korl_StringPool_CompareResult korl_stringPool_compareWithUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const wchar_t* cString);
