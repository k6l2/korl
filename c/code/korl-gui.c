#include "korl-gui.h"
#include "korl-memory.h"
#include "korl-math.h"
#include "korl-assert.h"
#include "korl-gfx.h"
#include "korl-gui-internal-common.h"
korl_internal void korl_gui_initialize(void)
{
    korl_memory_nullify(&_korl_gui_context, sizeof(_korl_gui_context));
    _korl_gui_context.allocatorStack                 = korl_memory_createAllocatorLinear(korl_math_megabytes(1));
    _korl_gui_context.style.colorWindow              = (Korl_Vulkan_Color){.rgb.r =  16, .rgb.g =  16, .rgb.b =  16};
    _korl_gui_context.style.colorWindowActive        = (Korl_Vulkan_Color){.rgb.r =  32, .rgb.g =  32, .rgb.b =  32};
    _korl_gui_context.style.colorTitleBar            = (Korl_Vulkan_Color){.rgb.r =   0, .rgb.g =  32, .rgb.b =   0};
    _korl_gui_context.style.colorTitleBarActive      = (Korl_Vulkan_Color){.rgb.r =  60, .rgb.g = 125, .rgb.b =  50};
    _korl_gui_context.style.fontWindowText           = NULL;// just use the default font inside korl-gfx
    _korl_gui_context.style.windowTextPixelSizeY     = 16.f;
    _korl_gui_context.style.windowTitleBarPixelSizeY = 20.f;
    _korl_gui_context.style.widgetSpacingY           = 3.f;
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
            goto done_currentWindowIndexValid;
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
done_currentWindowIndexValid:
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
    context->allocatorStack.callbackEmpty(context->allocatorStack.userData);
}
korl_internal void korl_gui_frameEnd(void)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex == korl_arraySize(context->windows));
    context->frameSequenceCounter--;
    /* nullify widgets which weren't used this frame */
    Korl_MemoryPool_Size widgetsRemaining = 0;
    for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(context->widgets); i++)
    {
        _Korl_Gui_Widget*const widget = &context->widgets[i];
        korl_assert(widget->identifier);
        korl_assert(widget->parentWindowIdentifier);
        if(widget->usedThisFrame)
        {
            widget->usedThisFrame = false;
            if(widgetsRemaining < i)
                context->widgets[widgetsRemaining] = *widget;
            widgetsRemaining++;
        }
        else
            continue;
    }
    KORL_MEMORY_POOL_RESIZE(context->widgets, widgetsRemaining);
    /* for each of the windows that we ended up using for this frame, generate 
        the necessary draw commands for them */
    Korl_Gfx_Camera guiCamera = korl_gfx_createCameraOrtho(1.f);
    korl_gfx_cameraOrthoSetOriginAnchor(&guiCamera, 0.f, 1.f);
    Korl_MemoryPool_Size windowsRemaining = 0;
    for(u$ i = 0; i < KORL_MEMORY_POOL_SIZE(context->windows); ++i)
    {
        _Korl_Gui_Window*const window = &context->windows[i];
        korl_assert(window->identifier);
        /* if the window wasn't used this frame, skip it and prepare to remove 
            it from the memory pool */
        if(window->usedThisFrame)
        {
            window->usedThisFrame = false;
            if(windowsRemaining < i)
                context->windows[windowsRemaining] = *window;
            windowsRemaining++;
        }
        else
            continue;
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
        Korl_Gfx_Batch*const batchWindowPanel = korl_gfx_createBatchRectangleColored(context->allocatorStack, window->size, KORL_ORIGIN_RATIO_UPPER_LEFT, windowColor);
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
            Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(context->allocatorStack, context->style.fontWindowText, window->identifier, context->style.windowTextPixelSizeY);
            korl_gfx_batchSetPosition(batchWindowTitleText, (Korl_Vulkan_Position){ .xyz.x = window->position.xy.x
                                                                                  , .xyz.y = window->position.xy.y - context->style.windowTitleBarPixelSizeY + 3.f
                                                                                  , .xyz.z = 0.f });
            korl_gfx_batch(batchWindowTitleText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        }
        /* render all this window's widgets within the window panel */
        Korl_Math_V2f32 widgetCursor = korl_math_v2f32_subtract(window->position, (Korl_Math_V2f32){.xy.x = 0, .xy.y = context->style.windowTitleBarPixelSizeY});
        for(u$ j = 0; j < KORL_MEMORY_POOL_SIZE(context->widgets); ++j)
        {
            widgetCursor.xy.y -= context->style.widgetSpacingY;
            _Korl_Gui_Widget*const widget = &context->widgets[j];
            if(widget->parentWindowIdentifier != window->identifier)
                continue;
            switch(widget->type)
            {
            case KORL_GUI_WIDGET_TYPE_TEXT:{
                Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorStack, context->style.fontWindowText, widget->subType.text.displayText, context->style.windowTextPixelSizeY);
                const f32 batchTextAabbSizeY = korl_gfx_batchTextGetAabbSizeY(batchText);
                /** @robustness: instead of using the AABB of this text batch, 
                 * we should be using the font's metrics!  Probably??  
                 * Different text batches of the same font will yield different 
                 * sizes here, which will cause widget sizes to vary... */
                korl_gfx_batchSetPosition(batchText, (Korl_Vulkan_Position){ .xyz.x = widgetCursor.xy.x
                                                                           , .xyz.y = widgetCursor.xy.y - batchTextAabbSizeY
                                                                           , .xyz.z = 0.f });
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                widgetCursor.xy.y -= batchTextAabbSizeY;
                break;}
            }
        }
    }
    KORL_MEMORY_POOL_RESIZE(context->windows, windowsRemaining);
}
korl_internal void korl_gui_widgetTextFormat(const wchar_t* textFormat, ...)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex < KORL_MEMORY_POOL_SIZE(context->windows));
    Korl_MemoryPool_Size widgetIndex = korl_arraySize(context->widgets);
    /* check to see if this widget's identifier set is already registered */
    for(Korl_MemoryPool_Size w = 0; w < KORL_MEMORY_POOL_SIZE(context->widgets); ++w)
    {
        if(    context->widgets[w].parentWindowIdentifier == context->windows[context->currentWindowIndex].identifier
            && context->widgets[w].identifier == textFormat)
        {
            widgetIndex = w;
            goto widgetIndexValid;
        }
    }
    /* otherwise, allocate a new widget */
    korl_assert(!KORL_MEMORY_POOL_ISFULL(context->widgets));
    widgetIndex = KORL_MEMORY_POOL_SIZE(context->widgets);
    KORL_MEMORY_POOL_ADD(context->widgets);
    korl_memory_nullify(&context->widgets[widgetIndex], sizeof(context->widgets[widgetIndex]));
    context->widgets[widgetIndex].identifier             = textFormat;
    context->widgets[widgetIndex].parentWindowIdentifier = context->windows[context->currentWindowIndex].identifier;
    context->widgets[widgetIndex].type                   = KORL_GUI_WIDGET_TYPE_TEXT;
widgetIndexValid:
    _Korl_Gui_Widget*const widget = &context->widgets[widgetIndex];
    korl_assert(widget->type == KORL_GUI_WIDGET_TYPE_TEXT);
    widget->usedThisFrame = true;
    /** @todo: do we need to reassign the order of the widgets for the current window?  
     * I wont bother with this until I learn a little bit about API usage... */
    /* setup the widget with the provided formatted text string */
    va_list vaList;
    va_start(vaList, textFormat);
    widget->subType.text.displayText = korl_memory_stringFormat(context->allocatorStack, textFormat, vaList);
    va_end(vaList);
}
