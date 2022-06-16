#include "korl-gui.h"
#include "korl-memory.h"
#include "korl-math.h"
#include "korl-gfx.h"
#include "korl-gui-internal-common.h"
#include "korl-time.h"
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
    korl_memory_zero(widget, sizeof(*widget));
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
        widgetCursor.y -= context->style.windowTitleBarPixelSizeY;
    widgetCursor.x -= contentCursorOffsetX;
    widgetCursor.y += contentCursorOffsetY;
    window->cachedContentAabb.min = window->cachedContentAabb.max = widgetCursor;
    for(u$ j = 0; j < KORL_MEMORY_POOL_SIZE(context->widgets); ++j)
    {
        _Korl_Gui_Widget*const widget = &context->widgets[j];
        if(widget->parentWindowIdentifier != window->identifier)
            continue;
        widgetCursor.y -= context->style.widgetSpacingY;
        switch(widget->type)
        {
        case KORL_GUI_WIDGET_TYPE_TEXT:{
            korl_time_probeStart(widget_text);
            korl_time_probeStart(create_batch_text);
            Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, widget->subType.text.displayText, context->style.windowTextPixelSizeY);
            korl_time_probeStop(create_batch_text);
            const Korl_Math_Aabb2f32 batchTextAabb = korl_gfx_batchTextGetAabb(batchText);
            const Korl_Math_V2f32 batchTextAabbSize = korl_math_aabb2f32_size(batchTextAabb);
            widget->cachedAabb.min = korl_math_v2f32_subtract(widgetCursor, (Korl_Math_V2f32){               0.f, batchTextAabbSize.y});
            widget->cachedAabb.max = korl_math_v2f32_add     (widgetCursor, (Korl_Math_V2f32){batchTextAabbSize.x,                0.f});
            widget->cachedIsInteractive = false;
            if(batchGraphics)
            {
                //KORL-ISSUE-000-000-008: instead of using the AABB of this text batch, we should be using the font's metrics!  Probably??  Different text batches of the same font will yield different sizes here, which will cause widget sizes to vary...
                korl_gfx_batchSetPosition2d(batchText, widgetCursor.x, widgetCursor.y);
                korl_time_probeStart(gfx_batch);
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                korl_time_probeStop(gfx_batch);
            }
            korl_time_probeStop(widget_text);
            break;}
        case KORL_GUI_WIDGET_TYPE_BUTTON:{
            Korl_Gfx_Batch*const batchText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, widget->subType.button.displayText, context->style.windowTextPixelSizeY);
            const Korl_Math_Aabb2f32 batchTextAabb = korl_gfx_batchTextGetAabb(batchText);
            const Korl_Math_V2f32 batchTextAabbSize = korl_math_aabb2f32_size(batchTextAabb);
            const f32 buttonAabbSizeX = batchTextAabbSize.x + context->style.widgetButtonLabelMargin * 2.f;
            const f32 buttonAabbSizeY = batchTextAabbSize.y + context->style.widgetButtonLabelMargin * 2.f;
            widget->cachedAabb.min = korl_math_v2f32_subtract(widgetCursor, (Korl_Math_V2f32){            0.f, buttonAabbSizeY});
            widget->cachedAabb.max = korl_math_v2f32_add     (widgetCursor, (Korl_Math_V2f32){buttonAabbSizeX,             0.f});
            widget->cachedIsInteractive = true;
            if(batchGraphics)
            {
                Korl_Gfx_Batch*const batchButton = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                                                        (Korl_Math_V2f32){ buttonAabbSizeX
                                                                                                         , buttonAabbSizeY}, 
                                                                                        (Korl_Math_V2f32){0.f, 1.f}, 
                                                                                        context->style.colorButtonInactive);
                korl_gfx_batchSetPosition2dV2f32(batchButton, widgetCursor);
                if(korl_math_aabb2f32_containsV2f32(widget->cachedAabb, context->mouseHoverPosition))
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
                //KORL-ISSUE-000-000-008: instead of using the AABB of this text batch, we should be using the font's metrics!  Probably??  Different text batches of the same font will yield different sizes here, which will cause widget sizes to vary...
                korl_gfx_batchSetPosition2d(batchText, 
                                            widgetCursor.x + context->style.widgetButtonLabelMargin, 
                                            widgetCursor.y - context->style.widgetButtonLabelMargin);
                korl_gfx_batch(batchText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            }
            break;}
        }
        window->cachedContentAabb = korl_math_aabb2f32_union(window->cachedContentAabb, widget->cachedAabb);
        widgetCursor.y -= widget->cachedAabb.max.y - widget->cachedAabb.min.y;
    }
}
korl_internal void korl_gui_initialize(void)
{
    korl_memory_zero(&_korl_gui_context, sizeof(_korl_gui_context));
    _korl_gui_context.allocatorHandleStack                 = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, korl_math_megabytes(1), L"korl-gui-stack", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, NULL/*let platform choose address*/);
    _korl_gui_context.style.colorWindow                    = (Korl_Vulkan_Color4u8){ 16,  16,  16, 200};
    _korl_gui_context.style.colorWindowActive              = (Korl_Vulkan_Color4u8){ 24,  24,  24, 230};
    _korl_gui_context.style.colorWindowBorder              = (Korl_Vulkan_Color4u8){  0,   0,   0, 230};
    _korl_gui_context.style.colorWindowBorderHovered       = (Korl_Vulkan_Color4u8){  0,  32,   0, 255};
    _korl_gui_context.style.colorWindowBorderResize        = (Korl_Vulkan_Color4u8){255, 255, 255, 255};
    _korl_gui_context.style.colorWindowBorderActive        = (Korl_Vulkan_Color4u8){ 60, 125,  50, 255};
    _korl_gui_context.style.colorTitleBar                  = (Korl_Vulkan_Color4u8){  0,  32,   0, 255};
    _korl_gui_context.style.colorTitleBarActive            = (Korl_Vulkan_Color4u8){ 60, 125,  50, 255};
    _korl_gui_context.style.colorButtonInactive            = (Korl_Vulkan_Color4u8){  0,  32,   0, 255};
    _korl_gui_context.style.colorButtonActive              = (Korl_Vulkan_Color4u8){ 60, 125,  50, 255};
    _korl_gui_context.style.colorButtonPressed             = (Korl_Vulkan_Color4u8){  0,   8,   0, 255};
    _korl_gui_context.style.colorButtonWindowTitleBarIcons = (Korl_Vulkan_Color4u8){255, 255, 255, 255};
    _korl_gui_context.style.colorButtonWindowCloseActive   = (Korl_Vulkan_Color4u8){255,   0,   0, 255};
    _korl_gui_context.style.colorScrollBar                 = (Korl_Vulkan_Color4u8){  8,   8,   8, 255};
    _korl_gui_context.style.colorScrollBarActive           = (Korl_Vulkan_Color4u8){ 32,  32,  32, 255};
    _korl_gui_context.style.colorScrollBarPressed          = (Korl_Vulkan_Color4u8){  0,   0,   0, 255};
    _korl_gui_context.style.fontWindowText                 = NULL;// just use the default font inside korl-gfx
    _korl_gui_context.style.windowTextPixelSizeY           = 24.f;
    _korl_gui_context.style.windowTitleBarPixelSizeY       = 20.f;
    _korl_gui_context.style.widgetSpacingY                 = 0.f;
    _korl_gui_context.style.widgetButtonLabelMargin        = 4.f;
    _korl_gui_context.style.windowScrollBarPixelWidth      = 12.f;
}
korl_internal KORL_PLATFORM_GUI_SET_FONT_ASSET(korl_gui_setFontAsset)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    if(!fontAssetName)
    {
        context->style.fontWindowText = NULL;
        return;
    }
    const i$ resultStringCopy = korl_memory_stringCopy(fontAssetName, context->fontAssetName, korl_arraySize(context->fontAssetName));
    korl_assert(resultStringCopy > 0);
    context->style.fontWindowText = context->fontAssetName;
}
korl_internal KORL_PLATFORM_GUI_WINDOW_BEGIN(korl_gui_windowBegin)
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
    korl_memory_zero(newWindow, sizeof(*newWindow));
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
            newWindow->hiddenContentPreviousSizeY = newWindow->size.y;
        else
            newWindow->size.y = newWindow->hiddenContentPreviousSizeY;
    }
    newWindow->specialWidgetFlagsPressed = KORL_GUI_SPECIAL_WIDGET_FLAGS_NONE;
}
korl_internal KORL_PLATFORM_GUI_WINDOW_END(korl_gui_windowEnd)
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
    korl_time_probeStart(nullify_unused_widgets);
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
    korl_time_probeStop(nullify_unused_widgets);
    /* for each of the windows that we ended up using for this frame, generate 
        the necessary draw commands for them */
    korl_time_probeStart(generate_draw_commands);
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
        Korl_Vulkan_Color4u8 windowColor   = context->style.colorWindow;
        Korl_Vulkan_Color4u8 titleBarColor = context->style.colorTitleBar;
        if(context->isTopLevelWindowActive && i == KORL_MEMORY_POOL_SIZE(context->windows) - 1)
        {
            windowColor   = context->style.colorWindowActive;
            titleBarColor = context->style.colorTitleBarActive;
        }
        if(window->isFirstFrame || (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE))
        {
            korl_time_probeStart(calculate_window_AABB);
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
                const Korl_Math_V2f32 batchTextSize = korl_math_aabb2f32_size(korl_gfx_batchTextGetAabb(batchWindowTitleText));
                f32 titleBarOptimalSizeX = batchTextSize.x;
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
                    titleBarOptimalSizeX += context->style.windowTitleBarPixelSizeY;
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
                    titleBarOptimalSizeX += context->style.windowTitleBarPixelSizeY;
                windowTotalAabb.max.x = KORL_MATH_MAX(windowTotalAabb.max.x, window->position.x + titleBarOptimalSizeX);
                windowTotalAabb.max.y += context->style.windowTitleBarPixelSizeY;
            }
            /* size the window based on the above metrics */
            window->size = korl_math_aabb2f32_size(windowTotalAabb);
            /* prevent the window from being too small */
            KORL_MATH_ASSIGN_CLAMP_MIN(window->size.x, context->style.windowTitleBarPixelSizeY);
            KORL_MATH_ASSIGN_CLAMP_MIN(window->size.y, context->style.windowTitleBarPixelSizeY);
            if(window->size.y < context->style.windowTitleBarPixelSizeY)
                window->size.y = context->style.windowTitleBarPixelSizeY;
            if(window->size.x < context->style.windowTitleBarPixelSizeY)
                window->size.x = context->style.windowTitleBarPixelSizeY;
            korl_time_probeStop(calculate_window_AABB);
        }
        if(window->isContentHidden)
            window->size.y = context->style.windowTitleBarPixelSizeY;
        /* bind the windows to the bounds of the swapchain, such that there will 
            always be a square of grabable geometry on the window whose 
            dimensions equal the height of the window title bar style at minimum */
        KORL_MATH_ASSIGN_CLAMP(window->position.x, 
                               -window->size.x + context->style.windowTitleBarPixelSizeY, 
                               swapChainSize.x - context->style.windowTitleBarPixelSizeY);
        KORL_MATH_ASSIGN_CLAMP(window->position.y, 
                               -KORL_C_CAST(f32, swapChainSize.y) + context->style.windowTitleBarPixelSizeY, 
                               0);
        /* draw the window panel */
        korl_time_probeStart(draw_window_panel);
        korl_gfx_cameraSetScissorPercent(&guiCamera, 0,0, 1,1);
        korl_gfx_useCamera(guiCamera);
        Korl_Gfx_Batch*const batchWindowPanel = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, window->size, KORL_ORIGIN_RATIO_UPPER_LEFT, windowColor);
        korl_gfx_batchSetPosition2dV2f32(batchWindowPanel, window->position);
        korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        /* prepare to draw all the window's contents by cutting out a scissor 
            region the size of the window, preventing us from accidentally 
            render any pixels outside the window */
        korl_gfx_cameraSetScissor(&guiCamera, 
                                   window->position.x, 
                                  -window->position.y/*inverted, because remember: korl-gui window-space uses _correct_ y-axis direction (+y is UP)*/, 
                                   window->size.x, 
                                   window->size.y);
        korl_gfx_useCamera(guiCamera);
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        {
            /* draw the window title bar */
            korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){window->size.x, context->style.windowTitleBarPixelSizeY});
            korl_gfx_batchRectangleSetColor(batchWindowPanel, titleBarColor);
            korl_gfx_batchSetVertexColor(batchWindowPanel, 0, context->style.colorTitleBar);
            korl_gfx_batchSetVertexColor(batchWindowPanel, 1, context->style.colorTitleBar);
            korl_gfx_batch(batchWindowPanel, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            /* draw the window title bar text */
            Korl_Gfx_Batch*const batchWindowTitleText = korl_gfx_createBatchText(context->allocatorHandleStack, context->style.fontWindowText, window->identifier, context->style.windowTextPixelSizeY);
            korl_gfx_batchSetPosition2d(batchWindowTitleText, 
                                        window->position.x, 
                                        window->position.y - context->style.windowTitleBarPixelSizeY + 3.f);
            korl_gfx_batch(batchWindowTitleText, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
            /**/
            Korl_Math_V2f32 titlebarButtonCursor = { window->position.x + window->size.x - context->style.windowTitleBarPixelSizeY
                                                   , window->position.y };
            if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
            {
                const Korl_Math_Aabb2f32 buttonAabb = korl_math_aabb2f32_fromPoints(titlebarButtonCursor.x, titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY, 
                                                                                    titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.y);
                korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){context->style.windowTitleBarPixelSizeY, context->style.windowTitleBarPixelSizeY});
                Korl_Vulkan_Color4u8 colorTitleBarButton = context->style.colorTitleBar;
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
                                                         (Korl_Math_V2f32){0.1f*context->style.windowTitleBarPixelSizeY
                                                                          ,     context->style.windowTitleBarPixelSizeY}, 
                                                         (Korl_Math_V2f32){0.5f, 0.5f}, 
                                                         context->style.colorButtonWindowTitleBarIcons);
                korl_gfx_batchSetPosition2d(batchWindowTitleCloseIconPiece, 
                                            titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY/2.f, 
                                            titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY/2.f);
                korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z,  KORL_PI32*0.25f);
                korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                korl_gfx_batchSetRotation(batchWindowTitleCloseIconPiece, KORL_MATH_V3F32_Z, -KORL_PI32*0.25f);
                korl_gfx_batch(batchWindowTitleCloseIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                titlebarButtonCursor.x -= context->style.windowTitleBarPixelSizeY;
            }//window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE
            if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
            {
                const Korl_Math_Aabb2f32 buttonAabb = korl_math_aabb2f32_fromPoints(titlebarButtonCursor.x, titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY, 
                                                                                    titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.y);
                korl_gfx_batchRectangleSetSize(batchWindowPanel, (Korl_Math_V2f32){context->style.windowTitleBarPixelSizeY, context->style.windowTitleBarPixelSizeY});
                Korl_Vulkan_Color4u8 colorTitleBarButton = context->style.colorTitleBar;
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
                                                         (Korl_Math_V2f32){     context->style.windowTitleBarPixelSizeY
                                                                          ,0.1f*context->style.windowTitleBarPixelSizeY}, 
                                                         (Korl_Math_V2f32){0.5f, 0.5f}, 
                                                         context->style.colorButtonWindowTitleBarIcons);
                korl_gfx_batchSetPosition2d(batchWindowTitleIconPiece, 
                                            titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY/2.f, 
                                            titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY/2.f);
                korl_gfx_batch(batchWindowTitleIconPiece, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
                titlebarButtonCursor.x -= context->style.windowTitleBarPixelSizeY;
            }//window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE
        }//window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR
        korl_time_probeStop(draw_window_panel);
        /* render all this window's widgets within the window panel */
        korl_time_probeStart(draw_widgets);
        // calculate how much area there is for the window's widgets to occupy & 
        //  how much visible space is available in the window //
        Korl_Math_V2f32 availableContentArea = window->size;
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            availableContentArea.y -= context->style.windowTitleBarPixelSizeY;
        const Korl_Math_V2f32 contentSize = korl_math_aabb2f32_size(window->cachedContentAabb);
        // determine which scrollbars we need to use to view all the content //
        window->specialWidgetFlags &= ~KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
        window->specialWidgetFlags &= ~KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
        if(contentSize.x > availableContentArea.x)
        {
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
            availableContentArea.y -= context->style.windowScrollBarPixelWidth;
        }
        if(contentSize.y > availableContentArea.y)
        {
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
            availableContentArea.x -= context->style.windowScrollBarPixelWidth;
        }
        // re-evaluate the presence of each scrollbar, only modifying the 
        //  available content area if the scrollbar wasn't already taken into 
        //  account, since the presence of one scrollbar can add the requirement 
        //  of another //
        if(contentSize.x > availableContentArea.x)
        {
            if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X))
                availableContentArea.y -= context->style.windowScrollBarPixelWidth;
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
        }
        if(contentSize.y > availableContentArea.y)
        {
            if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y))
                availableContentArea.x -= context->style.windowScrollBarPixelWidth;
            window->specialWidgetFlags |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
        }
        window->cachedAvailableContentSize = availableContentArea;
        // do scroll bar logic to determine the adjusted widget cursor position //
        korl_time_probeStart(scroll_bar_logic);
        korl_shared_const f32 SCROLLBAR_LENGTH_MIN = 8.f;
        Korl_Gfx_Batch* batchScrollBarX = NULL;
        Korl_Gfx_Batch* batchScrollBarY = NULL;
        if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X)
        {
            korl_time_probeStart(scroll_bar_x);
            /* determine scroll bar length */
            window->cachedScrollBarLengthX = availableContentArea.x * availableContentArea.x / contentSize.x;
            window->cachedScrollBarLengthX = KORL_MATH_MAX(window->cachedScrollBarLengthX, SCROLLBAR_LENGTH_MIN);
            /* bind the scroll bar to the current possible range */
            const f32 scrollBarRangeX = availableContentArea.x - window->cachedScrollBarLengthX;
            window->scrollBarPositionX = KORL_MATH_CLAMP(window->scrollBarPositionX, 0.f, scrollBarRangeX);
            const f32 scrollBarPositionX = window->position.x + window->scrollBarPositionX;
            /* batch scroll bar graphics */
            batchScrollBarX = korl_gfx_createBatchRectangleColored(context->allocatorHandleStack, 
                                                                   (Korl_Math_V2f32){window->cachedScrollBarLengthX, context->style.windowScrollBarPixelWidth}, 
                                                                   (Korl_Math_V2f32){0.f, 0.f}, 
                                                                   context->style.colorScrollBar);
            if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X)
                korl_gfx_batchRectangleSetColor(batchScrollBarX, context->style.colorScrollBarPressed);
            else if(context->identifierMouseHoveredWindow == window->identifier)
            {
                const Korl_Math_Aabb2f32 scrollBarAabb = korl_math_aabb2f32_fromPoints(scrollBarPositionX                                 , window->position.y - window->size.y, 
                                                                                       scrollBarPositionX + window->cachedScrollBarLengthX, window->position.y - window->size.y + context->style.windowScrollBarPixelWidth);
                if(korl_math_aabb2f32_containsV2f32(scrollBarAabb, context->mouseHoverPosition))
                    korl_gfx_batchRectangleSetColor(batchScrollBarX, context->style.colorScrollBarActive);
            }
            korl_gfx_batchSetPosition2d(batchScrollBarX, scrollBarPositionX, window->position.y - window->size.y);
            korl_time_probeStop(scroll_bar_x);
        }
        if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y)
        {
            korl_time_probeStart(scroll_bar_y);
            /* determine scroll bar length */
            window->cachedScrollBarLengthY = availableContentArea.y * availableContentArea.y / contentSize.y;
            window->cachedScrollBarLengthY = KORL_MATH_MAX(window->cachedScrollBarLengthY, SCROLLBAR_LENGTH_MIN);
            /* bind the scroll bar to the current possible range */
            const f32 scrollBarRangeY = ((window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR) ? window->size.y - context->style.windowTitleBarPixelSizeY : window->size.y)
                                      - window->cachedScrollBarLengthY;
            window->scrollBarPositionY = KORL_MATH_CLAMP(window->scrollBarPositionY, 0.f, scrollBarRangeY);
            f32 scrollBarPositionY = window->position.y - window->scrollBarPositionY;
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
                const Korl_Math_Aabb2f32 scrollBarAabb = korl_math_aabb2f32_fromPoints(window->position.x + window->size.x                                           , scrollBarPositionY, 
                                                                                       window->position.x + window->size.x - context->style.windowScrollBarPixelWidth, scrollBarPositionY - window->cachedScrollBarLengthY);
                if(korl_math_aabb2f32_containsV2f32(scrollBarAabb, context->mouseHoverPosition))
                    korl_gfx_batchRectangleSetColor(batchScrollBarY, context->style.colorScrollBarActive);
            }
            korl_gfx_batchSetPosition2d(batchScrollBarY, window->position.x + window->size.x, scrollBarPositionY);
            korl_time_probeStop(scroll_bar_y);
        }
        if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X))
            window->scrollBarPositionX = 0;
        if(!(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y))
            window->scrollBarPositionY = 0;
        // convert scroll bar positions to content cursor offset //
        const f32 scrollBarRangeX = availableContentArea.x - window->cachedScrollBarLengthX;
        const f32 scrollBarRangeY = ((window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR) ? window->size.y - context->style.windowTitleBarPixelSizeY : window->size.y)
                                  - window->cachedScrollBarLengthY;
        const f32 scrollBarRatioX = korl_math_isNearlyZero(scrollBarRangeX) ? 0 : window->scrollBarPositionX / scrollBarRangeX;
        const f32 scrollBarRatioY = korl_math_isNearlyZero(scrollBarRangeY) ? 0 : window->scrollBarPositionY / scrollBarRangeY;
        const f32 contentCursorOffsetX = scrollBarRatioX * (contentSize.x - availableContentArea.x);
        const f32 contentCursorOffsetY = scrollBarRatioY * (contentSize.y - availableContentArea.y);
        korl_time_probeStop(scroll_bar_logic);
        // before drawing the window contents, we need to make sure it will not 
        //  draw on top of the title bar if there is one present //
        if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
        {
            korl_gfx_cameraSetScissor(&guiCamera, 
                                      window->position.x, 
                                      /*natural +Y=>UP space => swap chain space conversion*/-window->position.y + context->style.windowTitleBarPixelSizeY, 
                                      window->size.x, 
                                      window->size.y - context->style.windowTitleBarPixelSizeY);
            korl_gfx_useCamera(guiCamera);
        }
        korl_time_probeStart(process_widget_gfx);
        _korl_gui_processWidgetGraphics(window, true, contentCursorOffsetX, contentCursorOffsetY);
        korl_time_probeStop(process_widget_gfx);
        /* draw scroll bars, if they are needed */
        korl_time_probeStart(batch_scroll_bars);
        if(batchScrollBarX)
            korl_gfx_batch(batchScrollBarX, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        if(batchScrollBarY)
            korl_gfx_batch(batchScrollBarY, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_time_probeStop(batch_scroll_bars);
        korl_time_probeStop(draw_widgets);
        korl_time_probeStart(draw_window_border);
        /* batch window border AFTER contents are drawn */
        Korl_Vulkan_Color4u8 colorBorder = context->style.colorWindowBorder;
        if(context->isTopLevelWindowActive && i == KORL_MEMORY_POOL_SIZE(context->windows) - 1)
            if(context->isMouseHovering 
                && context->identifierMouseHoveredWindow == window->identifier
                && context->mouseHoverWindowEdgeFlags == KORL_GUI_MOUSE_HOVER_FLAGS_NONE)
                colorBorder = context->style.colorWindowBorderResize;
            else
                colorBorder = context->style.colorWindowBorderActive;
        else if(context->isMouseHovering && context->identifierMouseHoveredWindow == window->identifier)
            colorBorder = context->style.colorWindowBorderHovered;
        const Korl_Vulkan_Color4u8 colorBorderUp    = (context->identifierMouseHoveredWindow == window->identifier && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_UP   )) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Vulkan_Color4u8 colorBorderRight = (context->identifierMouseHoveredWindow == window->identifier && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_RIGHT)) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Vulkan_Color4u8 colorBorderDown  = (context->identifierMouseHoveredWindow == window->identifier && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_DOWN )) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Vulkan_Color4u8 colorBorderLeft  = (context->identifierMouseHoveredWindow == window->identifier && (context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_LEFT )) ? context->style.colorWindowBorderResize : colorBorder;
        const Korl_Math_Aabb2f32 windowAabb = korl_math_aabb2f32_fromPoints(window->position.x                 , window->position.y - window->size.y, 
                                                                            window->position.x + window->size.x, window->position.y                  );
        Korl_Gfx_Batch*const batchWindowBorder = korl_gfx_createBatchLines(context->allocatorHandleStack, 4);
        //KORL-ISSUE-000-000-009
        korl_gfx_batchSetLine(batchWindowBorder, 0, (Korl_Vulkan_Position){windowAabb.min.x - 0.5f, windowAabb.max.y}, (Korl_Vulkan_Position){windowAabb.max.x, windowAabb.max.y}, colorBorderUp);
        korl_gfx_batchSetLine(batchWindowBorder, 1, (Korl_Vulkan_Position){windowAabb.max.x       , windowAabb.max.y}, (Korl_Vulkan_Position){windowAabb.max.x, windowAabb.min.y}, colorBorderRight);
        korl_gfx_batchSetLine(batchWindowBorder, 2, (Korl_Vulkan_Position){windowAabb.max.x       , windowAabb.min.y}, (Korl_Vulkan_Position){windowAabb.min.x, windowAabb.min.y}, colorBorderDown);
        korl_gfx_batchSetLine(batchWindowBorder, 3, (Korl_Vulkan_Position){windowAabb.min.x       , windowAabb.min.y}, (Korl_Vulkan_Position){windowAabb.min.x, windowAabb.max.y}, colorBorderLeft);
        korl_gfx_cameraSetScissorPercent(&guiCamera, 0,0, 1,1);
        korl_gfx_useCamera(guiCamera);
        korl_gfx_batch(batchWindowBorder, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_time_probeStop(draw_window_border);
    }
    korl_time_probeStop(generate_draw_commands);
    KORL_MEMORY_POOL_RESIZE(context->windows, windowsRemaining);
}
korl_internal KORL_PLATFORM_GUI_WIDGET_TEXT_FORMAT(korl_gui_widgetTextFormat)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(textFormat, KORL_GUI_WIDGET_TYPE_TEXT);
    va_list vaList;
    va_start(vaList, textFormat);
    widget->subType.text.displayText = korl_memory_stringFormatVaList(context->allocatorHandleStack, textFormat, vaList);
    va_end(vaList);
}
korl_internal KORL_PLATFORM_GUI_WIDGET_BUTTON_FORMAT(korl_gui_widgetButtonFormat)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    _Korl_Gui_Widget*const widget = _korl_gui_getWidget(textFormat, KORL_GUI_WIDGET_TYPE_BUTTON);
    va_list vaList;
    va_start(vaList, textFormat);
    widget->subType.button.displayText = korl_memory_stringFormatVaList(context->allocatorHandleStack, textFormat, vaList);
    va_end(vaList);
    const u8 resultActuationCount = widget->subType.button.actuationCount;
    widget->subType.button.actuationCount = 0;
    return resultActuationCount;
}
