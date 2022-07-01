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
/* C++ has some cool features, and this is not one of them.  
    It's honestly really stupid that I have to define these operators myself. */
korl_internal _Korl_StringPool_StringFlags operator|(_Korl_StringPool_StringFlags a, _Korl_StringPool_StringFlags b)
{
    return static_cast<_Korl_StringPool_StringFlags>(static_cast<int>(a) | static_cast<int>(b));
}
korl_internal _Korl_StringPool_StringFlags operator&(_Korl_StringPool_StringFlags a, _Korl_StringPool_StringFlags b)
{
    return static_cast<_Korl_StringPool_StringFlags>(static_cast<int>(a) & static_cast<int>(b));
}
inline _Korl_StringPool_StringFlags& operator|=(_Korl_StringPool_StringFlags& a, _Korl_StringPool_StringFlags b)
{
    return a = a | b;
}
inline _Korl_StringPool_StringFlags& operator&=(_Korl_StringPool_StringFlags& a, _Korl_StringPool_StringFlags b)
{
    return a = a & b;
}
inline _Korl_StringPool_StringFlags operator~(_Korl_StringPool_StringFlags a)
{
    return static_cast<_Korl_StringPool_StringFlags>(~static_cast<u$>(a));
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
    korl_assert(bytes > 0);
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
    korl_assert(bytes > 0);
    _korl_stringPool_allocation_quick_sort(context->allocations, context->allocationsSize);
    /* find the allocation matching the provided byte offset */
    u$ allocIndex = 0;
    for(; allocIndex < context->allocationsSize; allocIndex++)
        if(context->allocations[allocIndex].poolByteOffset == allocationOffset)
            break;
    korl_assert(allocIndex < context->allocationsSize);
    _Korl_StringPool_Allocation* allocationPrevious = &(context->allocations[allocIndex]);
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
    allocationPrevious = &(context->allocations[allocIndex]);// allocating can invalidate all pointers, so we _must_ recalculate this!
    korl_memory_copy(context->characterPool + newAllocOffset, 
                     context->characterPool + allocationPrevious->poolByteOffset, 
                     allocationPrevious->bytes);
    // this will release the previous allocation (no need to call free) //
    context->allocations[allocIndex] = context->allocations[context->allocationsSize - 1];
    context->allocationsSize--;
    // //
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
korl_internal _Korl_StringPool_String* _korl_stringPool_addNewString(Korl_StringPool* context, _Korl_StringPool_StringFlags stringFlags)
{
    /* find a unique handle for the new string */
    korl_assert(context->nextStringHandle != 0);
    const u32 maxHandles = KORL_U32_MAX - 1/*since 0 is an invalid handle*/;
    korl_assert(context->stringsSize < maxHandles);// ensure there is at least one possible unused valid handle value for a new string
    Korl_StringPool_StringHandle newHandle = 0;
    /* iterate over all possible handle values until we find the unused handle value */
    for(Korl_StringPool_StringHandle h = 0; h < maxHandles; h++)
    {
        newHandle = context->nextStringHandle;
        /* If another string has this handle, we nullify the newHandle so that 
            we can move on to the next handle candidate */
        for(u$ s = 0; s < context->stringsSize; s++)
            if(context->strings[s].handle == newHandle)
            {
                newHandle = 0;
                break;
            }
        if(context->nextStringHandle == KORL_U32_MAX)
            context->nextStringHandle = 1;
        else
            context->nextStringHandle++;
        if(newHandle)
            break;
    }
    korl_assert(newHandle);// sanity check
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
    return newString;
}
korl_internal Korl_StringPool_StringHandle _korl_stringPool_stringFromRawCommon(Korl_StringPool* context, const void* cString, u32 cStringSize, _Korl_StringPool_StringFlags stringFlags, const wchar_t* file, int line)
{
    _Korl_StringPool_String*const newString = _korl_stringPool_addNewString(context, stringFlags);
    u$ characterBytes   = 0;
    u32* poolByteOffset = NULL;
    u32* rawSize        = NULL;
    if(stringFlags == _KORL_STRINGPOOL_STRING_FLAG_UTF8)
    {
        characterBytes = 1;
        poolByteOffset = &(newString->poolByteOffsetUtf8);
        rawSize        = &(newString->rawSizeUtf8);
    }
    else if(stringFlags == _KORL_STRINGPOOL_STRING_FLAG_UTF16)
    {
        characterBytes = 2;
        poolByteOffset = &(newString->poolByteOffsetUtf16);
        rawSize        = &(newString->rawSizeUtf16);
    }
    else
        korl_log(ERROR, "string flags 0x%X not implemented", stringFlags);
    korl_assert(characterBytes);
    korl_assert(poolByteOffset);
    korl_assert(rawSize);
    *rawSize        = cStringSize;
    *poolByteOffset = _korl_stringPool_allocate(context, ((*rawSize) + 1/*for null terminator*/)*characterBytes, file, line);
    if(cString)
        korl_memory_copy(context->characterPool + *poolByteOffset, cString, (*rawSize)*characterBytes);
    else
        // clear the entire string //
        korl_memory_zero(context->characterPool + *poolByteOffset, (*rawSize)*characterBytes);
    // apply null termination:
    korl_memory_zero(context->characterPool + *poolByteOffset + (*rawSize)*characterBytes, characterBytes);
    return newString->handle;
}
korl_internal void _korl_stringPool_convert_utf16_to_utf8(Korl_StringPool* context, _Korl_StringPool_String* string, const wchar_t* file, int line)
{
    korl_assert(  string->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16);
    korl_assert(!(string->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF8));
    korl_assert(!"not implemented");
}
korl_internal void _korl_stringPool_convert_utf8_to_utf16(Korl_StringPool* context, _Korl_StringPool_String* string, const wchar_t* file, int line)
{
    korl_assert(  string->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF8);
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
        const u8*const utf8  =                   context->characterPool + string->poolByteOffsetUtf8;
        u16*const      utf16 = KORL_C_CAST(u16*, context->characterPool + string->poolByteOffsetUtf16);
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
korl_internal void _korl_stringPool_setStringFlags(Korl_StringPool* context, _Korl_StringPool_String* string, _Korl_StringPool_StringFlags flags)
{
    /* if we're removing any given encoding flag, we need to free the raw string 
        allocation for that encoding */
    if(    (string->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF8)
       && !(        flags & _KORL_STRINGPOOL_STRING_FLAG_UTF8))
    {
        _korl_stringPool_free(context, string->poolByteOffsetUtf8);
        string->poolByteOffsetUtf8 = 0;
        string->rawSizeUtf8        = 0;
        string->flags             &= ~_KORL_STRINGPOOL_STRING_FLAG_UTF8;
    }
    if(    (string->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16)
       && !(        flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16))
    {
        _korl_stringPool_free(context, string->poolByteOffsetUtf16);
        string->poolByteOffsetUtf16 = 0;
        string->rawSizeUtf16        = 0;
        string->flags              &= ~_KORL_STRINGPOOL_STRING_FLAG_UTF16;
    }
    if(string->flags & ~flags)// if the string flags still have bits that aren't present in the desired flag set
        korl_log(ERROR, "not all string encodings are implemented! flags=%i", string->flags);
    string->flags = flags;
}
korl_internal u$ _korl_stringPool_findIndexMatchingHandle(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle)
{
    if(stringHandle == 0)
        return context->stringsSize;
    u$ s = 0;
    for(; s < context->stringsSize; s++)
        if(context->strings[s].handle == stringHandle)
            break;
    return s;
}
korl_internal void _korl_stringPool_deduceUtf8(Korl_StringPool* context, _Korl_StringPool_String* string, const wchar_t* file, int line)
{
    if(!(string->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF8))
    {
        korl_assert(string->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16);
        _korl_stringPool_convert_utf16_to_utf8(context, string, file, line);
    }
}
/** If the provided string does not contain the encoding specified by flags, the 
 * other string encodings will be derived from the string's UTF-8 string.  If 
 * the string doesn't have UTF-8 encoding, it will be deduced from other 
 * encodings possessed by the string.  Inability to perform this deduction 
 * should be considered a fatal error. */
korl_internal void _korl_stringPool_deduceEncoding(Korl_StringPool* context, _Korl_StringPool_String* string, _Korl_StringPool_StringFlags flags, const wchar_t* file, int line)
{
    if(    (flags         & _KORL_STRINGPOOL_STRING_FLAG_UTF16)
       && !(string->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16))
    {
        _korl_stringPool_deduceUtf8(context, string, file, line);
        _korl_stringPool_convert_utf8_to_utf16(context, string, file, line);
    }
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
    //KORL-ISSUE-000-000-012: add a cleanup function to verify that there are not memory leaks (ensure that strings & allocations are empty)
    korl_free(context->allocatorHandle, context->strings);
    korl_free(context->allocatorHandle, context->allocations);
    korl_free(context->allocatorHandle, context->characterPool);
    korl_memory_zero(context, sizeof(*context));
}
korl_internal Korl_StringPool_StringHandle korl_stringPool_newFromUtf8(Korl_StringPool* context, const i8* cStringUtf8, const wchar_t* file, int line)
{
    return _korl_stringPool_stringFromRawCommon(context, cStringUtf8, 
                                                korl_checkCast_u$_to_u32(korl_memory_stringSizeUtf8(cStringUtf8)), 
                                                _KORL_STRINGPOOL_STRING_FLAG_UTF8, 
                                                file, line);
}
korl_internal Korl_StringPool_StringHandle korl_stringPool_newFromUtf16(Korl_StringPool* context, const u16* cStringUtf16, const wchar_t* file, int line)
{
    return _korl_stringPool_stringFromRawCommon(context, cStringUtf16, 
                                                korl_checkCast_u$_to_u32(korl_memory_stringSize(korl_checkCast_cpu16_to_cpwchar(cStringUtf16))), 
                                                _KORL_STRINGPOOL_STRING_FLAG_UTF16, 
                                                file, line);
}
korl_internal Korl_StringPool_StringHandle korl_stringPool_newEmptyUtf8(Korl_StringPool* context, u32 reservedSizeExcludingNullTerminator, const wchar_t* file, int line)
{
    return _korl_stringPool_stringFromRawCommon(context, NULL/*just leave the string empty*/, 
                                                reservedSizeExcludingNullTerminator, 
                                                _KORL_STRINGPOOL_STRING_FLAG_UTF8, 
                                                file, line);
}
korl_internal Korl_StringPool_StringHandle korl_stringPool_newEmptyUtf16(Korl_StringPool* context, u32 reservedSizeExcludingNullTerminator, const wchar_t* file, int line)
{
    return _korl_stringPool_stringFromRawCommon(context, NULL/*just leave the string empty*/, 
                                                reservedSizeExcludingNullTerminator, 
                                                _KORL_STRINGPOOL_STRING_FLAG_UTF16, 
                                                file, line);
}
korl_internal void korl_stringPool_free(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle)
{
    /* if the string handle is invalid, we don't have to do anything */
    if(!stringHandle)
        return;
    /* find the matching handle in the string array */
    u$ s = 0;
    for(; s < context->stringsSize; s++)
        if(context->strings[s].handle == stringHandle)
            break;
    korl_assert(s < context->stringsSize);
    /* deallocate any raw string allocations */
    if(context->strings[s].flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16)
        _korl_stringPool_free(context, context->strings[s].poolByteOffsetUtf16);
    /* remove the string from the string array */
    context->strings[s] = context->strings[context->stringsSize - 1];
    context->stringsSize--;
}
korl_internal void korl_stringPool_reserveUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, u32 reservedSizeExcludingNullTerminator, const wchar_t* file, int line)
{
    if(!stringHandle)
        return;// if the string handle is invalid, we don't have to do anything
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    /* if there are other raw strings representing this string pool entry, we 
        need to invalidate them since those strings will have to be re-encoded */
    _korl_stringPool_setStringFlags(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF16);
    /* perform a reallocation of the raw string */
    context->strings[s].rawSizeUtf16        = reservedSizeExcludingNullTerminator;
    context->strings[s].poolByteOffsetUtf16 = _korl_stringPool_reallocate(context, context->strings[s].poolByteOffsetUtf16, (context->strings[s].rawSizeUtf16 + 1/*null terminator*/)*sizeof(u16), file, line);
    /* make sure the raw string is null-terminated properly */
    u16*const rawUtf8 = KORL_C_CAST(u16*, context->characterPool + context->strings[s].poolByteOffsetUtf16);
    rawUtf8[context->strings[s].rawSizeUtf16] = L'\0';
}
korl_internal Korl_StringPool_CompareResult korl_stringPool_compare(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandleA, Korl_StringPool_StringHandle stringHandleB)
{
    /* find the matching handles in the string array */
    const u$ sA = _korl_stringPool_findIndexMatchingHandle(context, stringHandleA);
    korl_assert(sA < context->stringsSize);
    const u$ sB = _korl_stringPool_findIndexMatchingHandle(context, stringHandleB);
    korl_assert(sB < context->stringsSize);
    /* make sure both strings have UTF-8 */
    _korl_stringPool_deduceUtf8(context, &(context->strings[sA]), __FILEW__, __LINE__);
    _korl_stringPool_deduceUtf8(context, &(context->strings[sB]), __FILEW__, __LINE__);
    /* do the raw string comparison */
    const int resultCompare = korl_memory_arrayU8Compare(context->characterPool + context->strings[sA].poolByteOffsetUtf8, 
                                                         context->strings[sA].rawSizeUtf8, 
                                                         context->characterPool + context->strings[sB].poolByteOffsetUtf8, 
                                                         context->strings[sB].rawSizeUtf8);
    if(resultCompare == 0)
        return KORL_STRINGPOOL_COMPARE_RESULT_EQUAL;
    else if(resultCompare < 0)
        return KORL_STRINGPOOL_COMPARE_RESULT_LESS;
    else
        return KORL_STRINGPOOL_COMPARE_RESULT_GREATER;
}
korl_internal Korl_StringPool_CompareResult korl_stringPool_compareWithUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16)
{
    /* find the matching handle in the string array */
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
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
korl_internal Korl_StringPool_CompareResult korl_stringPool_compareWithUtf8(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const char* cStringUtf8)
{
    /* find the matching handle in the string array */
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    _korl_stringPool_deduceUtf8(context, &(context->strings[s]), __FILEW__, __LINE__);
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
korl_internal bool korl_stringPool_equals(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandleA, Korl_StringPool_StringHandle stringHandleB)
{
    return KORL_STRINGPOOL_COMPARE_RESULT_EQUAL == korl_stringPool_compare(context, stringHandleA, stringHandleB);
}
korl_internal bool korl_stringPool_equalsUtf8(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const char* cStringUtf8)
{
    return KORL_STRINGPOOL_COMPARE_RESULT_EQUAL == korl_stringPool_compareWithUtf8(context, stringHandle, cStringUtf8);
}
korl_internal bool korl_stringPool_equalsUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16)
{
    return KORL_STRINGPOOL_COMPARE_RESULT_EQUAL == korl_stringPool_compareWithUtf16(context, stringHandle, cStringUtf16);
}
korl_internal const char* korl_stringPool_getRawUtf8(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    /* if the utf8 version of the string hasn't been created, create it */
    _korl_stringPool_deduceEncoding(context, &context->strings[s], _KORL_STRINGPOOL_STRING_FLAG_UTF8, __FILEW__, __LINE__);
    return KORL_C_CAST(char*, context->characterPool + context->strings[s].poolByteOffsetUtf8);
}
korl_internal const wchar_t* korl_stringPool_getRawUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    /* if the utf16 version of the string hasn't been created, create it */
    _korl_stringPool_deduceEncoding(context, &context->strings[s], _KORL_STRINGPOOL_STRING_FLAG_UTF16, __FILEW__, __LINE__);
    return KORL_C_CAST(wchar_t*, context->characterPool + context->strings[s].poolByteOffsetUtf16);
}
korl_internal u32 korl_stringPool_getRawSizeUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    return context->strings[s].rawSizeUtf16;
}
korl_internal wchar_t* korl_stringPool_getRawWriteableUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    return korl_checkCast_pu16_to_pwchar(KORL_C_CAST(u16*, context->characterPool + context->strings[s].poolByteOffsetUtf16));
}
korl_internal Korl_StringPool_StringHandle korl_stringPool_copy(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const wchar_t* file, int line)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    _Korl_StringPool_String*const newString = _korl_stringPool_addNewString(context, context->strings[s].flags);
    newString->rawSizeUtf8  = context->strings[s].rawSizeUtf8;
    newString->rawSizeUtf16 = context->strings[s].rawSizeUtf16;
    _Korl_StringPool_StringFlags flagsAdded = _KORL_STRINGPOOL_STRING_FLAGS_NONE;
    if(newString->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF8)
    {
        flagsAdded |= _KORL_STRINGPOOL_STRING_FLAG_UTF8;
        newString->poolByteOffsetUtf8 = _korl_stringPool_allocate(context, newString->rawSizeUtf8*sizeof(u8), file, line);
        korl_memory_copy(context->characterPool + newString->poolByteOffsetUtf8, 
                         context->characterPool + context->strings[s].poolByteOffsetUtf8, 
                         newString->rawSizeUtf8*sizeof(u8));
        // apply null terminator //
        u8*const newUtf8 = context->characterPool + newString->poolByteOffsetUtf8;
        newUtf8[newString->rawSizeUtf8] = '\0';
    }
    if(newString->flags & _KORL_STRINGPOOL_STRING_FLAG_UTF16)
    {
        flagsAdded |= _KORL_STRINGPOOL_STRING_FLAG_UTF16;
        newString->poolByteOffsetUtf16 = _korl_stringPool_allocate(context, newString->rawSizeUtf16*sizeof(u16), file, line);
        korl_memory_copy(context->characterPool + newString->poolByteOffsetUtf16, 
                         context->characterPool + context->strings[s].poolByteOffsetUtf16, 
                         newString->rawSizeUtf16*sizeof(u16));
        // apply null terminator //
        u16*const newUtf16 = KORL_C_CAST(u16*, context->characterPool + newString->poolByteOffsetUtf16);
        newUtf16[newString->rawSizeUtf16] = L'\0';
    }
    if(flagsAdded != newString->flags)
        korl_log(ERROR, "not all string flags implemented! string flags=0x%X", newString->flags);
    return newString->handle;
}
korl_internal void korl_stringPool_append(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, Korl_StringPool_StringHandle stringHandleToAppend, const wchar_t* file, int line)
{
    const u$ s0 = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s0 < context->stringsSize);
    const u$ s1 = _korl_stringPool_findIndexMatchingHandle(context, stringHandleToAppend);
    korl_assert(s1 < context->stringsSize);
    _Korl_StringPool_String*const str0 = &(context->strings[s0]);
    _Korl_StringPool_String*const str1 = &(context->strings[s1]);
    /* for each matching type of raw string between the two string pool entries, 
        copy the raw string from s1 onto the end of s0 */
    _Korl_StringPool_StringFlags flagsS0 = str0->flags;
    _Korl_StringPool_StringFlags flagsS1 = str1->flags;
    _Korl_StringPool_StringFlags flagsMatched = _KORL_STRINGPOOL_STRING_FLAGS_NONE;
    if(   flagsS0 & _KORL_STRINGPOOL_STRING_FLAG_UTF8
       && flagsS1 & _KORL_STRINGPOOL_STRING_FLAG_UTF8)
    {
        flagsS0 &= ~_KORL_STRINGPOOL_STRING_FLAG_UTF8;
        flagsS1 &= ~_KORL_STRINGPOOL_STRING_FLAG_UTF8;
        flagsMatched |= _KORL_STRINGPOOL_STRING_FLAG_UTF8;
        str0->poolByteOffsetUtf8 = _korl_stringPool_reallocate(context, str0->poolByteOffsetUtf8, (str0->rawSizeUtf8 + str1->rawSizeUtf8 + 1/*null terminator*/)*sizeof(u8), file, line);
        korl_memory_copy(context->characterPool + str0->poolByteOffsetUtf8 + str0->rawSizeUtf8*sizeof(u8), 
                         context->characterPool + str1->poolByteOffsetUtf8, 
                         (str1->rawSizeUtf8 + 1/*include the null terminator*/)*sizeof(u8));
        str0->rawSizeUtf8 += str1->rawSizeUtf8;
    }
    if(   flagsS0 & _KORL_STRINGPOOL_STRING_FLAG_UTF16
       && flagsS1 & _KORL_STRINGPOOL_STRING_FLAG_UTF16)
    {
        flagsS0 &= ~_KORL_STRINGPOOL_STRING_FLAG_UTF16;
        flagsS1 &= ~_KORL_STRINGPOOL_STRING_FLAG_UTF16;
        flagsMatched |= _KORL_STRINGPOOL_STRING_FLAG_UTF16;
        str0->poolByteOffsetUtf16 = _korl_stringPool_reallocate(context, str0->poolByteOffsetUtf16, (str0->rawSizeUtf16 + str1->rawSizeUtf16 + 1/*null terminator*/)*sizeof(u16), file, line);
        korl_memory_copy(context->characterPool + str0->poolByteOffsetUtf16 + str0->rawSizeUtf16*sizeof(u16), 
                         context->characterPool + str1->poolByteOffsetUtf16, 
                         (str1->rawSizeUtf16 + 1/*include the null terminator*/)*sizeof(u16));
        str0->rawSizeUtf16 += str1->rawSizeUtf16;
    }
    if(flagsS0 & flagsS1)
        korl_log(ERROR, "not all string flags implemented! flagsS0=0x%X flagsS1=0x%X", flagsS0, flagsS1);
    /* if there were no matching raw string types, perform a UTF-8 deduction of 
        both strings, and perform the operation in UTF-8 encoding */
    if(flagsMatched)
        _korl_stringPool_setStringFlags(context, str0, flagsMatched);
    else
    {
        _korl_stringPool_deduceUtf8(context, str0, file, line);
        _korl_stringPool_deduceUtf8(context, str1, file, line);
        /* now that we know both strings have UTF-8, we can do the same UTF-8 
            append operation as the above code */
        str0->poolByteOffsetUtf8 = _korl_stringPool_reallocate(context, str0->poolByteOffsetUtf8, (str0->rawSizeUtf8 + str1->rawSizeUtf8 + 1/*null terminator*/)*sizeof(u8), file, line);
        korl_memory_copy(context->characterPool + str0->poolByteOffsetUtf8 + str0->rawSizeUtf8*sizeof(u8), 
                         context->characterPool + str1->poolByteOffsetUtf8, 
                         (str1->rawSizeUtf8 + 1/*include the null terminator*/)*sizeof(u8));
        str0->rawSizeUtf8 += str1->rawSizeUtf8;
    }
}
korl_internal void korl_stringPool_appendUtf8(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const i8* cStringUtf8, const wchar_t* file, int line)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    /* get UTF-8 encoding if we don't already have it */
    _korl_stringPool_deduceEncoding(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF8, file, line);
    /* invalidate other encodings, since we're just going to perform that work 
        on the UTF-8 raw string */
    _korl_stringPool_setStringFlags(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF8);
    /* actually perform the append */
    const u$ additionalStringSize = korl_memory_stringSizeUtf8(cStringUtf8);
    _Korl_StringPool_String*const str = &(context->strings[s]);
    str->poolByteOffsetUtf8 = _korl_stringPool_reallocate(context, str->poolByteOffsetUtf8, (str->rawSizeUtf8 + additionalStringSize + 1/*null terminator*/)*sizeof(u8), file, line);
    korl_memory_copy(context->characterPool + str->poolByteOffsetUtf8 + str->rawSizeUtf8*sizeof(u8), 
                     cStringUtf8, 
                     (additionalStringSize + 1/*include the null terminator*/)*sizeof(u8));
    str->rawSizeUtf8 += korl_checkCast_u$_to_u32(additionalStringSize);
}
korl_internal void korl_stringPool_appendUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16, const wchar_t* file, int line)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    /* get UTF-16 encoding if we don't already have it */
    _korl_stringPool_deduceEncoding(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF16, file, line);
    /* invalidate other encodings, since we're just going to perform that work 
        on the UTF-16 raw string */
    _korl_stringPool_setStringFlags(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF16);
    /* actually perform the append */
    const u$ additionalStringSize = korl_memory_stringSize(korl_checkCast_cpu16_to_cpwchar(cStringUtf16));
    _Korl_StringPool_String*const str = &(context->strings[s]);
    str->poolByteOffsetUtf16 = _korl_stringPool_reallocate(context, str->poolByteOffsetUtf16, (str->rawSizeUtf16 + additionalStringSize + 1/*null terminator*/)*sizeof(u16), file, line);
    korl_memory_copy(context->characterPool + str->poolByteOffsetUtf16 + str->rawSizeUtf16*sizeof(u16), 
                     cStringUtf16, 
                     (additionalStringSize + 1/*include the null terminator*/)*sizeof(u16));
    str->rawSizeUtf16 += korl_checkCast_u$_to_u32(additionalStringSize);
}
korl_internal void korl_stringPool_appendFormatUtf8(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const wchar_t* file, int line, const char* format, ...)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    /* get UTF-8 encoding if we don't already have it */
    _korl_stringPool_deduceEncoding(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF8, file, line);
    /* invalidate other encodings, since we're just going to perform that work 
        on the UTF-8 raw string */
    _korl_stringPool_setStringFlags(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF8);
    /* create the formatted string */
    va_list args;
    va_start(args, format);
    char*const cStringFormat = korl_memory_stringFormatVaListUtf8(context->allocatorHandle, format, args);
    va_end(args);
    /* perform the append */
    const u$ additionalStringSize = korl_memory_stringSizeUtf8(cStringFormat);
    _Korl_StringPool_String*const str = &(context->strings[s]);
    str->poolByteOffsetUtf8 = _korl_stringPool_reallocate(context, str->poolByteOffsetUtf8, (str->rawSizeUtf8 + additionalStringSize + 1/*null terminator*/)*sizeof(u8), file, line);
    korl_memory_copy(context->characterPool + str->poolByteOffsetUtf8 + str->rawSizeUtf8*sizeof(u8), 
                     cStringFormat, 
                     (additionalStringSize + 1/*include the null terminator*/)*sizeof(u8));
    str->rawSizeUtf8 += korl_checkCast_u$_to_u32(additionalStringSize);
    /* clean up the allocated format string */
    korl_free(context->allocatorHandle, cStringFormat);
}
korl_internal void korl_stringPool_appendFormatUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const wchar_t* file, int line, const wchar_t* format, ...)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    /* get UTF-16 encoding if we don't already have it */
    _korl_stringPool_deduceEncoding(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF16, file, line);
    /* invalidate other encodings, since we're just going to perform that work 
        on the UTF-16 raw string */
    _korl_stringPool_setStringFlags(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF16);
    /* create the formatted string */
    va_list args;
    va_start(args, format);
    wchar_t*const cStringFormat = korl_memory_stringFormatVaList(context->allocatorHandle, format, args);
    va_end(args);
    /* perform the append */
    const u$ additionalStringSize = korl_memory_stringSize(cStringFormat);
    _Korl_StringPool_String*const str = &(context->strings[s]);
    str->poolByteOffsetUtf16 = _korl_stringPool_reallocate(context, str->poolByteOffsetUtf16, (str->rawSizeUtf16 + additionalStringSize + 1/*null terminator*/)*sizeof(u16), file, line);
    korl_memory_copy(context->characterPool + str->poolByteOffsetUtf16 + str->rawSizeUtf16*sizeof(u16), 
                     cStringFormat, 
                     (additionalStringSize + 1/*include the null terminator*/)*sizeof(u16));
    str->rawSizeUtf16 += korl_checkCast_u$_to_u32(additionalStringSize);
    /* clean up the allocated format string */
    korl_free(context->allocatorHandle, cStringFormat);
}
korl_internal void korl_stringPool_prependUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16, const wchar_t* file, int line)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    /* get UTF-16 encoding if we don't already have it */
    _korl_stringPool_deduceEncoding(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF16, file, line);
    /* invalidate other encodings, since we're just going to perform that work 
        on the UTF-16 raw string */
    _korl_stringPool_setStringFlags(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF16);
    /* grow the string, move it, and copy the c-string to the beginning */
    const u$ additionalStringSize = korl_memory_stringSize(korl_checkCast_cpu16_to_cpwchar(cStringUtf16));
    _Korl_StringPool_String*const str = &(context->strings[s]);
    str->poolByteOffsetUtf16 = _korl_stringPool_reallocate(context, str->poolByteOffsetUtf16, (str->rawSizeUtf16 + additionalStringSize + 1/*null terminator*/)*sizeof(u16), file, line);
    korl_memory_move(context->characterPool + str->poolByteOffsetUtf16 + additionalStringSize*sizeof(u16), 
                     context->characterPool + str->poolByteOffsetUtf16, 
                     (str->rawSizeUtf16 + 1/*include the null terminator*/)*sizeof(u16));
    str->rawSizeUtf16 += korl_checkCast_u$_to_u32(additionalStringSize);
    korl_memory_copy(context->characterPool + str->poolByteOffsetUtf16, 
                     cStringUtf16, 
                     additionalStringSize*sizeof(u16));
}
korl_internal u32 korl_stringPool_findUtf16(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle, const u16* cStringUtf16, u32 characterOffsetStart, const wchar_t* file, int line)
{
    const u$ s = _korl_stringPool_findIndexMatchingHandle(context, stringHandle);
    korl_assert(s < context->stringsSize);
    _korl_stringPool_deduceEncoding(context, &(context->strings[s]), _KORL_STRINGPOOL_STRING_FLAG_UTF16, file, line);
    /* Example; searching the string "testString" for the substring "ring":
        Final iteration:
        [ t][ e][ s][ t][ S][ t][ r][ i][ n][ g][\0]
          0   1   2   3   4   5   6   7   8   9  10
                                [ r][ i][ n][ g][\0]
                                  0   1   2   3   4 */
    const _Korl_StringPool_String*const str = &(context->strings[s]);
    const u$ searchStringSize = korl_memory_stringSize(korl_checkCast_cpu16_to_cpwchar(cStringUtf16));
    if(searchStringSize > str->rawSizeUtf16)
        return str->rawSizeUtf16;// the search string can't even fit inside this string
    for(u32 i = 0; i <= str->rawSizeUtf16 - searchStringSize; i++)
        if(0 == korl_memory_arrayU16Compare(KORL_C_CAST(u16*, context->characterPool + str->poolByteOffsetUtf16 + i*sizeof(u16)), 
                                            searchStringSize, 
                                            cStringUtf16, 
                                            searchStringSize))
            return i;
    return str->rawSizeUtf16;
}
