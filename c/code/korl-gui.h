#pragma once
#include "korl-globalDefines.h"
#include "korl-memory.h"
#include "korl-gui-common.h"
#include "korl-interface-platform.h"
korl_internal void korl_gui_initialize(void);
korl_internal KORL_PLATFORM_GUI_SET_FONT_ASSET(korl_gui_setFontAsset);
/** Start batching a new GUI window.  The user _must_ call \c korl_gui_windowEnd 
 * after each call to \c korl_gui_windowBegin . */
korl_internal KORL_PLATFORM_GUI_WINDOW_BEGIN(korl_gui_windowBegin);
korl_internal KORL_PLATFORM_GUI_WINDOW_END(korl_gui_windowEnd);
/** Prepare a new GUI batch for the current application frame.  The user _must_ 
 * call \c korl_gui_frameEnd after each call to \c korl_gui_frameBegin . */
korl_internal void korl_gui_frameBegin(void);
korl_internal void korl_gui_frameEnd(void);
korl_internal KORL_PLATFORM_GUI_WIDGET_TEXT_FORMAT(korl_gui_widgetTextFormat);
/** 
 * \return The number of times this button was actuated (pressed & released) 
 * since the previous frame.  If the button was actuated more times than the max 
 * value of the return type, those actuations are not counted. */
korl_internal KORL_PLATFORM_GUI_WIDGET_BUTTON_FORMAT(korl_gui_widgetButtonFormat);
