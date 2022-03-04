#include "korl-stb-image.h"
#include "korl-memory.h"
#include "korl-math.h"
#include "korl-assert.h"
typedef struct _Korl_Stb_Image_Context
{
    Korl_Memory_Allocator allocator;
} _Korl_Stb_Image_Context;
korl_global_variable _Korl_Stb_Image_Context _korl_stb_image_context;
korl_internal void korl_stb_image_initialize(void)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    korl_memory_nullify(context, sizeof(*context));
    context->allocator = korl_memory_createAllocatorLinear(korl_math_megabytes(16));
}
void* _stbi_allocate(u$ bytes)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    return context->allocator.callbackAllocate(context->allocator.userData, bytes, L""__FILE__, __LINE__);
}
void* _stbi_reallocate(void* allocation, u$ bytes)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    return context->allocator.callbackReallocate(context->allocator.userData, allocation, bytes, L""__FILE__, __LINE__);
}
void _stbi_free(void* allocation)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    context->allocator.callbackFree(context->allocator.userData, allocation, L""__FILE__, __LINE__);
}
#define STBI_ASSERT(x)        korl_assert(x)
#define STBI_MALLOC(sz)       _stbi_allocate(sz)
#define STBI_REALLOC(p,newsz) _stbi_reallocate(p,newsz)
#define STBI_FREE(p)          _stbi_free(p)
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"