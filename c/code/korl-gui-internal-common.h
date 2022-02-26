#pragma once
#include "korl-globalDefines.h"
#include "korl-gui-common.h"
#include "korl-math.h"
#include "korl-memory.h"
#include "korl-vulkan.h"
typedef struct
{
    const void* identifier;
    bool usedThisFrame;
    Korl_Math_V2f32 position;// relative to the upper-left corner of the window
    Korl_Math_V2f32 size;
    u32 styleFlags;// uses the Korl_Gui_Window_Style_Flags enum
} _Korl_Gui_Window;
typedef struct
{
    const void* parentWindowIdentifier;
    const void* identifier;
    bool usedThisFrame;
    enum
    {
        KORL_GUI_WIDGET_TYPE_TEXT,
    } type;
    union
    {
        struct
        {
            const wchar_t* displayText;
        } text;
    } subType;
} _Korl_Gui_Widget;
typedef struct
{
    struct
    {
        Korl_Vulkan_Color colorWindow;
        Korl_Vulkan_Color colorWindowActive;
        Korl_Vulkan_Color colorTitleBar;
        Korl_Vulkan_Color colorTitleBarActive;
        const wchar_t* fontWindowText;
        f32 windowTextPixelSizeY;
        f32 windowTitleBarPixelSizeY;
        f32 widgetSpacingY;
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
    bool isMouseDown;// This flag is raised when we're in a "drag state".
    Korl_Math_V2f32 mouseDownWindowOffset;
    Korl_Memory_Allocator allocatorStack;
} _Korl_Gui_Context;
korl_global_variable _Korl_Gui_Context _korl_gui_context;
