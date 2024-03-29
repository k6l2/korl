#include "korl-stb-image.h"
#include "korl-memory.h"
#include "utility/korl-utility-math.h"
typedef struct _Korl_Stb_Image_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
} _Korl_Stb_Image_Context;
korl_global_variable _Korl_Stb_Image_Context _korl_stb_image_context;
korl_internal void korl_stb_image_initialize(void)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    korl_memory_zero(context, sizeof(*context));
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(16);
    context->allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_CRT, L"korl-stb-image", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
}
korl_internal void* _korl_stb_image_allocate(u$ bytes)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    return korl_allocate(context->allocatorHandle, bytes);
}
korl_internal void* _korl_stb_image_reallocate(void* allocation, u$ bytes)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    return korl_reallocate(context->allocatorHandle, allocation, bytes);
}
korl_internal void _korl_stb_image_free(void* allocation)
{
    _Korl_Stb_Image_Context*const context = &_korl_stb_image_context;
    korl_free(context->allocatorHandle, allocation);
}
#define STBI_ASSERT(x)        korl_assert(x)
#define STBI_MALLOC(sz)       _korl_stb_image_allocate(sz)
#define STBI_REALLOC(p,newsz) _korl_stb_image_reallocate(p,newsz)
#define STBI_FREE(p)          _korl_stb_image_free(p)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#pragma warning(push)
    #pragma warning(disable:4702)// stb_image is doing assert(0), which causes unreachable code, and that's OK
    #include "stb/stb_image.h"
#pragma warning(pop)
