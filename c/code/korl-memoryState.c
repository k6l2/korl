#include "korl-memoryState.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-string.h"
#include "korl-stb-ds.h"
#include "korl-command.h"
#include "korl-windows-window.h"
#include "korl-gui.h"
#include "korl-gfx.h"
#include "korl-assetCache.h"
#include "korl-resource.h"
typedef struct _Korl_MemoryState_Manifest_Heap
{
    const void* virtualRoot;
    u$          virtualBytes;
    u$          committedBytes;
    u$          allocatedBytes;
} _Korl_MemoryState_Manifest_Heap;
typedef struct _Korl_MemoryState_Manifest_Allocator
{
    wchar_t allocatorName[32];
    KORL_MEMORY_POOL_DECLARE(_Korl_MemoryState_Manifest_Heap, heaps, 8);
} _Korl_MemoryState_Manifest_Allocator;
typedef struct _Korl_MemoryState_Manifest
{
    KORL_MEMORY_POOL_DECLARE(_Korl_MemoryState_Manifest_Allocator, allocators, 32);
    u32 byteOffsetKorlCommand;
    u32 byteOffsetKorlWindow;
    u32 byteOffsetKorlGfx;
    u32 byteOffsetKorlGui;
    u32 byteOffsetKorlResource;
    u32 byteOffsetKorlAssetCache;
} _Korl_MemoryState_Manifest;
typedef struct _Korl_MemoryState_Create_EnumerateContext
{
    u8**                        pStbDaMemoryState;
    Korl_Memory_AllocatorHandle allocatorHandleResult;
    _Korl_MemoryState_Manifest  manifest;
} _Korl_MemoryState_Create_EnumerateContext;
korl_internal KORL_HEAP_ENUMERATE_CALLBACK(_korl_memoryState_create_enumerateAllocatorsCallback_enumerateHeapsCallback)
{
    _Korl_MemoryState_Create_EnumerateContext*const enumerateContext = KORL_C_CAST(_Korl_MemoryState_Create_EnumerateContext*, userData);
    _Korl_MemoryState_Manifest_Allocator*const      enumAllocator    = enumerateContext->manifest.allocators + KORL_MEMORY_POOL_SIZE(enumerateContext->manifest.allocators) - 1;
    _Korl_MemoryState_Manifest_Heap*const           enumHeap         = KORL_MEMORY_POOL_ADD(enumAllocator->heaps);
    korl_assert(virtualAddressEnd > virtualAddressStart);
    enumHeap->virtualRoot    = virtualAddressStart;
    enumHeap->virtualBytes   = KORL_C_CAST(u$, virtualAddressEnd) - KORL_C_CAST(u$, virtualAddressStart);
    enumHeap->committedBytes = committedBytes;
    enumHeap->allocatedBytes = allocatedBytes;
    const i$ memoryStateBytes = arrlen(*enumerateContext->pStbDaMemoryState);
    mcarrsetlen(KORL_STB_DS_MC_CAST(enumerateContext->allocatorHandleResult), *enumerateContext->pStbDaMemoryState, memoryStateBytes + allocatedBytes);
    korl_memory_copy(*enumerateContext->pStbDaMemoryState + memoryStateBytes, allocatedBytesStart, allocatedBytes);
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_memoryState_create_enumerateAllocatorsCallback)
{
    if(!(allocatorFlags & KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE))
        return true;// true => continue enumerating
    _Korl_MemoryState_Create_EnumerateContext*const enumerateContext = KORL_C_CAST(_Korl_MemoryState_Create_EnumerateContext*, userData);
    _Korl_MemoryState_Manifest_Allocator*const      enumAllocator    = KORL_MEMORY_POOL_ADD(enumerateContext->manifest.allocators);
    KORL_MEMORY_POOL_EMPTY(enumAllocator->heaps);
    korl_assert(0 < korl_string_copyUtf16(allocatorName, (au16){.data = enumAllocator->allocatorName, .size = korl_arraySize(enumAllocator->allocatorName)}));
    korl_memory_allocator_enumerateHeaps(opaqueAllocator, _korl_memoryState_create_enumerateAllocatorsCallback_enumerateHeapsCallback, enumerateContext);
    return true;// true => continue enumerating
}
korl_internal void* korl_memoryState_create(Korl_Memory_AllocatorHandle allocatorHandleResult)
{
    u8* stbDaMemoryState = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(allocatorHandleResult), stbDaMemoryState, korl_math_megabytes(8));
    _Korl_MemoryState_Create_EnumerateContext enumerateContext;
    enumerateContext.pStbDaMemoryState                      = &stbDaMemoryState;
    enumerateContext.allocatorHandleResult                  = allocatorHandleResult;
    KORL_MEMORY_POOL_EMPTY(enumerateContext.manifest.allocators);
    /* iterate over all "serialized" allocators; 
        determine the "allocated memory" address range; 
        copy the entire allocated address range into `stbDaMemoryState`, while remembering the byte offsets of each heap */
    korl_memory_allocator_enumerateAllocators(_korl_memoryState_create_enumerateAllocatorsCallback, &enumerateContext);
    /* call module-specific functions to write module-specific data to `stbDaMemoryState`, while remembering byte offsets for each module */
    enumerateContext.manifest.byteOffsetKorlCommand    = korl_command_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &stbDaMemoryState);
    enumerateContext.manifest.byteOffsetKorlWindow     = korl_windows_window_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &stbDaMemoryState);
    enumerateContext.manifest.byteOffsetKorlGfx        = korl_gfx_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &stbDaMemoryState);
    enumerateContext.manifest.byteOffsetKorlGui        = korl_gui_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &stbDaMemoryState);
    enumerateContext.manifest.byteOffsetKorlResource   = korl_resource_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &stbDaMemoryState);
    enumerateContext.manifest.byteOffsetKorlAssetCache = korl_assetCache_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &stbDaMemoryState);
    /* write a manifest into `stbDaMemoryState` containing the heap offsets for each allocator & offsets to each module-specific data */
    const i$ manifestByteOffset = arrlen(stbDaMemoryState);
    mcarrsetlen(KORL_STB_DS_MC_CAST(allocatorHandleResult), stbDaMemoryState, manifestByteOffset + sizeof(enumerateContext.manifest));
    korl_memory_copy(stbDaMemoryState + manifestByteOffset, &enumerateContext.manifest, sizeof(enumerateContext.manifest));
    return stbds_header(stbDaMemoryState);
}
