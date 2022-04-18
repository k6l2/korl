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
    //KORL-ISSUE-000-000-007
    return widget;
}
/**
 * When execution of this function completes, the AABB of the window's widgets 
 * will be cached in the \c window parameter's \c cachedAabb field.
 */
korl_internal void _korl_gui_processWidgetGraphics(_Korl_Gui_Window*const window, bool batchGraphics, f32 contentCursorOffsetX, f32 contentCursorOffsetY)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    if(window->isContentHidden)
        return;
    Korl_Math_V2f32 widgetCursor = window->position;
    if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        widgetCursor.xy.y -= context->style.windowTitleBarPixelSizeY;
    widgetCursor.xy.x -= contentCursorOffsetX;
    widgetCursor.xy.y += contentCursorOffsetY;
    window->cachedContentAabb.min = window->cachedContentAabb.max = widgetCursor;
    for(u$ j = 0; j < KORL_MEMORY_POOL_SIZE(context->widgets); ++j)
    {
        _Korl_Gui_Widget*const widget = &context->widgets[j];
        if(widget->parentWindowIdentifier != window->identifier)
            continue;
        widgetCursor.xy.y -= context->style.widgetSpacingY;
        switch(widget->type)
        {
        case KORL_GUI_WIDGET_TYPE_TEXT:{
            Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, widget->subType.text.displayText, context->style.windowTextPixelSizeY);
            const f32 batchTextAabbSizeX = korl_gfx_batchTextGetAabbSizeX(batchText);
            const f32 batchTextAabbSizeY = korl_gfx_batchTextGetAabbSizeY(batchText);
            widget->cachedAabb.min = korl_math_v2f32_subtract(widgetCursor, (Korl_Math_V2f32){.xy.x =                  0, .xy.y = batchTextAabbSizeY});
            widget->cachedAabb.max = korl_math_v2f32_add     (widgetCursor, (Korl_Math_V2f32){.xy.x = batchTextAabbSizeX, .xy.y =                  0});
            widget->cachedIsInteractive = false;
            if(batchGraphics)
            {
                //KORL-ISSUE-000-000-008
                korl_gfx_batchSetPosition2d(batchText, 
                                            widgetCursor.xy.x, 
                                            widgetCursor.xy.y - batchTextAabbSizeY);
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            }
            break;}
        case KORL_GUI_WIDGET_TYPE_BUTTON:{
            Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, widget->subType.button.displayText, context->style.windowTextPixelSizeY);
            const f32 batchTextAabbSizeX = korl_gfx_batchTextGetAabbSizeX(batchText);
            const f32 batchTextAabbSizeY = korl_gfx_batchTextGetAabbSizeY(batchText);
            const f32 buttonAabbSizeX = batchTextAabbSizeX + context->style.widgetButtonLabelMargin * 2.f;
            const f32 buttonAabbSizeY = batchTextAabbSizeY + context->style.widgetButtonLabelMargin * 2.f;
            widget->cachedAabb.min = korl_math_v2f32_subtract(widgetCursor, (Korl_Math_V2f32){.xy.x =               0, .xy.y = buttonAabbSizeY});
            widget->cachedAabb.max = korl_math_v2f32_add     (widgetCursor, (Korl_Math_V2f32){.xy.x = buttonAabbSizeX, .xy.y =               0});
            widget->cachedIsInteractive = true;
            if(batchGraphics)
            {
                Korl_Gfx_Batch*const batchButton = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                                                        (Korl_Math_V2f32){ .xy.x = buttonAabbSizeX
                                                                                                         , .xy.y = buttonAabbSizeY}, 
                                                                                        (Korl_Math_V2f32){.xy.x = 0.f, .xy.y = 1.f}, 
                                                                                        context->style.colorButtonInactive);
                korl_gfx_batchSetPosition2dV2f32(batchButton, widgetCursor);
                if(korl_math_aabb2f32_contains(widget->cachedAabb, context->mouseHoverPosition.xy.x, context->mouseHoverPosition.xy.y))
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
                //KORL-ISSUE-000-000-008
                korl_gfx_batchSetPosition2d(batchText, 
                                            widgetCursor.xy.x + context->style.widgetButtonLabelMargin, 
                                            widgetCursor.xy.y - context->style.widgetButtonLabelMargin - batchTextAabbSizeY);
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            }
            break;}
        }
        window->cachedContentAabb = korl_math_aabb2f32_union(window->cachedContentAabb, widget->cachedAabb);
        widgetCursor.xy.y -= widget->cachedAabb.max.xy.y - widget->cachedAabb.min.xy.y;
    }
}
korl_internal void korl_gui_initialize(void)
{
    korl_memory_nullify(&_korl_gui_context, sizeof(_korl_gui_context));
    _korl_gui_context.allocatorHandleStack                 = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_megabytes(1));
    _korl_gui_context.style.colorWindow                    = (Korl_Vulkan_Color){.rgb.r =  16, .rgb.g =  16, .rgb.b =  16};
    _korl_gui_context.style.colorWindowActive              = (Korl_Vulkan_Color){.rgb.r =  24, .rgb.g =  24, .rgb.b =  24};
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
    _korl_gui_context.style.colorScrollBar                 = (Korl_Vulkan_Color){.rgb.r =   8, .rgb.g =   8, .rgb.b =   8};
    _korl_gui_context.style.colorScrollBarActive           = (Korl_Vulkan_Color){.rgb.r =  32, .rgb.g =  32, .rgb.b =  32};
    _korl_gui_context.style.colorScrollBarPressed          = (Korl_Vulkan_Color){.rgb.r =   0, .rgb.g =   0, .rgb.b =   0};
    _korl_gui_context.style.fontWindowText                 = NULL;// just use the default font inside korl-gfx
    _korl_gui_context.style.windowTextPixelSizeY           = 16.f;
    _korl_gui_context.style.windowTitleBarPixelSizeY       = 20.f;
    _korl_gui_context.style.widgetSpacingY                 = 3.f;
    _korl_gui_context.style.widgetButtonLabelMargin        = 4.f;
    _korl_gui_context.style.windowScrollBarPixelWidth      = 12.f;
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
            if(!context->windows[i].isOpen && out_isOpen && *out_isOpen)
            {
                context->windows[i].isFirstFrame    = true;
                context->windows[i].isContentHidden = false;
            }
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
    newWindow->isOpen       = true;
    context->currentWindowIndex = korl_checkCast_u$_to_u8(KORL_MEMORY_POOL_SIZE(context->windows) - 1);
done_currentWindowIndexValid:
    newWindow = &context->windows[context->currentWindowIndex];
    newWindow->usedThisFrame       = true;
    newWindow->styleFlags          = styleFlags;
    newWindow->specialWidgetFlags  = KORL_GUI_SPECIAL_WIDGET_FLAGS_NONE;
    if(out_isOpen)
    {
        newWindow->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE;
        newWindow->isOpen = *out_isOpen;
        if(newWindow->specialWidgetFlagsPressed & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
            newWindow->isOpen = false;
        *out_isOpen = newWindow->isOpen;
    }
    else
        newWindow->specialWidgetFlags &= ~KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE;
    newWindow->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE;
    if(newWindow->specialWidgetFlagsPressed & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
    {
        newWindow->isContentHidden = !newWindow->isContentHidden;
        if(newWindow->isContentHidden)
            newWindow->hiddenContentPreviousSizeY = newWindow->size.xy.y;
        else
            newWindow->size.xy.y = newWindow->hiddenContentPreviousSizeY;
    }
    newWindow->specialWidgetFlagsPressed = KORL_GUI_SPECIAL_WIDGET_FLAGS_NONE;
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
    korl_memory_allocator_empty(context->allocatorHandleStack);
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
            _korl_gui_processWidgetGraphics(window, false, 0.f, 0.f);
            Korl_Math_Aabb2f32 windowTotalAabb = window->cachedContentAabb;
            /* take the AABB of the window's title bar into account as well */
            if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            {
                Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, window->identifier, context->style.windowTextPixelSizeY);
                f32 titleBarOptimalSizeX = korl_gfx_batchTextGetAabbSizeX(batchWindowTitleText);
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
                    titleBarOptimalSizeX += context->style.windowTitleBarPixelSizeY;
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
                    titleBarOptimalSizeX += context->style.windowTitleBarPixelSizeY;
                windowTotalAabb.max.xy.x = KORL_MATH_MAX(windowTotalAabb.max.xy.x, window->position.xy.x + titleBarOptimalSizeX);
                windowTotalAabb.max.xy.y += context->style.windowTitleBarPixelSizeY;
            }
            /* size the window based on the above metrics */
            window->size = korl_math_aabb2f32_size(windowTotalAabb);
            /* prevent the window from being too small */
            if(window->size.xy.y < context->style.windowTitleBarPixelSizeY)
                window->size.xy.y = context->style.windowTitleBarPixelSizeY;
            if(window->size.xy.x < context->style.windowTitleBarPixelSizeY)
                window->size.xy.x = context->style.windowTitleBarPixelSizeY;
        }
        if(window->isContentHidden)
            window->size.xy.y = context->style.windowTitleBarPixelSizeY;
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
        /* draw the window panel */
        korl_gfx_cameraSetScissorPercent(&guiCamera, 0,0, 1,1);
        korl_gfx_useCamera(guiCamera);
        Korl_Gfx_Batch*const batchWindowPanel = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, window->size, KORL_ORIGIN_RATIO_UPPER_LEFT, windowColor);
        korl_gfx_batchSetPosition2dV2f32(batchWindowPanel, window->position);
        korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        /* prepare to draw all the window's contents by cutting out a scissor 
            region the size of the window, preventing us from accidentally 
            render any pixels outside the window */
        korl_gfx_cameraSetScissor(&guiCamera, 
                                   window->position.xy.x, 
                                  -window->position.xy.y/*inverted, because remember: korl-gui window-space uses _correct_ y-axis direction (+y is UP)*/, 
                                   window->size.xy.x, 
                                   window->size.xy.y);
        korl_gfx_useCamera(guiCamera);
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        {
            /* draw the window title bar */
            korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){.xy.x = window->size.xy.x, .xy.y = context->style.windowTitleBarPixelSizeY});
            korl_gfx_batchRectangleSetColor(batchWindowPanel, titleBarColor);
            korl_gfx_batchSetVertexColor(batchWindowPanel, 0, context->style.colorTitleBar);
            korl_gfx_batchSetVertexColor(batchWindowPanel, 1, context->style.colorTitleBar);
            korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            /* draw the window title bar text */
            Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, window->identifier, context->style.windowTextPixelSizeY);
            korl_gfx_batchSetPosition2d(batchWindowTitleText, 
                                        window->position.xy.x, 
                                        window->position.xy.y - context->style.windowTitleBarPixelSizeY + 3.f);
            korl_gfx_batch(batchWindowTitleText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            /**/
            Korl_Math_V2f32 titlebarButtonCursor = { .xy.x = window->position.xy.x + window->size.xy.x - context->style.windowTitleBarPixelSizeY
                                                   , .xy.y = window->position.xy.y };
            if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
            {
                const Korl_Math_Aabb2f32 buttonAabb = korl_math_aabb2f32_fromPoints(titlebarButtonCursor.xy.x, titlebarButtonCursor.xy.y - context->style.windowTitleBarPixelSizeY, 
                                                                                    titlebarButtonCursor.xy.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.xy.y);
                korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){.xy.x = context->style.windowTitleBarPixelSizeY, .xy.y = context->style.windowTitleBarPixelSizeY});
                Korl_Vulkan_Color colorTitleBarButton = context->style.colorTitleBar;
                if(    context->identifierMouseHoveredWindow == window->identifier
                    && korl_math_aabb2f32_containsV2f32(buttonAabb, context->mouseHoverPosition))
                    if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
                        colorTitleBarButton = context->style.colorButtonPressed;
                    else
                        colorTitleBarButton = context->style.colorButtonWindowCloseActive;
                korl_gfx_batchRectangleSetColor(batchWindowPanel, colorTitleBarButton);
                korl_gfx_batchSetPosition2dV2f32(batchWindowPanel, titlebarButtonCursor);
                korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                Korl_Gfx_Batch*const batchWindowTitleCloseIconPiece = 
                    korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                         (Korl_Math_V2f32){ .xy.x = 0.1f*context->style.windowTitleBarPixelSizeY
                                                                          , .xy.y =      context->style.windowTitleBarPixelSizeY}, 
                                                         (Korl_Math_V2f32){0.5f, 0.5f}, 
                                                         context->style.colorButtonWindowTitleBarIcons);
                korl_gfx_batchSetPosition2d(batchWindowTitleCloseIconPiece, 
                                            titlebarButtonCursor.xy.x + context->style.windowTitleBarPixelSizeY/2.f, 
                                            titlebarButtonCursor.xy.y - context->style.windowTitleBarPixelSizeY/2.f);
                korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z,  KORL_PI32*0.25f);
                korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z, -KORL_PI32*0.25f);
                korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                titlebarButtonCursor.xy.x -= context->style.windowTitleBarPixelSizeY;
            }//window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE
            if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
            {
                const Korl_Math_Aabb2f32 buttonAabb = korl_math_aabb2f32_fromPoints(titlebarButtonCursor.xy.x, titlebarButtonCursor.xy.y - context->style.windowTitleBarPixelSizeY, 
                                                                                    titlebarButtonCursor.xy.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.xy.y);
                korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){.xy.x = context->style.windowTitleBarPixelSizeY, .xy.y = context->style.windowTitleBarPixelSizeY});
                Korl_Vulkan_Color colorTitleBarButton = context->style.colorTitleBar;
                if(    context->identifierMouseHoveredWindow == window->identifier
                    && korl_math_aabb2f32_containsV2f32(buttonAabb, context->mouseHoverPosition))
                    if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
                        colorTitleBarButton = context->style.colorButtonPressed;
                    else
                        colorTitleBarButton = context->style.colorTitleBarActive;
                korl_gfx_batchRectangleSetColor(batchWindowPanel, colorTitleBarButton);
                korl_gfx_batchSetPosition2dV2f32(batchWindowPanel, titlebarButtonCursor);
                korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                Korl_Gfx_Batch*const batchWindowTitleIconPiece = 
                    korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                         (Korl_Math_V2f32){ .xy.x =      context->style.windowTitleBarPixelSizeY
                                                                          , .xy.y = 0.1f*context->style.windowTitleBarPixelSizeY}, 
                                                         (Korl_Math_V2f32){0.5f, 0.5f}, 
                                                         context->style.colorButtonWindowTitleBarIcons);
                korl_gfx_batchSetPosition2d(batchWindowTitleIconPiece, 
                                            titlebarButtonCursor.xy.x + context->style.windowTitleBarPixelSizeY/2.f, 
                                            titlebarButtonCursor.xy.y - context->style.windowTitleBarPixelSizeY/2.f);
                korl_gfx_batch(batchWindowTitleIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                titlebarButtonCursor.xy.x -= context->style.windowTitleBarPixelSizeY;
            }//window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE
        }//window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR
        /* render all this window's widgets within the window panel */
        // calculate how much area there is for the window's widgets to occupy & 
        //  how much visible space is available in the window //
        Korl_Math_V2f32 availableContentArea = window->size;
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            availableContentArea.xy.y -= context->style.windowTitleBarPixelSizeY;
        const Korl_Math_V2f32 contentSize = korl_math_aabb2f32_size(window->cachedContentAabb);
        // determine which scrollbars we need to use to view all the content //
        window->specialWidgetFlags &= ~KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
        window->specialWidgetFlags &= ~KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
        if(contentSize.xy.x > availableContentArea.xy.x)
        {
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
            availableContentArea.xy.y -= context->style.windowScrollBarPixelWidth;
        }
        if(contentSize.xy.y > availableContentArea.xy.y)
        {
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
            availableContentArea.xy.x -= context->style.windowScrollBarPixelWidth;
        }
        // re-evaluate the presence of each scrollbar, only modifying the 
        //  available content area if the scrollbar wasn't already taken into 
        //  account, since the presence of one scrollbar can add the requirement 
        //  of another //
        if(contentSize.xy.x > availableContentArea.xy.x)
        {
            if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X))
                availableContentArea.xy.y -= context->style.windowScrollBarPixelWidth;
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
        }
        if(contentSize.xy.y > availableContentArea.xy.y)
        {
            if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y))
                availableContentArea.xy.x -= context->style.windowScrollBarPixelWidth;
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
        }
        window->cachedAvailableContentSize = availableContentArea;
        // do scroll bar logic to determine the adjusted widget cursor position //
        korl_shared_const f32 SCROLLBAR_LENGTH_MIN = 8.f;
        Korl_Gfx_Batch* batchScrollBarX = NULL;
        Korl_Gfx_Batch* batchScrollBarY = NULL;
        if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X)
        {
            /* determine scroll bar length */
            window->cachedScrollBarLengthX = availableContentArea.xy.x * availableContentArea.xy.x / contentSize.xy.x;
            window->cachedScrollBarLengthX = KORL_MATH_MAX(window->cachedScrollBarLengthX, SCROLLBAR_LENGTH_MIN);
            /* bind the scroll bar to the current possible range */
            const f32 scrollBarRangeX = availableContentArea.xy.x - window->cachedScrollBarLengthX;
            window->scrollBarPositionX = KORL_MATH_CLAMP(window->scrollBarPositionX, 0.f, scrollBarRangeX);
            const f32 scrollBarPositionX = window->position.xy.x + window->scrollBarPositionX;
            /* batch scroll bar graphics */
            batchScrollBarX = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                                   (Korl_Math_V2f32){window->cachedScrollBarLengthX, context->style.windowScrollBarPixelWidth}, 
                                                                   (Korl_Math_V2f32){0.f, 0.f}, 
                                                                   context->style.colorScrollBar);
            if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X)
                korl_gfx_batchRectangleSetColor(batchScrollBarX, context->style.colorScrollBarPressed);
            else if(context->identifierMouseHoveredWindow == window->identifier)
            {
                const Korl_Math_Aabb2f32 scrollBarAabb = korl_math_aabb2f32_fromPoints(scrollBarPositionX                                 , window->position.xy.y - window->size.xy.y, 
                                                                                       scrollBarPositionX + window->cachedScrollBarLengthX, window->position.xy.y - window->size.xy.y + context->style.windowScrollBarPixelWidth);
                if(korl_math_aabb2f32_containsV2f32(scrollBarAabb, context->mouseHoverPosition))
                    korl_gfx_batchRectangleSetColor(batchScrollBarX, context->style.colorScrollBarActive);
            }
            korl_gfx_batchSetPosition2d(batchScrollBarX, scrollBarPositionX, window->position.xy.y - window->size.xy.y);
        }
        if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y)
        {
            /* determine scroll bar length */
            window->cachedScrollBarLengthY = availableContentArea.xy.y * availableContentArea.xy.y / contentSize.xy.y;
            window->cachedScrollBarLengthY = KORL_MATH_MAX(window->cachedScrollBarLengthY, SCROLLBAR_LENGTH_MIN);
            /* bind the scroll bar to the current possible range */
            const f32 scrollBarRangeY = ((window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR) ? window->size.xy.y - context->style.windowTitleBarPixelSizeY : window->size.xy.y)
                                      - window->cachedScrollBarLengthY;
            window->scrollBarPositionY = KORL_MATH_CLAMP(window->scrollBarPositionY, 0.f, scrollBarRangeY);
            f32 scrollBarPositionY = window->position.xy.y - window->scrollBarPositionY;
            if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
                scrollBarPositionY -= context->style.windowTitleBarPixelSizeY;
            /* batch scroll bar graphics */
            batchScrollBarY = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                                   (Korl_Math_V2f32){context->style.windowScrollBarPixelWidth, window->cachedScrollBarLengthY}, 
                                                                   (Korl_Math_V2f32){1.f, 1.f}, 
                                                                   context->style.colorScrollBar);
            if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y)
                korl_gfx_batchRectangleSetColor(batchScrollBarY, context->style.colorScrollBarPressed);
            else if(context->identifierMouseHoveredWindow == window->identifier)
            {
                const Korl_Math_Aabb2f32 scrollBarAabb = korl_math_aabb2f32_fromPoints(window->position.xy.x + window->size.xy.x                                           , scrollBarPositionY, 
                                                                                       window->position.xy.x + window->size.xy.x - context->style.windowScrollBarPixelWidth, scrollBarPositionY - window->cachedScrollBarLengthY);
                if(korl_math_aabb2f32_containsV2f32(scrollBarAabb, context->mouseHoverPosition))
                    korl_gfx_batchRectangleSetColor(batchScrollBarY, context->style.colorScrollBarActive);
            }
            korl_gfx_batchSetPosition2d(batchScrollBarY, window->position.xy.x + window->size.xy.x, scrollBarPositionY);
        }
        if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X))
            window->scrollBarPositionX = 0;
        if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y))
            window->scrollBarPositionY = 0;
        // convert scroll bar positions to content cursor offset //
        const f32 scrollBarRangeX = availableContentArea.xy.x - window->cachedScrollBarLengthX;
        const f32 scrollBarRangeY = ((window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR) ? window->size.xy.y - context->style.windowTitleBarPixelSizeY : window->size.xy.y)
                                  - window->cachedScrollBarLengthY;
        const f32 scrollBarRatioX = korl_math_isNearlyZero(scrollBarRangeX) ? 0 : window->scrollBarPositionX / scrollBarRangeX;
        const f32 scrollBarRatioY = korl_math_isNearlyZero(scrollBarRangeY) ? 0 : window->scrollBarPositionY / scrollBarRangeY;
        const f32 contentCursorOffsetX = scrollBarRatioX * (contentSize.xy.x - availableContentArea.xy.x);
        const f32 contentCursorOffsetY = scrollBarRatioY * (contentSize.xy.y - availableContentArea.xy.y);
        // before drawing the window contents, we need to make sure it will not 
        //  draw on top of the title bar if there is one present //
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        {
            korl_gfx_cameraSetScissor(&guiCamera, 
                                      window->position.xy.x, 
                                      /*natural +Y=>UP space => swap chain space conversion*/-window->position.xy.y + context->style.windowTitleBarPixelSizeY, 
                                      window->size.xy.x, 
                                      window->size.xy.y - context->style.windowTitleBarPixelSizeY);
            korl_gfx_useCamera(guiCamera);
        }
        _korl_gui_processWidgetGraphics(window, true, contentCursorOffsetX, contentCursorOffsetY);
        /* draw scroll bars, if they are needed */
        if(batchScrollBarX)
            korl_gfx_batch(batchScrollBarX, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        if(batchScrollBarY)
            korl_gfx_batch(batchScrollBarY, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        /* batch window border AFTER contents are drawn */
        Korl_Vulkan_Color colorBorder = context->style.colorWindowBorder;
        if(context->isTopLevelWindowActive && i == KORL_MEMORY_POOL_SIZE(context->windows) - 1)
            if(context->isMouseHovering 
                && context->identifierMouseHoveredWindow == window->identifier
                && context->mouseHoverWindowEdgeFlags == KORL_GUI_MOUSE_HOVER_FLAGS_NONE)
                colorBorder = context->style.colorWindowBorderResize;
            else
                colorBorder = context->style.colorWindowBorderActive;
        else if(context->isMouseHovering && context->identifierMouseHoveredWindow == window->identifier)
            colorBorder = context->style.colorWindowBorderHovered;
        const Korl_Vulkan_Color colorBorderUp    = (context->identifierMouseHoveredWindow == window->identifier && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_UP   )) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Vulkan_Color colorBorderRight = (context->identifierMouseHoveredWindow == window->identifier && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_RIGHT)) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Vulkan_Color colorBorderDown  = (context->identifierMouseHoveredWindow == window->identifier && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_DOWN )) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Vulkan_Color colorBorderLeft  = (context->identifierMouseHoveredWindow == window->identifier && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_LEFT )) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Math_Aabb2f32 windowAabb = korl_math_aabb2f32_fromPoints(window->position.xy.x                    , window->position.xy.y - window->size.xy.y, 
                                                                            window->position.xy.x + window->size.xy.x, window->position.xy.y                    );
        Korl_Gfx_Batch*const batchWindowBorder = korl_gfx_createBatchLines(context->allocatorHandleStack, 4);
        //KORL-ISSUE-000-000-009
        korl_gfx_batchSetLine(batchWindowBorder, 0, (Korl_Vulkan_Position){windowAabb.min.xy.x - 0.5f, windowAabb.max.xy.y}, (Korl_Vulkan_Position){windowAabb.max.xy.x, windowAabb.max.xy.y}, colorBorderUp);
        korl_gfx_batchSetLine(batchWindowBorder, 1, (Korl_Vulkan_Position){windowAabb.max.xy.x       , windowAabb.max.xy.y}, (Korl_Vulkan_Position){windowAabb.max.xy.x, windowAabb.min.xy.y}, colorBorderRight);
        korl_gfx_batchSetLine(batchWindowBorder, 2, (Korl_Vulkan_Position){windowAabb.max.xy.x       , windowAabb.min.xy.y}, (Korl_Vulkan_Position){windowAabb.min.xy.x, windowAabb.min.xy.y}, colorBorderDown);
        korl_gfx_batchSetLine(batchWindowBorder, 3, (Korl_Vulkan_Position){windowAabb.min.xy.x       , windowAabb.min.xy.y}, (Korl_Vulkan_Position){windowAabb.min.xy.x, windowAabb.max.xy.y}, colorBorderLeft);
        korl_gfx_cameraSetScissorPercent(&guiCamera, 0,0, 1,1);
        korl_gfx_useCamera(guiCamera);
        korl_gfx_batch(batchWindowBorder, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    }
    KORL_MEMORY_POOL_RESIZE(context->windows, windowsRemaining);
}
korl_internal void korl_gui_widgetTextFormat(const wchar_t* textFormat, ...)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(textFormat, KORL_GUI_WIDGET_TYPE_TEXT);
    va_list vaList;
    va_start(vaList, textFormat);
    widget->subType.text.displayText = korl_memory_stringFormat(context->allocatorHandleStack, textFormat, vaList);
    va_end(vaList);
}
korl_internal u8 korl_gui_widgetButtonFormat(const wchar_t* textFormat, ...)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(textFormat, KORL_GUI_WIDGET_TYPE_BUTTON);
    va_list vaList;
    va_start(vaList, textFormat);
    widget->subType.button.displayText = korl_memory_stringFormat(context->allocatorHandleStack, textFormat, vaList);
    va_end(vaList);
    const u8 resultActuationCount = widget->subType.button.actuationCount;
    widget->subType.button.actuationCount = 0;
    return resultActuationCount;
}
