#pragma once
#include "korl-globalDefines.h"
typedef struct
{
    void* address;
    void* addressEnd;
} Korl_Memory_Allocation;
korl_internal void korl_memory_initialize(void);
korl_internal u32 korl_memory_pageSize(void);
korl_internal void* korl_memory_addressMin(void);
korl_internal void* korl_memory_addressMax(void);
/** 
 * \param desiredAddress if 0, the resulting address of the allocation will be 
 * chosen automatically
 */
korl_internal Korl_Memory_Allocation korl_memory_allocate(
    size_t bytes, void* desiredAddress);
korl_internal void korl_memory_free(void* address);
/**  Should function in the same way as memcmp from the C standard library.
 * \return \c -1 if the first byte that differs has a lower value in \c a than 
 *    in \c b, \c 1 if the first byte that differs has a higher value in \c a 
 *    than in \c b, and \c 0 if the two memory blocks are equal
 */
korl_internal int korl_memory_compare(const void* a, const void* b, size_t bytes);
