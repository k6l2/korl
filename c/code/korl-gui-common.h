#pragma once
#include "korl-globalDefines.h"
typedef enum Korl_Gui_Window_Style_Flags
{
    KORL_GUI_WINDOW_STYLE_FLAG_NONE        = 0,
    KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR    = 1 << 0,
    KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE = 1 << 1,// By default, windows will be sized to their contents _only_ on the first frame.  This flag will cause the window to be resized to its contents on every frame.
    KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT    = KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR,
} Korl_Gui_Window_Style_Flags;
