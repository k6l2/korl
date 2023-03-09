#include "korl-memoryState.h"
#include "korl-memory.h"
#include "korl-memoryPool.h"
#include "korl-string.h"
#include "korl-command.h"
#include "korl-windows-window.h"
#include "korl-gui.h"
#include "korl-gfx.h"
#include "korl-sfx.h"
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
    u32 byteOffsetKorlSfx;
    u32 byteOffsetKorlGui;
    u32 byteOffsetKorlResource;
    u32 byteOffsetKorlAssetCache;
} _Korl_MemoryState_Manifest;
typedef struct _Korl_MemoryState_Create_EnumerateContext
{
    Korl_Memory_ByteBuffer**    byteBuffer;
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
    korl_memory_byteBuffer_append(enumerateContext->byteBuffer, (acu8){.data = virtualAddressStart, .size = grossAllocatedBytes});
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
korl_internal Korl_Memory_ByteBuffer* korl_memoryState_create(Korl_Memory_AllocatorHandle allocatorHandleResult)
{
    Korl_Memory_ByteBuffer* result = korl_memory_byteBuffer_create(allocatorHandleResult, korl_math_megabytes(8), true/*fastAndDirty*/);
    _Korl_MemoryState_Create_EnumerateContext enumerateContext;
    enumerateContext.byteBuffer            = &result;
    enumerateContext.allocatorHandleResult = allocatorHandleResult;
    KORL_MEMORY_POOL_EMPTY(enumerateContext.manifest.allocators);
    /* iterate over all "serialized" allocators; 
        determine the "allocated memory" address range; 
        copy the entire allocated address range into `result`; 
        note: we don't have to remember the byte offsets of each heap, as this information is implicit */
    korl_memory_allocator_enumerateAllocators(_korl_memoryState_create_enumerateAllocatorsCallback, &enumerateContext);
    /* call module-specific functions to write module-specific data to `result`, while remembering byte offsets for each module */
    enumerateContext.manifest.byteOffsetKorlCommand    = korl_command_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &result);
    enumerateContext.manifest.byteOffsetKorlWindow     = korl_windows_window_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &result);
    enumerateContext.manifest.byteOffsetKorlGfx        = korl_gfx_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &result);
    enumerateContext.manifest.byteOffsetKorlSfx        = korl_sfx_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &result);
    enumerateContext.manifest.byteOffsetKorlGui        = korl_gui_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &result);
    enumerateContext.manifest.byteOffsetKorlResource   = korl_resource_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &result);
    enumerateContext.manifest.byteOffsetKorlAssetCache = korl_assetCache_memoryStateWrite(KORL_STB_DS_MC_CAST(allocatorHandleResult), &result);
    /* write a manifest into `result` containing the heap offsets for each allocator & offsets to each module-specific data */
    korl_memory_byteBuffer_append(&result, (acu8){.data = KORL_C_CAST(u8*, &enumerateContext.manifest), .size = sizeof(enumerateContext.manifest)});
    return result;
}
korl_internal void korl_memoryState_save(Korl_Memory_ByteBuffer* context, Korl_File_PathType pathType, const wchar_t* fileName)
{
    if(!context)
        return;// silently do nothing if the user's memory state context is NULL
    KORL_ZERO_STACK(Korl_File_Descriptor, fileDescriptor);
    if(   !korl_file_openClear(pathType, fileName, &fileDescriptor, false/*_not_ async*/)
       && !korl_file_create   (pathType, fileName, &fileDescriptor, false/*_not_ async*/))
    {
        korl_log(WARNING, "failed to open memory state file");
        return;
    }
    korl_file_write(fileDescriptor, context->data, context->size);
    korl_file_close(&fileDescriptor);
    u8        pathUtf8[256];
    const i32 pathUtf8Size = korl_file_makePathString(pathType, fileName, pathUtf8, sizeof(pathUtf8));
    if(pathUtf8Size <= 0)
        korl_log(WARNING, "korl_file_makePathString failed; result=%i", pathUtf8Size);
    else
        korl_log(INFO, "memory state saved to \"%hs\"", pathUtf8);
}
korl_internal KORL_HEAP_ENUMERATE_ALLOCATIONS_CALLBACK(_korl_memoryState_load_enumerateAllocatorsCallback_enumerateAllocationsCallback)
{
    meta->file = NULL;
    return true;// true => continue enumerating
}
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATORS_CALLBACK(_korl_memoryState_load_enumerateAllocatorsCallback)
{
    if(!(allocatorFlags & KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE))
        return true;// true => continue enumerating
    korl_memory_allocator_enumerateAllocations(opaqueAllocator, _korl_memoryState_load_enumerateAllocatorsCallback_enumerateAllocationsCallback, NULL);
    return true;// true => continue enumerating
}
korl_internal Korl_Memory_ByteBuffer* korl_memoryState_load(Korl_Memory_AllocatorHandle allocatorHandleResult, Korl_File_PathType pathType, const wchar_t* fileName)
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
    Korl_Memory_ByteBuffer*const result = korl_memory_byteBuffer_create(allocatorHandleResult, fileBytes, true/*fastAndDirty*/);
    korl_assert(korl_file_read(fileDescriptor, result->data, fileBytes));
    korl_file_close(&fileDescriptor);// we're done with the file, so close it immediately
    /* using the memory state's manifest, we can reconstruct all the memory 
        allocators which are stored within */
    const _Korl_MemoryState_Manifest*const manifest = KORL_C_CAST(_Korl_MemoryState_Manifest*, result->data + fileBytes - sizeof(*manifest));
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
            korl_memory_copy(KORL_C_CAST(void*/*fuck it*/, heap->virtualRoot), result->data + currentMemoryStateHeapByte, heap->grossAllocatedBytes);
            currentMemoryStateHeapByte += heap->grossAllocatedBytes;
        }
    }
    korl_memory_allocator_enumerateAllocators(_korl_memoryState_load_enumerateAllocatorsCallback, NULL);
    /* finally, we can perform module-specific memory state loading procedures */
    korl_command_memoryStateRead       (result->data + manifest->byteOffsetKorlCommand);
    korl_windows_window_memoryStateRead(result->data + manifest->byteOffsetKorlWindow);
    korl_gfx_memoryStateRead           (result->data + manifest->byteOffsetKorlGfx);
    korl_sfx_memoryStateRead           (result->data + manifest->byteOffsetKorlSfx);
    korl_gui_memoryStateRead           (result->data + manifest->byteOffsetKorlGui);
    korl_resource_memoryStateRead      (result->data + manifest->byteOffsetKorlResource);
    korl_assetCache_memoryStateRead    (result->data + manifest->byteOffsetKorlAssetCache);
    /**/
    korl_resource_flushUpdates();
    return result;
}
