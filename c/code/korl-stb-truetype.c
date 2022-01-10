#include "korl-stb-truetype.h"
#include "korl-memory.h"
#include "korl-math.h"
#include "korl-assert.h"
typedef struct _Korl_Stb_TrueType_Context
{
    Korl_Memory_Allocator allocator;
} _Korl_Stb_TrueType_Context;
korl_global_variable _Korl_Stb_TrueType_Context _korl_stb_truetype_context;
korl_internal void korl_stb_truetype_initialize(void)
{
    _Korl_Stb_TrueType_Context*const context = &_korl_stb_truetype_context;
    korl_memory_nullify(context, sizeof(*context));
    context->allocator = korl_memory_createAllocatorLinear(korl_math_kilobytes(512));
}
void* _stbtt_allocate(u$ bytes)
{
    _Korl_Stb_TrueType_Context*const context = &_korl_stb_truetype_context;
    return context->allocator.callbackAllocate(context->allocator.userData, bytes, L""__FILE__, __LINE__);
}
#if 0/// @unused
void* _stbtt_reallocate(void* allocation, u$ bytes)
{
    _Korl_Stb_TrueType_Context*const context = &_korl_stb_truetype_context;
    return context->allocator.callbackReallocate(context->allocator.userData, allocation, bytes, L""__FILE__, __LINE__);
}
#endif
void _stbtt_free(void* allocation)
{
    _Korl_Stb_TrueType_Context*const context = &_korl_stb_truetype_context;
    context->allocator.callbackFree(context->allocator.userData, allocation, L""__FILE__, __LINE__);
}
#define STBTT_malloc(x,u) ((void)(u),_stbtt_allocate(x))
#define STBTT_free(x,u)   ((void)(u),_stbtt_free(x))
#define STBTT_assert(x)   korl_assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"