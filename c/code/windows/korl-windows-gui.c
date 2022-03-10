#include "korl-windows-gui.h"
#include "korl-gui-internal-common.h"
#include "korl-checkCast.h"
#include "korl-windows-globalDefines.h"
#include "korl-vulkan.h"
korl_internal void korl_gui_windows_processMessage(const MSG* message)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 0);
    /* identify & process mouse events 
        https://docs.microsoft.com/en-us/windows/win32/learnwin32/mouse-clicks */
    switch(message->message)
    {
    case WM_LBUTTONDOWN:{
        const i32 mouseX =  GET_X_LPARAM(message->lParam);
        const i32 mouseY = -GET_Y_LPARAM(message->lParam);//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        /* deactivate the top level window, in case it wasn't already */
        context->isTopLevelWindowActive       = false;
        context->isMouseDown                  = false;
        context->isWindowDragged              = false;
        context->isWindowResizing             = false;
        context->identifierMouseDownWidget    = NULL;
        context->titlebarButtonFlagsMouseDown = KORL_GUI_TITLEBAR_BUTTON_FLAGS_NONE;
        /* check to see if we clicked on any windows from the previous frame 
            - note that we're processing windows from front->back, since 
              windows[0] is always the farthest back window */
        for(i$ w = KORL_MEMORY_POOL_SIZE(context->windows) - 1; w >= 0; w--)
        {
            const _Korl_Gui_Window*const window = &context->windows[w];
            if(!window->isOpen)
                continue;
            korl_assert(window->identifier);
            const f32 windowAabbExpansion = (window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE) ? _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS : 0.f;
            if(!(   mouseX >= window->position.xy.x                     - windowAabbExpansion 
                 && mouseX <= window->position.xy.x + window->size.xy.x + windowAabbExpansion 
                 && mouseY <= window->position.xy.y                     + windowAabbExpansion 
                 && mouseY >= window->position.xy.y - window->size.xy.y - windowAabbExpansion))
                continue;
            context->isMouseDown     = true;
            context->isWindowDragged = true;
            if(context->mouseHoverWindowEdgeFlags)
                context->isWindowResizing = true;
            /* check to see if we clicked on any widgets from the previous frame */
            if(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR)
            {
                Korl_Math_V2f32 titlebarButtonCursor = { .xy.x = window->position.xy.x + window->size.xy.x - context->style.windowTitleBarPixelSizeY
                                                       , .xy.y = window->position.xy.y };
                /* check to see if the mouse pressed the close button */
                if(window->titlebarButtonFlags & KORL_GUI_TITLEBAR_BUTTON_FLAG_CLOSE)
                {
                    if(    mouseX >= titlebarButtonCursor.xy.x
                        && mouseX <= titlebarButtonCursor.xy.x + context->style.windowTitleBarPixelSizeY
                        && mouseY >= titlebarButtonCursor.xy.y - context->style.windowTitleBarPixelSizeY
                        && mouseY <= titlebarButtonCursor.xy.y)
                    {
                        context->titlebarButtonFlagsMouseDown |= KORL_GUI_TITLEBAR_BUTTON_FLAG_CLOSE;
                        context->isWindowDragged  = false;
                        context->isWindowResizing = false;
                    }
                    titlebarButtonCursor.xy.x -= context->style.windowTitleBarPixelSizeY;
                }
                if(window->titlebarButtonFlags & KORL_GUI_TITLEBAR_BUTTON_FLAG_HIDE)
                {
                    if(    mouseX >= titlebarButtonCursor.xy.x
                        && mouseX <= titlebarButtonCursor.xy.x + context->style.windowTitleBarPixelSizeY
                        && mouseY >= titlebarButtonCursor.xy.y - context->style.windowTitleBarPixelSizeY
                        && mouseY <= titlebarButtonCursor.xy.y)
                    {
                        context->titlebarButtonFlagsMouseDown |= KORL_GUI_TITLEBAR_BUTTON_FLAG_HIDE;
                        context->isWindowDragged  = false;
                        context->isWindowResizing = false;
                    }
                    titlebarButtonCursor.xy.x -= context->style.windowTitleBarPixelSizeY;
                }
            }
            for(u$ wi = 0; wi < KORL_MEMORY_POOL_SIZE(context->widgets); wi++)
            {
                _Korl_Gui_Widget*const widget = &context->widgets[wi];
                if(widget->parentWindowIdentifier != window->identifier || !widget->cachedIsInteractive)
                    continue;
                if(    mouseX >= widget->cachedAabbMin.xy.x
                    && mouseX <= widget->cachedAabbMax.xy.x
                    && mouseY >= widget->cachedAabbMin.xy.y
                    && mouseY <= widget->cachedAabbMax.xy.y)
                {
                    context->mouseHoverPosition = (Korl_Math_V2f32){ korl_checkCast_i$_to_f32(mouseX)
                                                                   , korl_checkCast_i$_to_f32(mouseY) };
                    context->identifierMouseDownWidget = widget->identifier;
                    context->isWindowDragged  = false;
                    context->isWindowResizing = false;
                    break;
                }
            }
            /**/
            context->mouseDownWindowOffset = korl_math_v2f32_subtract((Korl_Math_V2f32){ korl_checkCast_i$_to_f32(mouseX)
                                                                                       , korl_checkCast_i$_to_f32(mouseY) }, 
                                                                      window->position);
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
            /*HWND hwndPreviousCapture = */SetCapture(message->hwnd);
        break;}
    case WM_LBUTTONUP:{
        const i32 mouseX =  GET_X_LPARAM(message->lParam);
        const i32 mouseY = -GET_Y_LPARAM(message->lParam);//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        if(context->titlebarButtonFlagsMouseDown)
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
                Korl_Math_V2f32 titlebarButtonCursor = { .xy.x = window->position.xy.x + window->size.xy.x - context->style.windowTitleBarPixelSizeY
                                                       , .xy.y = window->position.xy.y };
                if(window->titlebarButtonFlags & KORL_GUI_TITLEBAR_BUTTON_FLAG_CLOSE)
                {
                    if(context->titlebarButtonFlagsMouseDown & KORL_GUI_TITLEBAR_BUTTON_FLAG_CLOSE
                        && mouseX >= titlebarButtonCursor.xy.x
                        && mouseX <= titlebarButtonCursor.xy.x + context->style.windowTitleBarPixelSizeY
                        && mouseY >= titlebarButtonCursor.xy.y - context->style.windowTitleBarPixelSizeY
                        && mouseY <= titlebarButtonCursor.xy.y)
                        window->titlebarButtonFlagsPressed |= KORL_GUI_TITLEBAR_BUTTON_FLAG_CLOSE;
                    titlebarButtonCursor.xy.x -= context->style.windowTitleBarPixelSizeY;
                }
                if(window->titlebarButtonFlags & KORL_GUI_TITLEBAR_BUTTON_FLAG_HIDE)
                {
                    if(context->titlebarButtonFlagsMouseDown & KORL_GUI_TITLEBAR_BUTTON_FLAG_HIDE
                        && mouseX >= titlebarButtonCursor.xy.x
                        && mouseX <= titlebarButtonCursor.xy.x + context->style.windowTitleBarPixelSizeY
                        && mouseY >= titlebarButtonCursor.xy.y - context->style.windowTitleBarPixelSizeY
                        && mouseY <= titlebarButtonCursor.xy.y)
                        window->titlebarButtonFlagsPressed |= KORL_GUI_TITLEBAR_BUTTON_FLAG_HIDE;
                    titlebarButtonCursor.xy.x -= context->style.windowTitleBarPixelSizeY;
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
                if(!(    mouseX >= widget->cachedAabbMin.xy.x
                      && mouseX <= widget->cachedAabbMax.xy.x
                      && mouseY >= widget->cachedAabbMin.xy.y
                      && mouseY <= widget->cachedAabbMax.xy.y))
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
        context->titlebarButtonFlagsMouseDown = KORL_GUI_TITLEBAR_BUTTON_FLAGS_NONE;
        if(!ReleaseCapture())
            korl_logLastError("ReleaseCapture failed!");
        break;}
    case WM_MOUSEMOVE:{
        const i32 mouseX =  GET_X_LPARAM(message->lParam);
        const i32 mouseY = -GET_Y_LPARAM(message->lParam);//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        const Korl_Math_V2f32 mousePosition = { korl_checkCast_i$_to_f32(mouseX)
                                              , korl_checkCast_i$_to_f32(mouseY) };
        if(KORL_MEMORY_POOL_ISEMPTY(context->windows))
            break;
        if(context->isMouseDown)
        {
            if(context->isWindowDragged)
            {
                _Korl_Gui_Window*const window = &context->windows[KORL_MEMORY_POOL_SIZE(context->windows) - 1];
                if(context->mouseHoverWindowEdgeFlags)
                {
                    /* obtain the window's AABB */
                    Korl_Math_V2f32 windowAabbMin = { window->position.xy.x
                                                    , window->position.xy.y - window->size.xy.y };
                    Korl_Math_V2f32 windowAabbMax = { window->position.xy.x + window->size.xy.x
                                                    , window->position.xy.y };
                    /* adjust the AABB values based on which edge flags we're 
                        controlling, & ensure that the final AABB is valid */
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_LEFT)
                        windowAabbMin.xy.x = KORL_MATH_MIN(mousePosition.xy.x, windowAabbMax.xy.x - context->style.windowTitleBarPixelSizeY);
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_RIGHT)
                        windowAabbMax.xy.x = KORL_MATH_MAX(mousePosition.xy.x, windowAabbMin.xy.x + context->style.windowTitleBarPixelSizeY);
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_UP)
                        windowAabbMax.xy.y = KORL_MATH_MAX(mousePosition.xy.y, windowAabbMin.xy.y + context->style.windowTitleBarPixelSizeY);
                    if(context->mouseHoverWindowEdgeFlags & KORL_GUI_MOUSE_HOVER_FLAG_DOWN)
                        windowAabbMin.xy.y = KORL_MATH_MIN(mousePosition.xy.y, windowAabbMax.xy.y - context->style.windowTitleBarPixelSizeY);
                    /* set the window position/size based on the new AABB */
                    window->position.xy.x = windowAabbMin.xy.x;
                    window->position.xy.y = windowAabbMax.xy.y;
                    window->size.xy.x = windowAabbMax.xy.x - windowAabbMin.xy.x;
                    window->size.xy.y = windowAabbMax.xy.y - windowAabbMin.xy.y;
                }
                else
                    /* if we're click-dragging a window, update the window's new 
                        position relative to the original mouse-down position */
                    window->position = korl_math_v2f32_subtract(mousePosition, context->mouseDownWindowOffset);
            }
            else
                context->mouseHoverPosition = mousePosition;
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
                if(!(   mouseX >= window->position.xy.x                     - windowAabbExpansion 
                     && mouseX <= window->position.xy.x + window->size.xy.x + windowAabbExpansion 
                     && mouseY <= window->position.xy.y                     + windowAabbExpansion 
                     && mouseY >= window->position.xy.y - window->size.xy.y - windowAabbExpansion))
                    continue;
                if(    mouseX >= window->position.xy.x - _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS 
                    && mouseX <= window->position.xy.x + _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS + window->size.xy.x
                    && !window->isContentHidden/*disable resizing top/bottom edges when window content is hidden*/)
                {
                    if(    mouseY >= window->position.xy.y 
                        && mouseY <= window->position.xy.y + _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS)
                        context->mouseHoverWindowEdgeFlags |= KORL_GUI_MOUSE_HOVER_FLAG_UP;
                    if(    mouseY >= window->position.xy.y - window->size.xy.y - _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS 
                        && mouseY <= window->position.xy.y - window->size.xy.y)
                        context->mouseHoverWindowEdgeFlags |= KORL_GUI_MOUSE_HOVER_FLAG_DOWN;
                }
                if(    mouseY >= window->position.xy.y - _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS - window->size.xy.y 
                    && mouseY <= window->position.xy.y + _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS)
                {
                    if(    mouseX >= window->position.xy.x - _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS 
                        && mouseX <= window->position.xy.x)
                        context->mouseHoverWindowEdgeFlags |= KORL_GUI_MOUSE_HOVER_FLAG_LEFT;
                    if(    mouseX >= window->position.xy.x + window->size.xy.x 
                        && mouseX <= window->position.xy.x + window->size.xy.x + _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS )
                        context->mouseHoverWindowEdgeFlags |= KORL_GUI_MOUSE_HOVER_FLAG_RIGHT;
                }
                if(!(window->styleFlags & KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE))
                    context->mouseHoverWindowEdgeFlags = 0;
                context->isMouseHovering = true;
                context->mouseHoverPosition = mousePosition;
                context->identifierMouseHoveredWindow = window->identifier;
                for(u$ wi = 0; wi < KORL_MEMORY_POOL_SIZE(context->widgets); wi++)
                {
                    _Korl_Gui_Widget*const widget = &context->widgets[wi];
                    if(widget->parentWindowIdentifier != window->identifier)
                        continue;
                    if(    mouseX >= widget->cachedAabbMin.xy.x
                        && mouseX <= widget->cachedAabbMax.xy.x
                        && mouseY >= widget->cachedAabbMin.xy.y
                        && mouseY <= widget->cachedAabbMax.xy.y)
                    {
                        context->identifierMouseHoveredWidget = widget->identifier;
                        break;
                    }
                }
                break;
            }
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
    //case WM_MOUSEWHEEL:
    //case WM_MOUSEHWHEEL:
    //case WM_XBUTTONUP:
    //case WM_XBUTTONDOWN:
    //case WM_XBUTTONDBLCLK:
#endif
    }
}
