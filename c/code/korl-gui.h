#pragma once
#include "korl-globalDefines.h"
#include "korl-memory.h"
#include "korl-gui-common.h"
#include "korl-interface-platform-gui.h"
korl_internal void korl_gui_initialize(void);
//KORL-FEATURE-000-000-045: gui: add some kind of "style stack" API, and deprecate korl_gui_setFontAsset since it seems too inflexible
korl_internal KORL_FUNCTION_korl_gui_setFontAsset(korl_gui_setFontAsset);
/** Start batching a new GUI window.  The user _must_ call \c korl_gui_windowEnd 
 * after each call to \c korl_gui_windowBegin . */
korl_internal KORL_FUNCTION_korl_gui_windowBegin(korl_gui_windowBegin);
korl_internal KORL_FUNCTION_korl_gui_windowEnd(korl_gui_windowEnd);
korl_internal KORL_FUNCTION_korl_gui_windowSetPosition(korl_gui_windowSetPosition);
korl_internal KORL_FUNCTION_korl_gui_windowSetSize(korl_gui_windowSetSize);
/** Prepare a new GUI batch for the current application frame.  The user _must_ 
 * call \c korl_gui_frameEnd after each call to \c korl_gui_frameBegin . */
korl_internal void korl_gui_frameBegin(void);
korl_internal void korl_gui_frameEnd(void);
/** Make sure to reset this value back to \c 0 after you are finished looping! */
korl_internal KORL_FUNCTION_korl_gui_setLoopIndex(korl_gui_setLoopIndex);
korl_internal KORL_FUNCTION_korl_gui_realignY(korl_gui_realignY);
korl_internal KORL_FUNCTION_korl_gui_widgetTextFormat(korl_gui_widgetTextFormat);
korl_internal KORL_FUNCTION_korl_gui_widgetText(korl_gui_widgetText);
/** 
 * \return The number of times this button was actuated (pressed & released) 
 * since the previous frame.  If the button was actuated more times than the max 
 * value of the return type, those actuations are not counted. */
korl_internal KORL_FUNCTION_korl_gui_widgetButtonFormat(korl_gui_widgetButtonFormat);
korl_internal void korl_gui_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer);
/** I don't like how this API requires us to do file I/O in modules outside of 
 * korl-file; maybe improve this in the future to use korl-file API instea of 
 * Win32 API?
 * KORL-ISSUE-000-000-069: savestate/file: contain filesystem API to korl-file? */
korl_internal bool korl_gui_saveStateRead(HANDLE hFile);
