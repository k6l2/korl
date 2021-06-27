#pragma once
#include "korl-global-defines.h"
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
