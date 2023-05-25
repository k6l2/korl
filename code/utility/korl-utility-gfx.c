#include "utility/korl-utility-gfx.h"
#include "utility/korl-checkCast.h"
#include "korl-interface-platform.h"
korl_internal Korl_Gfx_Camera korl_gfx_camera_createFov(f32 fovVerticalDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 normalForward, Korl_Math_V3f32 normalUp)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.position                                 = position;
    result.normalForward                            = normalForward;
    result.normalUp                                 = normalUp;
    result._viewportScissorPosition                 = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){0, 0};
    result._viewportScissorSize                     = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){1, 1};
    result.subCamera.perspective.clipNear           = clipNear;
    result.subCamera.perspective.clipFar            = clipFar;
    result.subCamera.perspective.fovVerticalDegrees = fovVerticalDegrees;
    return result;
}
korl_internal Korl_Gfx_Camera korl_gfx_camera_createOrtho(f32 clipDepth)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC;
    result.position                            = KORL_MATH_V3F32_ZERO;
    result.normalForward                       = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result.normalUp                            = KORL_MATH_V3F32_Y;
    result._viewportScissorPosition            = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){0, 0};
    result._viewportScissorSize                = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){1, 1};
    result.subCamera.orthographic.clipDepth    = clipDepth;
    result.subCamera.orthographic.originAnchor = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){0.5f, 0.5f};
    return result;
}
korl_internal Korl_Gfx_Camera korl_gfx_camera_createOrthoFixedHeight(f32 fixedHeight, f32 clipDepth)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT;
    result.position                            = KORL_MATH_V3F32_ZERO;
    result.normalForward                       = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result.normalUp                            = KORL_MATH_V3F32_Y;
    result._viewportScissorPosition            = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){0, 0};
    result._viewportScissorSize                = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){1, 1};
    result.subCamera.orthographic.fixedHeight  = fixedHeight;
    result.subCamera.orthographic.clipDepth    = clipDepth;
    result.subCamera.orthographic.originAnchor = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){0.5f, 0.5f};
    return result;
}
korl_internal void korl_gfx_camera_fovRotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians, Korl_Math_V3f32 target)
{
    korl_assert(context->type == KORL_GFX_CAMERA_TYPE_PERSPECTIVE);
    const Korl_Math_Quaternion q32Rotation = korl_math_quaternion_fromAxisRadians(axisOfRotation, radians, false);
    const Korl_Math_V3f32 targetToNewPosition = korl_math_quaternion_transformV3f32(q32Rotation
                                                                                   ,korl_math_v3f32_subtract(context->position, target)
                                                                                   ,true);
    context->position      = korl_math_v3f32_add(target, targetToNewPosition);
    context->normalForward = korl_math_quaternion_transformV3f32(q32Rotation, context->normalForward, true);
    context->normalUp      = korl_math_quaternion_transformV3f32(q32Rotation, context->normalUp     , true);
}
korl_internal void korl_gfx_camera_setScissor(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY)
{
    f32 x2 = x + sizeX;
    f32 y2 = y + sizeY;
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x2 < 0) x2 = 0;
    if(y2 < 0) y2 = 0;
    context->_viewportScissorPosition.x = x;
    context->_viewportScissorPosition.y = y;
    context->_viewportScissorSize.x     = x2 - x;
    context->_viewportScissorSize.y     = y2 - y;
    context->_scissorType               = KORL_GFX_CAMERA_SCISSOR_TYPE_ABSOLUTE;
}
korl_internal void korl_gfx_camera_setScissorPercent(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight)
{
    f32 x2 = viewportRatioX + viewportRatioWidth;
    f32 y2 = viewportRatioY + viewportRatioHeight;
    if(viewportRatioX < 0) viewportRatioX = 0;
    if(viewportRatioY < 0) viewportRatioY = 0;
    if(x2 < 0) x2 = 0;
    if(y2 < 0) y2 = 0;
    context->_viewportScissorPosition.x = viewportRatioX;
    context->_viewportScissorPosition.y = viewportRatioY;
    context->_viewportScissorSize.x     = x2 - viewportRatioX;
    context->_viewportScissorSize.y     = y2 - viewportRatioY;
    context->_scissorType               = KORL_GFX_CAMERA_SCISSOR_TYPE_RATIO;
}
korl_internal void korl_gfx_camera_orthoSetOriginAnchor(Korl_Gfx_Camera*const context, f32 swapchainSizeRatioOriginX, f32 swapchainSizeRatioOriginY)
{
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        korl_assert(!"origin anchor not supported for perspective camera");
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        context->subCamera.orthographic.originAnchor.x = swapchainSizeRatioOriginX;
        context->subCamera.orthographic.originAnchor.y = swapchainSizeRatioOriginY;
        break;}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        context->subCamera.orthographic.originAnchor.x = swapchainSizeRatioOriginX;
        context->subCamera.orthographic.originAnchor.y = swapchainSizeRatioOriginY;
        break;}
    }
}
korl_internal void korl_gfx_drawable_mesh_initialize(Korl_Gfx_Drawable*const context, Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName)
{
    korl_memory_zero(context, sizeof(*context));
    context->type            = KORL_GFX_DRAWABLE_TYPE_MESH;
    context->_model.rotation = KORL_MATH_QUATERNION_IDENTITY;
    context->_model.scale    = KORL_MATH_V3F32_ONE;
    context->subType.mesh.resourceHandleScene3d = resourceHandleScene3d;
    korl_assert(utf8MeshName.size < korl_arraySize(context->subType.mesh.rawUtf8Scene3dMeshName));
    korl_memory_copy(context->subType.mesh.rawUtf8Scene3dMeshName, utf8MeshName.data, utf8MeshName.size);
    context->subType.mesh.rawUtf8Scene3dMeshName[utf8MeshName.size] = '\0';
    context->subType.mesh.rawUtf8Scene3dMeshNameSize = korl_checkCast_u$_to_u8(utf8MeshName.size);
}
