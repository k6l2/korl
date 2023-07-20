#include "utility/korl-utility-gfx.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-checkCast.h"
#include "korl-interface-platform.h"
/** used for Korl_Gfx_StagingAllocation reallocation procedures, as we need to 
 * offset the buffers of each vertex attribute within the allocation once 
 * reallocation occurs */
typedef struct _Korl_Gfx_SortableVertexBinding
{
    Korl_Gfx_VertexAttributeBinding binding;
    u32                             byteOffsetBuffer;
} _Korl_Gfx_SortableVertexBinding;
korl_internal KORL_ALGORITHM_COMPARE(_korl_gfx_sortVertexBindings_ascendByteOffset)
{
    const _Korl_Gfx_SortableVertexBinding*const sortableVertexBindingA = KORL_C_CAST(const _Korl_Gfx_SortableVertexBinding*, a);
    const _Korl_Gfx_SortableVertexBinding*const sortableVertexBindingB = KORL_C_CAST(const _Korl_Gfx_SortableVertexBinding*, b);
    return sortableVertexBindingA->byteOffsetBuffer > sortableVertexBindingB->byteOffsetBuffer;
}
korl_internal u8 korl_gfx_indexBytes(Korl_Gfx_VertexIndexType vertexIndexType)
{
    switch(vertexIndexType)
    {
    case KORL_GFX_VERTEX_INDEX_TYPE_U16    : return sizeof(u16);
    case KORL_GFX_VERTEX_INDEX_TYPE_U32    : return sizeof(u32);
    case KORL_GFX_VERTEX_INDEX_TYPE_INVALID: return 0;
    }
}
korl_internal Korl_Math_V4f32 korl_gfx_color_toLinear(Korl_Gfx_Color4u8 color)
{
    return KORL_STRUCT_INITIALIZE(Korl_Math_V4f32){KORL_C_CAST(f32, color.r) / KORL_C_CAST(f32, KORL_U8_MAX)
                                                  ,KORL_C_CAST(f32, color.g) / KORL_C_CAST(f32, KORL_U8_MAX)
                                                  ,KORL_C_CAST(f32, color.b) / KORL_C_CAST(f32, KORL_U8_MAX)
                                                  ,KORL_C_CAST(f32, color.a) / KORL_C_CAST(f32, KORL_U8_MAX)};
}
korl_internal Korl_Gfx_Material korl_gfx_material_defaultUnlit(Korl_Gfx_Material_PrimitiveType primitiveType, Korl_Gfx_Material_Mode_Flags flags, Korl_Math_V4f32 colorLinear4Base)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.primitiveType = primitiveType
                                                              ,.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         = flags}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = colorLinear4Base
                                                                              ,.factorColorEmissive = KORL_MATH_V3F32_ZERO
                                                                              ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                              ,.shininess           = 0}
                                                    ,.maps = {.resourceHandleTextureBase     = 0
                                                             ,.resourceHandleTextureSpecular = 0
                                                             ,.resourceHandleTextureEmissive = 0}
                                                    ,.shaders = {.resourceHandleShaderVertex   = 0
                                                                ,.resourceHandleShaderFragment = 0}};
}
korl_internal Korl_Gfx_Material korl_gfx_material_defaultLit(Korl_Gfx_Material_PrimitiveType primitiveType, Korl_Gfx_Material_Mode_Flags flags)
{
    return KORL_STRUCT_INITIALIZE(Korl_Gfx_Material){.modes = {.primitiveType = primitiveType
                                                              ,.polygonMode   = KORL_GFX_MATERIAL_POLYGON_MODE_FILL
                                                              ,.cullMode      = KORL_GFX_MATERIAL_CULL_MODE_BACK
                                                              ,.blend         = KORL_GFX_BLEND_ALPHA
                                                              ,.flags         = flags}
                                                    ,.fragmentShaderUniform = {.factorColorBase     = KORL_MATH_V4F32_ONE
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
korl_internal Korl_Math_M4f32 korl_gfx_camera_projection(const Korl_Gfx_Camera*const context)
{
    const Korl_Math_V2u32 surfaceSize = korl_gfx_getSurfaceSize();
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
korl_internal void korl_gfx_camera_drawFrustum(const Korl_Gfx_Camera*const context, const Korl_Gfx_Material* material)
{
    /* calculate the world-space rays for each corner of the render surface */
    const Korl_Math_V2u32 surfaceSize = korl_gfx_getSurfaceSize();
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
    /* draw the frustum by composing a set of 3 world-space staple-shaped lines per corner */
    Korl_Math_V3f32* positions;
    korl_gfx_drawLines3d(KORL_MATH_V3F32_ZERO, KORL_MATH_QUATERNION_IDENTITY, 3 * korl_arraySize(cameraWorldCorners), material, &positions, NULL);
    for(u8 i = 0; i < korl_arraySize(cameraWorldCorners); i++)
    {
        const u$ iNext = (i + 1) % korl_arraySize(cameraWorldCorners);
        /*line for the near-plane quad face*/
        positions[2 * (3 * i + 0) + 0] = cameraWorldCorners[i].position;
        positions[2 * (3 * i + 0) + 1] = cameraWorldCorners[iNext].position;
        /*line for the far-plane quad face*/
        positions[2 * (3 * i + 1) + 0] = cameraWorldCornersFar[i];
        positions[2 * (3 * i + 1) + 1] = cameraWorldCornersFar[iNext];
        /*line connecting the near-plane to the far-plane on the current corner*/
        positions[2 * (3 * i + 2) + 0] = cameraWorldCorners[i].position;
        positions[2 * (3 * i + 2) + 1] = cameraWorldCornersFar[i];
    }
}
korl_internal Korl_Gfx_Drawable _korl_gfx_immediate2d(Korl_Gfx_Material_PrimitiveType primitiveType, u32 vertexCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs)
{
    KORL_ZERO_STACK(Korl_Gfx_Drawable, result);
    result.type            = KORL_GFX_DRAWABLE_TYPE_IMMEDIATE;
    result.transform       = korl_math_transform3d_identity();
    result.subType.immediate.primitiveType = primitiveType;
    u32 byteOffsetBuffer = 0;
    result.subType.immediate.vertexStagingMeta.vertexCount = vertexCount;
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V2f32);
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 2;
    byteOffsetBuffer += result.subType.immediate.vertexStagingMeta.vertexCount * result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    if(o_colors)
    {
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer = byteOffsetBuffer;
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride       = sizeof(Korl_Gfx_Color4u8);
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].vectorSize       = 4;
        byteOffsetBuffer += result.subType.immediate.vertexStagingMeta.vertexCount * result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride;
    }
    if(o_uvs)
    {
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteOffsetBuffer = byteOffsetBuffer;
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteStride       = sizeof(Korl_Math_V2f32);
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].vectorSize       = 2;
        byteOffsetBuffer += result.subType.immediate.vertexStagingMeta.vertexCount * result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteStride;
    }
    result.subType.immediate.stagingAllocation = korl_gfx_stagingAllocate(&result.subType.immediate.vertexStagingMeta);
    *o_positions = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, result.subType.immediate.stagingAllocation.buffer) + result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    if(o_colors)
        *o_colors = KORL_C_CAST(Korl_Gfx_Color4u8*, KORL_C_CAST(u8*, result.subType.immediate.stagingAllocation.buffer) + result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer);
    if(o_uvs)
        *o_uvs = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, result.subType.immediate.stagingAllocation.buffer) + result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_UV].byteOffsetBuffer);
    return result;
}
korl_internal Korl_Gfx_Drawable _korl_gfx_immediate3d(Korl_Gfx_Material_PrimitiveType primitiveType, u32 vertexCount, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    KORL_ZERO_STACK(Korl_Gfx_Drawable, result);
    result.type            = KORL_GFX_DRAWABLE_TYPE_IMMEDIATE;
    result.transform       = korl_math_transform3d_identity();
    result.subType.immediate.primitiveType     = primitiveType;
    result.subType.immediate.materialModeFlags =   KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_TEST 
                               | KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_DEPTH_WRITE;
    u32 byteOffsetBuffer = 0;
    result.subType.immediate.vertexStagingMeta.vertexCount = vertexCount;
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer = byteOffsetBuffer;
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride       = sizeof(Korl_Math_V3f32);
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_F32;
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
    result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].vectorSize       = 3;
    byteOffsetBuffer += result.subType.immediate.vertexStagingMeta.vertexCount * result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteStride;
    if(o_colors)
    {
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer = byteOffsetBuffer;
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride       = sizeof(Korl_Gfx_Color4u8);
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U8;
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_VERTEX;
        result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].vectorSize       = 4;
        byteOffsetBuffer += result.subType.immediate.vertexStagingMeta.vertexCount * result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteStride;
    }
    result.subType.immediate.stagingAllocation = korl_gfx_stagingAllocate(&result.subType.immediate.vertexStagingMeta);
    *o_positions = KORL_C_CAST(Korl_Math_V3f32*, KORL_C_CAST(u8*, result.subType.immediate.stagingAllocation.buffer) + result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    if(o_colors)
        *o_colors = KORL_C_CAST(Korl_Gfx_Color4u8*, KORL_C_CAST(u8*, result.subType.immediate.stagingAllocation.buffer) + result.subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer);
    return result;
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateLines2d(u32 lineCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    return _korl_gfx_immediate2d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINES, 2 * lineCount, o_positions, o_colors, NULL);
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateLines3d(u32 lineCount, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    return _korl_gfx_immediate3d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINES, 2 * lineCount, o_positions, o_colors);
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateLineStrip2d(u32 vertexCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    korl_assert(vertexCount >= 2);
    return _korl_gfx_immediate2d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINE_STRIP, vertexCount, o_positions, o_colors, NULL);
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateTriangles2d(u32 triangleCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    return _korl_gfx_immediate2d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES, 3 * triangleCount, o_positions, o_colors, NULL);
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateTriangles3d(u32 triangleCount, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    return _korl_gfx_immediate3d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES, 3 * triangleCount, o_positions, o_colors);
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateTriangleFan2d(u32 vertexCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    korl_assert(vertexCount >= 3);
    return _korl_gfx_immediate2d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_FAN, vertexCount, o_positions, o_colors, NULL);
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateTriangleStrip2d(u32 vertexCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs)
{
    korl_assert(vertexCount >= 3);
    return _korl_gfx_immediate2d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_STRIP, vertexCount, o_positions, o_colors, o_uvs);
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateRectangle(Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs)
{
    Korl_Math_V2f32* positions;
    korl_shared_const Korl_Math_V2f32 QUAD_POSITION_NORMALS_TRI_STRIP[4] = {{0,1}, {0,0}, {1,1}, {1,0}};
    Korl_Gfx_Drawable result = _korl_gfx_immediate2d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_STRIP, korl_arraySize(QUAD_POSITION_NORMALS_TRI_STRIP), &positions, o_colors, o_uvs);
    for(u32 v = 0; v < korl_arraySize(QUAD_POSITION_NORMALS_TRI_STRIP); v++)
    {
        positions[v] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){QUAD_POSITION_NORMALS_TRI_STRIP[v].x * size.x - anchorRatio.x * size.x
                                                              ,QUAD_POSITION_NORMALS_TRI_STRIP[v].y * size.y - anchorRatio.y * size.y};
        if(o_uvs)
            (*o_uvs)[v] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){      QUAD_POSITION_NORMALS_TRI_STRIP[v].x
                                                                 ,1.f - QUAD_POSITION_NORMALS_TRI_STRIP[v].y};
    }
    if(o_positions)
        *o_positions = positions;
    return result;
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateCircle(Korl_Math_V2f32 anchorRatio, f32 radius, u32 circumferenceVertices, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs)
{
    korl_assert(circumferenceVertices >= 3);
    Korl_Math_V2f32* positions;
    const u32 vertexCount = 1/*center vertex*/ + circumferenceVertices + 1/*repeat circumference start vertex*/;
    Korl_Gfx_Drawable result = _korl_gfx_immediate2d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_FAN, vertexCount, &positions, o_colors, o_uvs);
    positions[0] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){radius - anchorRatio.x * 2 * radius
                                                          ,radius - anchorRatio.y * 2 * radius};
    if(o_uvs)
        (*o_uvs)[0] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){0.5f, 0.5f};
    const f32 radiansPerVertex = KORL_TAU32 / KORL_C_CAST(f32, circumferenceVertices);
    for(u32 v = 0; v < circumferenceVertices + 1; v++)
    {
        const Korl_Math_V2f32 spoke = korl_math_v2f32_fromRotationZ(1, KORL_C_CAST(f32, v % circumferenceVertices) * radiansPerVertex);
        positions[1 + v] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){radius + radius * spoke.x - anchorRatio.x * 2 * radius
                                                                  ,radius + radius * spoke.y - anchorRatio.y * 2 * radius};
        if(o_uvs)
            (*o_uvs)[1 + v] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){       0.5f + 0.5f * spoke.x
                                                                     ,1.f - (0.5f + 0.5f * spoke.y)};
    }
    if(o_positions)
        *o_positions = positions;
    return result;
}
korl_internal Korl_Gfx_Drawable korl_gfx_immediateAxisNormalLines(void)
{
    Korl_Math_V3f32*   positions = NULL;
    Korl_Gfx_Color4u8* colors    = NULL;
    Korl_Gfx_Drawable result     = korl_gfx_immediateLines3d(3, &positions, &colors);
    positions[0] = KORL_MATH_V3F32_ZERO; positions[1] = KORL_MATH_V3F32_X;
    positions[2] = KORL_MATH_V3F32_ZERO; positions[3] = KORL_MATH_V3F32_Y;
    positions[4] = KORL_MATH_V3F32_ZERO; positions[5] = KORL_MATH_V3F32_Z;
    colors[0] = colors[1] = KORL_COLOR4U8_RED;
    colors[2] = colors[3] = KORL_COLOR4U8_GREEN;
    colors[4] = colors[5] = KORL_COLOR4U8_BLUE;
    return result;
}
korl_internal Korl_Gfx_Drawable korl_gfx_mesh(Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName)
{
    KORL_ZERO_STACK(Korl_Gfx_Drawable, result);
    result.type                               = KORL_GFX_DRAWABLE_TYPE_MESH;
    result.transform                          = korl_math_transform3d_identity();
    result.subType.mesh.resourceHandleScene3d = resourceHandleScene3d;
    korl_assert(utf8MeshName.size < korl_arraySize(result.subType.mesh.rawUtf8Scene3dMeshName));
    korl_memory_copy(result.subType.mesh.rawUtf8Scene3dMeshName, utf8MeshName.data, utf8MeshName.size);
    result.subType.mesh.rawUtf8Scene3dMeshName[utf8MeshName.size] = '\0';
    result.subType.mesh.rawUtf8Scene3dMeshNameSize                = korl_checkCast_u$_to_u8(utf8MeshName.size);
    return result;
}
korl_internal void korl_gfx_addLines2d(Korl_Gfx_Drawable* drawableLines, u32 lineCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    korl_assert(drawableLines->type == KORL_GFX_DRAWABLE_TYPE_IMMEDIATE);
    korl_assert(drawableLines->subType.immediate.primitiveType == KORL_GFX_MATERIAL_PRIMITIVE_TYPE_LINES);
    Korl_Gfx_VertexStagingMeta newVertexStagingMeta = drawableLines->subType.immediate.vertexStagingMeta;
    newVertexStagingMeta.vertexCount += 2 * lineCount;
    /* get a list of vertex attribute bindings & their byte offsets, sorted by ascending byte offset */
    _Korl_Gfx_SortableVertexBinding sortableVertexBindings[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT];
    u32                             sortableVertexBindingsSize = 0;
    for(u32 i = 0; i < KORL_GFX_VERTEX_ATTRIBUTE_BINDING_ENUM_COUNT; i++)
        if(drawableLines->subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[i].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID)
            sortableVertexBindings[sortableVertexBindingsSize++] = 
                KORL_STRUCT_INITIALIZE(_Korl_Gfx_SortableVertexBinding){.binding          = KORL_C_CAST(Korl_Gfx_VertexAttributeBinding, i)
                                                                       ,.byteOffsetBuffer = drawableLines->subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[i].byteOffsetBuffer};
    korl_algorithm_sort_quick(sortableVertexBindings, sortableVertexBindingsSize, sizeof(*sortableVertexBindings), _korl_gfx_sortVertexBindings_ascendByteOffset);
    /* offset the vertex attribute byte offets for the new VertexStagingMeta by the total size of all previous attributes */
    u32 currentByteOffsetBuffer = 0;
    for(u32 i = 0; i < sortableVertexBindingsSize; i++)
    {
        newVertexStagingMeta.vertexAttributeDescriptors[sortableVertexBindings[i].binding].byteOffsetBuffer = currentByteOffsetBuffer;
        currentByteOffsetBuffer += newVertexStagingMeta.vertexCount * newVertexStagingMeta.vertexAttributeDescriptors[sortableVertexBindings[i].binding].byteStride;
    }
    /* reallocate the staging buffer */
    drawableLines->subType.immediate.stagingAllocation = korl_gfx_stagingReallocate(&newVertexStagingMeta, &drawableLines->subType.immediate.stagingAllocation);
    /* update vertex staging meta & associated vertex data pointers */
    *o_positions = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, drawableLines->subType.immediate.stagingAllocation.buffer) + newVertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    if(o_colors)
        *o_colors = KORL_C_CAST(Korl_Gfx_Color4u8*, KORL_C_CAST(u8*, drawableLines->subType.immediate.stagingAllocation.buffer) + newVertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer);
    /* temporarily re-obtain memory pointers to where this data _used_ to be located before the realloc; 
        then, because we know that the staging allocation is _expanded_ we can perform simple memory moves on the data, 
        starting at the highest attribute addresses; 
        NOTE: we do memory moves (instead of copies) to adjust the old attribute 
              buffers because it's highly likely that they will self-intersect 
              with their new ranges after stagingReallocate */
    Korl_Math_V2f32*const   old_positions = KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, drawableLines->subType.immediate.stagingAllocation.buffer) + drawableLines->subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer);
    Korl_Gfx_Color4u8*const old_colors    = drawableLines->subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].elementType != KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_INVALID
                                            ? KORL_C_CAST(Korl_Gfx_Color4u8*, KORL_C_CAST(u8*, drawableLines->subType.immediate.stagingAllocation.buffer) + drawableLines->subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR].byteOffsetBuffer)
                                            : NULL;
    drawableLines->subType.immediate.vertexStagingMeta = newVertexStagingMeta;// update the Drawable to use the new VertexStagingMeta
    for(u32 i = sortableVertexBindingsSize - 1; i < sortableVertexBindingsSize; i--)
    {
        switch(sortableVertexBindings[i].binding)
        {
        case KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION:{
            korl_memory_move(*o_positions, old_positions
                            ,  drawableLines->subType.immediate.vertexStagingMeta.vertexCount 
                             * drawableLines->subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[sortableVertexBindings[i].binding].byteStride);
            break;}
        case KORL_GFX_VERTEX_ATTRIBUTE_BINDING_COLOR:{
            if(o_colors && old_colors)// COLOR attribute is completely optional
                korl_memory_move(*o_colors, old_colors
                                ,  drawableLines->subType.immediate.vertexStagingMeta.vertexCount 
                                 * drawableLines->subType.immediate.vertexStagingMeta.vertexAttributeDescriptors[sortableVertexBindings[i].binding].byteStride);
            break;}
        default:
            korl_log(ERROR, "unsupported vertex binding: %u", sortableVertexBindings[i].binding);
        }
    }
}
korl_internal void korl_gfx_drawSphere(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, f32 radius, u32 latitudeSegments, u32 longitudeSegments, const Korl_Gfx_Material* material)
{
    const bool generateUvs = material && material->maps.resourceHandleTextureBase;
    Korl_Math_V3f32* positions;
    Korl_Math_V2f32* uvs       = NULL;
    Korl_Gfx_Drawable immediate = korl_gfx_immediateTriangles3d(korl_checkCast_u$_to_u32(korl_math_generateMeshSphereVertexCount(latitudeSegments, longitudeSegments) / 3), &positions, NULL);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    korl_math_generateMeshSphere(radius, latitudeSegments, longitudeSegments, positions, sizeof(*positions), generateUvs ? uvs : NULL, sizeof(*uvs));
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void _korl_gfx_drawRectangle(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_colors)
{
    korl_shared_const Korl_Math_V2f32 QUAD_POSITION_NORMALS_LOOP[4] = {{0,0}, {1,0}, {1,1}, {0,1}};
    const bool generateUvs = material && material->maps.resourceHandleTextureBase;
    Korl_Math_V2f32* positions;
    Korl_Math_V2f32* uvs       = NULL;
    Korl_Gfx_Drawable immediate = korl_gfx_immediateRectangle(anchorRatio, size, &positions, o_colors, generateUvs ? &uvs : NULL);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    korl_gfx_draw(&immediate, material, 1);
    if(materialOutline)
    {
        Korl_Math_V2f32* outlinePositions;
        Korl_Gfx_Drawable immediateOutline = outlineThickness == 0 
                                             ? korl_gfx_immediateLineStrip2d    (     korl_arraySize(QUAD_POSITION_NORMALS_LOOP) + 1 , &outlinePositions, NULL)
                                             : korl_gfx_immediateTriangleStrip2d(2 * (korl_arraySize(QUAD_POSITION_NORMALS_LOOP) + 1), &outlinePositions, NULL, NULL);
        immediateOutline.transform = korl_math_transform3d_rotateTranslate(versor, position);
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
        korl_gfx_draw(&immediateOutline, materialOutline, 1);
    }
}
korl_internal void korl_gfx_drawRectangle2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_colors)
{
    _korl_gfx_drawRectangle(KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, size, outlineThickness, material, materialOutline, o_colors);
}
korl_internal void korl_gfx_drawRectangle3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_colors)
{
    _korl_gfx_drawRectangle(position, versor, anchorRatio, size, outlineThickness, material, materialOutline, o_colors);
}
korl_internal void _korl_gfx_drawCircle(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, f32 radius, u32 circumferenceVertices, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_colors)
{
    const bool generateUvs = material && material->maps.resourceHandleTextureBase;
    Korl_Math_V2f32* positions;
    Korl_Math_V2f32* uvs       = NULL;
    Korl_Gfx_Drawable immediate = korl_gfx_immediateCircle(anchorRatio, radius, circumferenceVertices, &positions, o_colors, generateUvs ? &uvs : NULL);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    korl_gfx_draw(&immediate, material, 1);
    if(materialOutline)
    {
        Korl_Math_V2f32* outlinePositions;
        Korl_Gfx_Drawable immediateOutline = outlineThickness == 0 
                                             ? korl_gfx_immediateLineStrip2d    (     circumferenceVertices + 1 , &outlinePositions, NULL)
                                             : korl_gfx_immediateTriangleStrip2d(2 * (circumferenceVertices + 1), &outlinePositions, NULL, NULL);
        immediateOutline.transform = immediate.transform;
        const f32 radiansPerVertex = KORL_TAU32 / KORL_C_CAST(f32, circumferenceVertices);
        for(u32 v = 0; v < circumferenceVertices + 1; v++)
        {
            const u32             vMod    = v % circumferenceVertices;
            const f32             radians = KORL_C_CAST(f32, vMod) * radiansPerVertex;
            const Korl_Math_V2f32 spoke   = korl_math_v2f32_fromRotationZ(1, radians);
            if(outlineThickness == 0)
                outlinePositions[v] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){radius + radius * spoke.x - anchorRatio.x * 2 * radius
                                                                             ,radius + radius * spoke.y - anchorRatio.y * 2 * radius};
            else
            {
                outlinePositions[2 * v + 0] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){radius +   radius                     * spoke.x  - anchorRatio.x * 2 * radius
                                                                                     ,radius +   radius                     * spoke.y  - anchorRatio.y * 2 * radius};
                outlinePositions[2 * v + 1] = KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){radius + ((radius + outlineThickness) * spoke.x) - anchorRatio.x * 2 * radius
                                                                                     ,radius + ((radius + outlineThickness) * spoke.y) - anchorRatio.y * 2 * radius};
            }
        }
        korl_gfx_draw(&immediateOutline, materialOutline, 1);
    }
}
korl_internal void korl_gfx_drawCircle2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, f32 radius, u32 circumferenceVertices, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_colors)
{
    _korl_gfx_drawCircle(KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, radius, circumferenceVertices, outlineThickness, material, materialOutline, o_colors);
}
korl_internal void korl_gfx_drawLines2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    Korl_Gfx_Drawable immediate = korl_gfx_immediateLines2d(lineCount, o_positions, o_colors);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawLines3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    Korl_Gfx_Drawable immediate = korl_gfx_immediateLines3d(lineCount, o_positions, o_colors);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawLineStrip2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    korl_assert(vertexCount >= 2);
    Korl_Gfx_Drawable immediate = korl_gfx_immediateLineStrip2d(vertexCount, o_positions, o_colors);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawTriangles2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 triangleCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    Korl_Gfx_Drawable immediate = korl_gfx_immediateTriangles2d(triangleCount, o_positions, o_colors);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawTriangleFan2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    korl_assert(vertexCount >= 3);
    Korl_Gfx_Drawable immediate = _korl_gfx_immediate2d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_FAN, vertexCount, o_positions, o_colors, NULL);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position});
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void korl_gfx_drawTriangleFan3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors)
{
    korl_assert(vertexCount >= 3);
    Korl_Gfx_Drawable immediate = _korl_gfx_immediate3d(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLE_FAN, vertexCount, o_positions, o_colors);
    immediate.transform = korl_math_transform3d_rotateTranslate(versor, position);
    korl_gfx_draw(&immediate, material, 1);
}
korl_internal void _korl_gfx_drawUtf(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, const void* utfText, const u8 utfTextEncoding, u$ utfTextSize, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, bool enableDepthTest)
{
    Korl_Gfx_Material materialOverride;// the base color map of the material will always be set to the translucency mask texture containing the baked rasterized glyphs, so we need to override the material
    if(material)
    {
        materialOverride = *material;
        materialOverride.modes.primitiveType = KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES;
    }
    else
        materialOverride = korl_gfx_material_defaultUnlit(KORL_GFX_MATERIAL_PRIMITIVE_TYPE_TRIANGLES, KORL_GFX_MATERIAL_MODE_FLAGS_NONE, korl_gfx_color_toLinear(KORL_COLOR4U8_WHITE));
    materialOverride.modes.flags |= KORL_GFX_MATERIAL_MODE_FLAG_ENABLE_BLEND;// _all_ text drawn this way _must_ be translucent
    /* determine how many glyphs need to be drawn from utf8Text, as well as the 
        AABB size of the text so we can calculate position offset based on anchorRatio */
    Korl_Gfx_Font_TextMetrics textMetrics;
    switch(utfTextEncoding)
    {
    case  8: textMetrics = korl_gfx_font_getUtf8Metrics (utf16FontAssetName, textPixelHeight, KORL_STRUCT_INITIALIZE(acu8 ){.size = utfTextSize, .data = KORL_C_CAST(const u8* , utfText)}); break;
    case 16: textMetrics = korl_gfx_font_getUtf16Metrics(utf16FontAssetName, textPixelHeight, KORL_STRUCT_INITIALIZE(acu16){.size = utfTextSize, .data = KORL_C_CAST(const u16*, utfText)}); break;
    default:
        textMetrics = KORL_STRUCT_INITIALIZE_ZERO(Korl_Gfx_Font_TextMetrics);
        korl_log(ERROR, "unsupported UTF encoding: %hhu", utfTextEncoding);
        break;
    }
    /* configure the renderer draw state */
    const Korl_Gfx_Font_Resources fontResources = korl_gfx_font_getResources(utf16FontAssetName, textPixelHeight);
    materialOverride.maps.resourceHandleTextureBase = fontResources.resourceHandleTexture;
    if(!materialOverride.shaders.resourceHandleShaderVertex)
        materialOverride.shaders.resourceHandleShaderVertex = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-text.vert.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY);
    if(!materialOverride.shaders.resourceHandleShaderFragment)
        materialOverride.shaders.resourceHandleShaderFragment = korl_gfx_getBuiltInShaderFragment(&materialOverride);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_PushConstantData, pushConstantData);
    *KORL_C_CAST(Korl_Math_M4f32*, pushConstantData.vertex) = korl_math_makeM4f32_rotateTranslate(versor, position);
    KORL_ZERO_STACK(Korl_Gfx_DrawState_StorageBuffers, storageBuffers);
    storageBuffers.resourceHandleVertex = fontResources.resourceHandleSsboGlyphMeshVertices;
    KORL_ZERO_STACK(Korl_Gfx_DrawState, drawState);
    drawState.pushConstantData = &pushConstantData;
    drawState.material         = &materialOverride;
    drawState.storageBuffers   = &storageBuffers;
    korl_gfx_setDrawState(&drawState);
    /* allocate staging memory & issue draw command */
    KORL_ZERO_STACK(Korl_Gfx_VertexStagingMeta, stagingMeta);
    u32 byteOffsetBuffer = 0;
    stagingMeta.indexCount            = korl_arraySize(KORL_GFX_TRI_QUAD_INDICES);
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
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].byteOffsetBuffer = byteOffsetBuffer;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].byteStride       = sizeof(u32);
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].elementType      = KORL_GFX_VERTEX_ATTRIBUTE_ELEMENT_TYPE_U32;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].inputRate        = KORL_GFX_VERTEX_ATTRIBUTE_INPUT_RATE_INSTANCE;
    stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].vectorSize       = 1;
    byteOffsetBuffer += stagingMeta.instanceCount * stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0].byteStride;
    Korl_Gfx_StagingAllocation stagingAllocation = korl_gfx_stagingAllocate(&stagingMeta);
    korl_gfx_drawStagingAllocation(&stagingAllocation, &stagingMeta);
    /* generate the text vertex data */
    u16*const indices = KORL_C_CAST(u16*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.indexByteOffsetBuffer);
    korl_memory_copy(indices, KORL_GFX_TRI_QUAD_INDICES, sizeof(KORL_GFX_TRI_QUAD_INDICES));
    switch(utfTextEncoding)
    {
    case  8:
        korl_gfx_font_generateUtf8(utf16FontAssetName, textPixelHeight, KORL_STRUCT_INITIALIZE(acu8){.size = utfTextSize, .data = KORL_C_CAST(const u8*, utfText)}
                                  ,KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){-anchorRatio.x * textMetrics.aabbSize.x
                                                                          ,-anchorRatio.y * textMetrics.aabbSize.y + textMetrics.aabbSize.y}
                                  ,KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer)
                                  ,KORL_C_CAST(u32*            , KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0    ].byteOffsetBuffer));
        break;
    case 16:
        korl_gfx_font_generateUtf16(utf16FontAssetName, textPixelHeight, KORL_STRUCT_INITIALIZE(acu16){.size = utfTextSize, .data = KORL_C_CAST(const u16*, utfText)}
                                   ,KORL_STRUCT_INITIALIZE(Korl_Math_V2f32){-anchorRatio.x * textMetrics.aabbSize.x
                                                                           ,-anchorRatio.y * textMetrics.aabbSize.y + textMetrics.aabbSize.y}
                                   ,KORL_C_CAST(Korl_Math_V2f32*, KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_POSITION].byteOffsetBuffer)
                                   ,KORL_C_CAST(u32*            , KORL_C_CAST(u8*, stagingAllocation.buffer) + stagingMeta.vertexAttributeDescriptors[KORL_GFX_VERTEX_ATTRIBUTE_BINDING_EXTRA_0    ].byteOffsetBuffer));
        break;
    }
}
korl_internal void korl_gfx_drawUtf82d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline)
{
    _korl_gfx_drawUtf(KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, utf8Text.data, 8, utf8Text.size, utf16FontAssetName, textPixelHeight, outlineSize, material, materialOutline, false/* if the user is drawing 2D geometry, they most likely don't care about depth write/test, but we probably want a way to set this anyway*/);
}
korl_internal void korl_gfx_drawUtf83d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline)
{
    _korl_gfx_drawUtf(position, versor, anchorRatio, utf8Text.data, 8, utf8Text.size, utf16FontAssetName, textPixelHeight, outlineSize, material, materialOutline, true);
}
korl_internal void korl_gfx_drawUtf162d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline)
{
    _korl_gfx_drawUtf(KORL_STRUCT_INITIALIZE(Korl_Math_V3f32){.xy = position}, versor, anchorRatio, utf16Text.data, 16, utf16Text.size, utf16FontAssetName, textPixelHeight, outlineSize, material, materialOutline, false/* if the user is drawing 2D geometry, they most likely don't care about depth write/test, but we probably want a way to set this anyway*/);
}
korl_internal void korl_gfx_drawUtf163d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline)
{
    _korl_gfx_drawUtf(position, versor, anchorRatio, utf16Text.data, 16, utf16Text.size, utf16FontAssetName, textPixelHeight, outlineSize, material, materialOutline, true);
}
korl_internal void korl_gfx_drawMesh(Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, const Korl_Gfx_Material *materials, u8 materialsSize)
{
    Korl_Gfx_Drawable mesh = korl_gfx_mesh(resourceHandleScene3d, utf8MeshName);
    mesh.transform = korl_math_transform3d_rotateScaleTranslate(versor, scale, position);
    korl_gfx_draw(&mesh, materials, materialsSize);
}
