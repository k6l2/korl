#pragma once
#include "korl-gui-common.h"
#include "korl-globalDefines.h"
#define KORL_FUNCTION_korl_gui_setFontAsset(name)              void name(const wchar_t* fontAssetName)
#define KORL_FUNCTION_korl_gui_windowBegin(name)               void name(const wchar_t* titleBarText, bool* out_isOpen, Korl_Gui_Window_Style_Flags styleFlags)
#define KORL_FUNCTION_korl_gui_windowEnd(name)                 void name(void)
#define KORL_FUNCTION_korl_gui_setNextWidgetSize(name)         void name(Korl_Math_V2f32 size)
#define KORL_FUNCTION_korl_gui_setNextWidgetAnchor(name)       void name(Korl_Math_V2f32 localAnchorRatioRelativeToTopLeft)
#define KORL_FUNCTION_korl_gui_setNextWidgetParentAnchor(name) void name(Korl_Math_V2f32 anchorRatioRelativeToParentTopLeft)
#define KORL_FUNCTION_korl_gui_setNextWidgetParentOffset(name) void name(Korl_Math_V2f32 positionRelativeToAnchor)
#define KORL_FUNCTION_korl_gui_setLoopIndex(name)              void name(u$ loopIndex)
/**
 * When a widget is added to a window, the default behavior is to place the 
 * widget's window-space position at {0, previousWidgetAabb.bottom}.  Invoking 
 * this function allows the user to change this behavior, such that the new 
 * widget is placed at {previousWidgetAabb.right, previousWidgetAabb.top}, 
 * effectively alligning them on the same Y position within the window.
 */
#define KORL_FUNCTION_korl_gui_realignY(name)                  void name(void)
#define KORL_FUNCTION_korl_gui_widgetTextFormat(name)          void name(const wchar_t* textFormat, ...)
typedef enum Korl_Gui_Widget_Text_Flags
    { KORL_GUI_WIDGET_TEXT_FLAGS_NONE
    , KORL_GUI_WIDGET_TEXT_FLAG_LOG = 1<<0// format the text assuming that it is raw KORL log buffer text
} Korl_Gui_Widget_Text_Flags;
#define KORL_FUNCTION_korl_gui_widgetText(name)                void name(const wchar_t* identifier, acu16 newText, u32 maxLineCount, fnSig_korl_gfx_text_codepointTest* codepointTest, void* codepointTestUserData, Korl_Gui_Widget_Text_Flags flags)
#define KORL_FUNCTION_korl_gui_widgetButtonFormat(name)        u8   name(const wchar_t* textFormat, ...)
typedef enum Korl_Gui_Widget_ScrollArea_Flags
    { KORL_GUI_WIDGET_SCROLL_AREA_FLAGS_NONE
    , KORL_GUI_WIDGET_SCROLL_AREA_FLAG_STICK_MAX_SCROLL = 1 << 0
} Korl_Gui_Widget_ScrollArea_Flags;
#define KORL_FUNCTION_korl_gui_widgetScrollAreaBegin(name)     void name(acu16 label, Korl_Gui_Widget_ScrollArea_Flags flags)
#define KORL_FUNCTION_korl_gui_widgetScrollAreaEnd(name)       void name(void)
