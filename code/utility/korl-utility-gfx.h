#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-gfx.h"
korl_internal Korl_Gfx_Camera korl_gfx_camera_createFov(f32 fovVerticalDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 normalForward, Korl_Math_V3f32 normalUp);
korl_internal Korl_Gfx_Camera korl_gfx_camera_createOrtho(f32 clipDepth);
korl_internal Korl_Gfx_Camera korl_gfx_camera_createOrthoFixedHeight(f32 fixedHeight, f32 clipDepth);
korl_internal void            korl_gfx_camera_fovRotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians, Korl_Math_V3f32 target);
/** Note that scissor coordinates use swap chain coordinates, where the origin 
 * is the upper-left corner of the swap chain, & {+X,+Y} points to the 
 * bottom-right.  */
//KORL-ISSUE-000-000-004
korl_internal void            korl_gfx_camera_setScissor(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY);
/** Note that scissor coordinates use swap chain coordinates, where the origin is 
 * the upper-left corner of the swap chain, & {+X,+Y} points to the bottom-right.  */
korl_internal void            korl_gfx_camera_setScissorPercent(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight);
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
korl_internal void            korl_gfx_camera_orthoSetOriginAnchor(Korl_Gfx_Camera*const context, f32 swapchainSizeRatioOriginX, f32 swapchainSizeRatioOriginY);
korl_internal Korl_Math_M4f32 korl_gfx_camera_projection(const Korl_Gfx_Camera*const context, Korl_Math_V2u32 surfaceSize);
korl_internal Korl_Math_M4f32 korl_gfx_camera_view(const Korl_Gfx_Camera*const context);
korl_internal void            korl_gfx_camera_drawFrustum(const Korl_Gfx_Camera*const context, Korl_Math_V2u32 surfaceSize, Korl_Memory_AllocatorHandle allocator);
korl_internal void korl_gfx_drawable_mesh_initialize(Korl_Gfx_Drawable*const context, Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName);
korl_internal void korl_gfx_draw3dArrow(Korl_Gfx_Drawable meshCone, Korl_Gfx_Drawable meshCylinder, Korl_Math_V3f32 localStart, Korl_Math_V3f32 localEnd, f32 localRadius, const Korl_Math_M4f32*const baseTransform);
