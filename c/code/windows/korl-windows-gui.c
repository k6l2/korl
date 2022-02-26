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
        context->isTopLevelWindowActive = false;
        context->isMouseDown = false;
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
            context->isMouseDown = true;
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
        context->isMouseDown = false;
        /* bind the windows to the bounds of the swapchain, such that there will 
            always be a square of grabable geometry on the window whose 
            dimensions equal the height of the window title bar style at minimum */
        const Korl_Math_V2u32 swapChainSize = korl_vulkan_getSwapchainSize();
        for(i$ w = KORL_MEMORY_POOL_SIZE(context->windows) - 1; w >= 0; w--)
        {
            if(context->windows[w].position.xy.x < -context->windows[w].size.xy.x + context->style.windowTitleBarPixelSizeY)
                context->windows[w].position.xy.x = -context->windows[w].size.xy.x + context->style.windowTitleBarPixelSizeY;
            if(context->windows[w].position.xy.x > swapChainSize.xy.x - context->style.windowTitleBarPixelSizeY)
                context->windows[w].position.xy.x = swapChainSize.xy.x - context->style.windowTitleBarPixelSizeY;
            if(context->windows[w].position.xy.y > 0)
                context->windows[w].position.xy.y = 0;
            if(context->windows[w].position.xy.y < -KORL_C_CAST(f32, swapChainSize.xy.y) + context->style.windowTitleBarPixelSizeY)
                context->windows[w].position.xy.y = -KORL_C_CAST(f32, swapChainSize.xy.y) + context->style.windowTitleBarPixelSizeY;
        }
        if(!ReleaseCapture())
            korl_logLastError("ReleaseCapture failed!");
        break;}
    case WM_MOUSEMOVE:{
        const i32 mouseX =  GET_X_LPARAM(message->lParam);
        const i32 mouseY = -GET_Y_LPARAM(message->lParam);//inverted, since Windows desktop-space uses a y-axis that points down, which is really annoying to me - I will not tolerate bullshit that doesn't make sense anymore
        if(KORL_MEMORY_POOL_ISEMPTY(context->windows) || !context->isMouseDown)
            break;
        context->windows[KORL_MEMORY_POOL_SIZE(context->windows) - 1].position = korl_math_v2f32_subtract((Korl_Math_V2f32){ korl_checkCast_i$_to_f32(mouseX)
                                                                                                                           , korl_checkCast_i$_to_f32(mouseY) }, 
                                                                                                          context->mouseDownWindowOffset);
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
