#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
#include "korl-vulkan.h"
typedef struct Korl_Gfx_Camera
{
    enum
    {
        KORL_GFX_CAMERA_TYPE_PERSPECTIVE,
        KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC,
        KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT,
    } type;
    Korl_Math_V3f32 position;
    Korl_Math_V3f32 target;
    /** If the viewport scissor coordinates are stored as ratios, they can 
     * always be valid up until the time the camera gets used to draw, allowing 
     * the swap chain dimensions to change however it likes without requiring us 
     * to update these values.  The downside is that these coordinates must 
     * eventually be transformed into integral values at some point, so some 
     * kind of rounding strategy must occur.
     * The scissor coordinates are relative to the upper-left corner of the swap 
     * chain, with the lower-right corner of the swap chain lying in the 
     * positive coordinate space of both axes.  */
    Korl_Math_V2f32 _viewportScissorPosition;// should default to {0,0}
    Korl_Math_V2f32 _viewportScissorSize;// should default to {1,1}
    enum
    {
        KORL_GFX_CAMERA_SCISSOR_TYPE_RATIO,// default
        KORL_GFX_CAMERA_SCISSOR_TYPE_ABSOLUTE,
    } _scissorType;
    union
    {
        struct
        {
            f32 fovHorizonDegrees;
            f32 clipNear;
            f32 clipFar;
        } perspective;
        struct
        {
            Korl_Math_V2f32 originAnchor;
            f32 clipDepth;
        } orthographic;
        struct
        {
            Korl_Math_V2f32 originAnchor;
            f32 clipDepth;
            f32 fixedHeight;
        } orthographicFixedHeight;
    } subCamera;
} Korl_Gfx_Camera;
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
    /** @memory: you will never have a texture & font asset at the same time, so 
     * we could potentially overload this to the same string pointer */
    wchar_t* _assetNameFont;
    /** @memory: once again, only used by text batches; create a PTU here to 
     * save space? */
    wchar_t* _text;
    Korl_Vulkan_TextureHandle _fontTextureHandle;
    Korl_Vulkan_VertexIndex* _vertexIndices;
    Korl_Vulkan_Position*    _vertexPositions;
    Korl_Vulkan_Color*       _vertexColors;
    Korl_Vulkan_Uv*          _vertexUvs;
} Korl_Gfx_Batch;
typedef enum Korl_Gfx_Batch_Flags
{
    KORL_GFX_BATCH_FLAG_NONE = 0, 
    KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST = 1 << 0
} Korl_Gfx_Batch_Flags;
korl_internal void korl_gfx_initialize(void);
korl_internal Korl_Gfx_Camera korl_gfx_createCameraFov(f32 fovHorizonDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 target);
korl_internal Korl_Gfx_Camera korl_gfx_createCameraOrtho(f32 clipDepth);
korl_internal Korl_Gfx_Camera korl_gfx_createCameraOrthoFixedHeight(f32 fixedHeight, f32 clipDepth);
korl_internal void korl_gfx_cameraFov_rotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians);
korl_internal void korl_gfx_useCamera(Korl_Gfx_Camera camera);
/** @todo: maybe change the scissor parameters to be an integral data type, 
 * since at some point a rounding strategy must occur anyways, and this 
 * information is obscured by this API, just for the sake of making it easier to 
 * call with f32 data... Which, I don't know if that's a great tradeoff honestly */
korl_internal void korl_gfx_cameraSetScissor(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY);
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
korl_internal void korl_gfx_batch(Korl_Gfx_Batch*const batch, Korl_Gfx_Batch_Flags flags);
/** @simplify: is it possible to just have a "createRectangle" function, 
 * and then add texture or color components to it in later calls?  And if 
 * so, is this API good (benefits/detriments to usability/readability/performance?)? 
 * My initial thoughts on this are:
 *  - this would introduce unnecessary performance penalties since we cannot 
 *    know ahead of time what memory to allocate for the batch (whether or not 
 *    we need UVs, colors, etc.)
 *  - the resulting API might become more complex anyways, and increase friction 
 *    for the user */
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchRectangleTextured(Korl_Memory_Allocator allocator, Korl_Math_V2f32 size, const wchar_t* assetNameTexture);
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchRectangleColored(Korl_Memory_Allocator allocator, Korl_Math_V2f32 size, Korl_Math_V2f32 localOriginNormal, Korl_Vulkan_Color color);
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchLines(Korl_Memory_Allocator allocator, u32 lineCount);
/** 
 * \param assetNameFont If this value is \c NULL , a default internal font asset 
 * will be used.
 */
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchText(Korl_Memory_Allocator allocator, const wchar_t* assetNameFont, const wchar_t* text, f32 textPixelHeight);
korl_internal void korl_gfx_batchSetPosition(Korl_Gfx_Batch*const context, Korl_Vulkan_Position position);
korl_internal void korl_gfx_batchSetScale(Korl_Gfx_Batch*const context, Korl_Vulkan_Position scale);
korl_internal void korl_gfx_batchSetVertexColor(Korl_Gfx_Batch*const context, u32 vertexIndex, Korl_Vulkan_Color color);
korl_internal void korl_gfx_batchSetLine(Korl_Gfx_Batch*const context, u32 lineIndex, Korl_Vulkan_Position p0, Korl_Vulkan_Position p1, Korl_Vulkan_Color color);
korl_internal f32 korl_gfx_batchTextGetAabbSizeX(Korl_Gfx_Batch*const context);
korl_internal f32 korl_gfx_batchTextGetAabbSizeY(Korl_Gfx_Batch*const context);
korl_internal void korl_gfx_batchRectangleSetSize(Korl_Gfx_Batch*const context, Korl_Math_V2f32 size);
korl_internal void korl_gfx_batchRectangleSetColor(Korl_Gfx_Batch*const context, Korl_Vulkan_Color color);
