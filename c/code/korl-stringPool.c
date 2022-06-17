#include "korl-stringPool.h"
#include "korl-memory.h"
// #if 1
// typedef u16 _Korl_StringPool_HandleSalt;
// typedef u64 _Korl_StringPool_HandleIdentifier;
// #define _KORL_STRINGPOOL_HANDLE_IDENTIFIER_MIN 1
// #define _KORL_STRINGPOOL_HANDLE_IDENTIFIER_MAX 0xFFFFFFFFFFFF
// #else///@TODO: choose
// typedef union Korl_StringPool_StringHandleComponents
// {
//     struct
//     {
//         u32 salt;
//         u32 index;
//     } components;
//     Korl_StringPool_StringHandle value;
// } Korl_StringPool_StringHandleComponents;
// _STATIC_ASSERT(sizeof(Korl_StringPool_StringHandleComponents) == sizeof(Korl_StringPool_StringHandle));
// #endif
typedef struct _Korl_StringPool_Entry
{
    Korl_StringPool_StringHandle handle;
    u32 characterCount;
    struct
    {
        u32 poolByteOffset;
        bool used;// if false, it means that the string occupying this space has been freed
    } utf8, utf16;
    /* allocation meta data */
    const wchar_t* file;
    int line;
} _Korl_StringPool_Entry;
korl_internal Korl_StringPool korl_stringPool_create(Korl_Memory_AllocatorHandle allocatorHandle)
{
    KORL_ZERO_STACK(Korl_StringPool, result);
    result.allocatorHandle    = allocatorHandle;
    result.nextStringHandle   = 1;
    result.characterPoolBytes = korl_math_kilobytes(1);
    result.characterPool      = korl_allocate(result.allocatorHandle, result.characterPoolBytes);
    result.entriesCapacity    = 16;
    result.entries            = korl_allocate(result.allocatorHandle, sizeof(*result.entries) * result.entriesCapacity);
    korl_assert(result.characterPool);
    korl_assert(result.entries);
    return result;
}
korl_internal void korl_stringPool_destroy(Korl_StringPool* context)
{
    korl_free(context->allocatorHandle, context->entries);
    korl_free(context->allocatorHandle, context->characterPool);
    korl_memory_zero(context, sizeof(*context));
}
korl_internal Korl_StringPool_StringHandle korl_stringPool_addFromUtf16(Korl_StringPool* context, const wchar_t* cString, const wchar_t* file, int line)
{
    /* find a unique handle for the new string */
    korl_assert(context->nextStringHandle != NULL);
    const Korl_StringPool_StringHandle newHandle = context->nextStringHandle;
    if(context->nextStringHandle == KORL_U32_MAX)
        context->nextStringHandle = 1;
    else
        context->nextStringHandle++;
    for(u$ e = 0; e < context->entriesSize; e++)
        korl_assert(context->entries[e].handle != newHandle);
    /* add a new string entry */
    if(context->entriesSize >= context->entriesCapacity)
    {
        korl_assert(context->entriesCapacity > 0);
        context->entriesCapacity *= 2;
        context->entries = korl_reallocate(context->allocatorHandle, context->entries, sizeof(*context->entries) * context->entriesCapacity);
        korl_assert(context->entries);
    }
    _Korl_StringPool_Entry*const newEntry = &context->entries[context->entriesSize++];
    /* populate the new entry meta data */
    newEntry->handle = newHandle;
    newEntry->file   = file;
    newEntry->line   = line;
    /* populate the new entry string data */
    ///@TODO: 
    return newHandle;
}
korl_internal void korl_stringPool_remove(Korl_StringPool* context, Korl_StringPool_StringHandle stringHandle)
{
    ///@TODO: 
}