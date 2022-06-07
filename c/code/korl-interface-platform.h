#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
#include "korl-gui-common.h"
/** Do not use this value directly, since the meaning of this data is 
 * platform-dependent.  Instead, use the platform APIs which uses this type.  
 * This value should represent the highest possible time resolution 
 * (microseconds) of the platform, but it should only be valid for the current 
 * session of the underlying hardware.  (Do NOT save this value to disk and then 
 * expect it to be correct when you load it from disk on another 
 * session/machine!) */
typedef u64 PlatformTimeStamp;
#define KORL_PLATFORM_GET_TIMESTAMP(name) PlatformTimeStamp name(void)
#define KORL_PLATFORM_SECONDS_SINCE_TIMESTAMP(name) f32 name(PlatformTimeStamp pts)
typedef enum Korl_KeyboardCode
    { KORL_KEY_UNKNOWN
    , KORL_KEY_A
    , KORL_KEY_B
    , KORL_KEY_C
    , KORL_KEY_D
    , KORL_KEY_E
    , KORL_KEY_F
    , KORL_KEY_G
    , KORL_KEY_H
    , KORL_KEY_I
    , KORL_KEY_J
    , KORL_KEY_K
    , KORL_KEY_L
    , KORL_KEY_M
    , KORL_KEY_N
    , KORL_KEY_O
    , KORL_KEY_P
    , KORL_KEY_Q
    , KORL_KEY_R
    , KORL_KEY_S
    , KORL_KEY_T
    , KORL_KEY_U
    , KORL_KEY_V
    , KORL_KEY_W
    , KORL_KEY_X
    , KORL_KEY_Y
    , KORL_KEY_Z
    , KORL_KEY_COMMA
    , KORL_KEY_PERIOD
    , KORL_KEY_SLASH_FORWARD
    , KORL_KEY_SLASH_BACK
    , KORL_KEY_CURLYBRACE_LEFT
    , KORL_KEY_CURLYBRACE_RIGHT
    , KORL_KEY_SEMICOLON
    , KORL_KEY_QUOTE
    , KORL_KEY_GRAVE
    , KORL_KEY_TENKEYLESS_0
    , KORL_KEY_TENKEYLESS_1
    , KORL_KEY_TENKEYLESS_2
    , KORL_KEY_TENKEYLESS_3
    , KORL_KEY_TENKEYLESS_4
    , KORL_KEY_TENKEYLESS_5
    , KORL_KEY_TENKEYLESS_6
    , KORL_KEY_TENKEYLESS_7
    , KORL_KEY_TENKEYLESS_8
    , KORL_KEY_TENKEYLESS_9
    , KORL_KEY_TENKEYLESS_MINUS
    , KORL_KEY_EQUALS
    , KORL_KEY_BACKSPACE
    , KORL_KEY_ESCAPE
    , KORL_KEY_ENTER
    , KORL_KEY_SPACE
    , KORL_KEY_TAB
    , KORL_KEY_SHIFT_LEFT
    , KORL_KEY_SHIFT_RIGHT
    , KORL_KEY_CONTROL_LEFT
    , KORL_KEY_CONTROL_RIGHT
    , KORL_KEY_ALT_LEFT
    , KORL_KEY_ALT_RIGHT
    , KORL_KEY_F1
    , KORL_KEY_F2
    , KORL_KEY_F3
    , KORL_KEY_F4
    , KORL_KEY_F5
    , KORL_KEY_F6
    , KORL_KEY_F7
    , KORL_KEY_F8
    , KORL_KEY_F9
    , KORL_KEY_F10
    , KORL_KEY_F11
    , KORL_KEY_F12
    , KORL_KEY_ARROW_UP
    , KORL_KEY_ARROW_DOWN
    , KORL_KEY_ARROW_LEFT
    , KORL_KEY_ARROW_RIGHT
    , KORL_KEY_INSERT
    , KORL_KEY_DELETE
    , KORL_KEY_HOME
    , KORL_KEY_END
    , KORL_KEY_PAGE_UP
    , KORL_KEY_PAGE_DOWN
    , KORL_KEY_NUMPAD_0
    , KORL_KEY_NUMPAD_1
    , KORL_KEY_NUMPAD_2
    , KORL_KEY_NUMPAD_3
    , KORL_KEY_NUMPAD_4
    , KORL_KEY_NUMPAD_5
    , KORL_KEY_NUMPAD_6
    , KORL_KEY_NUMPAD_7
    , KORL_KEY_NUMPAD_8
    , KORL_KEY_NUMPAD_9
    , KORL_KEY_NUMPAD_PERIOD
    , KORL_KEY_NUMPAD_DIVIDE
    , KORL_KEY_NUMPAD_MULTIPLY
    , KORL_KEY_NUMPAD_MINUS
    , KORL_KEY_NUMPAD_ADD
    , KORL_KEY_COUNT// keep last!
} Korl_KeyboardCode;

typedef enum Korl_MouseButton
    { KORL_MOUSE_BUTTON_LEFT
    , KORL_MOUSE_BUTTON_MIDDLE
    , KORL_MOUSE_BUTTON_RIGHT
    , KORL_MOUSE_BUTTON_X1
    , KORL_MOUSE_BUTTON_X2
    , KORL_MOUSE_BUTTON_COUNT
} Korl_MouseButton;

typedef enum Korl_MouseEventType
    { KORL_MOUSE_EVENT_BUTTON_DOWN
    , KORL_MOUSE_EVENT_BUTTON_UP
    , KORL_MOUSE_EVENT_WHEEL
    , KORL_MOUSE_EVENT_HWHEEL
    , KORL_MOUSE_EVENT_MOVE
} Korl_MouseEventType;

typedef struct Korl_MouseEvent
{
    Korl_MouseEventType type;
    i32 x; // Mouse x position, measured from bottom-left. Positive is right.
    i32 y; // Mouse y position, measured from bottom-left. Positive is up.
    union
    {
        Korl_MouseButton button;
        i32 wheel;
    } which;
} Korl_MouseEvent;

#define KORL_PLATFORM_GUI_SET_FONT_ASSET(name)       void name(const wchar_t* fontAssetName)
#define KORL_PLATFORM_GUI_WINDOW_BEGIN(name)         void name(const wchar_t* identifier, bool* out_isOpen, Korl_Gui_Window_Style_Flags styleFlags)
#define KORL_PLATFORM_GUI_WINDOW_END(name)           void name(void)
#define KORL_PLATFORM_GUI_WIDGET_TEXT_FORMAT(name)   void name(const wchar_t* textFormat, ...)
#define KORL_PLATFORM_GUI_WIDGET_BUTTON_FORMAT(name) u8   name(const wchar_t* textFormat, ...)
enum KorlEnumLogLevel
    { KORL_LOG_LEVEL_ASSERT
    , KORL_LOG_LEVEL_ERROR
    , KORL_LOG_LEVEL_WARNING
    , KORL_LOG_LEVEL_INFO
    , KORL_LOG_LEVEL_VERBOSE };
/** Note that `logLevel` must be passed as the final word of the associated 
 * identifier in the `KorlEnumLogLevel` enumeration.
 * example usage: 
 *  korl_shared_const wchar_t*const USER_NAME = L"Kyle";
 *  korl_log(INFO, "hey there, %ls!", USER_NAME); */
#define KORL_PLATFORM_LOG(name) void name(\
    unsigned variadicArgumentCount, enum KorlEnumLogLevel logLevel, \
    const wchar_t* cStringFileName, const wchar_t* cStringFunctionName, \
    int lineNumber, const wchar_t* format, ...)
#define KORL_PLATFORM_ASSERT_FAILURE(name) void name(\
    const wchar_t* conditionString, const wchar_t* cStringFileName, \
    const wchar_t* cStringFunctionName, int lineNumber)
#define KORL_PLATFORM_MEMORY_ZERO(name) void name(void* memory, u$ bytes)
#define KORL_PLATFORM_MEMORY_COPY(name) void name(void* destination, const void* source, u$ bytes)
#define KORL_PLATFORM_MEMORY_MOVE(name) void name(void* destination, const void* source, u$ bytes)
typedef u16 Korl_Memory_AllocatorHandle;
typedef enum Korl_Memory_AllocatorType
    { KORL_MEMORY_ALLOCATOR_TYPE_LINEAR
} Korl_Memory_AllocatorType;
typedef enum Korl_Memory_AllocatorFlags
    { KORL_MEMORY_ALLOCATOR_FLAGS_NONE                        = 0
    , KORL_MEMORY_ALLOCATOR_FLAG_DISABLE_THREAD_SAFETY_CHECKS = 1 << 0
    , KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME            = 1 << 1
    , KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE         = 1 << 2
} Korl_Memory_AllocatorFlags;
/** As of right now, the smallest possible value for \c maxBytes for a linear 
 * allocator is 16 kilobytes (4 pages), since two pages are required for the 
 * allocator struct, and a minimum of two pages are required for each allocation 
 * (one for the meta data page, and one for the actual allocation itself).  
 * This behavior is subject to change in the future. 
 * \param address [OPTIONAL] the allocator itself will attempt to be placed at 
 * the specified virtual address.  If this value is \c NULL , the operating 
 * system will choose this address for us. */
#define KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR(name)     Korl_Memory_AllocatorHandle name(Korl_Memory_AllocatorType type, u$ maxBytes, const wchar_t* allocatorName, Korl_Memory_AllocatorFlags flags, void* address)
/** \param address [OPTIONAL] the allocation will be placed at this exact 
 * address in the allocator.  Safety checks are performed to ensure that the 
 * specified address is valid (within allocator address range, not overlapping 
 * other allocations).  If this value is \c NULL , the allocator will choose the 
 * address of the allocation automatically (as you would typically expect). */
#define KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE(name)   void*                       name(Korl_Memory_AllocatorHandle handle, u$ bytes, const wchar_t* file, int line, void* address)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(name) void*                       name(Korl_Memory_AllocatorHandle handle, void* allocation, u$ bytes, const wchar_t* file, int line)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_FREE(name)       void                        name(Korl_Memory_AllocatorHandle handle, void* allocation, const wchar_t* file, int line)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY(name)      void                        name(Korl_Memory_AllocatorHandle handle)
typedef u16             Korl_Vulkan_VertexIndex;
typedef Korl_Math_V3f32 Korl_Vulkan_Position;
typedef Korl_Math_V2f32 Korl_Vulkan_Uv;
typedef Korl_Math_V4u8  Korl_Vulkan_Color4u8;
typedef u16             Korl_Vulkan_TextureHandle;/** A value of \c 0 is designated as an INVALID texture handle */
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
typedef enum Korl_Gfx_Batch_Flags
{
    KORL_GFX_BATCH_FLAG_NONE = 0, 
    KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST = 1 << 0,
    KORL_GFX_BATCH_FLAG_DISABLE_BLENDING = 1 << 1
} Korl_Gfx_Batch_Flags;
typedef struct Korl_Gfx_Batch
{
    Korl_Memory_AllocatorHandle allocatorHandle;// storing the allocator handle allows us to simplify API which expands the capacity of the batch (example: add new line to a line batch)
    Korl_Vulkan_PrimitiveType primitiveType;
    Korl_Vulkan_Position _position;
    Korl_Vulkan_Position _scale;
    Korl_Math_Quaternion _rotation;
    //KORL-PERFORMANCE-000-000-017: GFX; separate batch capacity with batch vertex/index counts
    u32 _vertexIndexCount;
    u32 _vertexCount;
    f32 _textPixelHeight;
    wchar_t* _assetNameTexture;
    //KORL-PERFORMANCE-000-000-002
    wchar_t* _assetNameFont;
    //KORL-PERFORMANCE-000-000-003
    wchar_t* _text;
    Korl_Vulkan_BlendOperation opColor;       // only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendFactor factorColorSource;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendFactor factorColorTarget;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendOperation opAlpha;       // only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendFactor factorAlphaSource;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_BlendFactor factorAlphaTarget;// only valid when batched without KORL_GFX_BATCH_FLAG_DISABLE_BLENDING
    Korl_Vulkan_TextureHandle _fontTextureHandle;
    Korl_Vulkan_VertexIndex* _vertexIndices;
    Korl_Vulkan_Position*    _vertexPositions;
    Korl_Vulkan_Color4u8*    _vertexColors;
    Korl_Vulkan_Uv*          _vertexUvs;
} Korl_Gfx_Batch;
#define KORL_PLATFORM_GFX_CREATE_CAMERA_FOV(name)                Korl_Gfx_Camera    name(f32 fovHorizonDegrees, f32 clipNear, f32 clipFar, Korl_Math_V3f32 position, Korl_Math_V3f32 target)
#define KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO(name)              Korl_Gfx_Camera    name(f32 clipDepth)
#define KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO_FIXED_HEIGHT(name) Korl_Gfx_Camera    name(f32 fixedHeight, f32 clipDepth)
#define KORL_PLATFORM_GFX_CAMERA_FOV_ROTATE_AROUND_TARGET(name)  void               name(Korl_Gfx_Camera*const context, Korl_Math_V3f32 axisOfRotation, f32 radians)
#define KORL_PLATFORM_GFX_USE_CAMERA(name)                       void               name(Korl_Gfx_Camera camera)
#define KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR(name)               void               name(Korl_Gfx_Camera*const context, f32 x, f32 y, f32 sizeX, f32 sizeY)
#define KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR_PERCENT(name)       void               name(Korl_Gfx_Camera*const context, f32 viewportRatioX, f32 viewportRatioY, f32 viewportRatioWidth, f32 viewportRatioHeight)
#define KORL_PLATFORM_GFX_CAMERA_ORTHO_SET_ORIGIN_ANCHOR(name)   void               name(Korl_Gfx_Camera*const context, f32 swapchainSizeRatioOriginX, f32 swapchainSizeRatioOriginY)
#define KORL_PLATFORM_GFX_BATCH(name)                            void               name(Korl_Gfx_Batch*const batch, Korl_Gfx_Batch_Flags flags)
#define KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_TEXTURED(name)  Korl_Gfx_Batch*    name(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, const wchar_t* assetNameTexture)
#define KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_COLORED(name)   Korl_Gfx_Batch*    name(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, Korl_Math_V2f32 localOriginNormal, Korl_Vulkan_Color4u8 color)
#define KORL_PLATFORM_GFX_CREATE_BATCH_CIRCLE(name)              Korl_Gfx_Batch*    name(Korl_Memory_AllocatorHandle allocatorHandle, f32 radius, u32 pointCount, Korl_Vulkan_Color4u8 color)
#define KORL_PLATFORM_GFX_CREATE_BATCH_TRIANGLES(name)           Korl_Gfx_Batch*    name(Korl_Memory_AllocatorHandle allocatorHandle, u32 triangleCount)
#define KORL_PLATFORM_GFX_CREATE_BATCH_LINES(name)               Korl_Gfx_Batch*    name(Korl_Memory_AllocatorHandle allocatorHandle, u32 lineCount)
#define KORL_PLATFORM_GFX_CREATE_BATCH_TEXT(name)                Korl_Gfx_Batch*    name(Korl_Memory_AllocatorHandle allocatorHandle, const wchar_t* assetNameFont, const wchar_t* text, f32 textPixelHeight)
#define KORL_PLATFORM_GFX_BATCH_SET_BLEND_STATE(name)            void               name(Korl_Gfx_Batch*const context, Korl_Vulkan_BlendOperation opColor, Korl_Vulkan_BlendFactor factorColorSource, Korl_Vulkan_BlendFactor factorColorTarget, Korl_Vulkan_BlendOperation opAlpha, Korl_Vulkan_BlendFactor factorAlphaSource, Korl_Vulkan_BlendFactor factorAlphaTarget)
#define KORL_PLATFORM_GFX_BATCH_SET_POSITION(name)               void               name(Korl_Gfx_Batch*const context, Korl_Vulkan_Position position)
#define KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D(name)            void               name(Korl_Gfx_Batch*const context, f32 x, f32 y)
#define KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D_V2F32(name)      void               name(Korl_Gfx_Batch*const context, Korl_Math_V2f32 position)
#define KORL_PLATFORM_GFX_BATCH_SET_SCALE(name)                  void               name(Korl_Gfx_Batch*const context, Korl_Vulkan_Position scale)
#define KORL_PLATFORM_GFX_BATCH_SET_QUATERNION(name)             void               name(Korl_Gfx_Batch*const context, Korl_Math_Quaternion quaternion)
#define KORL_PLATFORM_GFX_BATCH_SET_ROTATION(name)               void               name(Korl_Gfx_Batch*const context, Korl_Math_V3f32 axisOfRotation, f32 radians)
#define KORL_PLATFORM_GFX_BATCH_SET_VERTEX_COLOR(name)           void               name(Korl_Gfx_Batch*const context, u32 vertexIndex, Korl_Vulkan_Color4u8 color)
#define KORL_PLATFORM_GFX_BATCH_ADD_LINE(name)                   void               name(Korl_Gfx_Batch**const pContext, Korl_Vulkan_Position p0, Korl_Vulkan_Position p1, Korl_Vulkan_Color4u8 color0, Korl_Vulkan_Color4u8 color1)
#define KORL_PLATFORM_GFX_BATCH_SET_LINE(name)                   void               name(Korl_Gfx_Batch*const context, u32 lineIndex, Korl_Vulkan_Position p0, Korl_Vulkan_Position p1, Korl_Vulkan_Color4u8 color)
#define KORL_PLATFORM_GFX_BATCH_TEXT_GET_AABB(name)              Korl_Math_Aabb2f32 name(Korl_Gfx_Batch*const batchContext)
#define KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_SIZE(name)         void               name(Korl_Gfx_Batch*const context, Korl_Math_V2f32 size)
#define KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_COLOR(name)        void               name(Korl_Gfx_Batch*const context, Korl_Vulkan_Color4u8 color)
typedef KORL_PLATFORM_ASSERT_FAILURE                      (fnSig_korlPlatformAssertFailure);
typedef KORL_PLATFORM_LOG                                 (fnSig_korlPlatformLog);
typedef KORL_PLATFORM_GET_TIMESTAMP                       (fnSig_korl_timeStamp);
typedef KORL_PLATFORM_SECONDS_SINCE_TIMESTAMP             (fnSig_korl_time_secondsSinceTimeStamp);
typedef KORL_PLATFORM_MEMORY_ZERO                         (fnSig_korl_memory_zero);
typedef KORL_PLATFORM_MEMORY_COPY                         (fnSig_korl_memory_copy);
typedef KORL_PLATFORM_MEMORY_MOVE                         (fnSig_korl_memory_move);
typedef KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR             (fnSig_korl_memory_allocator_create);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE           (fnSig_korl_memory_allocator_allocate);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE         (fnSig_korl_memory_allocator_reallocate);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_FREE               (fnSig_korl_memory_allocator_free);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY              (fnSig_korl_memory_allocator_empty);
typedef KORL_PLATFORM_GFX_CREATE_CAMERA_FOV               (fnSig_korl_gfx_createCameraFov);
typedef KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO             (fnSig_korl_gfx_createCameraOrtho);
typedef KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO_FIXED_HEIGHT(fnSig_korl_gfx_createCameraOrthoFixedHeight);
typedef KORL_PLATFORM_GFX_CAMERA_FOV_ROTATE_AROUND_TARGET (fnSig_korl_gfx_cameraFov_rotateAroundTarget);
typedef KORL_PLATFORM_GFX_USE_CAMERA                      (fnSig_korl_gfx_useCamera);
typedef KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR              (fnSig_korl_gfx_cameraSetScissor);
typedef KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR_PERCENT      (fnSig_korl_gfx_cameraSetScissorPercent);
typedef KORL_PLATFORM_GFX_CAMERA_ORTHO_SET_ORIGIN_ANCHOR  (fnSig_korl_gfx_cameraOrthoSetOriginAnchor);
typedef KORL_PLATFORM_GFX_BATCH                           (fnSig_korl_gfx_batch);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_TEXTURED (fnSig_korl_gfx_createBatchRectangleTextured);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_COLORED  (fnSig_korl_gfx_createBatchRectangleColored);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_CIRCLE             (fnSig_korl_gfx_createBatchCircle);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_TRIANGLES          (fnSig_korl_gfx_createBatchTriangles);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_LINES              (fnSig_korl_gfx_createBatchLines);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_TEXT               (fnSig_korl_gfx_createBatchText);
typedef KORL_PLATFORM_GFX_BATCH_SET_BLEND_STATE           (fnSig_korl_gfx_batchSetBlendState);
typedef KORL_PLATFORM_GFX_BATCH_SET_POSITION              (fnSig_korl_gfx_batchSetPosition);
typedef KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D           (fnSig_korl_gfx_batchSetPosition2d);
typedef KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D_V2F32     (fnSig_korl_gfx_batchSetPosition2dV2f32);
typedef KORL_PLATFORM_GFX_BATCH_SET_SCALE                 (fnSig_korl_gfx_batchSetScale);
typedef KORL_PLATFORM_GFX_BATCH_SET_QUATERNION            (fnSig_korl_gfx_batchSetQuaternion);
typedef KORL_PLATFORM_GFX_BATCH_SET_ROTATION              (fnSig_korl_gfx_batchSetRotation);
typedef KORL_PLATFORM_GFX_BATCH_SET_VERTEX_COLOR          (fnSig_korl_gfx_batchSetVertexColor);
typedef KORL_PLATFORM_GFX_BATCH_ADD_LINE                  (fnSig_korl_gfx_batchAddLine);
typedef KORL_PLATFORM_GFX_BATCH_SET_LINE                  (fnSig_korl_gfx_batchSetLine);
typedef KORL_PLATFORM_GFX_BATCH_TEXT_GET_AABB             (fnSig_korl_gfx_batchTextGetAabb);
typedef KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_SIZE        (fnSig_korl_gfx_batchRectangleSetSize);
typedef KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_COLOR       (fnSig_korl_gfx_batchRectangleSetColor);
typedef KORL_PLATFORM_GUI_SET_FONT_ASSET                  (fnSig_korl_gui_setFontAsset);
typedef KORL_PLATFORM_GUI_WINDOW_BEGIN                    (fnSig_korl_gui_windowBegin);
typedef KORL_PLATFORM_GUI_WINDOW_END                      (fnSig_korl_gui_windowEnd);
typedef KORL_PLATFORM_GUI_WIDGET_TEXT_FORMAT              (fnSig_korl_gui_widgetTextFormat);
typedef KORL_PLATFORM_GUI_WIDGET_BUTTON_FORMAT            (fnSig_korl_gui_widgetButtonFormat);
#define KORL_INTERFACE_PLATFORM_API_DECLARE \
    fnSig_korlPlatformAssertFailure*             _korl_crash_assertConditionFailed;\
    fnSig_korlPlatformLog*                       _korl_log_variadic;\
    fnSig_korl_timeStamp*                        korl_timeStamp;\
    fnSig_korl_time_secondsSinceTimeStamp*       korl_time_secondsSinceTimeStamp;\
    fnSig_korl_memory_zero*                      korl_memory_zero;\
    fnSig_korl_memory_copy*                      korl_memory_copy;\
    fnSig_korl_memory_move*                      korl_memory_move;\
    fnSig_korl_memory_allocator_create*          korl_memory_allocator_create;\
    fnSig_korl_memory_allocator_allocate*        korl_memory_allocator_allocate;\
    fnSig_korl_memory_allocator_reallocate*      korl_memory_allocator_reallocate;\
    fnSig_korl_memory_allocator_free*            korl_memory_allocator_free;\
    fnSig_korl_memory_allocator_empty*           korl_memory_allocator_empty;\
    fnSig_korl_gfx_createCameraFov*              korl_gfx_createCameraFov;\
    fnSig_korl_gfx_createCameraOrtho*            korl_gfx_createCameraOrtho;\
    fnSig_korl_gfx_createCameraOrthoFixedHeight* korl_gfx_createCameraOrthoFixedHeight;\
    fnSig_korl_gfx_cameraFov_rotateAroundTarget* korl_gfx_cameraFov_rotateAroundTarget;\
    fnSig_korl_gfx_useCamera*                    korl_gfx_useCamera;\
    fnSig_korl_gfx_cameraSetScissor*             korl_gfx_cameraSetScissor;\
    fnSig_korl_gfx_cameraSetScissorPercent*      korl_gfx_cameraSetScissorPercent;\
    fnSig_korl_gfx_cameraOrthoSetOriginAnchor*   korl_gfx_cameraOrthoSetOriginAnchor;\
    fnSig_korl_gfx_batch*                        korl_gfx_batch;\
    fnSig_korl_gfx_createBatchRectangleTextured* korl_gfx_createBatchRectangleTextured;\
    fnSig_korl_gfx_createBatchRectangleColored*  korl_gfx_createBatchRectangleColored;\
    fnSig_korl_gfx_createBatchCircle*            korl_gfx_createBatchCircle;\
    fnSig_korl_gfx_createBatchTriangles*         korl_gfx_createBatchTriangles;\
    fnSig_korl_gfx_createBatchLines*             korl_gfx_createBatchLines;\
    fnSig_korl_gfx_createBatchText*              korl_gfx_createBatchText;\
    fnSig_korl_gfx_batchSetBlendState*           korl_gfx_batchSetBlendState;\
    fnSig_korl_gfx_batchSetPosition*             korl_gfx_batchSetPosition;\
    fnSig_korl_gfx_batchSetPosition2d*           korl_gfx_batchSetPosition2d;\
    fnSig_korl_gfx_batchSetPosition2dV2f32*      korl_gfx_batchSetPosition2dV2f32;\
    fnSig_korl_gfx_batchSetScale*                korl_gfx_batchSetScale;\
    fnSig_korl_gfx_batchSetQuaternion*           korl_gfx_batchSetQuaternion;\
    fnSig_korl_gfx_batchSetRotation*             korl_gfx_batchSetRotation;\
    fnSig_korl_gfx_batchSetVertexColor*          korl_gfx_batchSetVertexColor;\
    fnSig_korl_gfx_batchAddLine*                 korl_gfx_batchAddLine;\
    fnSig_korl_gfx_batchSetLine*                 korl_gfx_batchSetLine;\
    fnSig_korl_gfx_batchTextGetAabb*             korl_gfx_batchTextGetAabb;\
    fnSig_korl_gfx_batchRectangleSetSize*        korl_gfx_batchRectangleSetSize;\
    fnSig_korl_gfx_batchRectangleSetColor*       korl_gfx_batchRectangleSetColor;\
    fnSig_korl_gui_setFontAsset*                 korl_gui_setFontAsset;\
    fnSig_korl_gui_windowBegin*                  korl_gui_windowBegin;\
    fnSig_korl_gui_windowEnd*                    korl_gui_windowEnd;\
    fnSig_korl_gui_widgetTextFormat*             korl_gui_widgetTextFormat;\
    fnSig_korl_gui_widgetButtonFormat*           korl_gui_widgetButtonFormat;
#define KORL_INTERFACE_PLATFORM_API_SET(apiVariableName) \
    (apiVariableName)._korl_crash_assertConditionFailed     = _korl_crash_assertConditionFailed;\
    (apiVariableName)._korl_log_variadic                    = _korl_log_variadic;\
    (apiVariableName).korl_timeStamp                        = korl_timeStamp;\
    (apiVariableName).korl_time_secondsSinceTimeStamp       = korl_time_secondsSinceTimeStamp;\
    (apiVariableName).korl_memory_zero                      = korl_memory_zero;\
    (apiVariableName).korl_memory_copy                      = korl_memory_copy;\
    (apiVariableName).korl_memory_move                      = korl_memory_move;\
    (apiVariableName).korl_memory_allocator_create          = korl_memory_allocator_create;\
    (apiVariableName).korl_memory_allocator_allocate        = korl_memory_allocator_allocate;\
    (apiVariableName).korl_memory_allocator_reallocate      = korl_memory_allocator_reallocate;\
    (apiVariableName).korl_memory_allocator_free            = korl_memory_allocator_free;\
    (apiVariableName).korl_memory_allocator_empty           = korl_memory_allocator_empty;\
    (apiVariableName).korl_gfx_createCameraFov              = korl_gfx_createCameraFov;\
    (apiVariableName).korl_gfx_createCameraOrtho            = korl_gfx_createCameraOrtho;\
    (apiVariableName).korl_gfx_createCameraOrthoFixedHeight = korl_gfx_createCameraOrthoFixedHeight;\
    (apiVariableName).korl_gfx_cameraFov_rotateAroundTarget = korl_gfx_cameraFov_rotateAroundTarget;\
    (apiVariableName).korl_gfx_useCamera                    = korl_gfx_useCamera;\
    (apiVariableName).korl_gfx_cameraSetScissor             = korl_gfx_cameraSetScissor;\
    (apiVariableName).korl_gfx_cameraSetScissorPercent      = korl_gfx_cameraSetScissorPercent;\
    (apiVariableName).korl_gfx_cameraOrthoSetOriginAnchor   = korl_gfx_cameraOrthoSetOriginAnchor;\
    (apiVariableName).korl_gfx_batch                        = korl_gfx_batch;\
    (apiVariableName).korl_gfx_createBatchRectangleTextured = korl_gfx_createBatchRectangleTextured;\
    (apiVariableName).korl_gfx_createBatchRectangleColored  = korl_gfx_createBatchRectangleColored;\
    (apiVariableName).korl_gfx_createBatchCircle            = korl_gfx_createBatchCircle;\
    (apiVariableName).korl_gfx_createBatchTriangles         = korl_gfx_createBatchTriangles;\
    (apiVariableName).korl_gfx_createBatchLines             = korl_gfx_createBatchLines;\
    (apiVariableName).korl_gfx_createBatchText              = korl_gfx_createBatchText;\
    (apiVariableName).korl_gfx_batchSetBlendState           = korl_gfx_batchSetBlendState;\
    (apiVariableName).korl_gfx_batchSetPosition             = korl_gfx_batchSetPosition;\
    (apiVariableName).korl_gfx_batchSetPosition2d           = korl_gfx_batchSetPosition2d;\
    (apiVariableName).korl_gfx_batchSetPosition2dV2f32      = korl_gfx_batchSetPosition2dV2f32;\
    (apiVariableName).korl_gfx_batchSetScale                = korl_gfx_batchSetScale;\
    (apiVariableName).korl_gfx_batchSetQuaternion           = korl_gfx_batchSetQuaternion;\
    (apiVariableName).korl_gfx_batchSetRotation             = korl_gfx_batchSetRotation;\
    (apiVariableName).korl_gfx_batchSetVertexColor          = korl_gfx_batchSetVertexColor;\
    (apiVariableName).korl_gfx_batchAddLine                 = korl_gfx_batchAddLine;\
    (apiVariableName).korl_gfx_batchSetLine                 = korl_gfx_batchSetLine;\
    (apiVariableName).korl_gfx_batchTextGetAabb             = korl_gfx_batchTextGetAabb;\
    (apiVariableName).korl_gfx_batchRectangleSetSize        = korl_gfx_batchRectangleSetSize;\
    (apiVariableName).korl_gfx_batchRectangleSetColor       = korl_gfx_batchRectangleSetColor;\
    (apiVariableName).korl_gui_setFontAsset                 = korl_gui_setFontAsset;\
    (apiVariableName).korl_gui_windowBegin                  = korl_gui_windowBegin;\
    (apiVariableName).korl_gui_windowEnd                    = korl_gui_windowEnd;\
    (apiVariableName).korl_gui_widgetTextFormat             = korl_gui_widgetTextFormat;\
    (apiVariableName).korl_gui_widgetButtonFormat           = korl_gui_widgetButtonFormat;
#define KORL_INTERFACE_PLATFORM_API_GET(apiVariableName) \
    _korl_crash_assertConditionFailed     = (apiVariableName)._korl_crash_assertConditionFailed;\
    _korl_log_variadic                    = (apiVariableName)._korl_log_variadic;\
    korl_timeStamp                        = (apiVariableName).korl_timeStamp;\
    korl_time_secondsSinceTimeStamp       = (apiVariableName).korl_time_secondsSinceTimeStamp;\
    korl_memory_zero                      = (apiVariableName).korl_memory_zero;\
    korl_memory_copy                      = (apiVariableName).korl_memory_copy;\
    korl_memory_move                      = (apiVariableName).korl_memory_move;\
    korl_memory_allocator_create          = (apiVariableName).korl_memory_allocator_create;\
    korl_memory_allocator_allocate        = (apiVariableName).korl_memory_allocator_allocate;\
    korl_memory_allocator_reallocate      = (apiVariableName).korl_memory_allocator_reallocate;\
    korl_memory_allocator_free            = (apiVariableName).korl_memory_allocator_free;\
    korl_memory_allocator_empty           = (apiVariableName).korl_memory_allocator_empty;\
    korl_gfx_createCameraFov              = (apiVariableName).korl_gfx_createCameraFov;\
    korl_gfx_createCameraOrtho            = (apiVariableName).korl_gfx_createCameraOrtho;\
    korl_gfx_createCameraOrthoFixedHeight = (apiVariableName).korl_gfx_createCameraOrthoFixedHeight;\
    korl_gfx_cameraFov_rotateAroundTarget = (apiVariableName).korl_gfx_cameraFov_rotateAroundTarget;\
    korl_gfx_useCamera                    = (apiVariableName).korl_gfx_useCamera;\
    korl_gfx_cameraSetScissor             = (apiVariableName).korl_gfx_cameraSetScissor;\
    korl_gfx_cameraSetScissorPercent      = (apiVariableName).korl_gfx_cameraSetScissorPercent;\
    korl_gfx_cameraOrthoSetOriginAnchor   = (apiVariableName).korl_gfx_cameraOrthoSetOriginAnchor;\
    korl_gfx_batch                        = (apiVariableName).korl_gfx_batch;\
    korl_gfx_createBatchRectangleTextured = (apiVariableName).korl_gfx_createBatchRectangleTextured;\
    korl_gfx_createBatchRectangleColored  = (apiVariableName).korl_gfx_createBatchRectangleColored;\
    korl_gfx_createBatchCircle            = (apiVariableName).korl_gfx_createBatchCircle;\
    korl_gfx_createBatchTriangles         = (apiVariableName).korl_gfx_createBatchTriangles;\
    korl_gfx_createBatchLines             = (apiVariableName).korl_gfx_createBatchLines;\
    korl_gfx_createBatchText              = (apiVariableName).korl_gfx_createBatchText;\
    korl_gfx_batchSetBlendState           = (apiVariableName).korl_gfx_batchSetBlendState;\
    korl_gfx_batchSetPosition             = (apiVariableName).korl_gfx_batchSetPosition;\
    korl_gfx_batchSetPosition2d           = (apiVariableName).korl_gfx_batchSetPosition2d;\
    korl_gfx_batchSetPosition2dV2f32      = (apiVariableName).korl_gfx_batchSetPosition2dV2f32;\
    korl_gfx_batchSetScale                = (apiVariableName).korl_gfx_batchSetScale;\
    korl_gfx_batchSetQuaternion           = (apiVariableName).korl_gfx_batchSetQuaternion;\
    korl_gfx_batchSetRotation             = (apiVariableName).korl_gfx_batchSetRotation;\
    korl_gfx_batchSetVertexColor          = (apiVariableName).korl_gfx_batchSetVertexColor;\
    korl_gfx_batchAddLine                 = (apiVariableName).korl_gfx_batchAddLine;\
    korl_gfx_batchSetLine                 = (apiVariableName).korl_gfx_batchSetLine;\
    korl_gfx_batchTextGetAabb             = (apiVariableName).korl_gfx_batchTextGetAabb;\
    korl_gfx_batchRectangleSetSize        = (apiVariableName).korl_gfx_batchRectangleSetSize;\
    korl_gfx_batchRectangleSetColor       = (apiVariableName).korl_gfx_batchRectangleSetColor;\
    korl_gui_setFontAsset                 = (apiVariableName).korl_gui_setFontAsset;\
    korl_gui_windowBegin                  = (apiVariableName).korl_gui_windowBegin;\
    korl_gui_windowEnd                    = (apiVariableName).korl_gui_windowEnd;\
    korl_gui_widgetTextFormat             = (apiVariableName).korl_gui_widgetTextFormat;\
    korl_gui_widgetButtonFormat           = (apiVariableName).korl_gui_widgetButtonFormat;
typedef struct KorlPlatformApi
{
    KORL_INTERFACE_PLATFORM_API_DECLARE
} KorlPlatformApi;
