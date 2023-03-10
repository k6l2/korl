#pragma once
#define STBDS_REALLOC(context, pointer, bytes, file, line) _korl_stb_ds_reallocate(context, pointer, bytes, file, line)
#define STBDS_FREE(context, pointer)                       _korl_stb_ds_free(context, pointer)
#define STBDS_API static
#include "stb/stb_ds.h"
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
#define KORL_STB_DS_MC_CAST(x) ((void*)x)
#ifdef KORL_PLATFORM
    korl_internal void korl_stb_ds_initialize(void);
#endif
korl_internal void korl_stb_ds_arrayAppendU8(void* memoryContext, u8** pStbDsArray, const void* data, u$ bytes);
/** we need a special function for this, since hash maps are quite complicated 
 * data structures with potentially _many_ dynamic allocation addresses */
korl_internal void _korl_stb_ds_stbDaDefragment_hashMap(void* stbDaDefragmentPointersMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void** pStbDsHashMap, const u$ hashMapElementBytes, void* allocationParent);
//KORL-ISSUE-000-000-120: interface-platform: remove KORL_DEFINED_INTERFACE_PLATFORM_API; this it just messy imo; if there is clear separation of code that should only exist in the platform layer, then we should probably just physically separate it out into separate source file(s)
#if !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)
    korl_internal KORL_FUNCTION__korl_stb_ds_reallocate(_korl_stb_ds_reallocate);
    korl_internal KORL_FUNCTION__korl_stb_ds_free(_korl_stb_ds_free);
#endif// !defined(KORL_DEFINED_INTERFACE_PLATFORM_API)
