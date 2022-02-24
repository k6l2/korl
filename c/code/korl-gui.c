#include "korl-gui.h"
#include "korl-assert.h"
#include "korl-gfx.h"
#include "korl-gui-common.h"
korl_internal void korl_gui_initialize(void)
{
    korl_memory_nullify(&_korl_gui_context, sizeof(_korl_gui_context));
}
korl_internal void korl_gui_windowBegin(const wchar_t* identifier)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex == korl_arraySize(context->windows));
    /* check to see if this identifier is already registered */
    for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(context->windows); ++i)
    {
        if(context->windows[i].identifier == identifier)
        {
            context->currentWindowIndex = korl_checkCast_u$_to_u8(i);
            goto done_markWindowUsedThisFrame;
        }
    }
    /* we are forced to allocate a new window in the memory pool */
    Korl_Math_V2f32 nextWindowPosition = KORL_MATH_V2F32_ZERO;
    if(!KORL_MEMORY_POOL_ISEMPTY(context->windows))
        nextWindowPosition = korl_math_v2f32_add(context->windows[KORL_MEMORY_POOL_SIZE(context->windows) - 1].position, 
                                                 (Korl_Math_V2f32){ 32.f, -32.f });
    korl_assert(!KORL_MEMORY_POOL_ISFULL(context->windows));
    _Korl_Gui_Window*const newWindow = KORL_MEMORY_POOL_ADD(context->windows);
    newWindow->identifier = identifier;
    newWindow->position   = nextWindowPosition;
    newWindow->size       = (Korl_Math_V2f32){ 128.f, 128.f };
    context->currentWindowIndex = korl_checkCast_u$_to_u8(KORL_MEMORY_POOL_SIZE(context->windows) - 1);
done_markWindowUsedThisFrame:
    context->windows[context->currentWindowIndex].usedThisFrame = true;
}
korl_internal void korl_gui_windowEnd(void)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex < KORL_MEMORY_POOL_SIZE(context->windows));
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
    Korl_Gfx_Camera guiCamera = korl_gfx_createCameraOrtho(1.f);
    korl_gfx_cameraOrthoSetOriginAnchor(&guiCamera, 0.f, 1.f);
    korl_gfx_useCamera(guiCamera);
    for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(context->windows); ++i)
    {
        korl_assert(context->windows[i].identifier);
        korl_shared_const Korl_Vulkan_Color WINDOW_COLOR        = {.rgb.r =  64, .rgb.g =  64, .rgb.b =  64};
        korl_shared_const Korl_Vulkan_Color WINDOW_COLOR_ACTIVE = {.rgb.r = 128, .rgb.g = 128, .rgb.b = 128};
        korl_shared_const Korl_Math_V2f32 KORL_ORIGIN_RATIO_UPPER_LEFT = {0, 1};//remember, +Y is UP!
        Korl_Vulkan_Color windowColor = WINDOW_COLOR;
        if(context->isTopLevelWindowActive && i == KORL_MEMORY_POOL_SIZE(context->windows) - 1)
            windowColor = WINDOW_COLOR_ACTIVE;
        Korl_Gfx_Batch*const batch = korl_gfx_createBatchRectangleColored(allocatorStack, context->windows[i].size, KORL_ORIGIN_RATIO_UPPER_LEFT, windowColor);
        korl_gfx_batchSetPosition(batch, (Korl_Vulkan_Position){.xyz.x = context->windows[i].position.xy.x, .xyz.y = context->windows[i].position.xy.y, .xyz.z = 0.f});
        korl_gfx_batch(batch, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        /* if the window with this identifier wasn't used this frame, nullify it */
        if(!context->windows[i].usedThisFrame)
            context->windows[i].identifier = NULL;
        context->windows[i].usedThisFrame = false;
    }
    /* clean up the Windows which got nullified in the previous loop, while 
        maintaining the order of the Windows */
    Korl_MemoryPool_Size windowsRemaining = 0;
    for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(context->windows); ++i)
    {
        if(context->windows[i].identifier)
            windowsRemaining++;
        else if(windowsRemaining < i)
            context->windows[windowsRemaining] = context->windows[i];
    }
    KORL_MEMORY_POOL_RESIZE(context->windows, windowsRemaining);
}
