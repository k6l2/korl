#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
#include "korl-vulkan.h"
#include "korl-interface-platform-gfx.h"
korl_internal void korl_gfx_initialize(void);
korl_internal void korl_gfx_updateSurfaceSize(Korl_Math_V2u32 size);
korl_internal void korl_gfx_flushGlyphPages(void);
typedef struct Korl_Gfx_Text
{
    Korl_Memory_AllocatorHandle allocator;
    Korl_Resource_Handle resourceHandleBufferText;// used to store the vertex buffer which contains all of the _Korl_Gfx_FontGlyphInstance data for all the lines contained in this Text object; each line will access an offset into this vertex buffer determined by the sum of all visible characters of all lines prior to it in the stbDaLines list
    struct _Korl_Gfx_Text_Line* stbDaLines;// the user can obtain the line count by calling arrlenu(text->stbDaLines)
    f32 textPixelHeight;
    acu16 utf16AssetNameFont;
    Korl_Math_V3f32      modelTranslate;// the model-space origin of a Gfx_Text is the upper-left corner of the model-space AABB
    Korl_Math_Quaternion modelRotate;
    Korl_Math_V3f32      modelScale;
    Korl_Math_Aabb2f32 _modelAabb;// not valid until fifo add/remove APIs have been called
    u32 totalVisibleGlyphs;// redundant value; acceleration for operations on resourceHandleBufferText
} Korl_Gfx_Text;
korl_internal Korl_Gfx_Text*     korl_gfx_text_create(Korl_Memory_AllocatorHandle allocator, acu16 utf16AssetNameFont, f32 textPixelHeight);
korl_internal void               korl_gfx_text_destroy(Korl_Gfx_Text* context);
korl_internal void               korl_gfx_text_fifoAdd(Korl_Gfx_Text* context, acu16 utf16Text, Korl_Memory_AllocatorHandle stackAllocator, fnSig_korl_gfx_text_codepointTest* codepointTest, void* codepointTestUserData);
korl_internal void               korl_gfx_text_fifoRemove(Korl_Gfx_Text* context, u$ lineCount);
korl_internal void               korl_gfx_text_draw(const Korl_Gfx_Text* context, Korl_Math_Aabb2f32 visibleRegion);
korl_internal Korl_Math_Aabb2f32 korl_gfx_text_getModelAabb(const Korl_Gfx_Text* context);
korl_internal KORL_FUNCTION_korl_gfx_font_getMetrics(korl_gfx_font_getMetrics);
// korl_internal Korl_Math_Aabb2f32 korl_gfx_font_getTextAabb(acu16 utf16AssetNameFont, f32 textPixelHeight, acu8 utf8Text);
korl_internal Korl_Math_V2f32    korl_gfx_font_textGraphemePosition(acu16 utf16AssetNameFont, f32 textPixelHeight, acu8 utf8Text, u$ graphemeIndex);
korl_internal KORL_FUNCTION_korl_gfx_createCameraFov(korl_gfx_createCameraFov);
korl_internal KORL_FUNCTION_korl_gfx_createCameraOrtho(korl_gfx_createCameraOrtho);
korl_internal KORL_FUNCTION_korl_gfx_createCameraOrthoFixedHeight(korl_gfx_createCameraOrthoFixedHeight);
korl_internal KORL_FUNCTION_korl_gfx_cameraFov_rotateAroundTarget(korl_gfx_cameraFov_rotateAroundTarget);
korl_internal KORL_FUNCTION_korl_gfx_useCamera(korl_gfx_useCamera);
korl_internal KORL_FUNCTION_korl_gfx_camera_getCurrent(korl_gfx_camera_getCurrent);
/** Note that scissor coordinates use swap chain coordinates, where the origin 
 * is the upper-left corner of the swap chain, & {+X,+Y} points to the 
 * bottom-right.  */
//KORL-ISSUE-000-000-004
korl_internal KORL_FUNCTION_korl_gfx_cameraSetScissor(korl_gfx_cameraSetScissor);
/** 
 * Note that scissor coordinates use swap chain coordinates, where the origin is 
 * the upper-left corner of the swap chain, & {+X,+Y} points to the bottom-right.  */
korl_internal KORL_FUNCTION_korl_gfx_cameraSetScissorPercent(korl_gfx_cameraSetScissorPercent);
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
korl_internal KORL_FUNCTION_korl_gfx_cameraOrthoSetOriginAnchor(korl_gfx_cameraOrthoSetOriginAnchor);
korl_internal KORL_FUNCTION_korl_gfx_cameraOrthoGetSize(korl_gfx_cameraOrthoGetSize);
korl_internal KORL_FUNCTION_korl_gfx_camera_windowToWorld(korl_gfx_camera_windowToWorld);
korl_internal KORL_FUNCTION_korl_gfx_camera_worldToWindow(korl_gfx_camera_worldToWindow);
korl_internal KORL_FUNCTION_korl_gfx_batch(korl_gfx_batch);
//KORL-ISSUE-000-000-005
korl_internal KORL_FUNCTION_korl_gfx_createBatchRectangleTextured(korl_gfx_createBatchRectangleTextured);
korl_internal KORL_FUNCTION_korl_gfx_createBatchRectangleColored(korl_gfx_createBatchRectangleColored);
korl_internal KORL_FUNCTION_korl_gfx_createBatchCircle(korl_gfx_createBatchCircle);
korl_internal KORL_FUNCTION_korl_gfx_createBatchCircleSector(korl_gfx_createBatchCircleSector);
korl_internal KORL_FUNCTION_korl_gfx_createBatchTriangles(korl_gfx_createBatchTriangles);
korl_internal KORL_FUNCTION_korl_gfx_createBatchLines(korl_gfx_createBatchLines);
/** 
 * \param assetNameFont If this value is \c NULL , a default internal font asset 
 * will be used.
 */
korl_internal KORL_FUNCTION_korl_gfx_createBatchText(korl_gfx_createBatchText);
korl_internal KORL_FUNCTION_korl_gfx_batchSetBlendState(korl_gfx_batchSetBlendState);
korl_internal KORL_FUNCTION_korl_gfx_batchSetPosition(korl_gfx_batchSetPosition);
korl_internal KORL_FUNCTION_korl_gfx_batchSetPosition2d(korl_gfx_batchSetPosition2d);
korl_internal KORL_FUNCTION_korl_gfx_batchSetPosition2dV2f32(korl_gfx_batchSetPosition2dV2f32);
korl_internal KORL_FUNCTION_korl_gfx_batchSetScale(korl_gfx_batchSetScale);
korl_internal KORL_FUNCTION_korl_gfx_batchSetQuaternion(korl_gfx_batchSetQuaternion);
korl_internal KORL_FUNCTION_korl_gfx_batchSetRotation(korl_gfx_batchSetRotation);
korl_internal KORL_FUNCTION_korl_gfx_batchSetVertexColor(korl_gfx_batchSetVertexColor);
korl_internal KORL_FUNCTION_korl_gfx_batchAddLine(korl_gfx_batchAddLine);
korl_internal KORL_FUNCTION_korl_gfx_batchSetLine(korl_gfx_batchSetLine);
korl_internal KORL_FUNCTION_korl_gfx_batchTextGetAabb(korl_gfx_batchTextGetAabb);
korl_internal KORL_FUNCTION_korl_gfx_batchTextSetPositionAnchor(korl_gfx_batchTextSetPositionAnchor);
korl_internal KORL_FUNCTION_korl_gfx_batchRectangleSetSize(korl_gfx_batchRectangleSetSize);
korl_internal KORL_FUNCTION_korl_gfx_batchRectangleSetColor(korl_gfx_batchRectangleSetColor);
korl_internal KORL_FUNCTION_korl_gfx_batchCircleSetColor(korl_gfx_batchCircleSetColor);
korl_internal void korl_gfx_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer);
/** I don't like how this API requires us to do file I/O in modules outside of 
 * korl-file; maybe improve this in the future to use korl-file API instea of 
 * Win32 API?
 * KORL-ISSUE-000-000-069: savestate/file: contain filesystem API to korl-file? */
korl_internal bool korl_gfx_saveStateRead(HANDLE hFile);
