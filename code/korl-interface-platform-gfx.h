#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
#include "korl-interface-platform-memory.h"
#include "korl-interface-platform-resource.h"
/** \return \c true if the codepoint should be drawn, \c false otherwise */
#define KORL_GFX_TEXT_CODEPOINT_TEST(name) bool name(void* userData, u32 codepoint, u8 codepointCodeUnits, const u8* currentCodeUnit, u8 bytesPerCodeUnit, Korl_Math_V4f32* o_currentLineColor)
typedef KORL_GFX_TEXT_CODEPOINT_TEST(fnSig_korl_gfx_text_codepointTest);
//KORL-ISSUE-000-000-096: interface-platform, gfx: get rid of all "Vulkan" identifiers here; we don't want to expose the underlying renderer implementation to the user!
typedef u16            Korl_Vulkan_VertexIndex;
typedef Korl_Math_V4u8 Korl_Vulkan_Color4u8;
typedef u64            Korl_Gfx_DeviceMemoryAllocationHandle;
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_TRANSPARENT = {  0,   0,   0,   0};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_RED         = {255,   0,   0, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_GREEN       = {  0, 255,   0, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_BLUE        = {  0,   0, 255, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_YELLOW      = {255, 255,   0, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_CYAN        = {  0, 255, 255, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_MAGENTA     = {255,   0, 255, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_WHITE       = {255, 255, 255, 255};
korl_global_const Korl_Vulkan_Color4u8 KORL_COLOR4U8_BLACK       = {  0,   0,   0, 255};
typedef enum Korl_Vulkan_PrimitiveType
{
    KORL_VULKAN_PRIMITIVETYPE_TRIANGLES,
    KORL_VULKAN_PRIMITIVETYPE_LINES
} Korl_Vulkan_PrimitiveType;
typedef enum Korl_Vulkan_BlendOperation
{
    KORL_BLEND_OP_ADD,
    KORL_BLEND_OP_SUBTRACT,
    KORL_BLEND_OP_REVERSE_SUBTRACT,
    KORL_BLEND_OP_MIN,
    KORL_BLEND_OP_MAX
} Korl_Vulkan_BlendOperation;
typedef enum Korl_Vulkan_BlendFactor
{
    KORL_BLEND_FACTOR_ZERO,
    KORL_BLEND_FACTOR_ONE,
    KORL_BLEND_FACTOR_SRC_COLOR,
    KORL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    KORL_BLEND_FACTOR_DST_COLOR,
    KORL_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    KORL_BLEND_FACTOR_SRC_ALPHA,
    KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    KORL_BLEND_FACTOR_DST_ALPHA,
    KORL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    KORL_BLEND_FACTOR_CONSTANT_COLOR,
    KORL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    KORL_BLEND_FACTOR_CONSTANT_ALPHA,
    KORL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    KORL_BLEND_FACTOR_SRC_ALPHA_SATURATE
} Korl_Vulkan_BlendFactor;
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
    Korl_Math_V3f32 worldUpNormal;
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
            f32 fixedHeight;
        } orthographic;
    } subCamera;
} Korl_Gfx_Camera;
typedef enum Korl_Gfx_Batch_Flags
{
    KORL_GFX_BATCH_FLAGS_NONE              = 0, 
    KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST = 1 << 0,
    KORL_GFX_BATCH_FLAG_DISABLE_BLENDING   = 1 << 1
} Korl_Gfx_Batch_Flags;
//KORL-ISSUE-000-000-097: interface-platform, gfx: maybe just destroy Korl_Gfx_Batch & start over, since we've gotten rid of the concept of "batching" in the platform renderer; the primitive storage struct might also benefit from being a polymorphic tagged union
typedef struct Korl_Gfx_Batch
{
    Korl_Memory_AllocatorHandle allocatorHandle;// storing the allocator handle allows us to simplify API which expands the capacity of the batch (example: add new line to a line batch)
    Korl_Vulkan_PrimitiveType primitiveType;
    Korl_Math_V3f32 _position;
    Korl_Math_V3f32 _scale;
    Korl_Math_Quaternion _rotation;
    //KORL-PERFORMANCE-000-000-017: GFX; separate batch capacity with batch vertex/index counts
    u32 _vertexIndexCount;
    u32 _vertexCount;
    u32 _instanceCount;
    Korl_Resource_Handle _texture;
    f32                  _textPixelHeight;
    f32                  _textPixelOutline;
    u$                   _textVisibleCharacterCount;
    Korl_Vulkan_Color4u8 _textColor;
    Korl_Vulkan_Color4u8 _textColorOutline;
    Korl_Math_Aabb2f32   _textAabb;
    /** A value of \c {0,0} means the text will be drawn such that the 
     * bottom-left corner of the text AABB will be equivalent to \c _position .  
     * A value of \c {1,1} means the text will be drawn such that the top-right 
     * corner of the text AABB will be equivalent to \c _position .  
     * Default value is \c {0,1} (upper-left corner of text AABB will be located 
     * at \c _position ). */
    Korl_Math_V2f32      _textPositionAnchor;
    //KORL-PERFORMANCE-000-000-002: memory: you will never have a texture & font asset at the same time, so we could potentially overload this to the same string pointer
    wchar_t* _assetNameFont;
    //KORL-PERFORMANCE-000-000-003: memory: once again, only used by text batches; create a PTU here to save space?
    wchar_t* _text;
    Korl_Vulkan_BlendOperation opColor;       // only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendFactor factorColorSource;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendFactor factorColorTarget;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendOperation opAlpha;       // only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendFactor factorAlphaSource;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendFactor factorAlphaTarget;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Resource_Handle _fontTextureHandle;
    Korl_Resource_Handle _glyphMeshBufferVertices;
    Korl_Vulkan_VertexIndex* _vertexIndices;
    u8                    _vertexPositionDimensions;
    f32*                  _vertexPositions;
    Korl_Vulkan_Color4u8* _vertexColors;
    Korl_Math_V2f32*      _vertexUvs;
    u8                    _instancePositionDimensions;
    f32*                  _instancePositions;
    u32*                  _instanceU32s;// we need a place to store the indices of each glyph in the instanced draw call
} Korl_Gfx_Batch;
typedef struct Korl_Gfx_ResultRay3d
{
    Korl_Math_V3f32 position;
    Korl_Math_V3f32 direction;
} Korl_Gfx_ResultRay3d;
/** to calculate the total spacing between two lines, use the formula: (ascent - decent) + lineGap */
typedef struct Korl_Gfx_Font_Metrics
{
    f32 ascent;
    f32 decent;
    f32 lineGap;
} Korl_Gfx_Font_Metrics;
#define KORL_FUNCTION_korl_gfx_font_getMetrics(name)              Korl_Gfx_Font_Metrics name(acu16 utf16AssetNameFont, f32 textPixelHeight)
#define KORL_FUNCTION_korl_gfx_createCameraFov(name)              Korl_Gfx_Camera       name(f32 fovHorizonDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 target)
#define KORL_FUNCTION_korl_gfx_createCameraOrtho(name)            Korl_Gfx_Camera       name(f32 clipDepth)
#define KORL_FUNCTION_korl_gfx_createCameraOrthoFixedHeight(name) Korl_Gfx_Camera       name(f32 fixedHeight, f32 clipDepth)
#define KORL_FUNCTION_korl_gfx_cameraFov_rotateAroundTarget(name) void                  name(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians)
#define KORL_FUNCTION_korl_gfx_useCamera(name)                    void                  name(Korl_Gfx_Camera camera)
#define KORL_FUNCTION_korl_gfx_camera_getCurrent(name)            Korl_Gfx_Camera       name(void)
#define KORL_FUNCTION_korl_gfx_cameraSetScissor(name)             void                  name(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY)
#define KORL_FUNCTION_korl_gfx_cameraSetScissorPercent(name)      void                  name(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight)
#define KORL_FUNCTION_korl_gfx_cameraOrthoSetOriginAnchor(name)   void                  name(Korl_Gfx_Camera*const context, f32 swapchainSizeRatioOriginX, f32 swapchainSizeRatioOriginY)
#define KORL_FUNCTION_korl_gfx_cameraOrthoGetSize(name)           Korl_Math_V2f32       name(const Korl_Gfx_Camera*const context)
/** \return \c {{NaN,NaN,NaN},{NaN,NaN,NaN}} if the coordinate transform fails */
#define KORL_FUNCTION_korl_gfx_camera_windowToWorld(name)         Korl_Gfx_ResultRay3d  name(const Korl_Gfx_Camera*const context, Korl_Math_V2i32 windowPosition)
/** \return \c {Nan,Nan} if the world position is not contained within the 
 * camera's clip space. This does NOT mean that \c non-{NaN,NaN} values are on 
 * the screen!*/
#define KORL_FUNCTION_korl_gfx_camera_worldToWindow(name)         Korl_Math_V2f32       name(const Korl_Gfx_Camera*const context, Korl_Math_V3f32 worldPosition)
#define KORL_FUNCTION_korl_gfx_batch(name)                        void                  name(Korl_Gfx_Batch*const batch, Korl_Gfx_Batch_Flags flags)
#define KORL_FUNCTION_korl_gfx_createBatchRectangleTextured(name) Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, Korl_Resource_Handle resourceHandleTexture)
/**
 * \param localOriginNormal relative to the rectangle's lower-left corner
 */
#define KORL_FUNCTION_korl_gfx_createBatchRectangleColored(name)  Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, Korl_Math_V2f32 localOriginNormal, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_createBatchCircle(name)            Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, f32 radius, u32 pointCount, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_createBatchCircleSector(name)      Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, f32 radius, u32 pointCount, Korl_Vulkan_Color4u8 color, f32 sectorRadians)
#define KORL_FUNCTION_korl_gfx_createBatchTriangles(name)         Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, u32 triangleCount)
#define KORL_FUNCTION_korl_gfx_createBatchLines(name)             Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, u32 lineCount)
#define KORL_FUNCTION_korl_gfx_createBatchText(name)              Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* assetNameFont, const wchar_t* text, f32 textPixelHeight, Korl_Vulkan_Color4u8 color, f32 outlinePixelSize, Korl_Vulkan_Color4u8 colorOutline)
#define KORL_FUNCTION_korl_gfx_batchSetBlendState(name)           void                  name(Korl_Gfx_Batch*const context, Korl_Vulkan_BlendOperation opColor, Korl_Vulkan_BlendFactor factorColorSource, Korl_Vulkan_BlendFactor factorColorTarget, Korl_Vulkan_BlendOperation opAlpha, Korl_Vulkan_BlendFactor factorAlphaSource, Korl_Vulkan_BlendFactor factorAlphaTarget)
#define KORL_FUNCTION_korl_gfx_batchSetPosition(name)             void                  name(Korl_Gfx_Batch*const context, const f32* position, u8 positionDimensions)
#define KORL_FUNCTION_korl_gfx_batchSetPosition2d(name)           void                  name(Korl_Gfx_Batch*const context, f32 x, f32 y)
#define KORL_FUNCTION_korl_gfx_batchSetPosition2dV2f32(name)      void                  name(Korl_Gfx_Batch*const context, Korl_Math_V2f32 position)
#define KORL_FUNCTION_korl_gfx_batchSetScale(name)                void                  name(Korl_Gfx_Batch*const context, Korl_Math_V3f32 scale)
#define KORL_FUNCTION_korl_gfx_batchSetQuaternion(name)           void                  name(Korl_Gfx_Batch*const context, Korl_Math_Quaternion quaternion)
#define KORL_FUNCTION_korl_gfx_batchSetRotation(name)             void                  name(Korl_Gfx_Batch*const context, Korl_Math_V3f32 axisOfRotation, f32 radians)
#define KORL_FUNCTION_korl_gfx_batchSetVertexColor(name)          void                  name(Korl_Gfx_Batch*const context, u32 vertexIndex, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batchAddLine(name)                 void                  name(Korl_Gfx_Batch**const pContext, const f32* p0, const f32* p1, u8 positionDimensions, Korl_Vulkan_Color4u8 color0, Korl_Vulkan_Color4u8 color1)
#define KORL_FUNCTION_korl_gfx_batchSetLine(name)                 void                  name(Korl_Gfx_Batch*const context, u32 lineIndex, const f32* p0, const f32* p1, u8 positionDimensions, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batchTextGetAabb(name)             Korl_Math_Aabb2f32    name(Korl_Gfx_Batch*const batchContext)
#define KORL_FUNCTION_korl_gfx_batchTextSetPositionAnchor(name)   void                  name(Korl_Gfx_Batch*const batchContext, Korl_Math_V2f32 textPositionAnchor)
#define KORL_FUNCTION_korl_gfx_batchRectangleSetSize(name)        void                  name(Korl_Gfx_Batch*const context, Korl_Math_V2f32 size)
#define KORL_FUNCTION_korl_gfx_batchRectangleSetColor(name)       void                  name(Korl_Gfx_Batch*const context, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batchCircleSetColor(name)          void                  name(Korl_Gfx_Batch*const context, Korl_Vulkan_Color4u8 color)
