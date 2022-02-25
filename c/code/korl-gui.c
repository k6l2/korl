#include "korl-gui.h"
#include "korl-assert.h"
#include "korl-gfx.h"
#include "korl-gui-internal-common.h"
korl_internal void korl_gui_initialize(void)
{
    korl_memory_nullify(&_korl_gui_context, sizeof(_korl_gui_context));
    _korl_gui_context.style.colorWindow              = (Korl_Vulkan_Color){.rgb.r =  16, .rgb.g =  16, .rgb.b =  16};
    _korl_gui_context.style.colorWindowActive        = (Korl_Vulkan_Color){.rgb.r =  32, .rgb.g =  32, .rgb.b =  32};
    _korl_gui_context.style.colorTitleBar            = (Korl_Vulkan_Color){.rgb.r =   0, .rgb.g =  32, .rgb.b =   0};
    _korl_gui_context.style.colorTitleBarActive      = (Korl_Vulkan_Color){.rgb.r =  60, .rgb.g = 125, .rgb.b =  50};
    _korl_gui_context.style.fontWindowText           = NULL;// just use the default font inside korl-gfx
    _korl_gui_context.style.windowTextPixelSizeY     = 16.f;
    _korl_gui_context.style.windowTitleBarPixelSizeY = 20.f;
}
korl_internal void korl_gui_windowBegin(const wchar_t* identifier, Korl_Gui_Window_Style_Flags styleFlags)
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
    context->windows[context->currentWindowIndex].styleFlags    = styleFlags;
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
    for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(context->windows); ++i)
    {
        _Korl_Gui_Window*const window = &context->windows[i];
        korl_assert(window->identifier);
        korl_shared_const Korl_Math_V2f32 KORL_ORIGIN_RATIO_UPPER_LEFT = {0, 1};// remember, +Y is UP!
        Korl_Vulkan_Color windowColor   = context->style.colorWindow;
        Korl_Vulkan_Color titleBarColor = context->style.colorTitleBar;
        if(context->isTopLevelWindowActive && i == KORL_MEMORY_POOL_SIZE(context->windows) - 1)
        {
            windowColor   = context->style.colorWindowActive;
            titleBarColor = context->style.colorTitleBarActive;
        }
        korl_gfx_cameraSetScissor(&guiCamera, 
                                  window->position.xy.x, 
                                 -window->position.xy.y/*inverted, because remember: korl-gui window-space uses _correct_ y-axis direction (+y is UP)*/, 
                                  window->size.xy.x, 
                                  window->size.xy.y);
        korl_gfx_useCamera(guiCamera);
        /* draw the window panel */
        Korl_Gfx_Batch*const batchWindowPanel = korl_gfx_createBatchRectangleColored(allocatorStack, window->size, KORL_ORIGIN_RATIO_UPPER_LEFT, windowColor);
        korl_gfx_batchSetPosition(batchWindowPanel, (Korl_Vulkan_Position){ .xyz.x = window->position.xy.x
                                                                          , .xyz.y = window->position.xy.y
                                                                          , .xyz.z = 0.f });
        korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        {
            /* draw the window title bar */
            korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){.xy.x = window->size.xy.x, context->style.windowTitleBarPixelSizeY});
            korl_gfx_batchRectangleSetColor(batchWindowPanel, titleBarColor);
            korl_gfx_batchSetVertexColor(batchWindowPanel, 0, context->style.colorTitleBar);
            korl_gfx_batchSetVertexColor(batchWindowPanel, 1, context->style.colorTitleBar);
            korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            /* draw the window title bar text */
            Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(allocatorStack, context->style.fontWindowText, window->identifier, context->style.windowTextPixelSizeY);
            korl_gfx_batchSetPosition(batchWindowTitleText, (Korl_Vulkan_Position){ .xyz.x = window->position.xy.x
                                                                                  , .xyz.y = window->position.xy.y - context->style.windowTitleBarPixelSizeY + 3.f
                                                                                  , .xyz.z = 0.f });
            korl_gfx_batch(batchWindowTitleText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        }
        /* if the window with this identifier wasn't used this frame, nullify it */
        if(!window->usedThisFrame)
            window->identifier = NULL;
        window->usedThisFrame = false;
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
