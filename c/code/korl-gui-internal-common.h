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
korl_global_const f32 _KORL_GUI_WINDOW_AABB_EDGE_THICKNESS = 10.f;
typedef enum _Korl_Gui_Keyboard_ModifierFlags
    {_KORL_GUI_KEYBOARD_MODIFIER_FLAG_SHIFT     = 1<<0
    ,_KORL_GUI_KEYBOARD_MODIFIER_FLAG_CONTROL   = 1<<1
    ,_KORL_GUI_KEYBOARD_MODIFIER_FLAG_ALTERNATE = 1<<2
} _Korl_Gui_Keyboard_ModifierFlags;
typedef struct _Korl_Gui_MouseEvent
{
    enum
        {_KORL_GUI_MOUSE_EVENT_TYPE_MOVE
        ,_KORL_GUI_MOUSE_EVENT_TYPE_BUTTON
        ,_KORL_GUI_MOUSE_EVENT_TYPE_WHEEL
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
            _Korl_Gui_Keyboard_ModifierFlags keyboardModifierFlags;
            enum
                {_KORL_GUI_MOUSE_EVENT_BUTTON_ID_LEFT
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
typedef enum Korl_Gui_ScrollBar_Region
    {KORL_GUI_SCROLL_BAR_REGION_PRE_SLIDER
    ,KORL_GUI_SCROLL_BAR_REGION_SLIDER
    ,KORL_GUI_SCROLL_BAR_REGION_POST_SLIDER
} Korl_Gui_ScrollBar_Region;
typedef struct _Korl_Gui_Widget
{
    u64 identifierHashParent;// a value of 0 => this Widget has no parent
    u64 identifierHash;// _all_ widgets _must_ have a non-zero identifierHash
    u16 transientChildCount;// cleared at the end of each frame; accumulated during the frame each time a direct child widget is added; directly affects the orderIndex of each child widget
    u16 orderIndex;// determines the order in which widgets are processed/displayed in their parent, as well as top-level widgets (windows) relative to one another; 0 => the bottom-most widget that is drawn below all other widgets at the same heirarchical depth; in other words, lower values are processed/drawn _first_
    bool isContentHidden;// disables all child widgets (all logic, including graphics)
    f32 hiddenContentPreviousSizeY;// used in combination with isContentHidden; allows us to restore the widget's original size.y when content is no longer hidden
    Korl_Math_V2f32 parentAnchor;// defines where on this widget's parent the coordinate origin is located; the default {0,0} makes the widget position relative to the parent's top-left corner
    Korl_Math_V2f32 parentOffset;// the default value {NaN,Nan} makes the widget position be set to the transient widget cursor position of its parent; the position offset to be applied to the final widget's position at the end of the frame from the parent anchor position; not applicable to widgets with no parent
    Korl_Math_V2f32 position;// relative to the top-left corner of the widget, where our coordinate frame origin is either the bottom-left corner of the rendering surface (for root widgets) or the position derived from the parent widget's placement (size & position) & the parentAnchor member, with the +Y axis pointing UP (as the graphics gods intended)
    Korl_Math_V2f32 size;
    bool usedThisFrame;// set each frame this widget is used/updated by the user; when this value is cleared, non-root widgets will be destroyed & cleaned up at the end of the frame
    bool isHovered;// reset at the end of each frame; set if a mouse hover event is propagated to this widget at the top of the frame
    bool realignY;// set when the user calls `korl_gui_realignY` after this widget
    bool isSizeCustom;// the user has called `korl_gui_setNextWidgetSize` to give this widget a custom size for the current frame
    enum
        {KORL_GUI_WIDGET_TYPE_WINDOW
        ,KORL_GUI_WIDGET_TYPE_SCROLL_AREA
        ,KORL_GUI_WIDGET_TYPE_SCROLL_BAR
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
            u8 titleBarButtonCount;// used for resizing the window on the first frame
        } window;
        struct
        {
            Korl_Math_V2f32 contentOffset;// actual scroll values of the SCROLL_AREA widget
            Korl_Math_V2f32 aabbChildrenSize;// size of the AABB of all child widget content
            Korl_Math_V2f32 aabbScrollableSize;// a modified version of `aabbChildrenSize` which includes overhead area to allow the user to scroll "past" the scroll bars so the content is not obscured by them
            bool hasScrollBarX;
            bool hasScrollBarY;
            bool isScrolledToEndY;// set to true if the scroll region is viewing the very bottom of the content region
        } scrollArea;
        struct
        {
            Korl_Gui_ScrollBar_Axis axis;
            f32 visibleRegionRatio;
            f32 scrollPositionRatio;
            Korl_Gui_ScrollBar_Region mouseDownRegion;
            Korl_Math_V2f32 mouseDownSliderOffset;// vector from slider=>mouse
            f32 draggedMagnitude;// set during processing of mouse input events, used & reset when the widget is invoked
            enum
                {_KORL_GUI_WIDGET_SCROLL_BAR_DRAG_MODE_SLIDER // we're click+dragging on the slider
                ,_KORL_GUI_WIDGET_SCROLL_BAR_DRAG_MODE_CONTROL// we held [Ctrl] & click+dragged anywhere
            } dragMode;
        } scrollBar;
        struct
        {
            acu16 displayText;// stored in the context stack allocator each frame
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
    /** We don't need this to be a member of window widgets because we 
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
    /** These properties are reset to an invalid value at the end of each frame.  
     * Once they are set to a valid value by any API, the next widget created 
     * via \c _korl_gui_getWidget will have the property applied to their 
     * respective member with the same identifier.  When all valid properties 
     * are applied, all these values will be reset to an invalid state.
     */
    struct
    {
        Korl_Math_V2f32 size;
        Korl_Math_V2f32 parentAnchor;
        Korl_Math_V2f32 parentOffset;
        i32             orderIndex;
    } transientNextWidgetModifiers;
    /// @TODO: could we get rid of currentWidgetIndex in favor of just adding a transient flag to transientNextWidgetModifiers ?  It seems like this would be more robust, considering that widgets should be able to spawn child widgets when they are spawned, so without this we have the potential in the future to accidentally set the realignY flag of one of the last spawned widget's child widgets
    i16 currentWidgetIndex;// used to modify properties of the last widget created via _korl_gui_getWidget; example: when korl_gui_realignY is called, we set a special flag in the last widget so that 
} _Korl_Gui_Context;
korl_global_variable _Korl_Gui_Context _korl_gui_context;
