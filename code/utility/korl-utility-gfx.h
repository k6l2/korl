#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-gfx.h"
korl_internal u8                 korl_gfx_indexBytes(Korl_Gfx_VertexIndexType vertexIndexType);
korl_internal Korl_Math_V4f32    korl_gfx_color_toLinear(Korl_Gfx_Color4u8 color);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultUnlit(Korl_Gfx_Material_PrimitiveType primitiveType, Korl_Gfx_Material_Mode_Flags flags, Korl_Math_V4f32 colorLinear4Base);
korl_internal Korl_Gfx_Material  korl_gfx_material_defaultLit(Korl_Gfx_Material_PrimitiveType primitiveType, Korl_Gfx_Material_Mode_Flags flags);
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
korl_internal Korl_Math_M4f32    korl_gfx_camera_projection(const Korl_Gfx_Camera*const context);
korl_internal Korl_Math_M4f32    korl_gfx_camera_view(const Korl_Gfx_Camera*const context);
korl_internal void               korl_gfx_camera_drawFrustum(const Korl_Gfx_Camera*const context, const Korl_Gfx_Material* material);
korl_internal Korl_Gfx_Drawable  korl_gfx_immediateLines2d(u32 lineCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal Korl_Gfx_Drawable  korl_gfx_immediateLines3d(u32 lineCount, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal Korl_Gfx_Drawable  korl_gfx_immediateLineStrip2d(u32 vertexCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal Korl_Gfx_Drawable  korl_gfx_immediateTriangles2d(u32 triangleCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal Korl_Gfx_Drawable  korl_gfx_immediateTriangles3d(u32 triangleCount, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal Korl_Gfx_Drawable  korl_gfx_immediateTriangleFan2d(u32 vertexCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal Korl_Gfx_Drawable  korl_gfx_immediateTriangleStrip2d(u32 vertexCount, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs);
korl_internal Korl_Gfx_Drawable  korl_gfx_immediateRectangle(Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors, Korl_Math_V2f32** o_uvs);
korl_internal Korl_Gfx_Drawable  korl_gfx_immediateAxisNormalLines(void);
korl_internal Korl_Gfx_Drawable  korl_gfx_mesh(Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName);
korl_internal void               korl_gfx_drawSphere(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, f32 radius, u32 latitudeSegments, u32 longitudeSegments, const Korl_Gfx_Material* material);
korl_internal void               korl_gfx_drawRectangle2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_colors);
korl_internal void               korl_gfx_drawRectangle3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, Korl_Math_V2f32 size, f32 outlineThickness, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline, Korl_Gfx_Color4u8** o_colors);
korl_internal void               korl_gfx_drawLines2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void               korl_gfx_drawLines3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 lineCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void               korl_gfx_drawTriangles2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 triangleCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void               korl_gfx_drawTriangleFan2d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V2f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void               korl_gfx_drawTriangleFan3d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, u32 vertexCount, const Korl_Gfx_Material* material, Korl_Math_V3f32** o_positions, Korl_Gfx_Color4u8** o_colors);
korl_internal void               korl_gfx_drawUtf82d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline);
korl_internal void               korl_gfx_drawUtf83d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu8 utf8Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline);
korl_internal void               korl_gfx_drawUtf162d(Korl_Math_V2f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline);
korl_internal void               korl_gfx_drawUtf163d(Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V2f32 anchorRatio, acu16 utf16Text, acu16 utf16FontAssetName, f32 textPixelHeight, f32 outlineSize, const Korl_Gfx_Material* material, const Korl_Gfx_Material* materialOutline);
korl_internal void               korl_gfx_drawAabb3(Korl_Math_Aabb3f32 aabb, const Korl_Gfx_Material* material);
korl_internal void               korl_gfx_drawMesh(Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName, Korl_Math_V3f32 position, Korl_Math_Quaternion versor, Korl_Math_V3f32 scale, const Korl_Gfx_Material *materials, u8 materialsSize);
