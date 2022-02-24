#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
#include "korl-memory.h"
typedef struct
{
    const void* identifier;
    bool usedThisFrame;
    /** relative to the upper-left corner of the window */
    Korl_Math_V2f32 position;
    Korl_Math_V2f32 size;
} _Korl_Gui_Window;
typedef struct
{
    /** Helps ensure that the user calls \c korl_gui_begin/end the correct # of 
     * times.  When this value == korl_arraySize(windows), a new window must be 
     * started before calling any widget API. */
    u8 currentWindowIndex;
    /** help ensure the user calls \c korl_gui_frameBegin/End the correct # of 
     * times */
    u8 frameSequenceCounter;
    /** Windows are stored from back=>front.  In other words, the window at 
     * index \c 0 will be drawn behind all other windows. */
    KORL_MEMORY_POOL_DECLARE(_Korl_Gui_Window, windows, 64);
    /** We don't need this to be a member of \c _Korl_Gui_Window because we 
     * already know:  
     * - there will only ever be ONE active window
     * - the active window will ALWAYS be the top level window */
    bool isTopLevelWindowActive;
} _Korl_Gui_Context;
korl_global_variable _Korl_Gui_Context _korl_gui_context;
