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
    u$          grossAllocatedBytes;// the size of the contiguous region within `committedBytes` which contains the Heap itself, as well as all allocations within the heap
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
    enumHeap->virtualRoot         = virtualAddressStart;
    enumHeap->virtualBytes        = KORL_C_CAST(u$, virtualAddressEnd) - KORL_C_CAST(u$, virtualAddressStart);
    enumHeap->committedBytes      = committedBytes;
    enumHeap->grossAllocatedBytes = grossAllocatedBytes;
    const i$ memoryStateBytes = arrlen(*enumerateContext->pStbDaMemoryState);
    mcarrsetlen(KORL_STB_DS_MC_CAST(enumerateContext->allocatorHandleResult), *enumerateContext->pStbDaMemoryState, memoryStateBytes + grossAllocatedBytes);
    korl_memory_copy(*enumerateContext->pStbDaMemoryState + memoryStateBytes, virtualAddressStart, grossAllocatedBytes);
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
    /*@TODO: > 50% of the time spent in this function is in reallocation of `stbDaMemoryState`; 
        specifically due to `korl_memory_zero` on each allocation, and just as bad: the destruction
        of this buffer _also_ causes a huge/frivolous `korl_memory_zero` invocation; 
        this alone is costing us nearly 750µs per optimized build frame; 
        perhaps it is time to allow the user to specifically tell the call to 
        korl_allocate/reallocate/free that we do _not_ want it to zero-out the memory for us, as well as with */
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
korl_internal void korl_memoryState_save(void* context, Korl_File_PathType pathType, const wchar_t* fileName)
{
    if(!context)
        return;// silently do nothing if the user's memory state context is NULL
    u8* stbDaMemoryState = KORL_C_CAST(u8*, (KORL_C_CAST(stbds_array_header*, context) + 1));
    KORL_ZERO_STACK(Korl_File_Descriptor, fileDescriptor);
    if(   !korl_file_openClear(pathType, fileName, &fileDescriptor, false/*_not_ async*/)
       && !korl_file_create   (pathType, fileName, &fileDescriptor, false/*_not_ async*/))
    {
        korl_log(WARNING, "failed to open memory state file");
        return;
    }
    korl_file_write(fileDescriptor, stbDaMemoryState, arrlenu(stbDaMemoryState));
    korl_file_close(&fileDescriptor);
    u8        pathUtf8[256];
    const i32 pathUtf8Size = korl_file_makePathString(pathType, fileName, pathUtf8, sizeof(pathUtf8));
    if(pathUtf8Size <= 0)
        korl_log(WARNING, "korl_file_makePathString failed; result=%i", pathUtf8Size);
    else
        korl_log(INFO, "memory state saved to \"%hs\"", pathUtf8);
}
korl_internal void* korl_memoryState_load(Korl_Memory_AllocatorHandle allocatorHandleResult, Korl_File_PathType pathType, const wchar_t* fileName)
{
    /* flush the state of some modules which depend on system resources prior to 
        overwriting the state of modules which depend on these resources */
    korl_log_clearAsyncIo();
    korl_file_finishAllAsyncOperations();
    korl_assetCache_clearAllFileHandles();
    korl_vulkan_clearAllDeviceAllocations();
    /* read in the entire memory state into memory right now */
    KORL_ZERO_STACK(Korl_File_Descriptor, fileDescriptor);
    if(!korl_file_open(pathType, fileName, &fileDescriptor, false/*_not_ async*/))
    {
        korl_log(WARNING, "failed to open memoryState");
        return NULL;
    }
    const u32 fileBytes = korl_file_getTotalBytes(fileDescriptor);
    u8* stbDaMemoryState = NULL;
    mcarrsetlen(KORL_STB_DS_MC_CAST(allocatorHandleResult), stbDaMemoryState, fileBytes);
    korl_assert(korl_file_read(fileDescriptor, stbDaMemoryState, fileBytes));
    korl_file_close(&fileDescriptor);// we're done with the file, so close it immediately
    /* using the memory state's manifest, we can reconstruct all the memory 
        allocators which are stored within */
    const _Korl_MemoryState_Manifest*const manifest = KORL_C_CAST(_Korl_MemoryState_Manifest*, stbDaMemoryState + fileBytes - sizeof(*manifest));
    u$ currentMemoryStateHeapByte = 0;// the first heap is at byte 0 of the memory state, and all heaps are contiguous
    for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(manifest->allocators); i++)
    {
        Korl_Memory_AllocatorHandle allocatorHandle = 0;
        korl_assert(korl_memory_allocator_findByName(manifest->allocators[i].allocatorName, &allocatorHandle));
        korl_memory_allocator_recreate(allocatorHandle
                                      ,KORL_MEMORY_POOL_SIZE(manifest->allocators[i].heaps)
                                      ,manifest->allocators[i].heaps
                                      ,sizeof(*manifest->allocators[i].heaps)
                                      ,offsetof(_Korl_MemoryState_Manifest_Heap, virtualRoot)
                                      ,offsetof(_Korl_MemoryState_Manifest_Heap, virtualBytes)
                                      ,offsetof(_Korl_MemoryState_Manifest_Heap, committedBytes));
        for(u$ j = 0; j < KORL_MEMORY_POOL_SIZE(manifest->allocators[i].heaps); j++)
        {
            const _Korl_MemoryState_Manifest_Heap*const heap = manifest->allocators[i].heaps + j;
            korl_memory_copy(KORL_C_CAST(void*/*fuck it*/, heap->virtualRoot), stbDaMemoryState + currentMemoryStateHeapByte, heap->grossAllocatedBytes);
            currentMemoryStateHeapByte += heap->grossAllocatedBytes;
        }
    }
    /* finally, we can perform module-specific memory state loading procedures */
    korl_command_memoryStateRead       (stbDaMemoryState + manifest->byteOffsetKorlCommand);
    korl_windows_window_memoryStateRead(stbDaMemoryState + manifest->byteOffsetKorlWindow);
    korl_gfx_memoryStateRead           (stbDaMemoryState + manifest->byteOffsetKorlGfx);
    korl_gui_memoryStateRead           (stbDaMemoryState + manifest->byteOffsetKorlGui);
    korl_resource_memoryStateRead      (stbDaMemoryState + manifest->byteOffsetKorlResource);
    korl_assetCache_memoryStateRead    (stbDaMemoryState + manifest->byteOffsetKorlAssetCache);
    /**/
    korl_resource_flushUpdates();
    return stbDaMemoryState ? stbds_header(stbDaMemoryState) : NULL;
}
