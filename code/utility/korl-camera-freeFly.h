#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-utility-math.h"
typedef enum Korl_Camera_FreeFly_InputFlags
    {KORL_CAMERA_FREEFLY_INPUT_FLAG_FORWARD
    ,KORL_CAMERA_FREEFLY_INPUT_FLAG_BACKWARD
    ,KORL_CAMERA_FREEFLY_INPUT_FLAG_LEFT
    ,KORL_CAMERA_FREEFLY_INPUT_FLAG_RIGHT
    ,KORL_CAMERA_FREEFLY_INPUT_FLAG_UP
    ,KORL_CAMERA_FREEFLY_INPUT_FLAG_DOWN
    ,KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_UP
    ,KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_DOWN
    ,KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_LEFT
    ,KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_RIGHT
} Korl_Camera_FreeFly_InputFlags;
typedef union Korl_Camera_FreeFly_YawPitch
{
    Korl_Math_V2f32 v2f32;
    struct { f32 yaw; f32 pitch; };
} Korl_Camera_FreeFly_YawPitch;
typedef struct Korl_Camera_FreeFly
{
    u32                          inputFlags;
    Korl_Math_V3f32              position;
    Korl_Math_V3f32              velocity;
    Korl_Camera_FreeFly_YawPitch yawPitch;
    Korl_Camera_FreeFly_YawPitch yawPitchVelocity;
} Korl_Camera_FreeFly;
korl_internal void            korl_camera_freeFly_setInput(Korl_Camera_FreeFly*const context, Korl_Camera_FreeFly_InputFlags inputFlag, bool isActive);
korl_internal void            korl_camera_freeFly_step(Korl_Camera_FreeFly*const context, f32 deltaSeconds);
korl_internal Korl_Math_V3f32 korl_camera_freeFly_forward(const Korl_Camera_FreeFly*const context);
