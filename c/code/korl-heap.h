#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_Heap_CreateInfo       Korl_Heap_CreateInfo;
typedef struct Korl_Memory_AllocationMeta Korl_Memory_AllocationMeta;
#define KORL_HEAP_ENUMERATE_ALLOCATIONS(name)          void name(void* allocatorUserData, fnSig_korl_heap_enumerateAllocationsCallback* callback, void* callbackUserData)
#define KORL_HEAP_ENUMERATE(name)                      void name(void* heap, fnSig_korl_heap_enumerateCallback* callback, void* callbackUserData)
typedef struct _Korl_Heap_General _Korl_Heap_General;
korl_internal _Korl_Heap_General*             korl_heap_general_create(const Korl_Heap_CreateInfo* createInfo);
korl_internal void                            korl_heap_general_destroy(_Korl_Heap_General* allocator);
korl_internal void                            korl_heap_general_empty(_Korl_Heap_General* allocator);
korl_internal void*                           korl_heap_general_allocate(_Korl_Heap_General* allocator, const wchar_t* allocatorName, u$ bytes, const wchar_t* file, int line, void* requestedAddress/*@TODO: remove requestedAddress; we should just do a single memcpy to restore heaps instead of injecting individual memory allocations!*/);
korl_internal void*                           korl_heap_general_reallocate(_Korl_Heap_General* allocator, const wchar_t* allocatorName, void* allocation, u$ bytes, const wchar_t* file, int line);
korl_internal void                            korl_heap_general_free(_Korl_Heap_General* allocator, void* allocation, const wchar_t* file, int line);
korl_internal KORL_HEAP_ENUMERATE            (korl_heap_general_enumerate);
korl_internal KORL_HEAP_ENUMERATE_ALLOCATIONS(korl_heap_general_enumerateAllocations);
typedef struct _Korl_Heap_Linear _Korl_Heap_Linear;
korl_internal _Korl_Heap_Linear*              korl_heap_linear_create(const Korl_Heap_CreateInfo* createInfo);
korl_internal void                            korl_heap_linear_destroy(_Korl_Heap_Linear*const allocator);
korl_internal void                            korl_heap_linear_empty(_Korl_Heap_Linear*const allocator);
korl_internal void*                           korl_heap_linear_allocate(_Korl_Heap_Linear*const allocator, const wchar_t* allocatorName, u$ bytes, const wchar_t* file, int line, void* requestedAddress/*@TODO: remove requestedAddress; we should just do a single memcpy to restore heaps instead of injecting individual memory allocations!*/);
korl_internal void*                           korl_heap_linear_reallocate(_Korl_Heap_Linear*const allocator, const wchar_t* allocatorName, void* allocation, u$ bytes, const wchar_t* file, int line);
korl_internal void                            korl_heap_linear_defragment(_Korl_Heap_Linear*const allocator, const wchar_t* allocatorName, void*** allocationPointerArray, u$ allocationPointerArraySize);
korl_internal void                            korl_heap_linear_free(_Korl_Heap_Linear*const allocator, void* allocation, const wchar_t* file, int line);
korl_internal KORL_HEAP_ENUMERATE            (korl_heap_linear_enumerate);
korl_internal KORL_HEAP_ENUMERATE_ALLOCATIONS(korl_heap_linear_enumerateAllocations);
korl_internal void                            korl_heap_linear_log(_Korl_Heap_Linear*const allocator, const wchar_t* allocatorName);
