#include "korl-stringPool.h"
#include "korl-checkCast.h"
typedef struct _Korl_StringPool_Allocation
{
    u32 poolByteOffset;
    u32 bytes;
    const wchar_t* file;
    int line;
} _Korl_StringPool_Allocation;
#define SORT_NAME _korl_stringPool_allocation
#define SORT_TYPE _Korl_StringPool_Allocation
#define SORT_CMP(x, y) ((x).poolByteOffset < (y).poolByteOffset ? -1 : ((x).poolByteOffset > (y).poolByteOffset ? 1 : 0))
#ifndef SORT_CHECK_CAST_INT_TO_SIZET
#define SORT_CHECK_CAST_INT_TO_SIZET(x) korl_checkCast_i$_to_u$(x)
#endif
#ifndef SORT_CHECK_CAST_SIZET_TO_INT
#define SORT_CHECK_CAST_SIZET_TO_INT(x) korl_checkCast_u$_to_i32(x)
#endif
#include "sort.h"
typedef enum _Korl_StringPool_StringFlags
    { _KORL_STRINGPOOL_STRING_FLAGS_NONE = 0
    , _KORL_STRINGPOOL_STRING_FLAG_UTF8  = 1<<0
    , _KORL_STRINGPOOL_STRING_FLAG_UTF16 = 1<<1
} _Korl_StringPool_StringFlags;
#if defined(__cplusplus)
korl_internal _Korl_StringPool_StringFlags operator|(_Korl_StringPool_StringFlags a, _Korl_StringPool_StringFlags b)
{
    return static_cast<_Korl_StringPool_StringFlags>(static_cast<int>(a) | static_cast<int>(b));
}
inline _Korl_StringPool_StringFlags& operator|=(_Korl_StringPool_StringFlags& a, _Korl_StringPool_StringFlags b)
{
    return a = a | b;
}
#endif
typedef struct _Korl_StringPool_String
{
    Korl_StringPool_StringHandle handle;
    u32 poolByteOffsetUtf8;
    u32 rawSizeUtf8;// _excluding_ any null-terminator characters
    u32 poolByteOffsetUtf16;
    u32 rawSizeUtf16;// _excluding_ any null-terminator characters
    _Korl_StringPool_StringFlags flags;
} _Korl_StringPool_String;
/** \return the byte offset of the new allocation */
korl_internal u32 _korl_stringPool_allocate(Korl_StringPool* context, u$ bytes, const wchar_t* file, int line)
{
    _korl_stringPool_allocation_quick_sort(context->allocations, context->allocationsSize);
    u$ currentPoolOffset  = 0;
    u$ currentUnusedBytes = context->characterPoolBytes;// value only used if there are no allocations
    for(u$ a = 0; a < context->allocationsSize; a++)
    {
        currentUnusedBytes = context->allocations[a].poolByteOffset - currentPoolOffset;// overwrite new unused byte count, since we know there is an allocation here
        /* check if there is sufficient unused space in the regions behind each 
            allocation in the pool */
        if(bytes < currentUnusedBytes)
            goto create_allocation_and_return_currentPoolOffset;
        currentPoolOffset = context->allocations[a].poolByteOffset + context->allocations[a].bytes;
        currentUnusedBytes = context->characterPoolBytes - currentPoolOffset;// value is only used in the code outside of this loop (after the last iteration)
    }
    /* check if there is sufficient unused space at the end of the pool (after 
        the last allocation, or if no allocations are present) */
    if(bytes < currentUnusedBytes)
        goto create_allocation_and_return_currentPoolOffset;
    /* the pool isn't big enough to contain `bytes` & must be expanded */
    context->characterPoolBytes = KORL_MATH_MAX(context->characterPoolBytes + bytes, 2*context->characterPoolBytes);
    context->characterPool = KORL_C_CAST(u8*, korl_reallocate(context->allocatorHandle, context->characterPool, context->characterPoolBytes));
    korl_assert(context->characterPool);
create_allocation_and_return_currentPoolOffset:
    KORL_ZERO_STACK(_Korl_StringPool_Allocation, newAllocation);
    newAllocation.bytes          = korl_checkCast_u$_to_u32(bytes);
    newAllocation.file           = file;
    newAllocation.line           = line;
    newAllocation.poolByteOffset = korl_checkCast_u$_to_u32(currentPoolOffset);
    if(context->allocationsSize >= context->allocationsCapacity)
    {
        context->allocationsCapacity *= 2;
        context->allocations = KORL_C_CAST(_Korl_StringPool_Allocation*, korl_reallocate(context->allocatorHandle, context->allocations, sizeof(*context->allocations) * context->allocationsCapacity));
        korl_assert(context->allocations);
    }
    context->allocations[context->allocationsSize++] = newAllocation;
    return newAllocation.poolByteOffset;
}
korl_internal u32 _korl_stringPool_reallocate(Korl_StringPool* context, u32 allocationOffset, u$ bytes, const wchar_t* file, int line)
{
    _korl_stringPool_allocation_quick_sort(context->allocations, context->allocationsSize);
    /* find the allocation matching the provided byte offset */
    u$ allocIndex = 0;
    for(; allocIndex < context->allocationsSize; allocIndex++)
        if(context->allocations[allocIndex].poolByteOffset == allocationOffset)
            break;
    korl_assert(allocIndex < context->allocationsSize);
    _Korl_StringPool_Allocation*const allocationPrevious = &(context->allocations[allocIndex]);
    if(bytes == allocationPrevious->bytes)
        return allocationOffset;
    /* if bytes is smaller, we can just shrink the allocation & we're done */
    if(bytes < allocationPrevious->bytes)
    {
        allocationPrevious->bytes = korl_checkCast_u$_to_u32(bytes);
        return allocationOffset;
    }
    /* check to see if we can expand the allocation */
    u32 expandOffsetMin = allocationPrevious->poolByteOffset;
    u32 expandOffsetMax = allocationPrevious->poolByteOffset + allocationPrevious->bytes;
    if(allocIndex == 0)
        expandOffsetMin = 0;
    else
        expandOffsetMin = context->allocations[allocIndex - 1].poolByteOffset + context->allocations[allocIndex - 1].bytes;
    if(allocIndex == context->allocationsSize - 1)
        expandOffsetMax = korl_checkCast_u$_to_u32(context->characterPoolBytes);
    else
        expandOffsetMax = context->allocations[allocIndex + 1].poolByteOffset;
    /* if we can expand to the higher offsets, we can just grow the allocation & we're done */
    if(expandOffsetMax - allocationPrevious->poolByteOffset >= bytes)
    {
        allocationPrevious->bytes = korl_checkCast_u$_to_u32(bytes);
        return allocationOffset;
    }
    /* if we can't expand to higher offsets, but we are the last allocation, we 
        can just grow the pool & increase our allocation size */
    else if(allocIndex == context->allocationsSize - 1)
    {
        context->characterPoolBytes = KORL_MATH_MAX(context->characterPoolBytes + bytes/*NOTE: this is an overestimate, but should still work*/, 2*context->characterPoolBytes);
        context->characterPool = KORL_C_CAST(u8*, korl_reallocate(context->allocatorHandle, context->characterPool, context->characterPoolBytes));
        korl_assert(context->characterPool);
        allocationPrevious->bytes = korl_checkCast_u$_to_u32(bytes);
        return allocationOffset;
    }
    /* if we can expand to a lower offset, we need to move the memory then update metrics */
    if(expandOffsetMax - expandOffsetMin >= bytes)
    {
        korl_memory_move(context->characterPool + expandOffsetMin, 
                         context->characterPool + allocationPrevious->poolByteOffset, 
                         allocationPrevious->bytes);
        allocationPrevious->poolByteOffset = expandOffsetMin;
        allocationPrevious->bytes          = korl_checkCast_u$_to_u32(bytes);
        return allocationPrevious->poolByteOffset;
    }
    /* we can't expand; we need to allocate->copy->free */
    const u32 newAllocOffset = _korl_stringPool_allocate(context, bytes, file, line);//KORL-PERFORMANCE-000-000-025: stringPool: we should tell the allocation function that it doesn't have to sort
    korl_memory_copy(context->characterPool + newAllocOffset, 
                     context->characterPool + allocationPrevious->poolByteOffset, 
                     allocationPrevious->bytes);
    return newAllocOffset;
}
korl_internal void _korl_stringPool_free(Korl_StringPool* context, u32 poolByteOffset)
{
    for(u$ a = 0; a < context->allocationsSize; a++)
    {
        if(context->allocations[a].poolByteOffset == poolByteOffset)
        {
            /* remove the allocation by replacing this allocation with the last 
                allocation in the collection, then decreasing the collection 
                size (should be quite fast, but requires allocate having to re-
                sort the collection, which is fine by me for now) */
            context->allocations[a] = context->allocations[context->allocationsSize - 1];
            context->allocationsSize--;
            return;
        }
    }
    korl_log(ERROR, "pool byte offset %i not found in string pool", poolByteOffset);
}
korl_internal Korl_StringPool_StringHandle _korl_stringPool_addStringCommon(Korl_StringPool* context, const void* cString, u32 cStringSize, _Korl_StringPool_StringFlags stringFlags, const wchar_t* file, int line)
{
    /* find a unique handle for the new string */
    korl_assert(context->nextStringHandle != 0);
    const Korl_StringPool_StringHandle newHandle = context->nextStringHandle;
    if(context->nextStringHandle == KORL_U32_MAX)
        context->nextStringHandle = 1;
    else
        context->nextStringHandle++;
    // ensure that the handle is unique //
    ///@TODO: stringpool: robustness; string handle uniqueness is not guaranteed
    for(u$ s = 0; s < context->stringsSize; s++)
        korl_assert(context->strings[s].handle != newHandle);
    /* add a new string entry */
    if(context->stringsSize >= context->stringsCapacity)
    {
        korl_assert(context->stringsCapacity > 0);
        context->stringsCapacity *= 2;
        context->strings = KORL_C_CAST(_Korl_StringPool_String*, korl_reallocate(context->allocatorHandle, context->strings, sizeof(*context->strings) * context->stringsCapacity));
        korl_assert(context->strings);
    }
    _Korl_StringPool_String*const newString = &context->strings[context->stringsSize++];
    korl_memory_zero(newString, sizeof(*newString));
    newString->handle = newHandle;
    newString->flags  = stringFlags;
    u$ characterSize    = 0;
    u32* poolByteOffset = NULL;
    u32* rawSize        = NULL;
    if(stringFlags == _KORL_STRINGPOOL_STRING_FLAG_UTF8)
    {
        characterSize  = 1;
        poolByteOffset = &(newString->poolByteOffsetUtf8);
        rawSize        = &(newString->rawSizeUtf8);
    }
    else if(stringFlags == _KORL_STRINGPOOL_STRING_FLAG_UTF16)
    {
        characterSize  = 2;
        poolByteOffset = &(newString->poolByteOffsetUtf16);
        rawSize        = &(newString->rawSizeUtf16);
    }
    korl_assert(characterSize);
    korl_assert(poolByteOffset);
    korl_assert(rawSize);
    *rawSize        = cStringSize;
    *poolByteOffset = _korl_stringPool_allocate(context, ((*rawSize) + 1)*characterSize, file, line);
    korl_memory_copy(context->characterPool + *poolByteOffset, cString, (*rawSize)*characterSize);
    return newHandle;
}
korl_internal void _korl_stringPool_convert_utf8_to_utf16(Korl_StringPool* context, _Korl_StringPool_String* string, const wchar_t* file, int line)
{
    korl_assert(!(string->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16));
    /* prepare some initial memory for the utf16 string */
    string->flags |= _KORL_STRINGPOOL_STRING_FLAG_UTF16;
    string->rawSizeUtf16        = string->rawSizeUtf8;// initial estimate; likely to change later
    string->poolByteOffsetUtf16 = _korl_stringPool_allocate(context, (string->rawSizeUtf16 + 1/*null-terminator*/)*sizeof(u16), file, line);
    /* perform the conversion; resources: 
        https://en.wikipedia.org/wiki/UTF-8
        https://en.wikipedia.org/wiki/UTF-16 */
    u32 currentUtf16 = 0;
    for(u32 cU8 = 0; cU8 < string->rawSizeUtf8; cU8++/*we always consume at least one UTF-8 byte (the leading byte)*/)
    {
        /* if the utf16 cursor is pointing to the last character in the string 
            pool (reserved for the null terminator), we know that the utf16 
            string needs to be expanded */
        if(currentUtf16 >= string->rawSizeUtf16 - 1/*in case we need to occupy 2 utf16 characters*/)
        {
            string->rawSizeUtf16       *= 2;
            string->poolByteOffsetUtf16 = _korl_stringPool_reallocate(context, string->poolByteOffsetUtf16, (string->rawSizeUtf16 + 1/*null-terminator*/)*sizeof(u16), file, line);
        }
        u8*const  utf8  = context->characterPool + string->poolByteOffsetUtf8;
        u16*const utf16 = KORL_C_CAST(u16*, context->characterPool + string->poolByteOffsetUtf16);
        if(utf8[cU8] < 0x80)// [0,127]; ASCII range; one byte per codepoint
            utf16[currentUtf16++] = utf8[cU8];// entire codepoint is contained in the leading UTF-8 byte
        else if(utf8[cU8] < 0xC0)// [0x80, 0xBF]
        {
            // ignored; reserved for UTF-8 encoding
        }
        else if(utf8[cU8] < 0xE0)// [128, 2047]; 2 bytes per codepoint
        {
            if(cU8 + 1 >= string->rawSizeUtf8)
                break;// invalid last codepoint (missing utf8 characters); just ignore it
            utf16[currentUtf16++] = (KORL_C_CAST(u16, utf8[cU8    ] & 0x1Fu) << 6) 
                                  |                  (utf8[cU8 + 1] & 0x3Fu);
            cU8++;// consume trailing utf8 character
        }
        else if(utf8[cU8] < 0xF0)// [2048, 65535]; 3 bytes per codepoint
        {
            if(cU8 + 2 >= string->rawSizeUtf8)
                break;// invalid last codepoint (missing utf8 characters); just ignore it
            utf16[currentUtf16] = (KORL_C_CAST(u16, utf8[cU8    ] & 0xFu ) << 12)
                                | (KORL_C_CAST(u16, utf8[cU8 + 1] & 0x3Fu) << 6)
                                |                  (utf8[cU8 + 2] & 0x3Fu);
            if(utf16[currentUtf16] >= 0xD800 && utf16[currentUtf16] <= 0xDFFF)
            {
                /* this codepoint range is reserved in UTF-16; we need to ignore 
                    this value; maybe warn on this condition?... */
            }
            else
                currentUtf16++;// the codepoint is valid
            cU8 += 2;// consume trailing utf8 characters
        }
        else if(utf8[cU8] < 0xF8)// [65536, 0x10FFFF]; 4 bytes per codepoint
        {
            if(cU8 + 3 >= string->rawSizeUtf8)
                break;// invalid last codepoint (missing utf8 characters); just ignore it
            const u32 codepoint = (KORL_C_CAST(u32, utf8[cU8    ] & 0x7 ) << 18)
                                | (KORL_C_CAST(u32, utf8[cU8 + 1] & 0x3F) << 12)
                                | (KORL_C_CAST(u32, utf8[cU8 + 2] & 0x3F) << 6)
                                |                  (utf8[cU8 + 3] & 0x3F);
            utf16[currentUtf16    ] = KORL_C_CAST(u16, (codepoint - 0x10000) >> 10);
            utf16[currentUtf16 + 1] = KORL_C_CAST(u16, (codepoint - 0x10000) & 0x4FF);
            currentUtf16 += 2;
            cU8 += 3;// consume trailing utf8 characters
        }
        else// invalid codepoint
        {
            // ignored; maybe warn on this condition?  Not sure...
        }
    }
    /* we now know exactly how many u16 characters are necessary to store the 
        entire UTF-16 string, but it's highly probable that we overestimated the 
        buffer required to store it, so we should now correct this to eliminate 
        any slack at the end of the string */
    if(currentUtf16 < string->rawSizeUtf16)
    {
        string->rawSizeUtf16        = currentUtf16;
        string->poolByteOffsetUtf16 = _korl_stringPool_reallocate(context, string->poolByteOffsetUtf16, (string->rawSizeUtf16 + 1/*null-terminator*/)*sizeof(u16), file, line);
    }
    /* and of course we should null-terminate the UTF-16 string */
    u16*const utf16 = KORL_C_CAST(u16*, context->characterPool + string->poolByteOffsetUtf16);
    utf16[currentUtf16] = L'\0';
}
korl_internal Korl_StringPool korl_stringPool_create(Korl_Memory_AllocatorHandle allocatorHandle)
{
    KORL_ZERO_STACK(Korl_StringPool, result);
    result.allocatorHandle     = allocatorHandle;
    result.nextStringHandle    = 1;
    result.characterPoolBytes  = korl_math_kilobytes(1);
    result.characterPool       = KORL_C_CAST(u8*, korl_allocate(result.allocatorHandle, result.characterPoolBytes));
    result.stringsCapacity     = 16;
    result.strings             = KORL_C_CAST(_Korl_StringPool_String*, korl_allocate(result.allocatorHandle, sizeof(*result.strings)     * result.stringsCapacity));
    result.allocationsCapacity = 16;
    result.allocations         = KORL_C_CAST(_Korl_StringPool_Allocation*, korl_allocate(result.allocatorHandle, sizeof(*result.allocations) * result.allocationsCapacity));
    korl_assert(result.characterPool);
    korl_assert(result.strings);
    korl_assert(result.allocations);
    return result;
}
korl_internal void korl_stringPool_destroy(Korl_StringPool* context)
{
    korl_free(context->allocatorHandle, context->strings);
    korl_free(context->allocatorHandle, context->allocations);
    korl_free(context->allocatorHandle, context->characterPool);
    korl_memory_zero(context, sizeof(*context));
}
korl_internal Korl_StringPool_StringHandle korl_stringPool_addFromUtf8(Korl_StringPool* context, const i8* cStringUtf8, const wchar_t* file, int line)
{
    return _korl_stringPool_addStringCommon(context, cStringUtf8, korl_checkCast_u$_to_u32(korl_memory_stringSizeUtf8(cStringUtf8)), _KORL_STRINGPOOL_STRING_FLAG_UTF8, file, line);
}
korl_internal Korl_StringPool_StringHandle korl_stringPool_addFromUtf16(Korl_StringPool* context, const u16* cStringUtf16, const wchar_t* file, int line)
{
    return _korl_stringPool_addStringCommon(context, cStringUtf16, korl_checkCast_u$_to_u32(korl_memory_stringSize(korl_checkCast_cpu16_to_cpwchar(cStringUtf16))), _KORL_STRINGPOOL_STRING_FLAG_UTF16, file, line);
}
korl_internal void korl_stringPool_remove(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle)
{
    /* find the matching handle in the string array */
    u$ s = 0;
    for(; s < context->stringsSize; s++)
        if(context->strings[s].handle == stringHandle)
            break;
    korl_assert(s);
    /* deallocate any raw string allocations */
    if(context->strings[s].flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16)
        _korl_stringPool_free(context, context->strings[s].poolByteOffsetUtf16);
    /* remove the string from the string array */
    context->strings[s] = context->strings[context->stringsSize - 1];
    context->stringsSize--;
}
korl_internal Korl_StringPool_CompareResult korl_stringPool_compareWithUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16)
{
    /* find the matching handle in the string array */
    u$ s = 0;
    for(; s < context->stringsSize; s++)
        if(context->strings[s].handle == stringHandle)
            break;
    korl_assert(s < context->stringsSize);
    /* if the utf16 version of the string hasn't been created, create it */
    if(!(context->strings[s].flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16))
    {
        korl_assert(context->strings[s].flags & _KORL_STRINGPOOL_STRING_FLAG_UTF8);// we need to convert from _something_; might as well ensure that UTF8 already exists
        _korl_stringPool_convert_utf8_to_utf16(context, &context->strings[s], __FILEW__, __LINE__);
    }
    /* do the raw string comparison */
    const int resultCompare = korl_memory_stringCompare(korl_checkCast_cpu16_to_cpwchar(KORL_C_CAST(u16*, context->characterPool + context->strings[s].poolByteOffsetUtf16)), 
                                                        korl_checkCast_cpu16_to_cpwchar(cStringUtf16));
    if(resultCompare == 0)
        return KORL_STRINGPOOL_COMPARE_RESULT_EQUAL;
    else if(resultCompare < 0)
        return KORL_STRINGPOOL_COMPARE_RESULT_LESS;
    else
        return KORL_STRINGPOOL_COMPARE_RESULT_GREATER;
}
korl_internal bool korl_stringPool_equalsUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16)
{
    return korl_stringPool_compareWithUtf16(context, stringHandle, cStringUtf16) == KORL_STRINGPOOL_COMPARE_RESULT_EQUAL;
}
korl_internal Korl_StringPool_CompareResult korl_stringPool_compareWithUtf8(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const char* cStringUtf8)
{
    /* find the matching handle in the string array */
    u$ s = 0;
    for(; s < context->stringsSize; s++)
        if(context->strings[s].handle == stringHandle)
            break;
    korl_assert(s < context->stringsSize);
    /* if the utf8 version of the string hasn't been created, create it */
    if(!(context->strings[s].flags & _KORL_STRINGPOOL_STRING_FLAG_UTF8))
    {
        korl_assert(!"not implemented!");
    }
    /* do the raw string comparison */
    const int resultCompare = korl_memory_stringCompareUtf8(korl_checkCast_cpu8_to_cpchar(context->characterPool + context->strings[s].poolByteOffsetUtf8), 
                                                            cStringUtf8);
    if(resultCompare == 0)
        return KORL_STRINGPOOL_COMPARE_RESULT_EQUAL;
    else if(resultCompare < 0)
        return KORL_STRINGPOOL_COMPARE_RESULT_LESS;
    else
        return KORL_STRINGPOOL_COMPARE_RESULT_GREATER;
}
korl_internal bool korl_stringPool_equalsUtf8(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const char* cStringUtf8)
{
    return korl_stringPool_compareWithUtf8(context, stringHandle, cStringUtf8) == KORL_STRINGPOOL_COMPARE_RESULT_EQUAL;
}
korl_internal const wchar_t* korl_stringPool_getRawUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle)
{
    /* find the matching handle in the string array */
    u$ s = 0;
    for(; s < context->stringsSize; s++)
        if(context->strings[s].handle == stringHandle)
            break;
    korl_assert(s < context->stringsSize);
    /* if the utf16 version of the string hasn't been created, create it */
    if(!(context->strings[s].flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16))
    {
        korl_assert(context->strings[s].flags & _KORL_STRINGPOOL_STRING_FLAG_UTF8);// we need to convert from _something_; might as well ensure that UTF8 already exists
        _korl_stringPool_convert_utf8_to_utf16(context, &context->strings[s], __FILEW__, __LINE__);
    }
    /**/
    return KORL_C_CAST(wchar_t*, context->characterPool + context->strings[s].poolByteOffsetUtf16);
}
