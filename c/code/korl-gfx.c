#include "korl-gfx.h"
#include "korl-io.h"
korl_internal Korl_Gfx_Camera korl_gfx_createCameraFov(f32 fovHorizonDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 target)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                    = KORL_GFX_CAMERA_TYPE_PERSPECTIVE;
    result.position                                = position;
    result.target                                  = target;
    result.subCamera.perspective.clipNear          = clipNear;
    result.subCamera.perspective.clipFar           = clipFar;
    result.subCamera.perspective.fovHorizonDegrees = fovHorizonDegrees;
    return result;
}
korl_internal Korl_Gfx_Camera korl_gfx_createCameraOrthoFixedHeight(f32 fixedHeight, f32 clipDepth)
{
    KORL_ZERO_STACK(Korl_Gfx_Camera, result);
    result.type                                          = KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC;
    result.position                                      = KORL_MATH_V3F32_ZERO;
    result.target                                        = korl_math_v3f32_multiplyScalar(KORL_MATH_V3F32_Z, -1);
    result.subCamera.orthographicFixedHeight.fixedHeight = fixedHeight;
    result.subCamera.orthographicFixedHeight.clipDepth   = clipDepth;
    return result;
}
korl_internal void korl_gfx_cameraFov_rotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians)
{
    korl_assert(context->type == KORL_GFX_CAMERA_TYPE_PERSPECTIVE);
    const Korl_Math_V3f32 newTargetOffset = 
        korl_math_quaternion_transformV3f32(
            korl_math_quaternion_fromAxisRadians(axisOfRotation, radians, false), 
            korl_math_v3f32_subtract(context->position, &context->target), 
            true);
    context->position = korl_math_v3f32_add(context->target, newTargetOffset);
}
korl_internal void korl_gfx_useCamera(const Korl_Gfx_Camera*const camera)
{
    switch(camera->type)
    {
    case KORL_GFX_CAMERA_TYPE_PERSPECTIVE:{
        korl_vulkan_setProjectionFov(camera->subCamera.perspective.fovHorizonDegrees, camera->subCamera.perspective.clipNear, camera->subCamera.perspective.clipFar);
        korl_vulkan_setView(camera->position, camera->target, KORL_MATH_V3F32_Z);
        }break;
    case KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC:{
        korl_vulkan_setProjectionOrthographicFixedHeight(camera->subCamera.orthographicFixedHeight.fixedHeight, camera->subCamera.orthographicFixedHeight.clipDepth);
        korl_vulkan_setView(camera->position, camera->target, KORL_MATH_V3F32_Y);
        }break;
    }
}
korl_internal void korl_gfx_batch(const Korl_Gfx_Batch*const batch, Korl_Gfx_Batch_Flags flags)
{
    if(batch->_assetNameTexture)
        korl_vulkan_useImageAssetAsTexture(batch->_assetNameTexture);
    korl_vulkan_setModel(batch->_position, batch->_rotation, batch->_scale);
    korl_vulkan_batchSetUseDepthTestAndWriteDepthBuffer(!(flags & KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST));
    korl_vulkan_batch(batch->primitiveType, 
        batch->_vertexIndexCount, batch->_vertexIndices, 
        batch->_vertexCount, batch->_vertexPositions, batch->_vertexColors, batch->_vertexUvs);
}
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchRectangleTextured(Korl_Memory_Allocator allocator, Korl_Math_V2f32 size, const wchar_t* assetNameTexture)
{
    /* calculate required amount of memory for the batch */
    const u$ assetNameTextureSize = korl_memory_stringSize(assetNameTexture) + 1;
    const u$ assetNameTextureBytes = assetNameTextureSize * sizeof(wchar_t);
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + assetNameTextureBytes
        + 6 * sizeof(Korl_Vulkan_VertexIndex)
        + 4 * sizeof(Korl_Vulkan_Position)
        + 4 * sizeof(Korl_Vulkan_Uv);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        allocator.callbackAllocate(allocator.userData, totalBytes, __FILEW__, __LINE__));
    /* initialize the batch struct */
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount = 6;
    result->_vertexCount      = 4;
    result->_assetNameTexture = KORL_C_CAST(wchar_t*, result + 1);
    result->_vertexIndices    = KORL_C_CAST(Korl_Vulkan_VertexIndex*, KORL_C_CAST(u8*, result->_assetNameTexture) + assetNameTextureBytes);
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*   , KORL_C_CAST(u8*, result->_vertexIndices   ) + 6*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexUvs        = KORL_C_CAST(Korl_Vulkan_Uv*         , KORL_C_CAST(u8*, result->_vertexPositions ) + 4*sizeof(Korl_Vulkan_Position));
    /* initialize the batch's dynamic data */
    if(korl_memory_stringCopy(assetNameTexture, result->_assetNameTexture, assetNameTextureSize) != korl_checkCast_u$_to_i$(assetNameTextureSize))
        korl_log(ERROR, "failed to copy asset name \"%s\" to batch", assetNameTexture);
    korl_shared_const Korl_Vulkan_VertexIndex vertexIndices[] = 
        { 0, 1, 3
        , 1, 2, 3 };
    memcpy(result->_vertexIndices, vertexIndices, sizeof(vertexIndices));
    korl_shared_const Korl_Vulkan_Position vertexPositions[] = 
        { {-0.5f, -0.5f, 0.f}
        , { 0.5f, -0.5f, 0.f}
        , { 0.5f,  0.5f, 0.f}
        , {-0.5f,  0.5f, 0.f} };
    memcpy(result->_vertexPositions, vertexPositions, sizeof(vertexPositions));
    // scale the rectangle mesh vertices by the provided size //
    for(u$ i = 0; i < korl_arraySize(vertexPositions); ++i)
    {
        result->_vertexPositions[i].xyz.x *= size.xy.x;
        result->_vertexPositions[i].xyz.y *= size.xy.y;
    }
    korl_shared_const Korl_Vulkan_Uv vertexTextureUvs[] = 
        { {0, 1}
        , {1, 1}
        , {1, 0}
        , {0, 0} };
    memcpy(result->_vertexUvs, vertexTextureUvs, sizeof(vertexTextureUvs));
    /* return the batch */
    return result;
}
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchRectangleColored(Korl_Memory_Allocator allocator, Korl_Math_V2f32 size, Korl_Vulkan_Color color)
{
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 6 * sizeof(Korl_Vulkan_VertexIndex)
        + 4 * sizeof(Korl_Vulkan_Position)
        + 4 * sizeof(Korl_Vulkan_Color);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        allocator.callbackAllocate(allocator.userData, totalBytes, __FILEW__, __LINE__));
    /* initialize the batch struct */
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_TRIANGLES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexIndexCount = 6;
    result->_vertexCount      = 4;
    result->_vertexIndices    = KORL_C_CAST(Korl_Vulkan_VertexIndex*, result + 1);
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*   , KORL_C_CAST(u8*, result->_vertexIndices   ) + 6*sizeof(Korl_Vulkan_VertexIndex));
    result->_vertexColors     = KORL_C_CAST(Korl_Vulkan_Color*      , KORL_C_CAST(u8*, result->_vertexPositions ) + 4*sizeof(Korl_Vulkan_Position));
    /* initialize the batch's dynamic data */
    korl_shared_const Korl_Vulkan_VertexIndex vertexIndices[] = 
        { 0, 1, 3
        , 1, 2, 3 };
    memcpy(result->_vertexIndices, vertexIndices, sizeof(vertexIndices));
    korl_shared_const Korl_Vulkan_Position vertexPositions[] = 
        { {-0.5f, -0.5f, 0.f}
        , { 0.5f, -0.5f, 0.f}
        , { 0.5f,  0.5f, 0.f}
        , {-0.5f,  0.5f, 0.f} };
    memcpy(result->_vertexPositions, vertexPositions, sizeof(vertexPositions));
    // scale the rectangle mesh vertices by the provided size //
    for(u$ i = 0; i < korl_arraySize(vertexPositions); ++i)
    {
        result->_vertexPositions[i].xyz.x *= size.xy.x;
        result->_vertexPositions[i].xyz.y *= size.xy.y;
    }
    for(u$ c = 0; c < result->_vertexCount; c++)
        result->_vertexColors[c] = color;
    /* return the batch */
    return result;
}
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchLines(Korl_Memory_Allocator allocator, u32 lineCount)
{
    /* calculate required amount of memory for the batch */
    const u$ totalBytes = sizeof(Korl_Gfx_Batch)
        + 2 * lineCount * sizeof(Korl_Vulkan_Position)
        + 2 * lineCount * sizeof(Korl_Vulkan_Color);
    /* allocate the memory */
    Korl_Gfx_Batch*const result = KORL_C_CAST(Korl_Gfx_Batch*, 
        allocator.callbackAllocate(allocator.userData, totalBytes, __FILEW__, __LINE__));
    /* initialize the batch struct */
    result->primitiveType     = KORL_VULKAN_PRIMITIVETYPE_LINES;
    result->_scale            = (Korl_Vulkan_Position){1.f, 1.f, 1.f};
    result->_rotation         = KORL_MATH_QUATERNION_IDENTITY;
    result->_vertexCount      = 2 * lineCount;
    result->_vertexPositions  = KORL_C_CAST(Korl_Vulkan_Position*, result + 1);
    result->_vertexColors     = KORL_C_CAST(Korl_Vulkan_Color*   , KORL_C_CAST(u8*, result->_vertexPositions ) + 2*lineCount*sizeof(Korl_Vulkan_Position));
    /* return the batch */
    return result;
}
korl_internal void korl_gfx_batchSetPosition(Korl_Gfx_Batch*const context, Korl_Vulkan_Position position)
{
    context->_position = position;
}
korl_internal void korl_gfx_batchSetScale(Korl_Gfx_Batch*const context, Korl_Vulkan_Position scale)
{
    context->_scale = scale;
}
korl_internal void korl_gfx_batchSetVertexColor(Korl_Gfx_Batch*const context, u32 vertexIndex, Korl_Vulkan_Color color)
{
    korl_assert(context->_vertexCount > vertexIndex);
    korl_assert(context->_vertexColors);
    context->_vertexColors[vertexIndex] = color;
}
korl_internal void korl_gfx_batchSetLine(Korl_Gfx_Batch*const context, u32 lineIndex, Korl_Vulkan_Position p0, Korl_Vulkan_Position p1, Korl_Vulkan_Color color)
{
    korl_assert(context->_vertexCount > 2*lineIndex + 1);
    korl_assert(context->_vertexColors);
    context->_vertexPositions[2*lineIndex + 0] = p0;
    context->_vertexPositions[2*lineIndex + 1] = p1;
    context->_vertexColors[2*lineIndex + 0] = color;
    context->_vertexColors[2*lineIndex + 1] = color;
}
