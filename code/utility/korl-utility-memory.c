#include "utility/korl-utility-memory.h"
korl_internal bool _korl_memory_isBigEndian(void)
{
    korl_shared_const i32 I = 1;
    return KORL_C_CAST(const u8*const, &I)[0] == 0;
}
korl_internal u$ _korl_memory_packCommon(const u8* data, u$ dataBytes, u8** bufferCursor, const u8*const bufferEnd)
{
    if(*bufferCursor + dataBytes > bufferEnd)
        return 0;
    if(_korl_memory_isBigEndian())
        korl_assert(!"big-endian not supported");
    else
        korl_memory_copy(*bufferCursor, data, dataBytes);
    *bufferCursor += dataBytes;
    return dataBytes;
}
korl_internal u$ _korl_memory_unpackCommon(void* unpackedData, u$ unpackedDataBytes, const u8** bufferCursor, const u8*const bufferEnd)
{
    if(*bufferCursor + unpackedDataBytes > bufferEnd)
        return 0;
    if(_korl_memory_isBigEndian())
        korl_assert(!"big-endian not supported");
    else
        korl_memory_copy(unpackedData, *bufferCursor, unpackedDataBytes);
    *bufferCursor += unpackedDataBytes;
    return unpackedDataBytes;
}
korl_internal bool korl_memory_isLittleEndian(void)
{
    return !_korl_memory_isBigEndian();
}
korl_internal u$ korl_memory_packStringI8(const i8* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, data), dataSize*sizeof(*data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packStringU16(const u16* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, data), dataSize*sizeof(*data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU$ (u$  data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU64(u64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU32(u32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU16(u16 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packU8(u8 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(&data, sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packI64(i64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packI32(i32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packI16(i16 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packI8(i8 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packF64(f64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_packF32(f32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_packCommon(KORL_C_CAST(u8*, &data), sizeof(data), bufferCursor, bufferEnd);
}
korl_internal u$ korl_memory_unpackStringI8(i8* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(data, dataSize*sizeof(*data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackStringU16(u16* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(data, dataSize*sizeof(*data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackU$(u$ data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackU64(u64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackU32(u32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackU16(u16 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackU8 (u8  data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackI64(i64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackI32(i32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackI16(i16 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackI8 (i8  data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackF64(f64 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal u$ korl_memory_unpackF32(f32 data, u8** bufferCursor, const u8*const bufferEnd)
{
    return _korl_memory_unpackCommon(&data, sizeof(data), KORL_C_CAST(const u8**, bufferCursor), bufferEnd);
}
korl_internal Korl_Memory_ByteBuffer* korl_memory_byteBuffer_create(Korl_Memory_AllocatorHandle allocator, u$ capacity, bool fastAndDirty)
{
    korl_assert(capacity);
    Korl_Memory_ByteBuffer* result = fastAndDirty 
                                     ? KORL_C_CAST(Korl_Memory_ByteBuffer*, korl_dirtyAllocate(allocator, sizeof(*result) - sizeof(result->data) + capacity))
                                     : KORL_C_CAST(Korl_Memory_ByteBuffer*, korl_allocate     (allocator, sizeof(*result) - sizeof(result->data) + capacity));
    *result = KORL_STRUCT_INITIALIZE(Korl_Memory_ByteBuffer){.allocator = allocator, .capacity = capacity, .fastAndDirty = fastAndDirty};
    return result;
}
korl_internal void korl_memory_byteBuffer_destroy(Korl_Memory_ByteBuffer** pContext)
{
    if(*pContext)// we can't obtain the ByteBuffer's allocator without a ByteBuffer lol
        if((*pContext)->fastAndDirty)
            korl_dirtyFree((*pContext)->allocator, *pContext);
        else
            korl_free((*pContext)->allocator, *pContext);
    *pContext = NULL;
}
korl_internal void korl_memory_byteBuffer_append(Korl_Memory_ByteBuffer** pContext, acu8 data)
{
    if((*pContext)->size + data.size > (*pContext)->capacity)
    {
        (*pContext)->capacity = KORL_MATH_MAX(2 * (*pContext)->capacity, (*pContext)->size + data.size);
        if((*pContext)->fastAndDirty)
            *pContext = KORL_C_CAST(Korl_Memory_ByteBuffer*, korl_dirtyReallocate((*pContext)->allocator, *pContext, sizeof(**pContext) - sizeof((*pContext)->data) + (*pContext)->capacity));
        else
            *pContext = KORL_C_CAST(Korl_Memory_ByteBuffer*, korl_reallocate((*pContext)->allocator, *pContext, sizeof(**pContext) - sizeof((*pContext)->data) + (*pContext)->capacity));
    }
    korl_memory_copy((*pContext)->data + (*pContext)->size, data.data, data.size);
    (*pContext)->size += data.size;
}
korl_internal void korl_memory_byteBuffer_trim(Korl_Memory_ByteBuffer** pContext)
{
    if((*pContext)->size == (*pContext)->capacity)
        return;
    (*pContext)->capacity = (*pContext)->size;
    if((*pContext)->fastAndDirty)
        *pContext = KORL_C_CAST(Korl_Memory_ByteBuffer*, korl_dirtyReallocate((*pContext)->allocator, *pContext, sizeof(**pContext) - sizeof((*pContext)->data) + (*pContext)->capacity));
    else
        *pContext = KORL_C_CAST(Korl_Memory_ByteBuffer*, korl_reallocate((*pContext)->allocator, *pContext, sizeof(**pContext) - sizeof((*pContext)->data) + (*pContext)->capacity));
}
korl_internal int korl_memory_compare_acu8(acu8 a, acu8 b)
{
    return a.size < b.size ? -1
           : a.size > b.size ? 1
             : korl_memory_compare(a.data, b.data, a.size);
}
korl_internal int korl_memory_compare_acu16(acu16 a, acu16 b)
{
    if(a.size < b.size)
        return -1;
    else if(a.size > b.size)
        return 1;
    else
        for(u$ i = 0; i < a.size; i++)
            if(a.data[i] < b.data[i])
                return -1;
            else if(a.data[i] > b.data[i])
                return 1;
    return 0;
}
korl_internal bool korl_memory_isNull(const void* p, size_t bytes)
{
    const void*const pEnd = KORL_C_CAST(const u8*, p) + bytes;
    for(; p != pEnd; p = KORL_C_CAST(const u8*, p) + 1)
        if(*KORL_C_CAST(const u8*, p))
            return false;
    return true;
}
