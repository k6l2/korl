#include "korl-stb-truetype.h"
#include "korl-memory.h"
#include "utility/korl-utility-math.h"
typedef struct _Korl_Stb_TrueType_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
} _Korl_Stb_TrueType_Context;
korl_global_variable _Korl_Stb_TrueType_Context _korl_stb_truetype_context;
korl_internal void* _korl_stb_truetype_allocate(u$ bytes)
{
    return korl_allocate(_korl_stb_truetype_context.allocatorHandle, bytes);
}
korl_internal void _korl_stb_truetype_free(void* allocation)
{
    korl_free(_korl_stb_truetype_context.allocatorHandle, allocation);
}
korl_internal void korl_stb_truetype_initialize(void)
{
    _Korl_Stb_TrueType_Context*const context = &_korl_stb_truetype_context;
    korl_memory_zero(context, sizeof(*context));
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_kilobytes(512);
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_CRT, L"korl-stb-truetype", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
}
#define STBTT_malloc(x,u) ((void)(u),_korl_stb_truetype_allocate(x))
#define STBTT_free(x,u)   ((void)(u),_korl_stb_truetype_free(x))
#define STBTT_assert(x)   korl_assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#pragma warning(push)
    #pragma warning(disable:4702)// stb_truetype is doing assert(0), which causes unreachable code, and that's OK
    #include "stb/stb_truetype.h"
#pragma warning(pop)
