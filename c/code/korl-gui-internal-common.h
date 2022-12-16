#pragma once
#include "korl-globalDefines.h"
#include "korl-gui-common.h"
#include "korl-math.h"
#include "korl-memory.h"
#include "korl-vulkan.h"
#include "korl-stringPool.h"
#include "korl-gfx.h"
/** the edges of the window must have their own individual AABBs to allow mouse 
 * interactions with them (window is hovered, resize windows), and this value 
 * defines how far from the edges of each window AABB this collision region is 
 * in both dimensions */
korl_global_const f32 _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS = 8.f;
typedef struct _Korl_Gui_MouseEvent
{
    enum
        { _KORL_GUI_MOUSE_EVENT_TYPE_MOVE
        , _KORL_GUI_MOUSE_EVENT_TYPE_BUTTON
        , _KORL_GUI_MOUSE_EVENT_TYPE_WHEEL
    } type;
    union
    {
        struct
        {
            Korl_Math_V2f32 position;
        } move;
        struct
        {
            Korl_Math_V2f32 position;
            enum
                { _KORL_GUI_MOUSE_EVENT_BUTTON_ID_LEFT
            } id;
            bool pressed;
        } button;
        struct
        {
            Korl_Math_V2f32 position;
            enum
                {_KORL_GUI_MOUSE_EVENT_WHEEL_AXIS_X
                ,_KORL_GUI_MOUSE_EVENT_WHEEL_AXIS_Y
            } axis;
            f32 value;
        } wheel;
    } subType;
} _Korl_Gui_MouseEvent;
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
    u16 transientChildCount;// cleared at the end of each frame; accumulated during the frame each time a direct child widget is added; directly affects the orderIndex of each child widget
    u16 orderIndex;// determines the order in which widgets are processed/displayed in their parent, as well as top-level widgets (windows) relative to one another; 0 => the bottom-most widget that is drawn below all other widgets at the same heirarchical depth; in other words, lower values are processed/drawn _first_
    bool isContentHidden;// disables all child widgets (all logic, including graphics)
    Korl_Math_V2f32 position;// relative to the top-left corner of the widget, where our coordinate frame origin is the bottom-left corner of the rendering surface, with the +Y axis pointing UP (as the graphics gods intended)
    Korl_Math_V2f32 size;
    bool usedThisFrame;// set each frame this widget is used/updated by the user; when this value is cleared, non-root widgets will be destroyed & cleaned up at the end of the frame
    bool isHovered;// reset at the end of each frame; set if a mouse hover event is propagated to this widget at the top of the frame
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
            Korl_StringPool_String titleBarText;
            bool isOpen;
            bool isFirstFrame;// used to auto-size the window on the first frame, since we don't have size values from the previous frame to go off of
            Korl_Gui_Window_Style_Flags styleFlags;
        } window;
        struct
        {
            acu16 displayText;
            Korl_Gfx_Text* gfxText;
        } text;
        struct
        {
            acu16 displayText;// stored in the context stack allocator each frame
            u8 actuationCount;
            enum
                {_KORL_GUI_WIDGET_BUTTON_DISPLAY_TEXT
                ,_KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_CLOSE
                ,_KORL_GUI_WIDGET_BUTTON_DISPLAY_WINDOW_MINIMIZE
            } display;
        } button;
    } subType;
} _Korl_Gui_Widget;
typedef struct _Korl_Gui_Context
{
    struct
    {
        Korl_Vulkan_Color4u8   colorWindow;
        Korl_Vulkan_Color4u8   colorWindowActive;
        Korl_Vulkan_Color4u8   colorWindowBorder;
        Korl_Vulkan_Color4u8   colorWindowBorderHovered;
        Korl_Vulkan_Color4u8   colorWindowBorderResize;
        Korl_Vulkan_Color4u8   colorWindowBorderActive;
        Korl_Vulkan_Color4u8   colorTitleBar;
        Korl_Vulkan_Color4u8   colorTitleBarActive;
        Korl_Vulkan_Color4u8   colorButtonInactive;
        Korl_Vulkan_Color4u8   colorButtonActive;
        Korl_Vulkan_Color4u8   colorButtonPressed;
        Korl_Vulkan_Color4u8   colorButtonWindowTitleBarIcons;
        Korl_Vulkan_Color4u8   colorButtonWindowCloseActive;
        Korl_Vulkan_Color4u8   colorScrollBar;
        Korl_Vulkan_Color4u8   colorScrollBarActive;
        Korl_Vulkan_Color4u8   colorScrollBarPressed;
        Korl_Vulkan_Color4u8   colorText;
        Korl_Vulkan_Color4u8   colorTextOutline;
        f32                    textOutlinePixelSize;
        Korl_StringPool_String fontWindowText;
        f32                    windowTextPixelSizeY;
        f32                    windowTitleBarPixelSizeY;
        f32                    widgetSpacingY;
        f32                    widgetButtonLabelMargin;
        f32                    windowScrollBarPixelWidth;
    } style;
    Korl_Memory_AllocatorHandle allocatorHandleHeap;
    Korl_Memory_AllocatorHandle allocatorHandleStack;
    Korl_StringPool stringPool;
    _Korl_Gui_Widget* stbDaWidgets;// everything is a Widget, including windows!
    struct _Korl_Gui_UsedWidget* stbDaUsedWidgets;// list of currently in-use widgets, sorted from back-to-front; re-built each call to korl_gui_frameEnd
    u16 rootWidgetOrderIndexHighest;// like stbDaUsedWidgets, this is updated each call to korl-gui-frameEnd
    u$ loopIndex;// combined with window/widget identifiers to create the final identifierHash
    /** Serves the following purposes:
     * - determines the parent hierarchy of each widget; when a new widget is 
     *   created, it becomes the direct child of the last widget in this stack
     * - help ensure user calls \c korl_gui_windowBegin/End the correct # of 
     *   times */
    i16* stbDaWidgetParentStack;// reallocated from the stack allocator each frame; each element represents an index into the stbDaWidgets collection
    /** We don't need this to be a member of \c _Korl_Gui_Window because we 
     * already know:  
     * - there will only ever be ONE active window
     * - the active window will ALWAYS be the top level window */
    bool isTopLevelWindowActive;
    u64 identifierHashWidgetMouseDown;
    u64 identifierHashWidgetDragged;
    u64 identifierHashWindowHovered;
    Korl_Math_V2f32 mouseDownWidgetOffset;
    enum
    {
        KORL_GUI_EDGE_FLAGS_NONE = 0,
        KORL_GUI_EDGE_FLAG_LEFT  = 1<<0,
        KORL_GUI_EDGE_FLAG_RIGHT = 1<<1,
        KORL_GUI_EDGE_FLAG_UP    = 1<<2,
        KORL_GUI_EDGE_FLAG_DOWN  = 1<<3,
    } mouseHoverWindowEdgeFlags;
#if 0//@TODO: recycle
    i16 currentWidgetIndex;
    /** Windows are stored from back=>front.  In other words, the window at 
     * index \c 0 will be drawn behind all other windows. */
    _Korl_Gui_Window* stbDaWindows;
    bool isWindowResizing;// only raised when we click a window's resize edge
    Korl_Math_V2f32 mouseDownOffsetSpecialWidget;
    u64 identifierHashMouseDownWidget;
    bool isMouseHovering;
    _Korl_Gui_SpecialWidgetFlags specialWidgetFlagsMouseDown;
    u64 identifierHashMouseHoveredWidget;
    u64 identifierHashMouseHoveredWindow;
    Korl_Math_V2f32 mouseHoverPosition;
#endif
} _Korl_Gui_Context;
korl_global_variable _Korl_Gui_Context _korl_gui_context;
