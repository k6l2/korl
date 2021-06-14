#pragma once
#include "korl-global-defines.h"
struct Korl_Memory_Allocation
{
    void* address;
    size_t bytes;
};
korl_internal void korl_memory_initialize(void);
korl_internal u32 korl_memory_pageSize(void);
korl_internal struct Korl_Memory_Allocation korl_memory_allocate(size_t bytes);
