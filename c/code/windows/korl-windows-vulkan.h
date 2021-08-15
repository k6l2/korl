#pragma once
#include "korl-vulkan-common.h"
#include "korl-windows-globalDefines.h"
/** This struct is to be passed as param 1 of \c _korl_vulkan_createSurface . */
typedef struct Korl_Windows_Vulkan_SurfaceUserData
{
    HINSTANCE hInstance;
    HWND hWnd;
} Korl_Windows_Vulkan_SurfaceUserData;
