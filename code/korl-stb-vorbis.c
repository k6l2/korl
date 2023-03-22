#include "korl-stb-vorbis.h"
#include "korl-crash.h"
#include "korl-algorithm.h"
korl_global_variable Korl_Memory_AllocatorHandle _korl_stb_vorbis_allocatorHandle;
korl_internal void* _korl_stb_vorbis_allocate(u$ bytes)
{
    return korl_allocate(_korl_stb_vorbis_allocatorHandle, bytes);
}
korl_internal void* _korl_stb_vorbis_reallocate(void* pointer, u$ bytes)
{
    return korl_reallocate(_korl_stb_vorbis_allocatorHandle, pointer, bytes);
}
korl_internal void _korl_stb_vorbis_free(void* pointer)
{
    korl_free(_korl_stb_vorbis_allocatorHandle, pointer);
}
korl_internal void korl_stb_vorbis_initialize(void)
{
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(16);
    _korl_stb_vorbis_allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_CRT, L"korl-stb-vorbis", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
}
#define stb_vorbis_assert(x)                                                korl_assert(x)
#define stb_vorbis_ldexp(x,exponent)                                        korl_math_loadExponent(x, exponent)
#define stb_vorbis_qsort(array,arraySize,arrayElementBytes,compareFunction) korl_algorithm_sort_quick(array, arraySize, arrayElementBytes, compareFunction)
#define stb_vorbis_floor(x)                                                 korl_math_f64_floor(x)
#define stb_vorbis_pow(x,exponent)                                          korl_math_f64_power(x, exponent)
#define stb_vorbis_alloca(bytes)                                            alloca(bytes)
#define stb_vorbis_malloc(bytes)                                            _korl_stb_vorbis_allocate(bytes)
#define stb_vorbis_realloc(pointer,bytes)                                   _korl_stb_vorbis_reallocate(pointer, bytes)
#define stb_vorbis_free(pointer)                                            _korl_stb_vorbis_free(pointer)
#define stb_vorbis_memcmp(p1,p2,bytes)                                      korl_memory_compare(p1, p2, bytes)
#define stb_vorbis_memcpy(target,source,bytes)                              korl_memory_copy(target, source, bytes)
#define stb_vorbis_memset(target,byteValue,bytes)                           korl_memory_set(target, byteValue, bytes)
#include "stb/stb_vorbis.c"
