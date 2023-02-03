#pragma once
#include "korl-globalDefines.h"
typedef struct _Korl_Heap_General _Korl_Heap_General;
korl_internal _Korl_Heap_General* korl_heap_general_create(u$ maxBytes, void* address);
korl_internal void                korl_heap_general_destroy(_Korl_Heap_General* allocator);
korl_internal void                korl_heap_general_empty(_Korl_Heap_General* allocator);
korl_internal void*               korl_heap_general_allocate(_Korl_Heap_General* allocator, u$ bytes, const wchar_t* file, int line, void* requestedAddress);
korl_internal void*               korl_heap_general_reallocate(_Korl_Heap_General* allocator, void* allocation, u$ bytes, const wchar_t* file, int line);
korl_internal void                korl_heap_general_free(_Korl_Heap_General* allocator, void* allocation, const wchar_t* file, int line);
korl_internal KORL_MEMORY_ALLOCATOR_ENUMERATE_ALLOCATIONS(korl_heap_general_enumerateAllocations);
