#include "utility/korl-utility-gfx.h"
#include "utility/korl-checkCast.h"
#include "korl-interface-platform.h"
korl_internal Korl_Math_V4f32 korl_gfx_color_toLinear(Korl_Vulkan_Color4u8 color)
{
    return KORL_STRUCT_INITIALIZE(Korl_Math_V4f32){KORL_C_CAST(f32, color.r) / KORL_C_CAST(f32, KORL_U8_MAX)
                                                  ,KORL_C_CAST(f32, color.g) / KORL_C_CAST(f32, KORL_U8_MAX)
                                                  ,KORL_C_CAST(f32, color.b) / KORL_C_CAST(f32, KORL_U8_MAX)
                                                  ,KORL_C_CAST(f32, color.a) / KORL_C_CAST(f32, KORL_U8_MAX)};
}
korl_internal Korl_Gfx_Material korl_gfx_material_defaultUnlit(void)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.drawState = {.polygonMode = KORL_GFX_POLYGON_MODE_FILL
                                                                  ,.cullMode    = KORL_GFX_CULL_MODE_BACK}
                                                    ,.properties = {.factorColorBase     = KORL_MATH_V4F32_ONE
                                                                   ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                   ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                   ,.shininess           = 0}
                                                    ,.maps = {.resourceHandleTextureBase     = 0
                                                             ,.resourceHandleTextureSpecular = 0
                                                             ,.resourceHandleTextureEmissive = 0}
                                                    ,.shaders = {.resourceHandleShaderVertex   = 0
                                                                ,.resourceHandleShaderFragment = 0}};
}
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
korl_internal Korl_Math_M4f32 korl_gfx_camera_projection(const Korl_Gfx_Camera*const context, Korl_Math_V2u32 surfaceSize)
{
    switch(context->type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        const f32 viewportWidthOverHeight = surfaceSize.y == 0 
            ? 1.f 
            :  KORL_C_CAST(f32, surfaceSize.x)
             / KORL_C_CAST(f32, surfaceSize.y);
        return korl_math_m4f32_projectionFov(context->subCamera.perspective.fovVerticalDegrees
                                            ,viewportWidthOverHeight
                                            ,context->subCamera.perspective.clipNear
                                            ,context->subCamera.perspective.clipFar);}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        const f32 left   = 0.f - context->subCamera.orthographic.originAnchor.x*KORL_C_CAST(f32, surfaceSize.x);
        const f32 bottom = 0.f - context->subCamera.orthographic.originAnchor.y*KORL_C_CAST(f32, surfaceSize.y);
        const f32 right  = KORL_C_CAST(f32, surfaceSize.x) - context->subCamera.orthographic.originAnchor.x*KORL_C_CAST(f32, surfaceSize.x);
        const f32 top    = KORL_C_CAST(f32, surfaceSize.y) - context->subCamera.orthographic.originAnchor.y*KORL_C_CAST(f32, surfaceSize.y);
        const f32 far    = -context->subCamera.orthographic.clipDepth;
        const f32 near   = 0.0000001f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        return korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);}
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT:{
        const f32 viewportWidthOverHeight = surfaceSize.y == 0 
            ? 1.f 
            :  KORL_C_CAST(f32, surfaceSize.x) 
             / KORL_C_CAST(f32, surfaceSize.y);
        /* w / fixedHeight == windowAspectRatio */
        const f32 width  = context->subCamera.orthographic.fixedHeight * viewportWidthOverHeight;
        const f32 left   = 0.f - context->subCamera.orthographic.originAnchor.x*width;
        const f32 bottom = 0.f - context->subCamera.orthographic.originAnchor.y*context->subCamera.orthographic.fixedHeight;
        const f32 right  = width       - context->subCamera.orthographic.originAnchor.x*width;
        const f32 top    = context->subCamera.orthographic.fixedHeight - context->subCamera.orthographic.originAnchor.y*context->subCamera.orthographic.fixedHeight;
        const f32 far    = -context->subCamera.orthographic.clipDepth;
        const f32 near   = 1e-7f;//a non-zero value here allows us to render objects with a Z coordinate of 0.f
        return korl_math_m4f32_projectionOrthographic(left, right, bottom, top, far, near);}
    default:{
        korl_log(ERROR, "invalid camera type: %i", context->type);
        return KORL_STRUCT_INITIALIZE_ZERO(Korl_Math_M4f32);}
    }
}
korl_internal Korl_Math_M4f32 korl_gfx_camera_view(const Korl_Gfx_Camera*const context)
{
    const Korl_Math_V3f32 cameraTarget = korl_math_v3f32_add(context->position, context->normalForward);
    return korl_math_m4f32_lookAt(&context->position, &cameraTarget, &context->normalUp);
}
korl_internal void korl_gfx_camera_drawFrustum(const Korl_Gfx_Camera*const context, Korl_Math_V2u32 surfaceSize, Korl_Memory_AllocatorHandle allocator)
{
    Korl_Gfx_ResultRay3d cameraWorldCorners[4];
    /* camera coordinates in window-space are CCW, starting from the lower-left:
        {lower-left, lower-right, upper-right, upper-left} */
    cameraWorldCorners[0] = korl_gfx_camera_windowToWorld(context, KORL_STRUCT_INITIALIZE(Korl_Math_V2i32 ){0, 0});
    cameraWorldCorners[1] = korl_gfx_camera_windowToWorld(context, KORL_STRUCT_INITIALIZE(Korl_Math_V2i32 ){korl_checkCast_u$_to_i32(surfaceSize.x), 0});
    cameraWorldCorners[2] = korl_gfx_camera_windowToWorld(context, KORL_STRUCT_INITIALIZE(Korl_Math_V2i32 ){korl_checkCast_u$_to_i32(surfaceSize.x)
                                                                                                           ,korl_checkCast_u$_to_i32(surfaceSize.y)});
    cameraWorldCorners[3] = korl_gfx_camera_windowToWorld(context, KORL_STRUCT_INITIALIZE(Korl_Math_V2i32 ){0, korl_checkCast_u$_to_i32(surfaceSize.y)});
    Korl_Math_V3f32 cameraWorldCornersFar[4];
    for(u8 i = 0; i < 4; i++)
    {
        /* sanity check the results */
        if(korl_math_f32_isNan(cameraWorldCorners[i].position.x))
            korl_log(ERROR, "window=>world translation failed for cameraAabbEyeRays[%hhu]", i);
        /* compute the world-space positions of the far-plane eye ray intersections */
        cameraWorldCornersFar[i] = korl_math_v3f32_add(cameraWorldCorners[i].position, korl_math_v3f32_multiplyScalar(cameraWorldCorners[i].direction, cameraWorldCorners[i].segmentDistance));
    }
    korl_shared_const u32 LINE_COUNT = 12/*box wireframe edge count*/;
    Korl_Gfx_Batch*const batch = korl_gfx_createBatchLines(allocator, LINE_COUNT);
    u8 lineIndex = 0;
    for(u8 i = 0; i < korl_arraySize(cameraWorldCorners); i++)
    {
        const u$ iNext = (i + 1) % korl_arraySize(cameraWorldCorners);
        /*line for the near-plane quad face*/
        korl_gfx_batchSetLine(batch, lineIndex++, cameraWorldCorners[i].position.elements, cameraWorldCorners[iNext].position.elements, korl_arraySize(cameraWorldCorners[i].position.elements), KORL_COLOR4U8_WHITE);
        /*line for the far-plane quad face*/
        korl_gfx_batchSetLine(batch, lineIndex++, cameraWorldCornersFar[i].elements, cameraWorldCornersFar[iNext].elements, korl_arraySize(cameraWorldCornersFar[iNext].elements), KORL_COLOR4U8_WHITE);
        /*line connecting the near-plane to the far-plane on the current corner*/
        korl_gfx_batchSetLine(batch, lineIndex++, cameraWorldCorners[i].position.elements, cameraWorldCornersFar[i].elements, korl_arraySize(cameraWorldCorners[i].position.elements), KORL_COLOR4U8_WHITE);
    }
    korl_assert(lineIndex == LINE_COUNT);
    korl_gfx_batch(batch, KORL_GFX_BATCH_FLAGS_NONE);
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
korl_internal void korl_gfx_draw3dArrow(Korl_Gfx_Drawable meshCone, Korl_Gfx_Drawable meshCylinder, Korl_Math_V3f32 localStart, Korl_Math_V3f32 localEnd, f32 localRadius, const Korl_Math_M4f32*const baseTransform)
{
    /* transform start & end points from local=>base */
    const Korl_Math_V3f32 baseStart = korl_math_m4f32_multiplyV3f32(baseTransform, localStart);
    const Korl_Math_V3f32 baseEnd   = korl_math_m4f32_multiplyV3f32(baseTransform, localEnd);
    /* compute the scaled base-space geometry of the arrow itself */
    Korl_Math_V3f32 startToEnd = korl_math_v3f32_subtract(baseEnd, baseStart);
    const f32 length = korl_math_v3f32_magnitude(&startToEnd);
    startToEnd = korl_math_v3f32_normalKnownMagnitude(startToEnd, length);
    korl_shared_const f32 PROPORTION_TIP_LENGTH = 0.2f;
    const f32   lengthCylinder = length * (1.f - PROPORTION_TIP_LENGTH);
    const f32   lengthCone     = length * PROPORTION_TIP_LENGTH;
    korl_shared_const f32 PROPORTION_CYLINDER_RADIUS = 0.3f;
    meshCylinder._model.position = korl_math_v3f32_add(baseStart, korl_math_v3f32_multiplyScalar(startToEnd, lengthCylinder / 2.f));
    meshCylinder._model.scale.xy = korl_math_v2f32_multiplyScalar(KORL_MATH_V2F32_ONE, localRadius * PROPORTION_CYLINDER_RADIUS);
    meshCylinder._model.scale.z  = lengthCylinder;
    meshCone._model.position = korl_math_v3f32_add(baseStart, korl_math_v3f32_multiplyScalar(startToEnd, lengthCylinder + lengthCone / 2));
    meshCone._model.scale.xy = korl_math_v2f32_multiplyScalar(KORL_MATH_V2F32_ONE, localRadius);
    meshCone._model.scale.z  = lengthCone;
    /* find axis & angle of rotation (quaternion) to rotate the arrow in the 
        direction of `startToEnd` */
    Korl_Math_Quaternion quatDesired = korl_math_quaternion_fromAxisRadians(korl_math_v3f32_cross(KORL_MATH_V3F32_Z, startToEnd)
                                                                           ,korl_math_v3f32_radiansBetween(KORL_MATH_V3F32_Z, startToEnd)
                                                                           ,false);
    meshCylinder._model.rotation = quatDesired;
    meshCone._model.rotation     = quatDesired;
    korl_gfx_draw(&meshCylinder);
    korl_gfx_draw(&meshCone);
}
korl_internal void korl_gfx_draw3dLine(const Korl_Math_V3f32 points[2], const Korl_Vulkan_Color4u8 colors[2], Korl_Memory_AllocatorHandle allocator)
{
    Korl_Gfx_Batch*const batch = korl_gfx_createBatchLines(allocator, 1);
    Korl_Math_V3f32*const linePositions = KORL_C_CAST(Korl_Math_V3f32*, batch->_vertexPositions);
    linePositions[0] = points[0];
    linePositions[1] = points[1];
    batch->_vertexColors[0] = colors[0];
    batch->_vertexColors[1] = colors[1];
    korl_gfx_batch(batch, KORL_GFX_BATCH_FLAGS_NONE);
}
korl_internal void korl_gfx_drawAabb3(const Korl_Math_Aabb3f32*const aabb, Korl_Vulkan_Color4u8 color, Korl_Memory_AllocatorHandle allocator)
{
    Korl_Gfx_Batch*const batch = korl_gfx_createBatchLines(allocator, 12);
    u32 currentLine = 0;
    /* bottom face */
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->min.y, aabb->min.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->min.y, aabb->min.z}.elements
                         ,3, color);
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->max.y, aabb->min.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->max.y, aabb->min.z}.elements
                         ,3, color);
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->min.y, aabb->min.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->max.y, aabb->min.z}.elements
                         ,3, color);
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->min.y, aabb->min.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->max.y, aabb->min.z}.elements
                         ,3, color);
    /* top face */
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->min.y, aabb->max.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->min.y, aabb->max.z}.elements
                         ,3, color);
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->max.y, aabb->max.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->max.y, aabb->max.z}.elements
                         ,3, color);
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->min.y, aabb->max.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->max.y, aabb->max.z}.elements
                         ,3, color);
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->min.y, aabb->max.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->max.y, aabb->max.z}.elements
                         ,3, color);
    /* connections between the top/bottom faces */
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->min.y, aabb->min.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->min.y, aabb->max.z}.elements
                         ,3, color);
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->min.y, aabb->min.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->min.y, aabb->max.z}.elements
                         ,3, color);
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->max.y, aabb->min.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->min.x, aabb->max.y, aabb->max.z}.elements
                         ,3, color);
    korl_gfx_batchSetLine(batch, currentLine++
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->max.y, aabb->min.z}.elements
                         ,KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){aabb->max.x, aabb->max.y, aabb->max.z}.elements
                         ,3, color);
    korl_gfx_batch(batch, KORL_GFX_BATCH_FLAGS_NONE);
}
