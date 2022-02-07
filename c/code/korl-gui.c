#include "korl-gui.h"
#include "korl-assert.h"
#include "korl-gfx.h"
typedef struct
{
    const void* identifier;
} _Korl_Gui_Window;
typedef struct
{
    /** Helps ensure that the user calls \c korl_gui_begin/end the correct # of 
     * times.  When this value == korl_arraySize(windows), a new window must be 
     * started before calling any widget API. */
    u8 currentWindowIndex;
    /** help ensure the user calls \c korl_gui_frameBegin/End the correct # of 
     * times */
    u8 frameSequenceCounter;
    _Korl_Gui_Window windows[64];
} _Korl_Gui_Context;
korl_global_variable _Korl_Gui_Context _korl_gui_context;
korl_internal void korl_gui_initialize(void)
{
    korl_memory_nullify(&_korl_gui_context, sizeof(_korl_gui_context));
}
korl_internal void korl_gui_windowBegin(const wchar_t* identifier)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex == korl_arraySize(context->windows));
    /* @todo: check to see if this identifier already exists so we can modify it for this frame */
    /* find a window with an empty modifier to use */
    for (u8 i = 0; i < korl_arraySize(context->windows); ++i)
    {
        if (!context->windows[i].identifier)
        {
            context->windows[i].identifier = identifier;
            context->currentWindowIndex = i;
            break;
        }
    }
    korl_assert(context->currentWindowIndex != korl_arraySize(context->windows));
}
korl_internal void korl_gui_windowEnd(void)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex != korl_arraySize(context->windows));
    context->currentWindowIndex = korl_arraySize(context->windows);
}
korl_internal void korl_gui_frameBegin(void)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 0);
    context->frameSequenceCounter++;
    context->currentWindowIndex = korl_arraySize(context->windows);
}
korl_internal void korl_gui_frameEnd(Korl_Memory_Allocator allocatorStack)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex == korl_arraySize(context->windows));
    context->frameSequenceCounter--;
    /* for each of the windows that we ended up using for this frame, generate 
        the necessary draw commands for them */
    korl_gfx_useCamera(korl_gfx_createCameraOrtho(1.f));
    for(u$ i = 0; i < korl_arraySize(context->windows); ++i)
    {
        if (!context->windows[i].identifier)
            continue;
        korl_shared_const Korl_Math_V2f32 WINDOW_SIZE = {.xy.x = 100.f, .xy.y = 100.f};
        korl_shared_const Korl_Vulkan_Color WINDOW_COLOR = {.rgb.r = 128, .rgb.g = 128, .rgb.b = 128};
        Korl_Gfx_Batch*const batch = korl_gfx_createBatchRectangleColored(allocatorStack, WINDOW_SIZE, WINDOW_COLOR);
        korl_gfx_batch(batch, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        /** @hack: for now, let's just discard the window data each frame (no persistent data) */
        context->windows[i].identifier = NULL;
    }
}