#pragma once
#include "korl-globalDefines.h"
typedef enum Korl_Gui_Window_Style_Flags
{
    KORL_GUI_WINDOW_STYLE_FLAG_NONE     = 0,
    KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR = 1 << 0,
    KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT = KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR,
} Korl_Gui_Window_Style_Flags;
