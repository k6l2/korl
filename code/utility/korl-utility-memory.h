#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-memory.h"
korl_internal bool korl_memory_isLittleEndian(void);
/** These memory (un)pack API can be used to quickly store & retrieve data 
 * to/from a memory buffer in a platform-agnostic way.  For example, if machine 
 * A packs a 64-bit integer into a memory buffer, and sends this data to machine 
 * B over the network or via a file saved to disk, machine B should be able to 
 * successfully unpack and read the data correctly using the same API. */
/** \param dataSize _not_ including the null-terminator */
korl_internal u$ korl_memory_packStringI8(const i8* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd);
/** \param dataSize _not_ including the null-terminator */
korl_internal u$ korl_memory_packStringU16(const u16* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU$ (u$  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU64(u64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU32(u32 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU16(u16 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packU8 (u8  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packI64(i64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packI32(i32 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packI16(i16 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packI8 (i8  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packF64(f64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_packF32(f32 data, u8** bufferCursor, const u8*const bufferEnd);
/** The caller is responsible for null-terminating the string. 
 * \param dataSize _not_ including the null-terminator required for the string */
korl_internal u$ korl_memory_unpackStringI8(i8* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd);
/** The caller is responsible for null-terminating the string. 
 * \param dataSize _not_ including the null-terminator required for the string */
korl_internal u$ korl_memory_unpackStringU16(u16* data, u$ dataSize, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU$ (u$  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU64(u64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU32(u32 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU16(u16 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackU8 (u8  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackI64(i64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackI32(i32 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackI16(i16 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackI8 (i8  data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackF64(f64 data, u8** bufferCursor, const u8*const bufferEnd);
korl_internal u$ korl_memory_unpackF32(f32 data, u8** bufferCursor, const u8*const bufferEnd);
typedef struct Korl_Memory_ByteBuffer
{
    Korl_Memory_AllocatorHandle allocator;
    u$                          size;
    u$                          capacity;
    bool                        fastAndDirty;// tell the allocator not to zero-out our memory when we (re)allocate/free this ByteBuffer
    u8                          data[1];// array size is actually `capacity`, and immediately follows this struct
} Korl_Memory_ByteBuffer;
korl_internal Korl_Memory_ByteBuffer* korl_memory_byteBuffer_create(Korl_Memory_AllocatorHandle allocator, u$ capacity, bool fastAndDirty);
korl_internal void                    korl_memory_byteBuffer_destroy(Korl_Memory_ByteBuffer** pContext);
korl_internal void                    korl_memory_byteBuffer_append(Korl_Memory_ByteBuffer** pContext, acu8 data);
korl_internal void                    korl_memory_byteBuffer_trim(Korl_Memory_ByteBuffer** pContext);
/**
 * \return \c 0 if the length & contents of the arrays are equal
 */
korl_internal int korl_memory_compare_acu8(acu8 a, acu8 b);
/**
 * \return \c 0 if the length & contents of the arrays are equal
 */
korl_internal int korl_memory_compare_acu16(acu16 a, acu16 b);
korl_internal bool korl_memory_isNull(const void* p, size_t bytes);
