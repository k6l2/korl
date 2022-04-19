#include "korl-stb-image.h"
#include "korl-memory.h"
#include "korl-math.h"
#include "korl-assert.h"
typedef struct _Korl_Stb_Image_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
} _Korl_Stb_Image_Context;
korl_global_variable _Korl_Stb_Image_Context _korl_stb_image_context;
korl_internal void korl_stb_image_initialize(void)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    korl_memory_zero(context, sizeof(*context));
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_megabytes(16));
}
void* _stbi_allocate(u$ bytes)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    return korl_memory_allocate(context->allocatorHandle, bytes);
}
void* _stbi_reallocate(void* allocation, u$ bytes)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    return korl_memory_reallocate(context->allocatorHandle, allocation, bytes);
}
void _stbi_free(void* allocation)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    korl_memory_free(context->allocatorHandle, allocation);
}
#define STBI_ASSERT(x)        korl_assert(x)
#define STBI_MALLOC(sz)       _stbi_allocate(sz)
#define STBI_REALLOC(p,newsz) _stbi_reallocate(p,newsz)
#define STBI_FREE(p)          _stbi_free(p)
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
