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
korl_internal void korl_gui_frameEnd(void);
/** \return the content scroll region delta (non-zero) if the user interacted with the scroll bar widget */
korl_internal f32  korl_gui_widgetScrollBar(acu16 label, Korl_Gui_ScrollBar_Axis axis, f32 scrollRegionVisible, f32 scrollRegionContent, f32 contentOffset);
korl_internal void korl_gui_defragment(Korl_Memory_AllocatorHandle stackAllocator);
korl_internal u32  korl_gui_memoryStateWrite(void* memoryContext, Korl_Memory_ByteBuffer** pByteBuffer);
korl_internal void korl_gui_memoryStateRead(const u8* memoryState);
