#include "korl-memoryState.h"
#include "korl-memory.h"
#include "korl-stb-ds.h"
korl_internal void* korl_memoryState_create(Korl_Memory_AllocatorHandle allocatorHandle)
{
    u8* stbDaMemoryState = NULL;
    mcarrsetcap(KORL_STB_DS_MC_CAST(allocatorHandle), stbDaMemoryState, korl_math_kilobytes(1));
    //@TODO
    return stbds_header(stbDaMemoryState);
}
