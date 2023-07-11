#include "utility/korl-utility-gfx.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-checkCast.h"
#include "korl-interface-platform.h"
korl_internal u8 korl_gfx_indexBytes(Korl_Gfx_VertexIndexType vertexIndexType)
{
    switch(vertexIndexType)
    {
    case KORL_GFX_VERTEX_INDEX_TYPE_U16    : return sizeof(u16);
    case KORL_GFX_VERTEX_INDEX_TYPE_U32    : return sizeof(u32);
    case KORL_GFX_VERTEX_INDEX_TYPE_INVALID: return 0;
    }
}
korl_internal Korl_Math_V4f32 korl_gfx_color_toLinear(Korl_Vulkan_Color4u8 color)
{
    return KORL_STRUCT_INITIALIZE(Korl_Math_V4f32){KORL_C_CAST(f32, color.r) / KORL_C_CAST(f32, KORL_U8_MAX)
                                                  ,KORL_C_CAST(f32, color.g) / KORL_C_CAST(f32, KORL_U8_MAX)
                                                  ,KORL_C_CAST(f32, color.b) / KORL_C_CAST(f32, KORL_U8_MAX)
                                                  ,KORL_C_CAST(f32, color.a) / KORL_C_CAST(f32, KORL_U8_MAX)};
}
korl_internal Korl_Gfx_Material korl_gfx_material_defaultUnlit(Korl_Math_V4f32 colorLinear4Base)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.drawState = {.polygonMode = KORL_GFX_POLYGON_MODE_FILL
                                                                  ,.cullMode    = KORL_GFX_CULL_MODE_BACK}
                                                    ,.properties = {.factorColorBase     = colorLinear4Base
                                                                   ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                   ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                   ,.shininess           = 0}
                                                    ,.maps = {.resourceHandleTextureBase     = 0
                                                             ,.resourceHandleTextureSpecular = 0
                                                             ,.resourceHandleTextureEmissive = 0}
                                                    ,.shaders = {.resourceHandleShaderVertex   = 0
                                                                ,.resourceHandleShaderFragment = 0}};
}
korl_internal Korl_Gfx_Material korl_gfx_material_defaultLit(void)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.drawState = {.polygonMode = KORL_GFX_POLYGON_MODE_FILL
                                                                  ,.cullMode    = KORL_GFX_CULL_MODE_BACK}
                                                    ,.properties = {.factorColorBase     = KORL_MATH_V4F32_ONE
                                                                   ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                   ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                   ,.shininess           = 32}
                                                    ,.maps = {.resourceHandleTextureBase     = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureSpecular = korl_gfx_getBlankTexture()
                                                             ,.resourceHandleTextureEmissive = korl_gfx_getBlankTexture()}
                                                    ,.shaders = {.resourceHandleShaderVertex   = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-lit.vert.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                ,.resourceHandleShaderFragment = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-lit.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)}};
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
korl_internal Korl_Gfx_Immediate _korl_gfx_immediate2d(Korl_Gfx_PrimitiveType primitiveType, u32 vertexCount, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    KORL_ZERO_STACK(Korl_Gfx_Immediate, result);
    result.primitiveType = primitiveType;
    u32 byteOffsetBuffer = 0;
    result.vertexStagingMeta.vertexCount = vertexCount;
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V2f32);
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
    byteOffsetBuffer += result.vertexStagingMeta.vertexCount * result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    if(o_colors)
    {
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer = byteOffsetBuffer;
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride       = sizeof(Korl_Vulkan_Color4u8);
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].vectorSize       = 4;
        byteOffsetBuffer += result.vertexStagingMeta.vertexCount * result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride;
    }
    result.stagingAllocation = korl_gfx_stagingAllocate(&result.vertexStagingMeta);
    *o_positions = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, result.stagingAllocation.buffer) + result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    if(o_colors)
        *o_colors = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, result.stagingAllocation.buffer) + result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer);
    return result;
}
korl_internal Korl_Gfx_Immediate _korl_gfx_immediate3d(Korl_Gfx_PrimitiveType primitiveType, u32 vertexCount, Korl_Math_V3f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    KORL_ZERO_STACK(Korl_Gfx_Immediate, result);
    result.primitiveType = primitiveType;
    u32 byteOffsetBuffer = 0;
    result.vertexStagingMeta.vertexCount = vertexCount;
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V3f32);
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
    result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 3;
    byteOffsetBuffer += result.vertexStagingMeta.vertexCount * result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    if(o_colors)
    {
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer = byteOffsetBuffer;
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride       = sizeof(Korl_Vulkan_Color4u8);
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].vectorSize       = 4;
        byteOffsetBuffer += result.vertexStagingMeta.vertexCount * result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride;
    }
    result.stagingAllocation = korl_gfx_stagingAllocate(&result.vertexStagingMeta);
    *o_positions = KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, result.stagingAllocation.buffer) + result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    if(o_colors)
        *o_colors = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, result.stagingAllocation.buffer) + result.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer);
    return result;
}
korl_internal Korl_Gfx_Immediate korl_gfx_immediateLines2d(u32 lineCount, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    return _korl_gfx_immediate2d(KORL_GFX_PRIMITIVE_TYPE_LINES, 2 * lineCount, o_positions, o_colors);
}
korl_internal Korl_Gfx_Immediate korl_gfx_immediateLines3d(u32 lineCount, Korl_Math_V3f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    return _korl_gfx_immediate3d(KORL_GFX_PRIMITIVE_TYPE_LINES, 2 * lineCount, o_positions, o_colors);
}
korl_internal Korl_Gfx_Immediate korl_gfx_immediateTriangles2d(u32 triangleCount, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    return _korl_gfx_immediate2d(KORL_GFX_PRIMITIVE_TYPE_TRIANGLES, 3 * triangleCount, o_positions, o_colors);
}
korl_internal Korl_Gfx_Immediate korl_gfx_immediateTriangles3d(u32 triangleCount, Korl_Math_V3f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    return _korl_gfx_immediate3d(KORL_GFX_PRIMITIVE_TYPE_TRIANGLES, 3 * triangleCount, o_positions, o_colors);
}
korl_internal Korl_Gfx_Immediate korl_gfx_immediateTriangleFan2d(u32 vertexCount, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    korl_assert(vertexCount >= 3);
    return _korl_gfx_immediate2d(KORL_GFX_PRIMITIVE_TYPE_TRIANGLE_FAN, vertexCount, o_positions, o_colors);
}
korl_internal Korl_Gfx_Immediate korl_gfx_immediateAxisNormalLines(void)
{
    Korl_Math_V3f32*      positions = NULL;
    Korl_Vulkan_Color4u8* colors    = NULL;
    Korl_Gfx_Immediate result = korl_gfx_immediateLines3d(3, &positions, &colors);
    positions[0] = KORL_MATH_V3F32_ZERO; positions[1] = KORL_MATH_V3F32_X;
    positions[2] = KORL_MATH_V3F32_ZERO; positions[3] = KORL_MATH_V3F32_Y;
    positions[4] = KORL_MATH_V3F32_ZERO; positions[5] = KORL_MATH_V3F32_Z;
    colors[0] = colors[1] = KORL_COLOR4U8_RED;
    colors[2] = colors[3] = KORL_COLOR4U8_GREEN;
    colors[4] = colors[5] = KORL_COLOR4U8_BLUE;
    return result;
}
korl_internal void korl_gfx_drawImmediate(const Korl_Gfx_Immediate* immediate, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, const Korl_Gfx_Material* material)
{
    /* configure the renderer draw state */
    Korl_Gfx_Material materialLocal;
    if(material)
        materialLocal = *material;
    else
        materialLocal = korl_gfx_material_defaultUnlit(korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    // KORL-ISSUE-000-000-156: gfx: if a texture is not present, default to a 1x1 "default" texture (base & specular => white, emissive => black); this would allow the user to choose which textures to provide to a lit material without having to use a different shader/pipeline
    const bool isMaterialTranslucent = materialLocal.properties.factorColorBase.w < 1.f;//@TODO: give material a "BLEND_MODE" property so we know if it's opaque/maskedTransparency/translucent
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Model, model);
    model.transform = korl_math_makeM4f32_rotateScaleTranslate(versor, scale, position);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Modes, drawMode);
    drawMode.primitiveType   = immediate->primitiveType;
    drawMode.cullMode        = materialLocal.drawState.cullMode;
    drawMode.polygonMode     = materialLocal.drawState.polygonMode;
    drawMode.enableDepthTest = true;
    drawMode.enableBlend     = isMaterialTranslucent;
    const Korl_Gfx_DrawState_Blend blend = KORL_GFX_BLEND_ALPHA;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.modes    = &drawMode;
    drawState.model    = &model;
    drawState.material = &materialLocal;
    drawState.blend    = &blend;
    korl_gfx_setDrawState(&drawState);
    korl_gfx_drawStagingAllocation(&immediate->stagingAllocation, &immediate->vertexStagingMeta);
}
korl_internal void korl_gfx_drawSphere(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, f32 radius, u32 latitudeSegments, u32 longitudeSegments, const Korl_Gfx_Material* material)
{
    /* configure the renderer draw state */
    // KORL-ISSUE-000-000-156: gfx: if a texture is not present, default to a 1x1 "default" texture (base & specular => white, emissive => black); this would allow the user to choose which textures to provide to a lit material without having to use a different shader/pipeline
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Model, model);
    model.transform = korl_math_makeM4f32_rotateTranslate(versor, position);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Modes, drawMode);
    drawMode.primitiveType   = KORL_GFX_PRIMITIVE_TYPE_TRIANGLES;
    drawMode.cullMode        = material->drawState.cullMode;
    drawMode.polygonMode     = material->drawState.polygonMode;
    drawMode.enableDepthTest = true;
    const Korl_Gfx_DrawState_Blend blend = KORL_GFX_BLEND_ALPHA;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.modes    = &drawMode;
    drawState.model    = &model;
    drawState.material = material;
    drawState.blend    = &blend;
    korl_gfx_setDrawState(&drawState);
    /* generate the mesh directly into graphics staging memory; draw it! */
    KORL_ZERO_STACK(Korl_Gfx_VertexStagingMeta, stagingMeta);
    u32 byteOffsetBuffer = 0;
    stagingMeta.vertexCount = korl_checkCast_u$_to_u32(korl_math_generateMeshSphereVertexCount(latitudeSegments, longitudeSegments));
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V3f32);
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 3;
    byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    //@TODO: only generate UV data if we're using texture maps
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteOffsetBuffer = byteOffsetBuffer;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteStride       = sizeof(Korl_Math_V2f32);
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].vectorSize       = 2;
    byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteStride;
    //@TODO: conditionally generate normals (needed for lit materials, for example)
    Korl_Gfx_StagingAllocation stagingAllocation = korl_gfx_stagingAllocate(&stagingMeta);
    Korl_Math_V3f32*const positions = KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    Korl_Math_V2f32*const uvs       = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV      ].byteOffsetBuffer);
    korl_math_generateMeshSphere(radius, latitudeSegments, longitudeSegments, positions, sizeof(*positions), uvs, sizeof(*uvs));
    korl_gfx_drawStagingAllocation(&stagingAllocation, &stagingMeta);
}
korl_internal void _korl_gfx_drawRectangle(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Vulkan_Color4u8** o_colors, bool enableDepthTest)
{
    /* configure the renderer draw state */
    Korl_Gfx_Material materialLocal;
    if(material)
        materialLocal = *material;
    else
        materialLocal = korl_gfx_material_defaultUnlit(korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    // KORL-ISSUE-000-000-156: gfx: if a texture is not present, default to a 1x1 "default" texture (base & specular => white, emissive => black); this would allow the user to choose which textures to provide to a lit material without having to use a different shader/pipeline
    const bool isMaterialTranslucent = materialLocal.properties.factorColorBase.w < 1.f;//@TODO: give material a "BLEND_MODE" property so we know if it's opaque/maskedTransparency/translucent
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Model, model);
    model.transform = korl_math_makeM4f32_rotateTranslate(versor, position);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Modes, drawMode);
    drawMode.primitiveType   = KORL_GFX_PRIMITIVE_TYPE_TRIANGLE_STRIP;// for separate individual quad draws, TRIANGLE_STRIP is the least amount of data we can possibly send to the renderer; if we wanted to draw _many_ quads all at once using this topology, we would need to enable a feature, and either set pipeline input state statically or dynamically (for Vulkan)
    drawMode.cullMode        = materialLocal.drawState.cullMode;
    drawMode.polygonMode     = materialLocal.drawState.polygonMode;
    drawMode.enableDepthTest = enableDepthTest;
    drawMode.enableBlend     = isMaterialTranslucent;
    const Korl_Gfx_DrawState_Blend blend = KORL_GFX_BLEND_ALPHA;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.modes    = &drawMode;
    drawState.model    = &model;
    drawState.material = &materialLocal;
    drawState.blend    = &blend;
    korl_gfx_setDrawState(&drawState);
    /* generate the mesh directly into graphics staging memory; draw it! */
    korl_shared_const Korl_Math_V2f32 QUAD_POSITION_NORMALS_TRI_STRIP[4] = {{0,1}, {0,0}, {1,1}, {1,0}};
    korl_shared_const Korl_Math_V2f32 QUAD_POSITION_NORMALS_LOOP     [4] = {{0,0}, {1,0}, {1,1}, {0,1}};
    KORL_ZERO_STACK(Korl_Gfx_VertexStagingMeta, stagingMeta);
    u32 byteOffsetBuffer = 0;
    stagingMeta.vertexCount = korl_arraySize(QUAD_POSITION_NORMALS_TRI_STRIP);
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V2f32);
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
    byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    if(materialLocal.maps.resourceHandleTextureBase)
    {
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteOffsetBuffer = byteOffsetBuffer;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteStride       = sizeof(Korl_Math_V2f32);
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].vectorSize       = 2;
        byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteStride;
    }
    if(o_colors)
    {
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer = byteOffsetBuffer;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride       = sizeof(Korl_Vulkan_Color4u8);
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].vectorSize       = 4;
        byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride;
    }
    Korl_Gfx_StagingAllocation stagingAllocation = korl_gfx_stagingAllocate(&stagingMeta);
    Korl_Math_V2f32*const positions = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    Korl_Math_V2f32*const uvs       = stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID 
                                      ? KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteOffsetBuffer)
                                      : NULL;
    if(o_colors)
        *o_colors = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer);
    for(u32 v = 0; v < korl_arraySize(QUAD_POSITION_NORMALS_TRI_STRIP); v++)
    {
        positions[v] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){QUAD_POSITION_NORMALS_TRI_STRIP[v].x * size.x - anchorRatio.x * size.x
                                                              ,QUAD_POSITION_NORMALS_TRI_STRIP[v].y * size.y - anchorRatio.y * size.y};
        if(uvs)
            uvs[v] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){      QUAD_POSITION_NORMALS_TRI_STRIP[v].x
                                                            ,1.f - QUAD_POSITION_NORMALS_TRI_STRIP[v].y};
    }
    // korl_math_generateMeshSphere(radius, latitudeSegments, longitudeSegments, positions, sizeof(*positions), uvs, sizeof(*uvs));
    korl_gfx_drawStagingAllocation(&stagingAllocation, &stagingMeta);
    if(materialOutline)
    {
        const bool isOutlineMaterialTranslucent = materialOutline->properties.factorColorBase.w < 1.f;//@TODO: give material a "BLEND_MODE" property so we know if it's opaque/maskedTransparency/translucent
        /* configure the renderer draw state */
        drawMode.primitiveType = outlineThickness == 0 ? KORL_GFX_PRIMITIVE_TYPE_LINE_STRIP : KORL_GFX_PRIMITIVE_TYPE_TRIANGLE_STRIP;
        drawMode.cullMode      = materialOutline->drawState.cullMode;
        drawMode.polygonMode   = materialOutline->drawState.polygonMode;
        drawMode.enableBlend   = isOutlineMaterialTranslucent;
        KORL_ZERO_STACK(Korl_Gfx_DrawState, drawStateOutline);
        drawStateOutline.modes    = &drawMode;
        drawStateOutline.material = materialOutline;
        korl_gfx_setDrawState(&drawStateOutline);
        /* generate the mesh directly into graphics staging memory; draw it! */
        korl_memory_zero(&stagingMeta, sizeof(stagingMeta));
        byteOffsetBuffer = 0;
        stagingMeta.vertexCount = outlineThickness == 0 
                                  ?      korl_arraySize(QUAD_POSITION_NORMALS_LOOP) + 1 
                                  : 2 * (korl_arraySize(QUAD_POSITION_NORMALS_LOOP) + 1);
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V2f32);
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
        byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
        stagingAllocation = korl_gfx_stagingAllocate(&stagingMeta);
        Korl_Math_V2f32*const outlinePositions = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
        const Korl_Math_V2f32 centerOfMass = korl_math_v2f32_subtract(korl_math_v2f32_multiplyScalar(size, 0.5f), korl_math_v2f32_multiply(anchorRatio, size));
        for(u32 v = 0; v < korl_arraySize(QUAD_POSITION_NORMALS_LOOP) + 1; v++)
        {
            const u32 cornerIndex = v % korl_arraySize(QUAD_POSITION_NORMALS_LOOP);
            if(outlineThickness == 0)
                outlinePositions[v] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){QUAD_POSITION_NORMALS_LOOP[cornerIndex].x * size.x - anchorRatio.x * size.x
                                                                             ,QUAD_POSITION_NORMALS_LOOP[cornerIndex].y * size.y - anchorRatio.y * size.y};
            else
            {
                outlinePositions[2 * v + 0] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){QUAD_POSITION_NORMALS_LOOP[cornerIndex].x * size.x - anchorRatio.x * size.x
                                                                                     ,QUAD_POSITION_NORMALS_LOOP[cornerIndex].y * size.y - anchorRatio.y * size.y};
                const Korl_Math_V2f32 fromCenter      = korl_math_v2f32_subtract(outlinePositions[2 * v + 0], centerOfMass);
                const Korl_Math_V2f32 fromCenterNorms = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){fromCenter.x / korl_math_f32_positive(fromCenter.x)
                                                                                              ,fromCenter.y / korl_math_f32_positive(fromCenter.y)};
                outlinePositions[2 * v + 1] = korl_math_v2f32_add(outlinePositions[2 * v + 0], korl_math_v2f32_multiplyScalar(fromCenterNorms, outlineThickness));
            }
        }
        korl_gfx_drawStagingAllocation(&stagingAllocation, &stagingMeta);
    }
}
korl_internal void korl_gfx_drawRectangle2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Vulkan_Color4u8** o_colors)
{
    _korl_gfx_drawRectangle(KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, size, outlineThickness, material, materialOutline, o_colors, false/*if the user is drawing 2D geometry, they most likely don't care about depth write/test, but we probably want a way to set this anyway*/);
}
korl_internal void korl_gfx_drawRectangle3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Vulkan_Color4u8** o_colors)
{
    _korl_gfx_drawRectangle(position, versor, anchorRatio, size, outlineThickness, material, materialOutline, o_colors, true);
}
korl_internal void korl_gfx_drawLines2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    /* configure the renderer draw state */
    Korl_Gfx_Material materialLocal;
    if(material)
        materialLocal = *material;
    else
        materialLocal = korl_gfx_material_defaultUnlit(korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    // KORL-ISSUE-000-000-156: gfx: if a texture is not present, default to a 1x1 "default" texture (base & specular => white, emissive => black); this would allow the user to choose which textures to provide to a lit material without having to use a different shader/pipeline
    const bool isMaterialTranslucent = materialLocal.properties.factorColorBase.w < 1.f;//@TODO: give material a "BLEND_MODE" property so we know if it's opaque/maskedTransparency/translucent
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Model, model);
    model.transform = korl_math_makeM4f32_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Modes, drawMode);
    drawMode.primitiveType   = KORL_GFX_PRIMITIVE_TYPE_LINES;
    drawMode.cullMode        = materialLocal.drawState.cullMode;
    drawMode.polygonMode     = materialLocal.drawState.polygonMode;
    drawMode.enableDepthTest = false;// if the user is drawing 2D geometry, they most likely don't care about depth write/test, but we probably want a way to set this anyway
    drawMode.enableBlend     = isMaterialTranslucent;
    const Korl_Gfx_DrawState_Blend blend = KORL_GFX_BLEND_ALPHA;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.modes    = &drawMode;
    drawState.model    = &model;
    drawState.material = &materialLocal;
    drawState.blend    = &blend;
    korl_gfx_setDrawState(&drawState);
    /* allocate staging memory & issue draw command */
    KORL_ZERO_STACK(Korl_Gfx_VertexStagingMeta, stagingMeta);
    u32 byteOffsetBuffer = 0;
    stagingMeta.vertexCount = 2 * lineCount;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V2f32);
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
    byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    if(o_colors)
    {
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer = byteOffsetBuffer;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride       = sizeof(Korl_Vulkan_Color4u8);
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].vectorSize       = 4;
        byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride;
    }
    Korl_Gfx_StagingAllocation stagingAllocation = korl_gfx_stagingAllocate(&stagingMeta);
    korl_gfx_drawStagingAllocation(&stagingAllocation, &stagingMeta);
    /* at this point, we leave it to the user who called us to populate the vertex data with the desired values */
    *o_positions = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    if(o_colors)
        *o_colors = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer);
}
korl_internal void korl_gfx_drawTriangles2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 triangleCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    /* configure the renderer draw state */
    Korl_Gfx_Material materialLocal;
    if(material)
        materialLocal = *material;
    else
        materialLocal = korl_gfx_material_defaultUnlit(korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    // KORL-ISSUE-000-000-156: gfx: if a texture is not present, default to a 1x1 "default" texture (base & specular => white, emissive => black); this would allow the user to choose which textures to provide to a lit material without having to use a different shader/pipeline
    const bool isMaterialTranslucent = materialLocal.properties.factorColorBase.w < 1.f;//@TODO: give material a "BLEND_MODE" property so we know if it's opaque/maskedTransparency/translucent
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Model, model);
    model.transform = korl_math_makeM4f32_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Modes, drawMode);
    drawMode.primitiveType   = KORL_GFX_PRIMITIVE_TYPE_TRIANGLES;
    drawMode.cullMode        = materialLocal.drawState.cullMode;
    drawMode.polygonMode     = materialLocal.drawState.polygonMode;
    drawMode.enableDepthTest = false;// if the user is drawing 2D geometry, they most likely don't care about depth write/test, but we probably want a way to set this anyway
    drawMode.enableBlend     = isMaterialTranslucent;
    const Korl_Gfx_DrawState_Blend blend = KORL_GFX_BLEND_ALPHA;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.modes    = &drawMode;
    drawState.model    = &model;
    drawState.material = &materialLocal;
    drawState.blend    = &blend;
    korl_gfx_setDrawState(&drawState);
    /* allocate staging memory & issue draw command */
    KORL_ZERO_STACK(Korl_Gfx_VertexStagingMeta, stagingMeta);
    u32 byteOffsetBuffer = 0;
    stagingMeta.vertexCount = 3 * triangleCount;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V2f32);
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
    byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    if(o_colors)
    {
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer = byteOffsetBuffer;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride       = sizeof(Korl_Vulkan_Color4u8);
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].vectorSize       = 4;
        byteOffsetBuffer += stagingMeta.vertexCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride;
    }
    Korl_Gfx_StagingAllocation stagingAllocation = korl_gfx_stagingAllocate(&stagingMeta);
    korl_gfx_drawStagingAllocation(&stagingAllocation, &stagingMeta);
    /* at this point, we leave it to the user who called us to populate the vertex data with the desired values */
    *o_positions = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    if(o_colors)
        *o_colors = KORL_C_CAST(Korl_Vulkan_Color4u8*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer);
}
korl_internal void korl_gfx_drawTriangleFan2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    korl_assert(vertexCount >= 3);
    const Korl_Gfx_Immediate immediate = _korl_gfx_immediate2d(KORL_GFX_PRIMITIVE_TYPE_TRIANGLE_FAN, vertexCount, o_positions, o_colors);
    //@TODO: disable depth test somehow
    korl_gfx_drawImmediate(&immediate, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, KORL_MATH_V3F32_ONE, material);
}
korl_internal void korl_gfx_drawTriangleFan3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Vulkan_Color4u8** o_colors)
{
    korl_assert(vertexCount >= 3);
    const Korl_Gfx_Immediate immediate = _korl_gfx_immediate3d(KORL_GFX_PRIMITIVE_TYPE_TRIANGLE_FAN, vertexCount, o_positions, o_colors);
    korl_gfx_drawImmediate(&immediate, position, versor, KORL_MATH_V3F32_ONE, material);
}
korl_internal void _korl_gfx_drawText(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, bool enableDepthTest)
{
    Korl_Gfx_Material materialOverride;// the base color map of the material will always be set to the translucency mask texture containing the baked rasterized glyphs, so we need to override the material
    if(material)
        materialOverride = *material;
    else
        materialOverride = korl_gfx_material_defaultUnlit(korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    /* determine how many glyphs need to be drawn from utf8Text, as well as the 
        AABB size of the text so we can calculate position offset based on anchorRatio */
    const Korl_Gfx_Font_TextMetrics textMetrics = korl_gfx_font_getTextMetrics(utf16FontAssetName, textPixelHeight, utf8Text);
    /* configure the renderer draw state */
    const Korl_Gfx_Font_Resources fontResources = korl_gfx_font_getResources(utf16FontAssetName, textPixelHeight);
    // KORL-ISSUE-000-000-156: gfx: if a texture is not present, default to a 1x1 "default" texture (base & specular => white, emissive => black); this would allow the user to choose which textures to provide to a lit material without having to use a different shader/pipeline
    materialOverride.maps.resourceHandleTextureBase = fontResources.resourceHandleTexture;
    const bool isMaterialTranslucent = true/*_all_ text drawn this way _must_ be translucent*/;//@TODO: give material a "BLEND_MODE" property so we know if it's opaque/maskedTransparency/translucent
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Model, model);
    model.transform = korl_math_makeM4f32_rotateTranslate(versor, position);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_Modes, drawMode);
    drawMode.primitiveType   = KORL_GFX_PRIMITIVE_TYPE_TRIANGLES;
    drawMode.cullMode        = materialOverride.drawState.cullMode;
    drawMode.polygonMode     = materialOverride.drawState.polygonMode;
    drawMode.enableDepthTest = enableDepthTest;
    drawMode.enableBlend     = isMaterialTranslucent;
    const Korl_Gfx_DrawState_Blend blend = KORL_GFX_BLEND_ALPHA;
    KORL_ZERO_STACK(Korl_Gfx_DrawState_StorageBuffers, storageBuffers);
    storageBuffers.resourceHandleVertex = fontResources.resourceHandleSsboGlyphMeshVertices;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.modes          = &drawMode;
    drawState.model          = &model;
    drawState.material       = &materialOverride;
    drawState.blend          = &blend;
    drawState.storageBuffers = &storageBuffers;
    korl_gfx_setDrawState(&drawState);
    /* allocate staging memory & issue draw command */
    KORL_ZERO_STACK(Korl_Gfx_VertexStagingMeta, stagingMeta);
    u32 byteOffsetBuffer = 0;
    stagingMeta.indexCount            = korl_arraySize(KORL_GFX_TRI_QUAD_INDICES);// @TODO: is there a way to just bake this into korl-text.vert?
    stagingMeta.indexType             = KORL_GFX_VERTEX_INDEX_TYPE_U16;
    stagingMeta.indexByteOffsetBuffer = byteOffsetBuffer;
    byteOffsetBuffer += stagingMeta.indexCount * sizeof(*KORL_GFX_TRI_QUAD_INDICES);
    stagingMeta.instanceCount = textMetrics.visibleGlyphCount;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V2f32);
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
    byteOffsetBuffer += stagingMeta.instanceCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT].byteOffsetBuffer = byteOffsetBuffer;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT].byteStride       = sizeof(u32);
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT].vectorSize       = 1;
    byteOffsetBuffer += stagingMeta.instanceCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT].byteStride;
    Korl_Gfx_StagingAllocation stagingAllocation = korl_gfx_stagingAllocate(&stagingMeta);
    korl_gfx_drawStagingAllocation(&stagingAllocation, &stagingMeta);
    /* generate the text vertex data */
    u16*const indices = KORL_C_CAST(u16*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.indexByteOffsetBuffer);
    korl_memory_copy(indices, KORL_GFX_TRI_QUAD_INDICES, sizeof(KORL_GFX_TRI_QUAD_INDICES));
    korl_gfx_font_generateText(utf16FontAssetName, textPixelHeight, utf8Text
                              ,KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){-anchorRatio.x * textMetrics.aabbSize.x
                                                                      ,-anchorRatio.y * textMetrics.aabbSize.y + textMetrics.aabbSize.y}
                              ,KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer)
                              ,KORL_C_CAST(u32*            , KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UINT    ].byteOffsetBuffer));
}
korl_internal void korl_gfx_drawText2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline)
{
    _korl_gfx_drawText(KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, utf8Text, utf16FontAssetName, textPixelHeight, outlineSize, material, materialOutline, false/* if the user is drawing 2D geometry, they most likely don't care about depth write/test, but we probably want a way to set this anyway*/);
}
korl_internal void korl_gfx_drawText3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline)
{
    _korl_gfx_drawText(position, versor, anchorRatio, utf8Text, utf16FontAssetName, textPixelHeight, outlineSize, material, materialOutline, true);
}
