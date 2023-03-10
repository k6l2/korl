#pragma once
#include "korl-globalDefines.h"
typedef enum Korl_KeyboardCode
    { KORL_KEY_UNKNOWN
    , KORL_KEY_A
    , KORL_KEY_B
    , KORL_KEY_C
    , KORL_KEY_D
    , KORL_KEY_E
    , KORL_KEY_F
    , KORL_KEY_G
    , KORL_KEY_H
    , KORL_KEY_I
    , KORL_KEY_J
    , KORL_KEY_K
    , KORL_KEY_L
    , KORL_KEY_M
    , KORL_KEY_N
    , KORL_KEY_O
    , KORL_KEY_P
    , KORL_KEY_Q
    , KORL_KEY_R
    , KORL_KEY_S
    , KORL_KEY_T
    , KORL_KEY_U
    , KORL_KEY_V
    , KORL_KEY_W
    , KORL_KEY_X
    , KORL_KEY_Y
    , KORL_KEY_Z
    , KORL_KEY_COMMA
    , KORL_KEY_PERIOD
    , KORL_KEY_SLASH_FORWARD
    , KORL_KEY_SLASH_BACK
    , KORL_KEY_CURLYBRACE_LEFT
    , KORL_KEY_CURLYBRACE_RIGHT
    , KORL_KEY_SEMICOLON
    , KORL_KEY_QUOTE
    , KORL_KEY_GRAVE
    , KORL_KEY_TENKEYLESS_0
    , KORL_KEY_TENKEYLESS_1
    , KORL_KEY_TENKEYLESS_2
    , KORL_KEY_TENKEYLESS_3
    , KORL_KEY_TENKEYLESS_4
    , KORL_KEY_TENKEYLESS_5
    , KORL_KEY_TENKEYLESS_6
    , KORL_KEY_TENKEYLESS_7
    , KORL_KEY_TENKEYLESS_8
    , KORL_KEY_TENKEYLESS_9
    , KORL_KEY_TENKEYLESS_MINUS
    , KORL_KEY_EQUALS
    , KORL_KEY_BACKSPACE
    , KORL_KEY_ESCAPE
    , KORL_KEY_ENTER
    , KORL_KEY_SPACE
    , KORL_KEY_TAB
    , KORL_KEY_SHIFT_LEFT
    , KORL_KEY_SHIFT_RIGHT
    , KORL_KEY_CONTROL_LEFT
    , KORL_KEY_CONTROL_RIGHT
    , KORL_KEY_ALT_LEFT
    , KORL_KEY_ALT_RIGHT
    , KORL_KEY_F1
    , KORL_KEY_F2
    , KORL_KEY_F3
    , KORL_KEY_F4
    , KORL_KEY_F5
    , KORL_KEY_F6
    , KORL_KEY_F7
    , KORL_KEY_F8
    , KORL_KEY_F9
    , KORL_KEY_F10
    , KORL_KEY_F11
    , KORL_KEY_F12
    , KORL_KEY_ARROW_UP
    , KORL_KEY_ARROW_DOWN
    , KORL_KEY_ARROW_LEFT
    , KORL_KEY_ARROW_RIGHT
    , KORL_KEY_INSERT
    , KORL_KEY_DELETE
    , KORL_KEY_HOME
    , KORL_KEY_END
    , KORL_KEY_PAGE_UP
    , KORL_KEY_PAGE_DOWN
    , KORL_KEY_NUMPAD_0
    , KORL_KEY_NUMPAD_1
    , KORL_KEY_NUMPAD_2
    , KORL_KEY_NUMPAD_3
    , KORL_KEY_NUMPAD_4
    , KORL_KEY_NUMPAD_5
    , KORL_KEY_NUMPAD_6
    , KORL_KEY_NUMPAD_7
    , KORL_KEY_NUMPAD_8
    , KORL_KEY_NUMPAD_9
    , KORL_KEY_NUMPAD_PERIOD
    , KORL_KEY_NUMPAD_DIVIDE
    , KORL_KEY_NUMPAD_MULTIPLY
    , KORL_KEY_NUMPAD_MINUS
    , KORL_KEY_NUMPAD_ADD
    , KORL_KEY_COUNT// keep last!
} Korl_KeyboardCode;
typedef enum Korl_MouseButton
    { KORL_MOUSE_BUTTON_LEFT
    , KORL_MOUSE_BUTTON_MIDDLE
    , KORL_MOUSE_BUTTON_RIGHT
    , KORL_MOUSE_BUTTON_X1
    , KORL_MOUSE_BUTTON_X2
    , KORL_MOUSE_BUTTON_COUNT
} Korl_MouseButton;
typedef enum Korl_MouseEventType
    { KORL_MOUSE_EVENT_BUTTON_DOWN
    , KORL_MOUSE_EVENT_BUTTON_UP
    , KORL_MOUSE_EVENT_WHEEL
    , KORL_MOUSE_EVENT_HWHEEL
    , KORL_MOUSE_EVENT_MOVE
    , KORL_MOUSE_EVENT_MOVE_RAW // raw mouse mickeys; use this data for doing things like first-person camera control
} Korl_MouseEventType;
typedef struct Korl_MouseEvent
{
    Korl_MouseEventType type;
    i32 x; // Mouse x position, measured from bottom-left. Positive is right.
    i32 y; // Mouse y position, measured from bottom-left. Positive is up.
    union
    {
        Korl_MouseButton button;
        i32 wheel;
    } which;
} Korl_MouseEvent;
typedef enum Korl_GamepadEventType
    { KORL_GAMEPAD_EVENT_TYPE_BUTTON
    , KORL_GAMEPAD_EVENT_TYPE_AXIS
} Korl_GamepadEventType;
typedef enum Korl_GamepadButton
    { KORL_GAMEPAD_BUTTON_CLUSTER_LEFT_UP
    , KORL_GAMEPAD_BUTTON_CLUSTER_LEFT_DOWN
    , KORL_GAMEPAD_BUTTON_CLUSTER_LEFT_LEFT
    , KORL_GAMEPAD_BUTTON_CLUSTER_LEFT_RIGHT
    , KORL_GAMEPAD_BUTTON_CLUSTER_CENTER_RIGHT
    , KORL_GAMEPAD_BUTTON_CLUSTER_CENTER_LEFT
    , KORL_GAMEPAD_BUTTON_STICK_LEFT
    , KORL_GAMEPAD_BUTTON_STICK_RIGHT
    , KORL_GAMEPAD_BUTTON_CLUSTER_TOP_LEFT
    , KORL_GAMEPAD_BUTTON_CLUSTER_TOP_RIGHT
    , KORL_GAMEPAD_BUTTON_CLUSTER_CENTER_MIDDLE
    , KORL_GAMEPAD_BUTTON_CLUSTER_RIGHT_DOWN
    , KORL_GAMEPAD_BUTTON_CLUSTER_RIGHT_RIGHT
    , KORL_GAMEPAD_BUTTON_CLUSTER_RIGHT_LEFT
    , KORL_GAMEPAD_BUTTON_CLUSTER_RIGHT_UP
    /// NOTE: the order of this enumeration _does_ matter for some logic
} Korl_GamepadButton;
typedef enum Korl_GamepadAxis
    { KORL_GAMEPAD_AXIS_STICK_LEFT_X
    , KORL_GAMEPAD_AXIS_STICK_LEFT_Y
    , KORL_GAMEPAD_AXIS_STICK_RIGHT_X
    , KORL_GAMEPAD_AXIS_STICK_RIGHT_Y
    , KORL_GAMEPAD_AXIS_TRIGGER_LEFT
    , KORL_GAMEPAD_AXIS_TRIGGER_RIGHT
} Korl_GamepadAxis;
typedef struct Korl_GamepadEvent
{
    Korl_GamepadEventType type;
    union
    {
        struct
        {
            Korl_GamepadButton button;
            bool pressed;
        } button;
        struct
        {
            Korl_GamepadAxis axis;
            f32 value;
        } axis;
    } subType;
} Korl_GamepadEvent;
