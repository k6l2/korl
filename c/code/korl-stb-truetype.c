#include "korl-stb-truetype.h"
#include "korl-memory.h"
#include "korl-math.h"
#include "korl-assert.h"
typedef struct _Korl_Stb_TrueType_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
} _Korl_Stb_TrueType_Context;
korl_global_variable _Korl_Stb_TrueType_Context _korl_stb_truetype_context;
korl_internal void korl_stb_truetype_initialize(void)
{
    _Korl_Stb_TrueType_Context*const context = &_korl_stb_truetype_context;
    korl_memory_nullify(context, sizeof(*context));
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_kilobytes(512));
}
void* _stbtt_allocate(u$ bytes)
{
    _Korl_Stb_TrueType_Context*const context = &_korl_stb_truetype_context;
    return korl_memory_allocate(context->allocatorHandle, bytes);
}
void _stbtt_free(void* allocation)
{
    _Korl_Stb_TrueType_Context*const context = &_korl_stb_truetype_context;
    korl_memory_free(context->allocatorHandle, allocation);
}
#define STBTT_malloc(x,u) ((void)(u),_stbtt_allocate(x))
#define STBTT_free(x,u)   ((void)(u),_stbtt_free(x))
#define STBTT_assert(x)   korl_assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"