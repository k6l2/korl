#pragma once
#include "korl-globalDefines.h"
#include "korl-gui-common.h"
#include "korl-math.h"
#include "korl-memory.h"
#include "korl-vulkan.h"
/** the edges of the window must have their own individual AABBs to allow mouse 
 * interactions with them (window is hovered, resize windows), and this value 
 * defines how far from the edges of each window AABB this collision region is 
 * in both dimensions */
korl_global_const f32 _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS = 8.f;
#if 0//@TODO: recycle
typedef enum _Korl_Gui_SpecialWidgetFlags
{
    KORL_GUI_SPECIAL_WIDGET_FLAGS_NONE         = 0,
    KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_CLOSE  = 1 << 0,
    KORL_GUI_SPECIAL_WIDGET_FLAG_BUTTON_HIDE   = 1 << 1,
    KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_X  = 1 << 2,
    KORL_GUI_SPECIAL_WIDGET_FLAG_SCROLL_BAR_Y  = 1 << 3,
} _Korl_Gui_SpecialWidgetFlags;
typedef struct _Korl_Gui_Window
{
    _Korl_Gui_SpecialWidgetFlags specialWidgetFlags;// flags raised indicate the presence of special widgets
    _Korl_Gui_SpecialWidgetFlags specialWidgetFlagsPressed;// flags are raised when the corresponding special widget is pressed
    f32 hiddenContentPreviousSizeY;
    Korl_Math_V2f32 size;
    u32 styleFlags;// uses the Korl_Gui_Window_Style_Flags enum
    Korl_Math_Aabb2f32 cachedContentAabb;// "content" refers to the accumulation of all widgets contained in this window
    Korl_Math_V2f32 cachedAvailableContentSize;
    f32 cachedScrollBarLengthX;
    f32 cachedScrollBarLengthY;
    f32 scrollBarPositionX;
    f32 scrollBarPositionY;
    u16 widgets;// related to _Korl_Gui_Widget::orderIndex; the total # of direct widget children
} _Korl_Gui_Window;
#endif
typedef struct _Korl_Gui_Widget
{
    u64 identifierHashParent;// a value of 0 => this Widget has no parent
    u64 identifierHash;// _all_ widgets _must_ have a non-zero identifierHash
    u16 orderIndex;// determines the order in which widgets are processed/displayed in their parent, as well as top-level widgets (windows) relative to one another; 0 => the bottom-most widget that is drawn below all other widgets at the same heirarchical depth; in other words, lower values are processed/drawn _first_
    bool isContentHidden;// disables all child widgets (all logic, including graphics)
    Korl_Math_V2f32 position;// relative to the top-left corner of the widget, where our coordinate frame origin is the bottom-left corner of the rendering surface, with the +Y axis pointing UP (as the graphics gods intended)
    Korl_Math_V2f32 size;
    bool usedThisFrame;// set each frame this widget is used/updated by the user; when this value is cleared, 
#if 0//@TODO: recycle
    bool realignY;
    Korl_Math_Aabb2f32 cachedAabb;// invalid until after the next call to korl_gui_frameEnd
    bool cachedIsInteractive;
#endif
    enum
        {KORL_GUI_WIDGET_TYPE_WINDOW
        ,KORL_GUI_WIDGET_TYPE_TEXT
        ,KORL_GUI_WIDGET_TYPE_BUTTON
    } type;
    union
    {
        struct
        {
            // const wchar_t* titleBarText;// @TODO: use string pool
            bool isOpen;
            bool isFirstFrame;// used to auto-size the window on the first frame, since we don't have size values from the previous frame to go off of
            Korl_Gui_Window_Style_Flags styleFlags;
        } window;
        struct
        {
            // const wchar_t* displayText;// @TODO: use string pool
            Korl_Gfx_Text* gfxText;
        } text;
        struct
        {
            // const wchar_t* displayText;// @TODO: use string pool
            u8 actuationCount;
        } button;
    } subType;
} _Korl_Gui_Widget;
typedef struct _Korl_Gui_Context
{
    struct
    {
        Korl_Vulkan_Color4u8 colorWindow;
        Korl_Vulkan_Color4u8 colorWindowActive;
        Korl_Vulkan_Color4u8 colorWindowBorder;
        Korl_Vulkan_Color4u8 colorWindowBorderHovered;
        Korl_Vulkan_Color4u8 colorWindowBorderResize;
        Korl_Vulkan_Color4u8 colorWindowBorderActive;
        Korl_Vulkan_Color4u8 colorTitleBar;
        Korl_Vulkan_Color4u8 colorTitleBarActive;
        Korl_Vulkan_Color4u8 colorButtonInactive;
        Korl_Vulkan_Color4u8 colorButtonActive;
        Korl_Vulkan_Color4u8 colorButtonPressed;
        Korl_Vulkan_Color4u8 colorButtonWindowTitleBarIcons;
        Korl_Vulkan_Color4u8 colorButtonWindowCloseActive;
        Korl_Vulkan_Color4u8 colorScrollBar;
        Korl_Vulkan_Color4u8 colorScrollBarActive;
        Korl_Vulkan_Color4u8 colorScrollBarPressed;
        Korl_Vulkan_Color4u8 colorText;
        Korl_Vulkan_Color4u8 colorTextOutline;
        f32                  textOutlinePixelSize;
        // au16                 fontWindowText;// @TODO: use string pool
        f32                  windowTextPixelSizeY;
        f32                  windowTitleBarPixelSizeY;
        f32                  widgetSpacingY;
        f32                  widgetButtonLabelMargin;
        f32                  windowScrollBarPixelWidth;
    } style;
    Korl_Memory_AllocatorHandle allocatorHandleHeap;
    Korl_Memory_AllocatorHandle allocatorHandleStack;
    _Korl_Gui_Widget* stbDaWidgets;// everything is a Widget, including windows!
    u$ loopIndex;// combined with window/widget identifiers to create the final identifierHash
    /** Helps ensure that the user calls \c korl_gui_windowBegin/End the correct 
     * # of times.  When this value < 0, a new window must be started before 
     * calling any widget API.  If the user calls a widget function outside of 
     * the \c korl_gui_windowBegin/End calls, a default "debug" window will be 
     * automatically selected. */
    i16 currentWindowIndex;
#if 0//@TODO: recycle
    i16 currentWidgetIndex;
    /** Windows are stored from back=>front.  In other words, the window at 
     * index \c 0 will be drawn behind all other windows. */
    _Korl_Gui_Window* stbDaWindows;
    /** We don't need this to be a member of \c _Korl_Gui_Window because we 
     * already know:  
     * - there will only ever be ONE active window
     * - the active window will ALWAYS be the top level window */
    bool isTopLevelWindowActive;
    bool isMouseDown;// This flag is raised when we mouse down inside a window
    bool isWindowDragged;// only raised when we click a window outside of widgets
    bool isWindowResizing;// only raised when we click a window's resize edge
    Korl_Math_V2f32 mouseDownWindowOffset;
    Korl_Math_V2f32 mouseDownOffsetSpecialWidget;
    u64 identifierHashMouseDownWidget;
    bool isMouseHovering;
    _Korl_Gui_SpecialWidgetFlags specialWidgetFlagsMouseDown;
    u64 identifierHashMouseHoveredWidget;
    u64 identifierHashMouseHoveredWindow;
    enum
    {
        KORL_GUI_MOUSE_HOVER_FLAGS_NONE = 0,
        KORL_GUI_MOUSE_HOVER_FLAG_LEFT  = 1<<0,
        KORL_GUI_MOUSE_HOVER_FLAG_RIGHT = 1<<1,
        KORL_GUI_MOUSE_HOVER_FLAG_UP    = 1<<2,
        KORL_GUI_MOUSE_HOVER_FLAG_DOWN  = 1<<3,
    } mouseHoverWindowEdgeFlags;
    Korl_Math_V2f32 mouseHoverPosition;
#endif
} _Korl_Gui_Context;
korl_global_variable _Korl_Gui_Context _korl_gui_context;
