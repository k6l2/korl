#pragma once
#include "korl-globalDefines.h"
enum
{
    KORL_GUI_WINDOW_STYLE_FLAG_NONE           = 0,
    KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR       = 1 << 0,
    KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE    = 1 << 1,// By default, windows will be sized to their contents _only_ on the first frame.  This flag will cause the window to be resized to its contents on every frame.
    KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE      = 1 << 2,// incompatible with the AUTO_RESIZE flag; this is not the same as not having the AUTO_RESIZE flag, as we want the ability to have a window which does not auto-resize _or_ allow the user to resize it
    KORL_GUI_WINDOW_STYLE_FLAG_DEFAULT_ACTIVE = 1 << 3,// when the window is first created, it will automatically become activated
    KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT       = KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR | KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE,
};
typedef enum Korl_Gui_ScrollBar_Axis
    { KORL_GUI_SCROLL_BAR_AXIS_X
    , KORL_GUI_SCROLL_BAR_AXIS_Y
} Korl_Gui_ScrollBar_Axis;
typedef u32 Korl_Gui_Window_Style_Flags;// separate flag type definition for C++ compatibility reasons (C++ does not easily allow the | operator for enums; cool language, bro)
