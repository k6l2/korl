#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
#include "korl-vulkan.h"
typedef struct Korl_Gfx_Camera
{
    enum
    {
        KORL_GFX_CAMERA_TYPE_PERSPECTIVE,
        KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC
    } type;
    Korl_Math_V3f32 position;
    Korl_Math_V3f32 target;
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
            f32 fixedHeight;
            f32 clipDepth;
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
    wchar_t* _assetNameTexture;
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
korl_internal Korl_Gfx_Camera korl_gfx_createCameraFov(f32 fovHorizonDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 target);
korl_internal Korl_Gfx_Camera korl_gfx_createCameraOrthoFixedHeight(f32 fixedHeight, f32 clipDepth);
korl_internal void korl_gfx_cameraFov_rotateAroundTarget(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians);
korl_internal void korl_gfx_useCamera(const Korl_Gfx_Camera*const camera);
korl_internal void korl_gfx_batch(const Korl_Gfx_Batch*const batch, Korl_Gfx_Batch_Flags flags);
/// @simplify: is it possible to just have a "createRectangle" function, 
/// and then add texture or color components to it in later calls?  And if 
/// so, is this API good (benefits/detriments to usability/readability/performance?)?
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchRectangleTextured(Korl_Memory_Allocator allocator, Korl_Math_V2f32 size, const wchar_t* assetNameTexture);
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchRectangleColored(Korl_Memory_Allocator allocator, Korl_Math_V2f32 size, Korl_Vulkan_Color color);
korl_internal Korl_Gfx_Batch* korl_gfx_createBatchLines(Korl_Memory_Allocator allocator, u32 lineCount);
korl_internal void korl_gfx_batchSetPosition(Korl_Gfx_Batch*const context, Korl_Vulkan_Position position);
korl_internal void korl_gfx_batchSetScale(Korl_Gfx_Batch*const context, Korl_Vulkan_Position scale);
korl_internal void korl_gfx_batchSetVertexColor(Korl_Gfx_Batch*const context, u32 vertexIndex, Korl_Vulkan_Color color);
korl_internal void korl_gfx_batchSetLine(Korl_Gfx_Batch*const context, u32 lineIndex, Korl_Vulkan_Position p0, Korl_Vulkan_Position p1, Korl_Vulkan_Color color);
