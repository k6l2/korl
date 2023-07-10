#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-gfx.h"
typedef struct Korl_Gfx_Immediate
{
    Korl_Gfx_PrimitiveType     primitiveType;
    Korl_Gfx_VertexStagingMeta vertexStagingMeta;
    Korl_Gfx_StagingAllocation stagingAllocation;
} Korl_Gfx_Immediate;
korl_internal u8                 korl_gfx_indexBytes(Korl_Gfx_VertexIndexType vertexIndexType);
korl_internal Korl_Math_V4f32    korl_gfx_color_toLinear(Korl_Vulkan_Color4u8 color);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultUnlit(Korl_Math_V4f32 colorLinear4Base);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultLit(void);
korl_internal Korl_Gfx_Camera    korl_gfx_camera_createFov(f32 fovVerticalDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 normalForward, Korl_Math_V3f32 normalUp);
korl_internal Korl_Gfx_Camera    korl_gfx_camera_createOrtho(f32 clipDepth);
korl_internal Korl_Gfx_Camera    korl_gfx_camera_createOrthoFixedHeight(f32 fixedHeight, f32 clipDepth);
korl_internal void               korl_gfx_camera_fovRotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians, Korl_Math_V3f32 target);
/** Note that scissor coordinates use swap chain coordinates, where the origin 
 * is the upper-left corner of the swap chain, & {+X,+Y} points to the 
 * bottom-right.  */
//KORL-ISSUE-000-000-004
korl_internal void               korl_gfx_camera_setScissor(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY);
/** Note that scissor coordinates use swap chain coordinates, where the origin is 
 * the upper-left corner of the swap chain, & {+X,+Y} points to the bottom-right.  */
korl_internal void               korl_gfx_camera_setScissorPercent(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight);
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
korl_internal void               korl_gfx_camera_orthoSetOriginAnchor(Korl_Gfx_Camera*const context, f32 swapchainSizeRatioOriginX, f32 swapchainSizeRatioOriginY);
korl_internal Korl_Math_M4f32    korl_gfx_camera_projection(const Korl_Gfx_Camera*const context, Korl_Math_V2u32 surfaceSize);
korl_internal Korl_Math_M4f32    korl_gfx_camera_view(const Korl_Gfx_Camera*const context);
korl_internal void               korl_gfx_camera_drawFrustum(const Korl_Gfx_Camera*const context, Korl_Math_V2u32 surfaceSize, Korl_Memory_AllocatorHandle allocator);
korl_internal void               korl_gfx_drawable_mesh_initialize(Korl_Gfx_Drawable*const context, Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName);
//@TODO: deprecate/delete
korl_internal void               korl_gfx_draw3dArrow(Korl_Gfx_Drawable meshCone, Korl_Gfx_Drawable meshCylinder, Korl_Math_V3f32 localStart, Korl_Math_V3f32 localEnd, f32 localRadius, const Korl_Math_M4f32*const baseTransform);
//@TODO: deprecate/delete
korl_internal void               korl_gfx_draw3dLine(const Korl_Math_V3f32 points[2], const Korl_Vulkan_Color4u8 colors[2], Korl_Memory_AllocatorHandle allocator);
//@TODO: deprecate/delete
korl_internal void               korl_gfx_drawAabb3(const Korl_Math_Aabb3f32*const aabb, Korl_Vulkan_Color4u8 color, Korl_Memory_AllocatorHandle allocator);
korl_internal Korl_Gfx_Immediate korl_gfx_immediateLines2d(u32 lineCount, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors);
korl_internal Korl_Gfx_Immediate korl_gfx_immediateLines3d(u32 lineCount, Korl_Math_V3f32** o_positions, Korl_Vulkan_Color4u8** o_colors);
korl_internal Korl_Gfx_Immediate korl_gfx_immediateAxisNormalLines(void);
korl_internal void               korl_gfx_drawImmediate(const Korl_Gfx_Immediate* immediate, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, const Korl_Gfx_Material* material);
korl_internal void               korl_gfx_drawSphere(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, f32 radius, u32 latitudeSegments, u32 longitudeSegments, const Korl_Gfx_Material* material);
korl_internal void               korl_gfx_drawRectangle2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Vulkan_Color4u8** o_colors);
korl_internal void               korl_gfx_drawRectangle3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Vulkan_Color4u8** o_colors);
korl_internal void               korl_gfx_drawLines2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors);
korl_internal void               korl_gfx_drawTriangles2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 triangleCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Vulkan_Color4u8** o_colors);
korl_internal void               korl_gfx_drawText2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline);
korl_internal void               korl_gfx_drawText3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline);
