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
typedef struct _Korl_Gui_Window
{
    const void* identifier;
    bool usedThisFrame;
    bool isFirstFrame;
    bool hasTitleBarButtonClose;
    bool titleBarButtonPressedClose;
    bool isOpen;
    Korl_Math_V2f32 position;// relative to the upper-left corner of the window
    Korl_Math_V2f32 size;
    u32 styleFlags;// uses the Korl_Gui_Window_Style_Flags enum
} _Korl_Gui_Window;
typedef struct _Korl_Gui_Widget
{
    const void* parentWindowIdentifier;
    const void* identifier;
    bool usedThisFrame;
    Korl_Math_V2f32 cachedAabbMin;// invalid until after the next call to korl_gui_frameEnd
    Korl_Math_V2f32 cachedAabbMax;// invalid until after the next call to korl_gui_frameEnd
    bool cachedIsInteractive;
    enum
    {
        KORL_GUI_WIDGET_TYPE_TEXT,
        KORL_GUI_WIDGET_TYPE_BUTTON,
    } type;
    union
    {
        struct
        {
            const wchar_t* displayText;
        } text;
        struct
        {
            const wchar_t* displayText;
            u8 actuationCount;
        } button;
    } subType;
} _Korl_Gui_Widget;
typedef struct _Korl_Gui_Context
{
    Korl_Memory_Allocator allocatorStack;
    struct
    {
        Korl_Vulkan_Color colorWindow;
        Korl_Vulkan_Color colorWindowActive;
        Korl_Vulkan_Color colorWindowBorder;
        Korl_Vulkan_Color colorWindowBorderHovered;
        Korl_Vulkan_Color colorWindowBorderResize;
        Korl_Vulkan_Color colorWindowBorderActive;
        Korl_Vulkan_Color colorTitleBar;
        Korl_Vulkan_Color colorTitleBarActive;
        Korl_Vulkan_Color colorButtonInactive;
        Korl_Vulkan_Color colorButtonActive;
        Korl_Vulkan_Color colorButtonPressed;
        Korl_Vulkan_Color colorButtonWindowTitleBarIcons;
        Korl_Vulkan_Color colorButtonWindowCloseActive;
        const wchar_t* fontWindowText;
        f32 windowTextPixelSizeY;
        f32 windowTitleBarPixelSizeY;
        f32 widgetSpacingY;
        f32 widgetButtonLabelMargin;
    } style;
    /** Helps ensure that the user calls \c korl_gui_windowBegin/End the correct 
     * # of times.  When this value == korl_arraySize(windows), a new window 
     * must be started before calling any widget API. */
    u8 currentWindowIndex;
    /** help ensure the user calls \c korl_gui_frameBegin/End the correct # of 
     * times */
    u8 frameSequenceCounter;
    /** Windows are stored from back=>front.  In other words, the window at 
     * index \c 0 will be drawn behind all other windows. */
    KORL_MEMORY_POOL_DECLARE(_Korl_Gui_Window, windows, 64);
    KORL_MEMORY_POOL_DECLARE(_Korl_Gui_Widget, widgets, 64);
    /** We don't need this to be a member of \c _Korl_Gui_Window because we 
     * already know:  
     * - there will only ever be ONE active window
     * - the active window will ALWAYS be the top level window */
    bool isTopLevelWindowActive;
    bool isMouseDown;// This flag is raised when we mouse down inside a window
    bool isWindowDragged;// only raised when we click a window outside of widgets
    bool isWindowResizing;// only raised when we click a window's resize edge
    Korl_Math_V2f32 mouseDownWindowOffset;
    const void* identifierMouseDownWidget;
    bool isMouseHovering;
    enum
    {
        KORL_GUI_MOUSE_TITLEBAR_BUTTON_FLAGS_NONE = 0,
        KORL_GUI_MOUSE_TITLEBAR_BUTTON_FLAG_CLOSE = 1 << 0,
    } titlebarButtonFlagsMouseDown;
    const void* identifierMouseHoveredWidget;
    const void* identifierMouseHoveredWindow;
    enum
    {
        KORL_GUI_MOUSE_HOVER_FLAGS_NONE = 0,
        KORL_GUI_MOUSE_HOVER_FLAG_LEFT  = 1<<0,
        KORL_GUI_MOUSE_HOVER_FLAG_RIGHT = 1<<1,
        KORL_GUI_MOUSE_HOVER_FLAG_UP    = 1<<2,
        KORL_GUI_MOUSE_HOVER_FLAG_DOWN  = 1<<3,
    } mouseHoverWindowEdgeFlags;
    Korl_Math_V2f32 mouseHoverPosition;
} _Korl_Gui_Context;
korl_global_variable _Korl_Gui_Context _korl_gui_context;
