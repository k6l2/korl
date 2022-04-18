#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
#include "korl-vulkan.h"
#include "korl-interface-platform.h"
typedef struct Korl_Gfx_Batch
{
    Korl_Vulkan_PrimitiveType primitiveType;
    Korl_Vulkan_Position _position;
    Korl_Vulkan_Position _scale;
    Korl_Math_Quaternion _rotation;
    u32 _vertexIndexCount;
    u32 _vertexCount;
    f32 _textPixelHeight;
    wchar_t* _assetNameTexture;
    //KORL-PERFORMANCE-000-000-002
    wchar_t* _assetNameFont;
    //KORL-PERFORMANCE-000-000-003
    wchar_t* _text;
    Korl_Vulkan_TextureHandle _fontTextureHandle;
    Korl_Vulkan_VertexIndex* _vertexIndices;
    Korl_Vulkan_Position*    _vertexPositions;
    Korl_Vulkan_Color*       _vertexColors;
    Korl_Vulkan_Uv*          _vertexUvs;
} Korl_Gfx_Batch;
korl_internal void korl_gfx_initialize(void);
korl_internal Korl_Gfx_Camera korl_gfx_createCameraFov(f32 fovHorizonDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 target);
korl_internal KORL_GFX_CREATE_CAMERA_ORTHO(korl_gfx_createCameraOrtho);
korl_internal Korl_Gfx_Camera korl_gfx_createCameraOrthoFixedHeight(f32 fixedHeight, f32 clipDepth);
korl_internal void korl_gfx_cameraFov_rotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians);
korl_internal KORL_GFX_USE_CAMERA(korl_gfx_useCamera);
/** Note that scissor coordinates use swap chain coordinates, where the origin 
 * is the upper-left corner of the swap chain, & {+X,+Y} points to the 
 * bottom-right.  */
//KORL-ISSUE-000-000-004
korl_internal void korl_gfx_cameraSetScissor(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY);
/** 
 * Note that scissor coordinates use swap chain coordinates, where the origin is 
 * the upper-left corner of the swap chain, & {+X,+Y} points to the bottom-right.  */
korl_internal void korl_gfx_cameraSetScissorPercent(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight);
/** Translate the camera such that the orthographic camera's origin is located 
 * at the position in the swapchain's coordinate space relative to the 
 * bottom-left corner.
 * Negative size ratio values are valid.
 * This function does NOT flip the coordinate space or anything like that!
 * Examples:
 * - by default (without calling this function) the origin will be 
 *   \c {0.5f,0.5f}, which is the center of the screen
 * - if you want the origin to be in the bottom-left corner of the window, pass 
 *   \c {0.f,0.f} as size ratio coordinates */
korl_internal void korl_gfx_cameraOrthoSetOriginAnchor(Korl_Gfx_Camera*const context, f32 swapchainSizeRatioOriginX, f32 swapchainSizeRatioOriginY);
korl_internal KORL_GFX_BATCH(korl_gfx_batch);
//KORL-ISSUE-000-000-005
korl_internal KORL_GFX_CREATE_BATCH_RECTANGLE_TEXTURED(korl_gfx_createBatchRectangleTextured);
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchRectangleColored(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, Korl_Math_V2f32 localOriginNormal, Korl_Vulkan_Color color);
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchLines(Korl_Memory_AllocatorHandle allocatorHandle, u32 lineCount);
/** 
 * \param assetNameFont If this value is \c NULL , a default internal font asset 
 * will be used.
 */
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchText(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* assetNameFont, const wchar_t* text, f32 textPixelHeight);
korl_internal void korl_gfx_batchSetPosition(Korl_Gfx_Batch*const context, Korl_Vulkan_Position position);
korl_internal void korl_gfx_batchSetPosition2d(Korl_Gfx_Batch*const context, f32 x, f32 y);
korl_internal void korl_gfx_batchSetPosition2dV2f32(Korl_Gfx_Batch*const context, Korl_Math_V2f32 position);
korl_internal void korl_gfx_batchSetScale(Korl_Gfx_Batch*const context, Korl_Vulkan_Position scale);
korl_internal void korl_gfx_batchSetRotation(Korl_Gfx_Batch*const context, Korl_Math_V3f32 axisOfRotation, f32 radians);
korl_internal void korl_gfx_batchSetVertexColor(Korl_Gfx_Batch*const context, u32 vertexIndex, Korl_Vulkan_Color color);
korl_internal void korl_gfx_batchSetLine(Korl_Gfx_Batch*const context, u32 lineIndex, Korl_Vulkan_Position p0, Korl_Vulkan_Position p1, Korl_Vulkan_Color color);
korl_internal f32 korl_gfx_batchTextGetAabbSizeX(Korl_Gfx_Batch*const context);
korl_internal f32 korl_gfx_batchTextGetAabbSizeY(Korl_Gfx_Batch*const context);
korl_internal void korl_gfx_batchRectangleSetSize(Korl_Gfx_Batch*const context, Korl_Math_V2f32 size);
korl_internal void korl_gfx_batchRectangleSetColor(Korl_Gfx_Batch*const context, Korl_Vulkan_Color color);
