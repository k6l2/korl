#include "korl-stringPool.h"
#include "korl-checkCast.h"
typedef struct _Korl_StringPool_Allocation
{
    u32 poolByteOffset;
    u32 bytes;
    const wchar_t* file;
    int line;
} _Korl_StringPool_Allocation;
#define SORT_NAME korl_stringPool_allocation
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
typedef struct _Korl_StringPool_String
{
    Korl_StringPool_StringHandle handle;
    u32 characterCount;
    u32 poolByteOffsetUtf16;
    _Korl_StringPool_StringFlags flags;
} _Korl_StringPool_String;
/** \return the byte offset of the new allocation */
korl_internal u32 _korl_stringPool_allocate(Korl_StringPool* context, u$ bytes, const wchar_t* file, int line)
{
    korl_stringPool_allocation_quick_sort(context->allocations, context->allocationsSize);
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
korl_internal Korl_StringPool_StringHandle korl_stringPool_addFromUtf16(Korl_StringPool* context, const u16* cStringUtf16, const wchar_t* file, int line)
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
    newString->handle              = newHandle;
    newString->characterCount      = korl_checkCast_u$_to_u32(korl_memory_stringSize(korl_checkCast_cpu16_to_cpwchar(cStringUtf16)));
    newString->poolByteOffsetUtf16 = _korl_stringPool_allocate(context, (newString->characterCount + 1)*sizeof(*cStringUtf16), file, line);
    newString->flags               = _KORL_STRINGPOOL_STRING_FLAG_UTF16;
    korl_memory_copy(context->characterPool + newString->poolByteOffsetUtf16, cStringUtf16, newString->characterCount*sizeof(*cStringUtf16));
    return newHandle;
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
        ///@TODO: 
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
