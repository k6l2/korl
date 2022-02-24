#include "korl-windows-gui.h"
#include "korl-gui-common.h"
#include "korl-checkCast.h"
korl_internal void korl_gui_windows_processMessage(const MSG* message)
{
    _Korl_Gui_Context*const context = &_korl_gui_context;
    korl_assert(context->frameSequenceCounter == 0);
    /* identify & process mouse events 
        https://docs.microsoft.com/en-us/windows/win32/learnwin32/mouse-clicks */
    switch(message->message)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    /*case WM_LBUTTONDBLCLK:*/
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    /*case WM_RBUTTONDBLCLK:*/
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    /*case WM_MBUTTONDBLCLK:*/{
        const i32 x =  LOWORD(message->lParam);
        const i32 y = -HIWORD(message->lParam);//inverted, since Windows window-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        const bool isDown = (message->message - WM_LBUTTONDOWN) % 3 == 0;
        /* deactivate the top level window, in case it wasn't already */
        context->isTopLevelWindowActive = false;
        /* check to see if we clicked on any windows from the previous frame */
        for(i$ w = KORL_MEMORY_POOL_SIZE(context->windows) - 1; w >= 0; w--)
        {
            const _Korl_Gui_Window*const window = &context->windows[w];
            if(!window->identifier)
                continue;
            const bool mouseTouchingWindow = (x >= window->position.xy.x 
                                           && x <= window->position.xy.x + window->size.xy.x 
                                           && y <= window->position.xy.y 
                                           && y >= window->position.xy.y - window->size.xy.y);
            if(!mouseTouchingWindow)
                continue;
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
        }break;
    //case WM_MOUSEWHEEL:
    //case WM_MOUSEHWHEEL:
    //case WM_XBUTTONUP:
    //case WM_XBUTTONDOWN:
    //case WM_XBUTTONDBLCLK:
    //case WM_MOUSEMOVE:
    }
}
