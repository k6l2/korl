#pragma once
#include "korl-globalDefines.h"
#include "korl-memory.h"
#include "korl-gui-common.h"
korl_internal void korl_gui_initialize(void);
/** Start batching a new GUI window.  The user _must_ call \c korl_gui_windowEnd 
 * after each call to \c korl_gui_windowBegin . */
korl_internal void korl_gui_windowBegin(const wchar_t* identifier, Korl_Gui_Window_Style_Flags styleFlags);
korl_internal void korl_gui_windowEnd(void);
/** Prepare a new GUI batch for the current application frame.  The user _must_ 
 * call \c korl_gui_frameEnd after each call to \c korl_gui_frameBegin . */
korl_internal void korl_gui_frameBegin(void);
korl_internal void korl_gui_frameEnd(void);
korl_internal void korl_gui_widgetTextFormat(const wchar_t* textFormat, ...);
