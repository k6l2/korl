#pragma once
#include "korl-globalDefines.h"
#include "utility/korl-utility-math.h"
#include "korl-interface-platform-memory.h"
#include "korl-interface-platform-resource.h"
/** \return \c true if the codepoint should be drawn, \c false otherwise */
#define KORL_GFX_TEXT_CODEPOINT_TEST(name) bool name(void* userData, u32 codepoint, u8 codepointCodeUnits, const u8* currentCodeUnit, u8 bytesPerCodeUnit, Korl_Math_V4f32* o_currentLineColor)
typedef KORL_GFX_TEXT_CODEPOINT_TEST(fnSig_korl_gfx_text_codepointTest);
//KORL-ISSUE-000-000-096: interface-platform, gfx: get rid of all "Vulkan" identifiers here; we don't want to expose the underlying renderer implementation to the user!
typedef u16            Korl_Vulkan_VertexIndex;
typedef Korl_Math_V4u8 Korl_Vulkan_Color4u8;
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
typedef struct Korl_Gfx_Blend
{
    struct
    {
        Korl_Vulkan_BlendOperation operation;
        Korl_Vulkan_BlendFactor    factorSource;
        Korl_Vulkan_BlendFactor    factorTarget;
    } color, alpha;
} Korl_Gfx_Blend;
korl_global_const Korl_Gfx_Blend KORL_GFX_BLEND_ALPHA               = {.color = {KORL_BLEND_OP_ADD, KORL_BLEND_FACTOR_SRC_ALPHA, KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}
                                                                      ,.alpha = {KORL_BLEND_OP_ADD, KORL_BLEND_FACTOR_ONE, KORL_BLEND_FACTOR_ZERO}};
korl_global_const Korl_Gfx_Blend KORL_GFX_BLEND_ALPHA_PREMULTIPLIED = {.color = {KORL_BLEND_OP_ADD, KORL_BLEND_FACTOR_ONE, KORL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA}
                                                                      ,.alpha = {KORL_BLEND_OP_ADD, KORL_BLEND_FACTOR_ONE, KORL_BLEND_FACTOR_ZERO}};
korl_global_const Korl_Gfx_Blend KORL_GFX_BLEND_ADD                 = {.color = {KORL_BLEND_OP_ADD, KORL_BLEND_FACTOR_SRC_ALPHA, KORL_BLEND_FACTOR_ONE}
                                                                      ,.alpha = {KORL_BLEND_OP_ADD, KORL_BLEND_FACTOR_ONE, KORL_BLEND_FACTOR_ONE}};
korl_global_const Korl_Gfx_Blend KORL_GFX_BLEND_ADD_PREMULTIPLIED   = {.color = {KORL_BLEND_OP_ADD, KORL_BLEND_FACTOR_ONE, KORL_BLEND_FACTOR_ONE}
                                                                      ,.alpha = {KORL_BLEND_OP_ADD, KORL_BLEND_FACTOR_ONE, KORL_BLEND_FACTOR_ONE}};
typedef struct Korl_Gfx_Camera
{
    enum
    {
        KORL_GFX_CAMERA_TYPE_PERSPECTIVE,
        KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC,
        KORL_GFX_CAMERA_TYPE_ORTHOGRAPHIC_FIXED_HEIGHT,
    } type;
    Korl_Math_V3f32 position;
    //KORL-ISSUE-000-000-162: gfx: the camera should _not_ store `target`, as this requires the user to constantly change the camera's position _and_ target when they want to just move the camera around; also, we shouldn't store `worldUpNormal` here, as this implies this is a specialized camera type, like based on pitch/yaw or something
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
            f32 fovVerticalDegrees;
            f32 clipNear;
            f32 clipFar;
        } perspective;
        struct
        {
            Korl_Math_V2f32 originAnchor;
            f32             clipDepth;
            f32             fixedHeight;
        } orthographic;
    } subCamera;
} Korl_Gfx_Camera;
typedef enum Korl_Gfx_Drawable_Type
    // KORL-ISSUE-000-000-157: gfx: it doesn't make much sense to draw a "SCENE3D"; Drawable types should really be more primitive, such as "MODEL", "TEXT", "MESH"
    {KORL_GFX_DRAWABLE_TYPE_SCENE3D
} Korl_Gfx_Drawable_Type;
typedef struct Korl_Gfx_Material_Properties
{
    Korl_Math_V4f32 factorColorBase;
    Korl_Math_V3f32 factorColorEmissive;
    f32             _padding_0;
    Korl_Math_V4f32 factorColorSpecular;
    f32             shininess;
    f32             _padding_1[3];
} Korl_Gfx_Material_Properties;
typedef struct Korl_Gfx_Material_Maps
{
    Korl_Resource_Handle resourceHandleTextureBase;
    Korl_Resource_Handle resourceHandleTextureSpecular;
    Korl_Resource_Handle resourceHandleTextureEmissive;
} Korl_Gfx_Material_Maps;
typedef struct Korl_Gfx_Material_Shaders
{
    Korl_Resource_Handle resourceHandleShaderVertex;
    Korl_Resource_Handle resourceHandleShaderFragment;
} Korl_Gfx_Material_Shaders;
typedef struct Korl_Gfx_Material
{
    Korl_Gfx_Material_Properties properties;
    Korl_Gfx_Material_Maps       maps;
    Korl_Gfx_Material_Shaders    shaders;
} Korl_Gfx_Material;
korl_global_const Korl_Gfx_Material KORL_GFX_MATERIAL_DEFAULT = {.properties = {.factorColorBase     = {1,1,1,1}
                                                                               ,.factorColorEmissive = {0,0,0}
                                                                               ,.factorColorSpecular = {1,1,1,1}
                                                                               ,.shininess           = 32}};
typedef struct Korl_Gfx_Drawable_MaterialSlot
{
    Korl_Gfx_Material material;
    bool              used;
} Korl_Gfx_Drawable_MaterialSlot;
typedef struct Korl_Gfx_Drawable
{
    struct
    {
        Korl_Math_V3f32      position;
        Korl_Math_V3f32      scale;
        Korl_Math_Quaternion rotation;
    } _model;
    Korl_Gfx_Drawable_Type type;
    union
    {
        struct
        {
            Korl_Resource_Handle           resourceHandle;
            Korl_Gfx_Drawable_MaterialSlot materialSlots[1];
        } scene3d;
    } subType;
} Korl_Gfx_Drawable;
typedef enum Korl_Gfx_Light_Type
    {KORL_GFX_LIGHT_TYPE_POINT       = 1
    ,KORL_GFX_LIGHT_TYPE_DIRECTIONAL = 2
    ,KORL_GFX_LIGHT_TYPE_SPOT        = 3
} Korl_Gfx_Light_Type;
typedef struct Korl_Gfx_Light
{
    u32             type;// Korl_Gfx_Light_Type
    u32             _padding_0[3];
    Korl_Math_V3f32 position; // in world-space; NOTE: this will get automatically transformed into view-space for GLSL
    f32             _padding_1;
    Korl_Math_V3f32 direction;// in world-space; NOTE: this will get automatically transformed into view-space for GLSL
    f32             _padding_2;
    struct
    {
        Korl_Math_V3f32 ambient;
        f32             _padding_0;
        Korl_Math_V3f32 diffuse;
        f32             _padding_1;
        Korl_Math_V3f32 specular;
        f32             _padding_2;
    } color;
    struct
    {
        f32 constant;
        f32 linear;
        f32 quadratic;
    } attenuation;
    struct
    {
        f32 inner;
        f32 outer;
        f32 _padding_0[2];
    } cutOffCosines;
    f32 _padding_3;
} Korl_Gfx_Light;
enum
    {KORL_GFX_BATCH_FLAGS_NONE              = 0
    ,KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST = 1 << 0
    ,KORL_GFX_BATCH_FLAG_DISABLE_BLENDING   = 1 << 1
};
typedef u32 Korl_Gfx_Batch_Flags;// separate flag type definition for C++ compatibility reasons (C++ does not easily allow the | operator for enums; cool language, bro)
//KORL-ISSUE-000-000-097: interface-platform, gfx: maybe just destroy Korl_Gfx_Batch & start over, since we've gotten rid of the concept of "batching" in the platform renderer; the primitive storage struct might also benefit from being a polymorphic tagged union
typedef struct Korl_Gfx_Batch
{
    Korl_Memory_AllocatorHandle allocatorHandle;// storing the allocator handle allows us to simplify API which expands the capacity of the batch (example: add new line to a line batch)
    Korl_Vulkan_PrimitiveType primitiveType;
    Korl_Math_V3f32 _position;
    Korl_Math_V3f32 _scale;
    Korl_Math_Quaternion _rotation;
    Korl_Vulkan_Color4u8 modelColor;
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
    Korl_Gfx_Blend blend;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Resource_Handle _fontTextureHandle;
    Korl_Resource_Handle _glyphMeshBufferVertices;
    Korl_Vulkan_VertexIndex* _vertexIndices;
    u8                    _vertexPositionDimensions;
    f32*                  _vertexPositions;
    Korl_Vulkan_Color4u8* _vertexColors;
    Korl_Math_V2f32*      _vertexUvs;
    f32                   uvAabbOffset;
    u8                    _instancePositionDimensions;
    f32*                  _instancePositions;
    u32*                  _instanceU32s;// we need a place to store the indices of each glyph in the instanced draw call
    Korl_Resource_Handle resourceHandleShaderFragment;
    Korl_Resource_Handle resourceHandleShaderVertex;
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
#define KORL_FUNCTION_korl_gfx_font_getMetrics(name)                         Korl_Gfx_Font_Metrics name(acu16 utf16AssetNameFont, f32 textPixelHeight)
#define KORL_FUNCTION_korl_gfx_createCameraFov(name)                         Korl_Gfx_Camera       name(f32 fovVerticalDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 target, Korl_Math_V3f32 up)
#define KORL_FUNCTION_korl_gfx_createCameraOrtho(name)                       Korl_Gfx_Camera       name(f32 clipDepth)
#define KORL_FUNCTION_korl_gfx_createCameraOrthoFixedHeight(name)            Korl_Gfx_Camera       name(f32 fixedHeight, f32 clipDepth)
#define KORL_FUNCTION_korl_gfx_cameraFov_rotateAroundTarget(name)            void                  name(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians)
#define KORL_FUNCTION_korl_gfx_useCamera(name)                               void                  name(Korl_Gfx_Camera camera)
#define KORL_FUNCTION_korl_gfx_camera_getCurrent(name)                       Korl_Gfx_Camera       name(void)
/** Note that scissor coordinates use swap chain coordinates, where the origin 
 * is the upper-left corner of the swap chain, & {+X,+Y} points to the 
 * bottom-right.  */
//KORL-ISSUE-000-000-004
#define KORL_FUNCTION_korl_gfx_cameraSetScissor(name)                        void                  name(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY)
/** 
 * Note that scissor coordinates use swap chain coordinates, where the origin is 
 * the upper-left corner of the swap chain, & {+X,+Y} points to the bottom-right.  */
#define KORL_FUNCTION_korl_gfx_cameraSetScissorPercent(name)                 void                  name(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight)
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
#define KORL_FUNCTION_korl_gfx_cameraOrthoSetOriginAnchor(name)              void                  name(Korl_Gfx_Camera*const context, f32 swapchainSizeRatioOriginX, f32 swapchainSizeRatioOriginY)
#define KORL_FUNCTION_korl_gfx_cameraOrthoGetSize(name)                      Korl_Math_V2f32       name(const Korl_Gfx_Camera*const context)
/** \return \c {{NaN,NaN,NaN},{NaN,NaN,NaN}} if the coordinate transform fails */
#define KORL_FUNCTION_korl_gfx_camera_windowToWorld(name)                    Korl_Gfx_ResultRay3d  name(const Korl_Gfx_Camera*const context, Korl_Math_V2i32 windowPosition)
/** \return \c {Nan,Nan} if the world position is not contained within the 
 * camera's clip space. This does NOT mean that \c non-{NaN,NaN} values are on 
 * the screen!*/
#define KORL_FUNCTION_korl_gfx_camera_worldToWindow(name)                    Korl_Math_V2f32       name(const Korl_Gfx_Camera*const context, Korl_Math_V3f32 worldPosition)
#define KORL_FUNCTION_korl_gfx_batch(name)                                   void                  name(Korl_Gfx_Batch*const batch, Korl_Gfx_Batch_Flags flags)
//KORL-ISSUE-000-000-005
#define KORL_FUNCTION_korl_gfx_createBatchRectangleTextured(name)            Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, Korl_Resource_Handle resourceHandleTexture)
/**
 * \param localOriginNormal relative to the rectangle's lower-left corner
 */
#define KORL_FUNCTION_korl_gfx_createBatchRectangleColored(name)             Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, Korl_Math_V2f32 localOriginNormal, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_createBatchCircle(name)                       Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, f32 radius, u32 pointCount, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_createBatchCircleSector(name)                 Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, f32 radius, u32 pointCount, Korl_Vulkan_Color4u8 color, f32 sectorRadians)
#define KORL_FUNCTION_korl_gfx_createBatchTriangles(name)                    Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, u32 triangleCount)
#define KORL_FUNCTION_korl_gfx_createBatchQuadsTextured(name)                Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, u32 quadCount, Korl_Resource_Handle resourceHandleTexture)
#define KORL_FUNCTION_korl_gfx_createBatchQuadsTexturedColored(name)         Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, u32 quadCount, Korl_Resource_Handle resourceHandleTexture)
#define KORL_FUNCTION_korl_gfx_createBatchLines(name)                        Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, u32 lineCount)
/** 
 * \param assetNameFont If this value is \c NULL , a default internal font asset 
 * will be used.
 */
#define KORL_FUNCTION_korl_gfx_createBatchText(name)                         Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* assetNameFont, const wchar_t* text, f32 textPixelHeight, Korl_Vulkan_Color4u8 color, f32 outlinePixelSize, Korl_Vulkan_Color4u8 colorOutline)
#define KORL_FUNCTION_korl_gfx_createBatchAxisLines(name)                    Korl_Gfx_Batch*       name(Korl_Memory_AllocatorHandle allocatorHandle)
#define KORL_FUNCTION_korl_gfx_batchSetBlendState(name)                      void                  name(Korl_Gfx_Batch*const context, Korl_Gfx_Blend blend)
#define KORL_FUNCTION_korl_gfx_batchSetPosition(name)                        void                  name(Korl_Gfx_Batch*const context, const f32* position, u8 positionDimensions)
#define KORL_FUNCTION_korl_gfx_batchSetPosition2d(name)                      void                  name(Korl_Gfx_Batch*const context, f32 x, f32 y)
#define KORL_FUNCTION_korl_gfx_batchSetPosition2dV2f32(name)                 void                  name(Korl_Gfx_Batch*const context, Korl_Math_V2f32 position)
#define KORL_FUNCTION_korl_gfx_batchSetScale(name)                           void                  name(Korl_Gfx_Batch*const context, Korl_Math_V3f32 scale)
#define KORL_FUNCTION_korl_gfx_batchSetQuaternion(name)                      void                  name(Korl_Gfx_Batch*const context, Korl_Math_Quaternion quaternion)
#define KORL_FUNCTION_korl_gfx_batchSetRotation(name)                        void                  name(Korl_Gfx_Batch*const context, Korl_Math_V3f32 axisOfRotation, f32 radians)
#define KORL_FUNCTION_korl_gfx_batchSetVertexColor(name)                     void                  name(Korl_Gfx_Batch*const context, u32 vertexIndex, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batchAddLine(name)                            void                  name(Korl_Gfx_Batch**const pContext, const f32* p0, const f32* p1, u8 positionDimensions, Korl_Vulkan_Color4u8 color0, Korl_Vulkan_Color4u8 color1)
#define KORL_FUNCTION_korl_gfx_batchSetLine(name)                            void                  name(Korl_Gfx_Batch*const context, u32 lineIndex, const f32* p0, const f32* p1, u8 positionDimensions, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batchTextGetAabb(name)                        Korl_Math_Aabb2f32    name(Korl_Gfx_Batch*const batchContext)
#define KORL_FUNCTION_korl_gfx_batchTextSetPositionAnchor(name)              void                  name(Korl_Gfx_Batch*const batchContext, Korl_Math_V2f32 textPositionAnchor)
#define KORL_FUNCTION_korl_gfx_batchRectangleSetSize(name)                   void                  name(Korl_Gfx_Batch*const context, Korl_Math_V2f32 size)
#define KORL_FUNCTION_korl_gfx_batchRectangleSetColor(name)                  void                  name(Korl_Gfx_Batch*const context, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batch_rectangle_setUv_raw(name)               void                  name(Korl_Gfx_Batch*const context, Korl_Math_Aabb2f32 aabbRawUvs)
#define KORL_FUNCTION_korl_gfx_batch_rectangle_setUv_pixel_to_normal(name)   void                  name(Korl_Gfx_Batch*const context, Korl_Math_Aabb2f32 pixelSpaceAabb)
#define KORL_FUNCTION_korl_gfx_batchCircleSetColor(name)                     void                  name(Korl_Gfx_Batch*const context, Korl_Vulkan_Color4u8 color)
#define KORL_FUNCTION_korl_gfx_batch_quadsTexturedColored_set(name)          void                  name(Korl_Gfx_Batch*const context, u32 quadIndex, Korl_Math_V2f32 positions[4], Korl_Math_Aabb2f32 aabbRawUvs, const Korl_Vulkan_Color4u8 colors[4], u8 colorsSize)
#define KORL_FUNCTION_korl_gfx_batch_quadsTextured_raw(name)                 void                  name(Korl_Gfx_Batch*const context, u32 quadIndex, Korl_Math_V2f32 size, Korl_Math_V2f32 positionMinimum, Korl_Math_Aabb2f32 aabbRawUvs, const Korl_Vulkan_Color4u8 colors[4], u8 colorsSize)
#define KORL_FUNCTION_korl_gfx_batch_quadsTextured_pixel_to_normal(name)     void                  name(Korl_Gfx_Batch*const context, u32 quadIndex, Korl_Math_V2f32 size, Korl_Math_V2f32 positionMinimum, Korl_Math_Aabb2f32 pixelSpaceAabb)
/** NOTE: this API is stupid; during the inevitable re-write of korl-gfx, 
 * destroy any data structure member pointers which reference memory within the 
 * same allocation */
#define KORL_FUNCTION_korl_gfx_batch_collectDefragmentPointers(name)         void                  name(Korl_Gfx_Batch**const pContext, void* stbDaMemoryContext, Korl_Heap_DefragmentPointer** pStbDaDefragmentPointers, void* parent)
#define KORL_FUNCTION_korl_gfx_setClearColor(name)                           void                  name(u8 red, u8 green, u8 blue)
#define KORL_FUNCTION_korl_gfx_drawable_scene3d_initialize(name)             void                  name(Korl_Gfx_Drawable*const context, Korl_Resource_Handle resourceHandleScene3d)
//KORL-ISSUE-000-000-157: once this issue is fixed, we can remove the `acu8 utf8MeshName` parameter from `korl_gfx_draw`
#define KORL_FUNCTION_korl_gfx_draw(name)                                    void                  name(const Korl_Gfx_Drawable*const context, acu8 utf8MeshName)
#define KORL_FUNCTION_korl_gfx_light_use(name)                               void                  name(const Korl_Gfx_Light*const lights, u$ lightsSize)
