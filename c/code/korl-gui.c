#include "korl-gui.h"
#include "korl-memory.h"
#include "korl-math.h"
#include "korl-assert.h"
#include "korl-gfx.h"
#include "korl-gui-internal-common.h"
korl_internal _Korl_Gui_Widget* _korl_gui_getWidget(const void*const identifier, u$ widgetType)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 1);
    korl_assert(context->currentWindowIndex < KORL_MEMORY_POOL_SIZE(context->windows));
    Korl_MemoryPool_Size widgetIndex = korl_arraySize(context->widgets);
    /* check to see if this widget's identifier set is already registered */
    for(Korl_MemoryPool_Size w = 0; w < KORL_MEMORY_POOL_SIZE(context->widgets); ++w)
    {
        if(    context->widgets[w].parentWindowIdentifier == context->windows[context->currentWindowIndex].identifier
            && context->widgets[w].identifier == identifier)
        {
            widgetIndex = w;
            goto widgetIndexValid;
        }
    }
    /* otherwise, allocate a new widget */
    korl_assert(!KORL_MEMORY_POOL_ISFULL(context->widgets));
    widgetIndex = KORL_MEMORY_POOL_SIZE(context->widgets);
    KORL_MEMORY_POOL_ADD(context->widgets);
    _Korl_Gui_Widget* widget = &context->widgets[widgetIndex];
    korl_memory_nullify(widget, sizeof(*widget));
    widget->identifier             = identifier;
    widget->parentWindowIdentifier = context->windows[context->currentWindowIndex].identifier;
    widget->type                   = widgetType;
widgetIndexValid:
    widget = &context->widgets[widgetIndex];
    korl_assert(widget->type == widgetType);
    widget->usedThisFrame = true;
    /** @todo: do we need to reassign the order of the widgets for the current window?  
     * I wont bother with this until I learn a little bit about API usage... */
    return widget;
}
korl_internal void _korl_gui_processWidgetGraphics(_Korl_Gui_Window*const window, 
                                                   Korl_Math_V2f32*const out_contentAabbMin, Korl_Math_V2f32*const out_contentAabbMax, 
                                                   bool batchGraphics)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    Korl_Math_V2f32 widgetCursor = window->position;
    if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        widgetCursor = korl_math_v2f32_subtract(widgetCursor, (Korl_Math_V2f32){.xy.x = 0, .xy.y = context->style.windowTitleBarPixelSizeY});
    for(u$ j = 0; j < KORL_MEMORY_POOL_SIZE(context->widgets); ++j)
    {
        _Korl_Gui_Widget*const widget = &context->widgets[j];
        if(widget->parentWindowIdentifier != window->identifier)
            continue;
        widgetCursor.xy.y -= context->style.widgetSpacingY;
        switch(widget->type)
        {
        case KORL_GUI_WIDGET_TYPE_TEXT:{
            Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorStack, context->style.fontWindowText, widget->subType.text.displayText, context->style.windowTextPixelSizeY);
            const f32 batchTextAabbSizeX = korl_gfx_batchTextGetAabbSizeX(batchText);
            const f32 batchTextAabbSizeY = korl_gfx_batchTextGetAabbSizeY(batchText);
            widget->cachedAabbMin = korl_math_v2f32_subtract(widgetCursor, (Korl_Math_V2f32){.xy.x =                  0, .xy.y = batchTextAabbSizeY});
            widget->cachedAabbMax = korl_math_v2f32_add     (widgetCursor, (Korl_Math_V2f32){.xy.x = batchTextAabbSizeX, .xy.y =                  0});
            widget->cachedIsInteractive = false;
            if(batchGraphics)
            {
                /** @robustness: instead of using the AABB of this text batch, 
                 * we should be using the font's metrics!  Probably??  
                 * Different text batches of the same font will yield different 
                 * sizes here, which will cause widget sizes to vary... */
                korl_gfx_batchSetPosition2d(batchText, (Korl_Math_V2f32){ .xy.x = widgetCursor.xy.x
                                                                        , .xy.y = widgetCursor.xy.y - batchTextAabbSizeY});
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            }
            break;}
        case KORL_GUI_WIDGET_TYPE_BUTTON:{
            Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorStack, context->style.fontWindowText, widget->subType.button.displayText, context->style.windowTextPixelSizeY);
            const f32 batchTextAabbSizeX = korl_gfx_batchTextGetAabbSizeX(batchText);
            const f32 batchTextAabbSizeY = korl_gfx_batchTextGetAabbSizeY(batchText);
            const f32 buttonAabbSizeX = batchTextAabbSizeX + context->style.widgetButtonLabelMargin * 2.f;
            const f32 buttonAabbSizeY = batchTextAabbSizeY + context->style.widgetButtonLabelMargin * 2.f;
            widget->cachedAabbMin = korl_math_v2f32_subtract(widgetCursor, (Korl_Math_V2f32){.xy.x =               0, .xy.y = buttonAabbSizeY});
            widget->cachedAabbMax = korl_math_v2f32_add     (widgetCursor, (Korl_Math_V2f32){.xy.x = buttonAabbSizeX, .xy.y =               0});
            widget->cachedIsInteractive = true;
            if(batchGraphics)
            {
                Korl_Gfx_Batch*const batchButton = korl_gfx_createBatchRectangleColored(context->allocatorStack, 
                                                                                        (Korl_Math_V2f32){ .xy.x = buttonAabbSizeX
                                                                                                         , .xy.y = buttonAabbSizeY}, 
                                                                                        (Korl_Math_V2f32){.xy.x = 0.f, .xy.y = 1.f}, 
                                                                                        context->style.colorButtonInactive);
                korl_gfx_batchSetPosition2d(batchButton, (Korl_Math_V2f32){ .xy.x = widgetCursor.xy.x
                                                                          , .xy.y = widgetCursor.xy.y});
                if(    context->mouseHoverPosition.xy.x >= widget->cachedAabbMin.xy.x 
                    && context->mouseHoverPosition.xy.x <= widget->cachedAabbMax.xy.x 
                    && context->mouseHoverPosition.xy.y >= widget->cachedAabbMin.xy.y 
                    && context->mouseHoverPosition.xy.y <= widget->cachedAabbMax.xy.y)
                {
                    if(context->isMouseDown && !context->isWindowDragged && context->identifierMouseDownWidget == widget->identifier)
                    {
                        korl_gfx_batchRectangleSetColor(batchButton, context->style.colorButtonPressed);
                    }
                    else if(context->isMouseHovering && context->identifierMouseHoveredWidget == widget->identifier)
                    {
                        korl_gfx_batchRectangleSetColor(batchButton, context->style.colorButtonActive);
                    }
                }
                korl_gfx_batch(batchButton, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                /** @robustness: instead of using the AABB of this text batch, 
                 * we should be using the font's metrics!  Probably??  
                 * Different text batches of the same font will yield different 
                 * sizes here, which will cause widget sizes to vary... */
                korl_gfx_batchSetPosition2d(batchText, (Korl_Math_V2f32){ .xy.x = widgetCursor.xy.x + context->style.widgetButtonLabelMargin
                                                                        , .xy.y = widgetCursor.xy.y - context->style.widgetButtonLabelMargin - batchTextAabbSizeY});
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            }
            break;}
        }
        if(out_contentAabbMin)
        {
            korl_assert(out_contentAabbMax);
            *out_contentAabbMin = korl_math_v2f32_min(*out_contentAabbMin, widget->cachedAabbMin);
            *out_contentAabbMax = korl_math_v2f32_max(*out_contentAabbMax, widget->cachedAabbMax);
        }
        widgetCursor.xy.y -= widget->cachedAabbMax.xy.y - widget->cachedAabbMin.xy.y;
    }
}
korl_internal void korl_gui_initialize(void)
{
    korl_memory_nullify(&_korl_gui_context, sizeof(_korl_gui_context));
    _korl_gui_context.allocatorStack                       = korl_memory_createAllocatorLinear(korl_math_megabytes(1));
    _korl_gui_context.style.colorWindow                    = (Korl_Vulkan_Color){.rgb.r =  16, .rgb.g =  16, .rgb.b =  16};
    _korl_gui_context.style.colorWindowActive              = (Korl_Vulkan_Color){.rgb.r =  32, .rgb.g =  32, .rgb.b =  32};
    _korl_gui_context.style.colorWindowBorder              = (Korl_Vulkan_Color){.rgb.r =   0, .rgb.g =   0, .rgb.b =   0};
    _korl_gui_context.style.colorWindowBorderHovered       = (Korl_Vulkan_Color){.rgb.r =   0, .rgb.g =  32, .rgb.b =   0};
    _korl_gui_context.style.colorWindowBorderResize        = (Korl_Vulkan_Color){.rgb.r = 255, .rgb.g = 255, .rgb.b = 255};
    _korl_gui_context.style.colorWindowBorderActive        = (Korl_Vulkan_Color){.rgb.r =  60, .rgb.g = 125, .rgb.b =  50};
    _korl_gui_context.style.colorTitleBar                  = (Korl_Vulkan_Color){.rgb.r =   0, .rgb.g =  32, .rgb.b =   0};
    _korl_gui_context.style.colorTitleBarActive            = (Korl_Vulkan_Color){.rgb.r =  60, .rgb.g = 125, .rgb.b =  50};
    _korl_gui_context.style.colorButtonInactive            = (Korl_Vulkan_Color){.rgb.r =   0, .rgb.g =  32, .rgb.b =   0};
    _korl_gui_context.style.colorButtonActive              = (Korl_Vulkan_Color){.rgb.r =  60, .rgb.g = 125, .rgb.b =  50};
    _korl_gui_context.style.colorButtonPressed             = (Korl_Vulkan_Color){.rgb.r =   0, .rgb.g =   8, .rgb.b =   0};
    _korl_gui_context.style.colorButtonWindowTitleBarIcons = (Korl_Vulkan_Color){.rgb.r = 255, .rgb.g = 255, .rgb.b = 255};
    _korl_gui_context.style.colorButtonWindowCloseActive   = (Korl_Vulkan_Color){.rgb.r = 255, .rgb.g =   0, .rgb.b =   0};
    _korl_gui_context.style.fontWindowText                 = NULL;// just use the default font inside korl-gfx
    _korl_gui_context.style.windowTextPixelSizeY           = 16.f;
    _korl_gui_context.style.windowTitleBarPixelSizeY       = 20.f;
    _korl_gui_context.style.widgetSpacingY                 = 3.f;
    _korl_gui_context.style.widgetButtonLabelMargin        = 4.f;
}
korl_internal void korl_gui_windowBegin(const wchar_t* identifier, bool* out_isOpen, Korl_Gui_Window_Style_Flags styleFlags)
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
    _Korl_Gui_Window* newWindow = KORL_MEMORY_POOL_ADD(context->windows);
    korl_memory_nullify(newWindow, sizeof(*newWindow));
    newWindow->identifier   = identifier;
    newWindow->position     = nextWindowPosition;
    newWindow->size         = (Korl_Math_V2f32){ 128.f, 128.f };
    newWindow->isFirstFrame = true;
    newWindow->isOpen = true;
    context->currentWindowIndex = korl_checkCast_u$_to_u8(KORL_MEMORY_POOL_SIZE(context->windows) - 1);
done_currentWindowIndexValid:
    newWindow = &context->windows[context->currentWindowIndex];
    newWindow->usedThisFrame = true;
    newWindow->styleFlags    = styleFlags;
    if(out_isOpen)
    {
        newWindow->hasTitleBarButtonClose = true;
        newWindow->isOpen = *out_isOpen;
        if(newWindow->titleBarButtonPressedClose)
            newWindow->isOpen = false;
        *out_isOpen = newWindow->isOpen;
    }
    newWindow->titleBarButtonPressedClose = false;
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
    const Korl_Math_V2u32 swapChainSize = korl_vulkan_getSwapchainSize();
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
        /**/
        if(!window->isOpen)
            continue;
        korl_shared_const Korl_Math_V2f32 KORL_ORIGIN_RATIO_UPPER_LEFT = {0, 1};// remember, +Y is UP!
        Korl_Vulkan_Color windowColor   = context->style.colorWindow;
        Korl_Vulkan_Color titleBarColor = context->style.colorTitleBar;
        if(context->isTopLevelWindowActive && i == KORL_MEMORY_POOL_SIZE(context->windows) - 1)
        {
            windowColor   = context->style.colorWindowActive;
            titleBarColor = context->style.colorTitleBarActive;
        }
        if(window->isFirstFrame || (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE))
        {
            window->isFirstFrame = false;
            /* iterate over all this window's widgets, obtaining their AABBs & 
                accumulating their geometry to determine how big the window 
                needs to be */
            Korl_Math_V2f32 contentAabbMin = window->position;
            Korl_Math_V2f32 contentAabbMax = window->position;
            _korl_gui_processWidgetGraphics(window, &contentAabbMin, &contentAabbMax, false);
            /* take the AABB of the window's title bar into account as well */
            if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            {
                Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(context->allocatorStack, context->style.fontWindowText, window->identifier, context->style.windowTextPixelSizeY);
                const f32 titleBarOptimalSizeX = korl_gfx_batchTextGetAabbSizeX(batchWindowTitleText) 
                                               + /*size required for close button*/context->style.windowTitleBarPixelSizeY;
                contentAabbMax.xy.x = KORL_MATH_MAX(contentAabbMax.xy.x, window->position.xy.x + titleBarOptimalSizeX);
            }
            /* size the window based on the above metrics */
            window->size.xy.x = contentAabbMax.xy.x - contentAabbMin.xy.x;
            window->size.xy.y = contentAabbMax.xy.y - contentAabbMin.xy.y;
            /* prevent the window from being too small */
            if(window->size.xy.y < context->style.windowTitleBarPixelSizeY)
                window->size.xy.y = context->style.windowTitleBarPixelSizeY;
            if(window->size.xy.x < context->style.windowTitleBarPixelSizeY)
                window->size.xy.x = context->style.windowTitleBarPixelSizeY;
        }
        /* bind the windows to the bounds of the swapchain, such that there will 
            always be a square of grabable geometry on the window whose 
            dimensions equal the height of the window title bar style at minimum */
        if(window->position.xy.x < -window->size.xy.x + context->style.windowTitleBarPixelSizeY)
            window->position.xy.x = -window->size.xy.x + context->style.windowTitleBarPixelSizeY;
        if(window->position.xy.x > swapChainSize.xy.x - context->style.windowTitleBarPixelSizeY)
            window->position.xy.x = swapChainSize.xy.x - context->style.windowTitleBarPixelSizeY;
        if(window->position.xy.y > 0)
            window->position.xy.y = 0;
        if(window->position.xy.y < -KORL_C_CAST(f32, swapChainSize.xy.y) + context->style.windowTitleBarPixelSizeY)
            window->position.xy.y = -KORL_C_CAST(f32, swapChainSize.xy.y) + context->style.windowTitleBarPixelSizeY;
        /* before getting ready to batch the window panel & contents, let's draw 
            some geometry around the window AABB to better indicate certain state */
        korl_gfx_cameraSetScissorPercent(&guiCamera, 0,0, 1,1);
        korl_gfx_useCamera(guiCamera);
        Korl_Vulkan_Color colorBorder = context->style.colorWindowBorder;
        if(context->isTopLevelWindowActive && i == KORL_MEMORY_POOL_SIZE(context->windows) - 1)
            colorBorder = context->style.colorWindowBorderActive;
        else if(context->isMouseHovering && context->identifierMouseHoveredWindow == window->identifier)
            colorBorder = context->style.colorWindowBorderHovered;
        Korl_Gfx_Batch*const batchWindowBorderVertical   = korl_gfx_createBatchRectangleColored(context->allocatorStack, (Korl_Math_V2f32){.xy.x =                       WINDOW_AABB_EDGE_THICKNESS, .xy.y = window->size.xy.y + 2*WINDOW_AABB_EDGE_THICKNESS}, KORL_ORIGIN_RATIO_UPPER_LEFT, colorBorder);
        Korl_Gfx_Batch*const batchWindowBorderHorizontal = korl_gfx_createBatchRectangleColored(context->allocatorStack, (Korl_Math_V2f32){.xy.x = window->size.xy.x + 2*WINDOW_AABB_EDGE_THICKNESS, .xy.y =                       WINDOW_AABB_EDGE_THICKNESS}, KORL_ORIGIN_RATIO_UPPER_LEFT, colorBorder);
        korl_gfx_batchSetPosition2d(batchWindowBorderVertical, (Korl_Math_V2f32){ .xy.x = window->position.xy.x - WINDOW_AABB_EDGE_THICKNESS
                                                                                , .xy.y = window->position.xy.y + WINDOW_AABB_EDGE_THICKNESS });
        if(context->identifierMouseHoveredWindow == window->identifier && (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE))
            if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_LEFT)
                korl_gfx_batchRectangleSetColor(batchWindowBorderVertical, context->style.colorWindowBorderResize);
            else
                korl_gfx_batchRectangleSetColor(batchWindowBorderVertical, colorBorder);
        korl_gfx_batch(batchWindowBorderVertical, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_gfx_batchSetPosition2d(batchWindowBorderVertical, (Korl_Math_V2f32){ .xy.x = window->position.xy.x + window->size.xy.x
                                                                                , .xy.y = window->position.xy.y + WINDOW_AABB_EDGE_THICKNESS });
        if(context->identifierMouseHoveredWindow == window->identifier && (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE))
            if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_RIGHT)
                korl_gfx_batchRectangleSetColor(batchWindowBorderVertical, context->style.colorWindowBorderResize);
            else
                korl_gfx_batchRectangleSetColor(batchWindowBorderVertical, colorBorder);
        korl_gfx_batch(batchWindowBorderVertical, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_gfx_batchSetPosition2d(batchWindowBorderHorizontal, (Korl_Math_V2f32){ .xy.x = window->position.xy.x - WINDOW_AABB_EDGE_THICKNESS
                                                                                  , .xy.y = window->position.xy.y + WINDOW_AABB_EDGE_THICKNESS });
        if(context->identifierMouseHoveredWindow == window->identifier && (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE))
            if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_UP)
                korl_gfx_batchRectangleSetColor(batchWindowBorderHorizontal, context->style.colorWindowBorderResize);
            else
                korl_gfx_batchRectangleSetColor(batchWindowBorderHorizontal, colorBorder);
        korl_gfx_batch(batchWindowBorderHorizontal, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_gfx_batchSetPosition2d(batchWindowBorderHorizontal, (Korl_Math_V2f32){ .xy.x = window->position.xy.x - WINDOW_AABB_EDGE_THICKNESS
                                                                                  , .xy.y = window->position.xy.y - window->size.xy.y });
        if(context->identifierMouseHoveredWindow == window->identifier && (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE))
            if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_DOWN)
                korl_gfx_batchRectangleSetColor(batchWindowBorderHorizontal, context->style.colorWindowBorderResize);
            else
                korl_gfx_batchRectangleSetColor(batchWindowBorderHorizontal, colorBorder);
        korl_gfx_batch(batchWindowBorderHorizontal, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        /* prepare to draw all the window's contents by cutting out a scissor 
            region the size of the window, preventing us from accidentally 
            render any pixels outside the window */
        korl_gfx_cameraSetScissor(&guiCamera, 
                                   window->position.xy.x, 
                                  -window->position.xy.y/*inverted, because remember: korl-gui window-space uses _correct_ y-axis direction (+y is UP)*/, 
                                   window->size.xy.x, 
                                   window->size.xy.y);
        korl_gfx_useCamera(guiCamera);
        /* draw the window panel */
        Korl_Gfx_Batch*const batchWindowPanel = korl_gfx_createBatchRectangleColored(context->allocatorStack, window->size, KORL_ORIGIN_RATIO_UPPER_LEFT, windowColor);
        korl_gfx_batchSetPosition2d(batchWindowPanel, window->position);
        korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        {
            /* draw the window title bar */
            korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){.xy.x = window->size.xy.x, .xy.y = context->style.windowTitleBarPixelSizeY});
            korl_gfx_batchRectangleSetColor(batchWindowPanel, titleBarColor);
            korl_gfx_batchSetVertexColor(batchWindowPanel, 0, context->style.colorTitleBar);
            korl_gfx_batchSetVertexColor(batchWindowPanel, 1, context->style.colorTitleBar);
            korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            /* draw the window title bar text */
            Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(context->allocatorStack, context->style.fontWindowText, window->identifier, context->style.windowTextPixelSizeY);
            korl_gfx_batchSetPosition2d(batchWindowTitleText, (Korl_Math_V2f32){ .xy.x = window->position.xy.x
                                                                               , .xy.y = window->position.xy.y - context->style.windowTitleBarPixelSizeY + 3.f });
            korl_gfx_batch(batchWindowTitleText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            /* draw the window title bar close button */
            if(window->hasTitleBarButtonClose)
            {
                korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){.xy.x = context->style.windowTitleBarPixelSizeY, .xy.y = context->style.windowTitleBarPixelSizeY});
                Korl_Vulkan_Color colorButtonClose = context->style.colorTitleBar;
                if(context->identifierMouseHoveredWindow == window->identifier)
                {
                    const Korl_Math_V2f32 titleBarButtonCloseAabbMin = { .xy.x = window->position.xy.x + window->size.xy.x - context->style.windowTitleBarPixelSizeY
                                                                       , .xy.y = window->position.xy.y                     - context->style.windowTitleBarPixelSizeY };
                    const Korl_Math_V2f32 titleBarButtonCloseAabbMax = { .xy.x = window->position.xy.x + window->size.xy.x
                                                                       , .xy.y = window->position.xy.y };
                    if(    context->mouseHoverPosition.xy.x >= titleBarButtonCloseAabbMin.xy.x
                        && context->mouseHoverPosition.xy.x <= titleBarButtonCloseAabbMax.xy.x
                        && context->mouseHoverPosition.xy.y >= titleBarButtonCloseAabbMin.xy.y
                        && context->mouseHoverPosition.xy.y <= titleBarButtonCloseAabbMax.xy.y)
                    {
                        if(context->titlebarButtonFlagsMouseDown & KORL_GUI_MOUSE_TITLEBAR_BUTTON_FLAG_CLOSE)
                            colorButtonClose = context->style.colorButtonPressed;
                        else
                            colorButtonClose = context->style.colorButtonWindowCloseActive;
                    }
                }
                korl_gfx_batchRectangleSetColor(batchWindowPanel, colorButtonClose);
                korl_gfx_batchSetPosition2d(batchWindowPanel, (Korl_Math_V2f32){ .xy.x = window->position.xy.x + window->size.xy.x - context->style.windowTitleBarPixelSizeY
                                                                               , .xy.y = window->position.xy.y });
                korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                Korl_Gfx_Batch*const batchWindowTitleCloseIconPiece = korl_gfx_createBatchRectangleColored(context->allocatorStack, 
                                                                                                           (Korl_Math_V2f32){ .xy.x = 0.1f*context->style.windowTitleBarPixelSizeY
                                                                                                                            , .xy.y =      context->style.windowTitleBarPixelSizeY}, 
                                                                                                           (Korl_Math_V2f32){0.5f, 0.5f}, 
                                                                                                           context->style.colorButtonWindowTitleBarIcons);
                korl_gfx_batchSetPosition2d(batchWindowTitleCloseIconPiece, 
                                            (Korl_Math_V2f32){ .xy.x = window->position.xy.x + window->size.xy.x - context->style.windowTitleBarPixelSizeY/2.f
                                                             , .xy.y = window->position.xy.y - context->style.windowTitleBarPixelSizeY/2.f });
                korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z,  KORL_PI32*0.25f);
                korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z, -KORL_PI32*0.25f);
                korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            }//window->hasTitleBarButtonClose
        }//window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR
        /* render all this window's widgets within the window panel */
        _korl_gui_processWidgetGraphics(window, NULL, NULL, true);
    }
    KORL_MEMORY_POOL_RESIZE(context->windows, windowsRemaining);
}
korl_internal void korl_gui_widgetTextFormat(const wchar_t* textFormat, ...)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(textFormat, KORL_GUI_WIDGET_TYPE_TEXT);
    va_list vaList;
    va_start(vaList, textFormat);
    widget->subType.text.displayText = korl_memory_stringFormat(context->allocatorStack, textFormat, vaList);
    va_end(vaList);
}
korl_internal u8 korl_gui_widgetButtonFormat(const wchar_t* textFormat, ...)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(textFormat, KORL_GUI_WIDGET_TYPE_BUTTON);
    va_list vaList;
    va_start(vaList, textFormat);
    widget->subType.button.displayText = korl_memory_stringFormat(context->allocatorStack, textFormat, vaList);
    va_end(vaList);
    const u8 resultActuationCount = widget->subType.button.actuationCount;
    widget->subType.button.actuationCount = 0;
    return resultActuationCount;
}
