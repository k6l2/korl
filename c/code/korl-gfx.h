#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
#include "korl-vulkan.h"
#include "korl-interface-platform.h"
korl_internal void korl_gfx_initialize(void);
korl_internal KORL_PLATFORM_GFX_CREATE_CAMERA_FOV(korl_gfx_createCameraFov);
korl_internal KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO(korl_gfx_createCameraOrtho);
korl_internal KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO_FIXED_HEIGHT(korl_gfx_createCameraOrthoFixedHeight);
korl_internal KORL_PLATFORM_GFX_CAMERA_FOV_ROTATE_AROUND_TARGET(korl_gfx_cameraFov_rotateAroundTarget);
korl_internal KORL_PLATFORM_GFX_USE_CAMERA(korl_gfx_useCamera);
/** Note that scissor coordinates use swap chain coordinates, where the origin 
 * is the upper-left corner of the swap chain, & {+X,+Y} points to the 
 * bottom-right.  */
//KORL-ISSUE-000-000-004
korl_internal KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR(korl_gfx_cameraSetScissor);
/** 
 * Note that scissor coordinates use swap chain coordinates, where the origin is 
 * the upper-left corner of the swap chain, & {+X,+Y} points to the bottom-right.  */
korl_internal KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR_PERCENT(korl_gfx_cameraSetScissorPercent);
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
korl_internal KORL_PLATFORM_GFX_CAMERA_ORTHO_SET_ORIGIN_ANCHOR(korl_gfx_cameraOrthoSetOriginAnchor);
korl_internal KORL_PLATFORM_GFX_BATCH(korl_gfx_batch);
//KORL-ISSUE-000-000-005
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_TEXTURED(korl_gfx_createBatchRectangleTextured);
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_COLORED(korl_gfx_createBatchRectangleColored);
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_TRIANGLES(korl_gfx_createBatchTriangles);
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_LINES(korl_gfx_createBatchLines);
/** 
 * \param assetNameFont If this value is \c NULL , a default internal font asset 
 * will be used.
 */
korl_internal KORL_PLATFORM_GFX_CREATE_BATCH_TEXT(korl_gfx_createBatchText);
korl_internal KORL_PLATFORM_GFX_BATCH_SET_POSITION(korl_gfx_batchSetPosition);
korl_internal KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D(korl_gfx_batchSetPosition2d);
korl_internal KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D_V2F32(korl_gfx_batchSetPosition2dV2f32);
korl_internal KORL_PLATFORM_GFX_BATCH_SET_SCALE(korl_gfx_batchSetScale);
korl_internal KORL_PLATFORM_GFX_BATCH_SET_QUATERNION(korl_gfx_batchSetQuaternion);
korl_internal KORL_PLATFORM_GFX_BATCH_SET_ROTATION(korl_gfx_batchSetRotation);
korl_internal KORL_PLATFORM_GFX_BATCH_SET_VERTEX_COLOR(korl_gfx_batchSetVertexColor);
korl_internal KORL_PLATFORM_GFX_BATCH_ADD_LINE(korl_gfx_batchAddLine);
korl_internal KORL_PLATFORM_GFX_BATCH_SET_LINE(korl_gfx_batchSetLine);
korl_internal KORL_PLATFORM_GFX_BATCH_TEXT_GET_AABB_SIZE_X(korl_gfx_batchTextGetAabbSizeX);
korl_internal KORL_PLATFORM_GFX_BATCH_TEXT_GET_AABB_SIZE_Y(korl_gfx_batchTextGetAabbSizeY);
korl_internal KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_SIZE(korl_gfx_batchRectangleSetSize);
korl_internal KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_COLOR(korl_gfx_batchRectangleSetColor);
