#include "korl-windows-vulkan.h"
#include <vulkan/vulkan_win32.h>
korl_internal void _korl_vulkan_createSurface(void* userData)
{
    _Korl_Vulkan_Context*const context               = &g_korl_vulkan_context;
    _Korl_Vulkan_SurfaceContext*const surfaceContext = &g_korl_vulkan_surfaceContext;
    Korl_Windows_Vulkan_SurfaceUserData*const windowsUserData = 
        KORL_C_CAST(Korl_Windows_Vulkan_SurfaceUserData*, userData);
    memset(surfaceContext, 0, sizeof(*surfaceContext));
    VkResult vkResult = VK_SUCCESS;
    /* create the Windows-specific surface */
    KORL_ZERO_STACK(VkWin32SurfaceCreateInfoKHR, createInfoSurface);
    createInfoSurface.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfoSurface.hinstance = windowsUserData->hInstance;
    createInfoSurface.hwnd      = windowsUserData->hWnd;
    vkResult = 
        vkCreateWin32SurfaceKHR(
            context->instance, &createInfoSurface, context->allocator, 
            &surfaceContext->surface);
    korl_assert(vkResult == VK_SUCCESS);
}
