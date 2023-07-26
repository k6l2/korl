#include "korl-camera-freeFly.h"
#include "utility/korl-utility-gfx.h"
#include "korl-interface-platform.h"
korl_global_const Korl_Math_V3f32 _KORL_CAMERA_FREEFLY_DEFAULT_FORWARD = { 0, -1, 0};// blender model space
korl_global_const Korl_Math_V3f32 _KORL_CAMERA_FREEFLY_DEFAULT_RIGHT   = {-1,  0, 0};// blender model space
korl_global_const Korl_Math_V3f32 _KORL_CAMERA_FREEFLY_DEFAULT_UP      = { 0,  0, 1};// blender model space
korl_internal Korl_Camera_FreeFly korl_camera_freeFly_create(f32 acceleration, f32 speedMax)
{
    KORL_ZERO_STACK(Korl_Camera_FreeFly, result);
    result.acceleration = acceleration;
    result.speedMax     = speedMax;
    return result;
}
korl_internal void korl_camera_freeFly_setInput(Korl_Camera_FreeFly*const context, Korl_Camera_FreeFly_InputFlags inputFlag, bool isActive)
{
    if(isActive)
        context->inputFlags |=  (1 << inputFlag);
    else
        context->inputFlags &= ~(1 << inputFlag);
}
korl_internal void korl_camera_freeFly_step(Korl_Camera_FreeFly*const context, f32 deltaSeconds)
{
    Korl_Math_V2f32 cameraLook = KORL_MATH_V2F32_ZERO;
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_UP))    cameraLook.y += 1;
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_DOWN))  cameraLook.y -= 1;
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_LEFT))  cameraLook.x += 1;
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_RIGHT)) cameraLook.x -= 1;
    const f32 cameraLookMagnitude = korl_math_v2f32_magnitude(&cameraLook);
    if(!korl_math_isNearlyZero(cameraLookMagnitude))
    {
        korl_shared_const f32 CAMERA_LOOK_ACCELERATION = 10;
        cameraLook = korl_math_v2f32_normalKnownMagnitude(cameraLook, cameraLookMagnitude);
        if(korl_math_v2f32_dot(context->yawPitchVelocity.v2f32, cameraLook) < 0)
            context->yawPitchVelocity.v2f32 = KORL_MATH_V2F32_ZERO;
        korl_math_v2f32_assignAdd(&context->yawPitchVelocity.v2f32, korl_math_v2f32_multiplyScalar(cameraLook, deltaSeconds * CAMERA_LOOK_ACCELERATION));
    }
    else
        context->yawPitchVelocity.v2f32 = KORL_MATH_V2F32_ZERO;
    {/* cap look speed */
        korl_shared_const f32 CAMERA_LOOK_SPEED_MAX = 10;
        f32 lookSpeed = korl_math_v2f32_magnitude(&context->yawPitchVelocity.v2f32);
        if(lookSpeed > CAMERA_LOOK_SPEED_MAX)
        {
            context->yawPitchVelocity.v2f32 = korl_math_v2f32_normalKnownMagnitude(context->yawPitchVelocity.v2f32, lookSpeed);
            korl_math_v2f32_assignMultiplyScalar(&context->yawPitchVelocity.v2f32, CAMERA_LOOK_SPEED_MAX);
        }
    }
    Korl_Math_V3f32 cameraMove = KORL_MATH_V3F32_ZERO;
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_FORWARD))  korl_math_v3f32_assignAdd     (&cameraMove, _KORL_CAMERA_FREEFLY_DEFAULT_FORWARD);
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_BACKWARD)) korl_math_v3f32_assignSubtract(&cameraMove, _KORL_CAMERA_FREEFLY_DEFAULT_FORWARD);
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_RIGHT))    korl_math_v3f32_assignAdd     (&cameraMove, _KORL_CAMERA_FREEFLY_DEFAULT_RIGHT);
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_LEFT))     korl_math_v3f32_assignSubtract(&cameraMove, _KORL_CAMERA_FREEFLY_DEFAULT_RIGHT);
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_UP))       korl_math_v3f32_assignAdd     (&cameraMove, _KORL_CAMERA_FREEFLY_DEFAULT_UP);
    if(context->inputFlags & (1 << KORL_CAMERA_FREEFLY_INPUT_FLAG_DOWN))     korl_math_v3f32_assignSubtract(&cameraMove, _KORL_CAMERA_FREEFLY_DEFAULT_UP);
    const f32 cameraMoveMagnitude = korl_math_v3f32_magnitude(&cameraMove);
    if(!korl_math_isNearlyZero(cameraMoveMagnitude))
    {
        cameraMove = korl_math_v3f32_normalKnownMagnitude(cameraMove, cameraMoveMagnitude);
        const Korl_Math_Quaternion quaternionCameraPitch = korl_math_quaternion_fromAxisRadians(_KORL_CAMERA_FREEFLY_DEFAULT_RIGHT, context->yawPitch.pitch, true);
        const Korl_Math_Quaternion quaternionCameraYaw   = korl_math_quaternion_fromAxisRadians(_KORL_CAMERA_FREEFLY_DEFAULT_UP   , context->yawPitch.yaw  , true);
        const Korl_Math_V3f32      accelDirection        = korl_math_quaternion_transformV3f32(korl_math_quaternion_multiply(quaternionCameraYaw, quaternionCameraPitch), cameraMove, false);
        if(korl_math_v3f32_dot(context->velocity, accelDirection) < 0)
            context->velocity = KORL_MATH_V3F32_ZERO;
        korl_math_v3f32_assignAdd(&context->velocity, korl_math_v3f32_multiplyScalar(accelDirection, deltaSeconds * context->acceleration));
    }
    else
        /* come to a full-stop if the user is not trying to move at all */
        context->velocity = KORL_MATH_V3F32_ZERO;
    {/* cap speed */
        f32 speed = korl_math_v3f32_magnitude(&context->velocity);
        if(speed > context->speedMax)
        {
            context->velocity = korl_math_v3f32_normalKnownMagnitude(context->velocity, speed);
            korl_math_v3f32_assignMultiplyScalar(&context->velocity, context->speedMax);
        }
    }
    {/* movement */
        korl_math_v2f32_assignAdd(&context->yawPitch.v2f32, korl_math_v2f32_multiplyScalar(context->yawPitchVelocity.v2f32, deltaSeconds));
        KORL_MATH_ASSIGN_CLAMP(context->yawPitch.pitch, -KORL_PI32/2, KORL_PI32/2);
        korl_math_v3f32_assignAdd(&context->position, korl_math_v3f32_multiplyScalar(context->velocity, deltaSeconds));
    }
}
korl_internal Korl_Math_V3f32 korl_camera_freeFly_forward(const Korl_Camera_FreeFly*const context)
{
    const Korl_Math_Quaternion quaternionCameraPitch = korl_math_quaternion_fromAxisRadians(_KORL_CAMERA_FREEFLY_DEFAULT_RIGHT, context->yawPitch.pitch, true);
    const Korl_Math_Quaternion quaternionCameraYaw   = korl_math_quaternion_fromAxisRadians(_KORL_CAMERA_FREEFLY_DEFAULT_UP   , context->yawPitch.yaw  , true);
    const Korl_Math_V3f32      cameraForward         = korl_math_quaternion_transformV3f32(korl_math_quaternion_multiply(quaternionCameraYaw, quaternionCameraPitch), _KORL_CAMERA_FREEFLY_DEFAULT_FORWARD, false);
    return cameraForward;
}
korl_internal Korl_Gfx_Camera korl_camera_freeFly_createGfxCamera(const Korl_Camera_FreeFly*const context, f32 fovVerticalDegrees, f32 clipNear, f32 clipFar)
{
    const Korl_Math_Quaternion quaternionCameraPitch = korl_math_quaternion_fromAxisRadians(_KORL_CAMERA_FREEFLY_DEFAULT_RIGHT, context->yawPitch.pitch, true);
    const Korl_Math_Quaternion quaternionCameraYaw   = korl_math_quaternion_fromAxisRadians(_KORL_CAMERA_FREEFLY_DEFAULT_UP   , context->yawPitch.yaw  , true);
    const Korl_Math_V3f32      cameraForward         = korl_math_quaternion_transformV3f32(korl_math_quaternion_multiply(quaternionCameraYaw, quaternionCameraPitch), _KORL_CAMERA_FREEFLY_DEFAULT_FORWARD, false);
    const Korl_Math_V3f32      cameraUp              = korl_math_quaternion_transformV3f32(korl_math_quaternion_multiply(quaternionCameraYaw, quaternionCameraPitch), _KORL_CAMERA_FREEFLY_DEFAULT_UP, false);
    return korl_gfx_camera_createFov(fovVerticalDegrees, clipNear, clipFar, context->position, cameraForward, cameraUp);
}
