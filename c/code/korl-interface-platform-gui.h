#pragma once
#include "korl-gui-common.h"
#include "korl-globalDefines.h"
typedef enum Korl_Gui_Widget_Text_Flags
    { KORL_GUI_WIDGET_TEXT_FLAGS_NONE
    , KORL_GUI_WIDGET_TEXT_FLAG_LOG// format the text assuming that it is raw KORL log buffer text
} Korl_Gui_Widget_Text_Flags;
#define KORL_FUNCTION_korl_gui_setFontAsset(name)       void name(const wchar_t* fontAssetName)
#define KORL_FUNCTION_korl_gui_windowBegin(name)        void name(const wchar_t* titleBarText, bool* out_isOpen, Korl_Gui_Window_Style_Flags styleFlags)
#define KORL_FUNCTION_korl_gui_windowEnd(name)          void name(void)
/**
 * \param anchorX Ratio to be applied to the window size in the \c X dimension, 
 * which will define the local window-space offset used by \c positionX to 
 * define the final window position in screen-space.  Local window-space is 
 * relative to the top-left corner of the window (for now).
 * \param anchorY Ratio to be applied to the window size in the \c Y dimension, 
 * which will define the local window-space offset used by \c positionY to 
 * define the final window position in screen-space.  Local window-space is 
 * relative to the top-left corner of the window (for now).
 * \param positionX relative to the bottom-left corner of the application window
 * \param positionY relative to the bottom-left corner of the application window
 */
#define KORL_FUNCTION_korl_gui_windowSetPosition(name)  void name(f32 anchorX, f32 anchorY, f32 positionX, f32 positionY)
#define KORL_FUNCTION_korl_gui_windowSetSize(name)      void name(f32 sizeX, f32 sizeY)
#define KORL_FUNCTION_korl_gui_setLoopIndex(name)       void name(u$ loopIndex)
/**
 * When a widget is added to a window, the default behavior is to place the 
 * widget's window-space position at {0, previousWidgetAabb.bottom}.  Invoking 
 * this function allows the user to change this behavior, such that the new 
 * widget is placed at {previousWidgetAabb.right, previousWidgetAabb.top}, 
 * effectively alligning them on the same Y position within the window.
 */
#define KORL_FUNCTION_korl_gui_realignY(name)           void name(void)
#define KORL_FUNCTION_korl_gui_widgetTextFormat(name)   void name(const wchar_t* textFormat, ...)
#define KORL_FUNCTION_korl_gui_widgetText(name)         void name(const wchar_t* identifier, acu16 newText, u32 maxLineCount, fnSig_korl_gfx_text_codepointTest* codepointTest, void* codepointTestUserData, Korl_Gui_Widget_Text_Flags flags)
#define KORL_FUNCTION_korl_gui_widgetButtonFormat(name) u8   name(const wchar_t* textFormat, ...)
