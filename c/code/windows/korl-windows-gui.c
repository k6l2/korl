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
        context->isTopLevelWindowActive    = false;
        context->isMouseDown               = false;
        context->isWindowDragged           = false;
        context->identifierMouseDownWidget = NULL;
        /* check to see if we clicked on any windows from the previous frame 
            - note that we're processing windows from front->back, since 
              windows[0] is always the farthest back window */
        for(i$ w = KORL_MEMORY_POOL_SIZE(context->windows) - 1; w >= 0; w--)
        {
            const _Korl_Gui_Window*const window = &context->windows[w];
            korl_assert(window->identifier);
            if(!(   mouseX >= window->position.xy.x 
                 && mouseX <= window->position.xy.x + window->size.xy.x 
                 && mouseY <= window->position.xy.y 
                 && mouseY >= window->position.xy.y - window->size.xy.y))
                continue;
            context->isMouseDown     = true;
            context->isWindowDragged = true;
            /* check to see if we clicked on any widgets from the previous frame */
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
                    context->isWindowDragged = false;
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
                    break;}
                default:{
                    korl_log(ERROR, "mouse actuation not implemented for widget type==%u", widget->type);
                    break;}
                }
                break;
            }
        }
        context->isMouseDown = false;
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
                /* if we're click-dragging a window, update the window's new 
                    position relative to the original mouse-down position */
                context->windows[KORL_MEMORY_POOL_SIZE(context->windows) - 1].position = korl_math_v2f32_subtract(mousePosition, context->mouseDownWindowOffset);
            else
                context->mouseHoverPosition = mousePosition;
        }
        else
        {
            /* check to see whether or not we're hovering over any windows from 
                the previous frame, and if we are record the cursor position 
                relative to the window */
            context->isMouseHovering              = false;
            context->identifierMouseHoveredWidget = NULL;
            for(i$ w = KORL_MEMORY_POOL_SIZE(context->windows) - 1; w >= 0; w--)
            {
                const _Korl_Gui_Window*const window = &context->windows[w];
                korl_assert(window->identifier);
                if(!(   mouseX >= window->position.xy.x 
                     && mouseX <= window->position.xy.x + window->size.xy.x 
                     && mouseY <= window->position.xy.y 
                     && mouseY >= window->position.xy.y - window->size.xy.y))
                    continue;
                context->isMouseHovering = true;
                context->mouseHoverPosition = mousePosition;
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
