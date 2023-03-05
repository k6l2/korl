#pragma once
#include "korl-globalDefines.h"
#include "korl-memory.h"
#include "korl-gui-common.h"
#include "korl-interface-platform-gui.h"
#include "korl-interface-platform-gfx.h"
#include "korl-windows-globalDefines.h"
typedef struct _Korl_Gui_MouseEvent     _Korl_Gui_MouseEvent;    // internally used by gui modules only
typedef struct _Korl_Gui_CodepointEvent _Korl_Gui_CodepointEvent;// internally used by gui modules only
typedef struct _Korl_Gui_KeyEvent       _Korl_Gui_KeyEvent;// internally used by gui modules only
korl_internal void korl_gui_initialize(void);
korl_internal void korl_gui_onMouseEvent(const _Korl_Gui_MouseEvent* mouseEvent);
korl_internal void korl_gui_onKeyEvent(const _Korl_Gui_KeyEvent* keyEvent);
korl_internal void korl_gui_onCodepointEvent(const _Korl_Gui_CodepointEvent* codepointEvent);
//KORL-FEATURE-000-000-045: gui: add some kind of "style stack" API, and deprecate korl_gui_setFontAsset since it seems too inflexible
korl_internal KORL_FUNCTION_korl_gui_setFontAsset(korl_gui_setFontAsset);
korl_internal KORL_FUNCTION_korl_gui_windowBegin(korl_gui_windowBegin);
korl_internal KORL_FUNCTION_korl_gui_windowEnd(korl_gui_windowEnd);
korl_internal KORL_FUNCTION_korl_gui_setNextWidgetSize(korl_gui_setNextWidgetSize);
korl_internal KORL_FUNCTION_korl_gui_setNextWidgetAnchor(korl_gui_setNextWidgetAnchor);
korl_internal KORL_FUNCTION_korl_gui_setNextWidgetParentAnchor(korl_gui_setNextWidgetParentAnchor);
korl_internal KORL_FUNCTION_korl_gui_setNextWidgetParentOffset(korl_gui_setNextWidgetParentOffset);
korl_internal void korl_gui_frameEnd(void);
korl_internal KORL_FUNCTION_korl_gui_setLoopIndex(korl_gui_setLoopIndex);
korl_internal KORL_FUNCTION_korl_gui_realignY(korl_gui_realignY);
korl_internal KORL_FUNCTION_korl_gui_widgetTextFormat(korl_gui_widgetTextFormat);
korl_internal KORL_FUNCTION_korl_gui_widgetText(korl_gui_widgetText);
korl_internal KORL_FUNCTION_korl_gui_widgetButtonFormat(korl_gui_widgetButtonFormat);
korl_internal KORL_FUNCTION_korl_gui_widgetScrollAreaBegin(korl_gui_widgetScrollAreaBegin);
korl_internal KORL_FUNCTION_korl_gui_widgetScrollAreaEnd(korl_gui_widgetScrollAreaEnd);
korl_internal KORL_FUNCTION_korl_gui_widgetInputText(korl_gui_widgetInputText);
/** \return the content scroll region delta (non-zero) if the user interacted with the scroll bar widget */
korl_internal f32 korl_gui_widgetScrollBar(acu16 label, Korl_Gui_ScrollBar_Axis axis, f32 scrollRegionVisible, f32 scrollRegionContent, f32 contentOffset);
korl_internal void korl_gui_defragment(Korl_Memory_AllocatorHandle stackAllocator);
korl_internal u32  korl_gui_memoryStateWrite(void* memoryContext, u8** pStbDaMemoryState);
korl_internal bool korl_gui_memoryStateRead(u8* memoryState);
