#include "korl-windows-gui.h"
#include "korl-windows-globalDefines.h"
#include "korl-windows-utilities.h"
#include "korl-gui-internal-common.h"
#include "korl-checkCast.h"
#include "korl-vulkan.h"
#include "korl-memoryPool.h"
korl_internal void korl_gui_windows_processMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    /* we can't just assert that the frameSequenceCounter == 0 here because this 
        function will be getting called when the message box of an assert is 
        running & pumping the message loop, which could happen at any time! */
    if(context->frameSequenceCounter != 0)
    {
        korl_log(WARNING, "korl_gui_windows_processMessage called outside of korl_gui_frameBegin/End; message will be ignored");
        return;
    }
    korl_assert(context->frameSequenceCounter == 0);
    /* identify & process mouse events 
        https://docs.microsoft.com/en-us/windows/win32/learnwin32/mouse-clicks */
    switch(message)
    {
    case WM_LBUTTONDOWN:{
        const i32 mouseX =  GET_X_LPARAM(lParam);
        const i32 mouseY = -GET_Y_LPARAM(lParam);//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        const Korl_Math_V2f32 mouseV2f32 = { korl_checkCast_i$_to_f32(mouseX)
                                           , korl_checkCast_i$_to_f32(mouseY) };
        /* deactivate the top level window, in case it wasn't already */
        context->isTopLevelWindowActive       = false;
        context->isMouseDown                  = false;
        context->isWindowDragged              = false;
        context->isWindowResizing             = false;
        context->identifierMouseDownWidget    = NULL;
        context->specialWidgetFlagsMouseDown  = KORL_GUI_SPECIAL_WIDGET_FLAGS_NONE;
        /* check to see if we clicked on any windows from the previous frame 
            - note that we're processing windows from front->back, since 
              windows[0] is always the farthest back window */
        for(i$ w = KORL_C_CAST(i$, KORL_MEMORY_POOL_SIZE(context->windows)) - 1; w >= 0; w--)
        {
            const _Korl_Gui_Window*const window = &context->windows[w];
            if(!window->isOpen)
                continue;
            korl_assert(window->identifier);
            const f32 windowAabbExpansion = (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE) ? _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS : 0.f;
            const Korl_Math_Aabb2f32 windowExpandedAabb = 
                { .min = { window->position.x                  - windowAabbExpansion, window->position.y - window->size.y - windowAabbExpansion }
                , .max = { window->position.x + window->size.x + windowAabbExpansion, window->position.y                  + windowAabbExpansion } };
            if(!korl_math_aabb2f32_containsV2f32(windowExpandedAabb, mouseV2f32))
                continue;
            context->isMouseDown     = true;
            context->isWindowDragged = true;
            if(context->mouseHoverWindowEdgeFlags)
                context->isWindowResizing = true;
            /* check to see if we clicked on any widgets from the previous frame */
            if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            {
                Korl_Math_V2f32 titlebarButtonCursor = { window->position.x + window->size.x - context->style.windowTitleBarPixelSizeY
                                                       , window->position.y };
                /* check to see if the mouse pressed the close button */
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
                {
                    const Korl_Math_Aabb2f32 aabbButton = 
                        { .min = { titlebarButtonCursor.x                                          , titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY }
                        , .max = { titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.y                                           } };
                    if(korl_math_aabb2f32_containsV2f32(aabbButton, mouseV2f32))
                    {
                        context->specialWidgetFlagsMouseDown |= KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE;
                        context->isWindowDragged  = false;
                        context->isWindowResizing = false;
                    }
                    titlebarButtonCursor.x -= context->style.windowTitleBarPixelSizeY;
                }
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
                {
                    const Korl_Math_Aabb2f32 aabbButton = 
                        { .min = { titlebarButtonCursor.x                                          , titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY }
                        , .max = { titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.y                                           } };
                    if(korl_math_aabb2f32_containsV2f32(aabbButton, mouseV2f32))
                    {
                        context->specialWidgetFlagsMouseDown |= KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE;
                        context->isWindowDragged  = false;
                        context->isWindowResizing = false;
                    }
                    titlebarButtonCursor.x -= context->style.windowTitleBarPixelSizeY;
                }
            }
            if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X)
            {
                const f32 scrollBarPositionX = window->position.x + window->scrollBarPositionX;
                const Korl_Math_Aabb2f32 scrollBarAabb = korl_math_aabb2f32_fromPoints(scrollBarPositionX                                 , window->position.y - window->size.y, 
                                                                                       scrollBarPositionX + window->cachedScrollBarLengthX, window->position.y - window->size.y + context->style.windowScrollBarPixelWidth);
                if(korl_math_aabb2f32_containsV2f32(scrollBarAabb, mouseV2f32))
                {
                    context->specialWidgetFlagsMouseDown |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X;
                    context->isWindowDragged  = false;
                    context->isWindowResizing = false;
                    context->mouseDownOffsetSpecialWidget = korl_math_v2f32_subtract(scrollBarAabb.min, mouseV2f32);
                }
            }
            if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y)
            {
                f32 scrollBarPositionY = window->position.y - window->scrollBarPositionY;
                if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
                    scrollBarPositionY -= context->style.windowTitleBarPixelSizeY;
                const Korl_Math_Aabb2f32 scrollBarAabb = korl_math_aabb2f32_fromPoints(window->position.x + window->size.x                                           , scrollBarPositionY, 
                                                                                       window->position.x + window->size.x - context->style.windowScrollBarPixelWidth, scrollBarPositionY - window->cachedScrollBarLengthY);
                if(korl_math_aabb2f32_containsV2f32(scrollBarAabb, mouseV2f32))
                {
                    context->specialWidgetFlagsMouseDown |= KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y;
                    context->isWindowDragged  = false;
                    context->isWindowResizing = false;
                    context->mouseDownOffsetSpecialWidget = korl_math_v2f32_subtract(scrollBarAabb.max, mouseV2f32);
                }
            }
            for(u$ wi = 0; wi < KORL_MEMORY_POOL_SIZE(context->widgets); wi++)
            {
                _Korl_Gui_Widget*const widget = &context->widgets[wi];
                if(widget->parentWindowIdentifier != window->identifier || !widget->cachedIsInteractive)
                    continue;
                if(korl_math_aabb2f32_containsV2f32(widget->cachedAabb, mouseV2f32))
                {
                    context->mouseHoverPosition = mouseV2f32;
                    context->identifierMouseDownWidget = widget->identifier;
                    context->isWindowDragged  = false;
                    context->isWindowResizing = false;
                    break;
                }
            }
            /**/
            context->mouseDownWindowOffset = korl_math_v2f32_subtract(mouseV2f32, window->position);
            /* bring this window to the "front" & activate it, while maintaining 
                the order of the existing windows */
            _Korl_Gui_Window temp = context->windows[w];
            for(u$ i = korl_checkCast_i$_to_u$(w); KORL_C_CAST(i$, i) < KORL_C_CAST(i$, KORL_MEMORY_POOL_SIZE(context->windows)) - 1; i++)
                context->windows[i] = context->windows[i + 1];
            context->windows[KORL_MEMORY_POOL_SIZE(context->windows) - 1] = temp;
            context->isTopLevelWindowActive = true;
            /* once we've submitted mouse input to the nearest window, we can 
                stop iterating, since windows below this position should not 
                receive the same input */
            break;
        }
        if(context->isTopLevelWindowActive)
            /*HWND hwndPreviousCapture = */SetCapture(hWnd);
        break;}
    case WM_LBUTTONUP:{
        const i32 mouseX =  GET_X_LPARAM(lParam);
        const i32 mouseY = -GET_Y_LPARAM(lParam);//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        const Korl_Math_V2f32 mouseV2f32 = { korl_checkCast_i$_to_f32(mouseX)
                                           , korl_checkCast_i$_to_f32(mouseY) };
        if(context->specialWidgetFlagsMouseDown)
        {
            /* find the mouse-down window so we can check to see if we're still 
                hovering a pressed title bar button, allowing us to perform a 
                full button actuation */
            for(u$ w = 0; w < KORL_MEMORY_POOL_SIZE(context->windows); w++)
            {
                _Korl_Gui_Window*const window = &context->windows[w];
                if(!window->isOpen)
                    continue;
                if(window->identifier != context->identifierMouseHoveredWindow)
                    continue;
                Korl_Math_V2f32 titlebarButtonCursor = { window->position.x + window->size.x - context->style.windowTitleBarPixelSizeY
                                                       , window->position.y };
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE)
                {
                    const Korl_Math_Aabb2f32 aabbButton = 
                        { .min = { titlebarButtonCursor.x                                          , titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY }
                        , .max = { titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.y                                           } };
                    if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE
                        && korl_math_aabb2f32_containsV2f32(aabbButton, mouseV2f32))
                        window->specialWidgetFlagsPressed |= KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE;
                    titlebarButtonCursor.x -= context->style.windowTitleBarPixelSizeY;
                }
                if(window->specialWidgetFlags & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE)
                {
                    const Korl_Math_Aabb2f32 aabbButton = 
                        { .min = { titlebarButtonCursor.x                                          , titlebarButtonCursor.y - context->style.windowTitleBarPixelSizeY }
                        , .max = { titlebarButtonCursor.x + context->style.windowTitleBarPixelSizeY, titlebarButtonCursor.y                                           } };
                    if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE
                        && korl_math_aabb2f32_containsV2f32(aabbButton, mouseV2f32))
                        window->specialWidgetFlagsPressed |= KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE;
                    titlebarButtonCursor.x -= context->style.windowTitleBarPixelSizeY;
                }
                break;
            }
        }
        if(context->identifierMouseDownWidget)
        {
            /* find the mouse-down widget, and check to see if we're still 
                hovering it, allowing us to perform a full mouse click */
            for(u$ w = 0; w < KORL_MEMORY_POOL_SIZE(context->widgets); w++)
            {
                _Korl_Gui_Widget*const widget = &context->widgets[w];
                if(widget->identifier != context->identifierMouseDownWidget)
                    continue;
                if(!korl_math_aabb2f32_containsV2f32(widget->cachedAabb, mouseV2f32))
                    continue;
                switch(widget->type)
                {
                case KORL_GUI_WIDGET_TYPE_BUTTON:{
                    widget->subType.button.actuationCount++;
                    /* cap the button actuation count at the maximum possible 
                        value for that unsigned type, instead of wrapping around 
                        back to zero if we overflow */
                    if(widget->subType.button.actuationCount == 0)
                        widget->subType.button.actuationCount--;
                    break;}
                default:{
                    korl_log(ERROR, "mouse actuation not implemented for widget type==%u", widget->type);
                    break;}
                }
                break;
            }
        }
        context->isMouseDown                  = false;
        context->specialWidgetFlagsMouseDown = KORL_GUI_SPECIAL_WIDGET_FLAGS_NONE;
        if(!ReleaseCapture())
            korl_logLastError("ReleaseCapture failed!");
        break;}
    case WM_MOUSEMOVE:{
        const i32 mouseX =  GET_X_LPARAM(lParam);
        const i32 mouseY = -GET_Y_LPARAM(lParam);//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        const Korl_Math_V2f32 mouseV2f32 = { korl_checkCast_i$_to_f32(mouseX)
                                           , korl_checkCast_i$_to_f32(mouseY) };
        if(KORL_MEMORY_POOL_ISEMPTY(context->windows))
            break;
        if(context->isMouseDown)
        {
            _Korl_Gui_Window*const window = &context->windows[KORL_MEMORY_POOL_SIZE(context->windows) - 1];
            if(context->isWindowDragged)
            {
                if(context->mouseHoverWindowEdgeFlags)
                {
                    /* obtain the window's AABB */
                    Korl_Math_Aabb2f32 aabbWindow = 
                        { .min = { window->position.x                 , window->position.y - window->size.y }
                        , .max = { window->position.x + window->size.x, window->position.y                  } };
                    /* adjust the AABB values based on which edge flags we're 
                        controlling, & ensure that the final AABB is valid */
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_LEFT)
                        aabbWindow.min.x = KORL_MATH_MIN(mouseV2f32.x, aabbWindow.max.x - context->style.windowTitleBarPixelSizeY);
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_RIGHT)
                        aabbWindow.max.x = KORL_MATH_MAX(mouseV2f32.x, aabbWindow.min.x + context->style.windowTitleBarPixelSizeY);
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_UP)
                        aabbWindow.max.y = KORL_MATH_MAX(mouseV2f32.y, aabbWindow.min.y + context->style.windowTitleBarPixelSizeY);
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_DOWN)
                        aabbWindow.min.y = KORL_MATH_MIN(mouseV2f32.y, aabbWindow.max.y - context->style.windowTitleBarPixelSizeY);
                    /* set the window position/size based on the new AABB */
                    window->position.x = aabbWindow.min.x;
                    window->position.y = aabbWindow.max.y;
                    window->size.x = aabbWindow.max.x - aabbWindow.min.x;
                    window->size.y = aabbWindow.max.y - aabbWindow.min.y;
                }
                else
                    /* if we're click-dragging a window, update the window's new 
                        position relative to the original mouse-down position */
                    window->position = korl_math_v2f32_subtract(mouseV2f32, context->mouseDownWindowOffset);
            }
            else
            {
                const Korl_Math_V2f32 mouseWindowOffset = korl_math_v2f32_subtract(mouseV2f32, window->position);
                if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X)
                    window->scrollBarPositionX = KORL_MATH_CLAMP(mouseWindowOffset.x + context->mouseDownOffsetSpecialWidget.x, 
                                                                 0.f, 
                                                                 window->cachedAvailableContentSize.x - window->cachedScrollBarLengthX);
                if(context->specialWidgetFlagsMouseDown & KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y)
                {
                    f32 scrollBarPositionY = 0.f;
                    if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
                        scrollBarPositionY -= context->style.windowTitleBarPixelSizeY;
                    const f32 scrollBarAreaSizeY = window->size.y + scrollBarPositionY;
                    window->scrollBarPositionY = KORL_MATH_CLAMP(scrollBarPositionY - mouseWindowOffset.y - context->mouseDownOffsetSpecialWidget.y, 
                                                                 0.f, 
                                                                 scrollBarAreaSizeY - window->cachedScrollBarLengthY);
                }
                context->mouseHoverPosition = mouseV2f32;
            }
        }
        else
        {
            /* check to see whether or not we're hovering over any windows from 
                the previous frame, and if we are record the cursor position 
                relative to the window */
            context->isMouseHovering               = false;
            context->identifierMouseHoveredWidget  = NULL;
            context->identifierMouseHoveredWindow  = NULL;
            context->mouseHoverWindowEdgeFlags     = 0;
            for(i$ w = KORL_MEMORY_POOL_SIZE(context->windows) - 1; w >= 0; w--)
            {
                const _Korl_Gui_Window*const window = &context->windows[w];
                if(!window->isOpen)
                    continue;
                korl_assert(window->identifier);
                const f32 windowAabbExpansion = (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE) ? _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS : 0.f;
                const Korl_Math_Aabb2f32 windowExpandedAabb = 
                    { .min = { window->position.x                  - windowAabbExpansion, window->position.y - window->size.y - windowAabbExpansion }
                    , .max = { window->position.x + window->size.x + windowAabbExpansion, window->position.y                  + windowAabbExpansion } };
                if(!korl_math_aabb2f32_containsV2f32(windowExpandedAabb, mouseV2f32))
                    continue;
                if(    mouseX >= window->position.x - _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS 
                    && mouseX <= window->position.x + _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS + window->size.x
                    && !window->isContentHidden/*disable resizing top/bottom edges when window content is hidden*/)
                {
                    if(    mouseY >= window->position.y 
                        && mouseY <= window->position.y + _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS)
                        context->mouseHoverWindowEdgeFlags |= KORL_GUI_MOUSE_HOVER_FLAG_UP;
                    if(    mouseY >= window->position.y - window->size.y - _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS 
                        && mouseY <= window->position.y - window->size.y)
                        context->mouseHoverWindowEdgeFlags |= KORL_GUI_MOUSE_HOVER_FLAG_DOWN;
                }
                if(    mouseY >= window->position.y - _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS - window->size.y 
                    && mouseY <= window->position.y + _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS)
                {
                    if(    mouseX >= window->position.x - _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS 
                        && mouseX <= window->position.x)
                        context->mouseHoverWindowEdgeFlags |= KORL_GUI_MOUSE_HOVER_FLAG_LEFT;
                    if(    mouseX >= window->position.x + window->size.x 
                        && mouseX <= window->position.x + window->size.x + _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS )
                        context->mouseHoverWindowEdgeFlags |= KORL_GUI_MOUSE_HOVER_FLAG_RIGHT;
                }
                if(!(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE))
                    context->mouseHoverWindowEdgeFlags = 0;
                context->isMouseHovering = true;
                context->mouseHoverPosition = mouseV2f32;
                context->identifierMouseHoveredWindow = window->identifier;
                for(u$ wi = 0; wi < KORL_MEMORY_POOL_SIZE(context->widgets); wi++)
                {
                    _Korl_Gui_Widget*const widget = &context->widgets[wi];
                    if(widget->parentWindowIdentifier != window->identifier)
                        continue;
                    if(korl_math_aabb2f32_containsV2f32(widget->cachedAabb, mouseV2f32))
                    {
                        context->identifierMouseHoveredWidget = widget->identifier;
                        break;
                    }
                }
                break;
            }
        }
        break;}
    case WM_MOUSEWHEEL:{
        POINT pointMouse;
        pointMouse.x = GET_X_LPARAM(lParam);//screen-space, NOT client-space like the other mouse events!!! >:[
        pointMouse.y = GET_Y_LPARAM(lParam);//screen-space, NOT client-space like the other mouse events!!! >:[
        if(!ScreenToClient(hWnd, &pointMouse))
            korl_logLastError("ScreenToClient failed!");
        const i32 mouseX =  pointMouse.x;
        const i32 mouseY = -pointMouse.y;//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        const Korl_Math_V2f32 mouseV2f32 = { korl_checkCast_i$_to_f32(mouseX)
                                           , korl_checkCast_i$_to_f32(mouseY) };
        const i32 mouseWheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        const bool keyDownShift = LOWORD(wParam) & MK_SHIFT;
        /* check if the mouse if hovering over a window */
        for(i$ w = KORL_C_CAST(i$, KORL_MEMORY_POOL_SIZE(context->windows)) - 1; w >= 0; w--)
        {
            _Korl_Gui_Window*const window = &context->windows[w];
            if(!window->isOpen)
                continue;
            korl_assert(window->identifier);
            const f32 windowAabbExpansion = (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE) ? _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS : 0.f;
            const Korl_Math_Aabb2f32 windowExpandedAabb = 
                { .min = { window->position.x                  - windowAabbExpansion, window->position.y - window->size.y - windowAabbExpansion }
                , .max = { window->position.x + window->size.x + windowAabbExpansion, window->position.y                  + windowAabbExpansion } };
            if(!korl_math_aabb2f32_containsV2f32(windowExpandedAabb, mouseV2f32))
                continue;
            /* if it is, modify the X/Y scroll position based on presence of [SHIFT] */
            if(keyDownShift)
                window->scrollBarPositionX -= 10.f*(mouseWheelDelta/(f32)WHEEL_DELTA);
            else
                window->scrollBarPositionY -= 10.f*(mouseWheelDelta/(f32)WHEEL_DELTA);
            break;
        }
        break;}
#if 0
    /*case WM_LBUTTONDBLCLK:*/
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    /*case WM_RBUTTONDBLCLK:*/
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    /*case WM_MBUTTONDBLCLK:*/
    //case WM_XBUTTONUP:
    //case WM_XBUTTONDOWN:
    //case WM_XBUTTONDBLCLK:
    case WM_GESTURE:{
        korl_log(VERBOSE, "WM_GESTURE");
        break;}
    case WM_VSCROLL:{
        korl_log(VERBOSE, "WM_VSCROLL");
        break;}
    case WM_HSCROLL:{
        korl_log(VERBOSE, "WM_HSCROLL");
        break;}
    case WM_MOUSEHWHEEL:{
        const i32 mouseX =  GET_X_LPARAM(lParam);
        const i32 mouseY = -GET_Y_LPARAM(lParam);//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        const Korl_Math_V2f32 mouseV2f32 = { korl_checkCast_i$_to_f32(mouseX)
                                           , korl_checkCast_i$_to_f32(mouseY) };
        const i32 mouseWheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        break;}
#endif
    }
}
