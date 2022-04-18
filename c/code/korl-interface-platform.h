#pragma once
#include "korl-globalDefines.h"
#include "korl-math.h"
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
enum KorlEnumLogLevel
    { KORL_LOG_LEVEL_ERROR
    , KORL_LOG_LEVEL_WARNING
    , KORL_LOG_LEVEL_INFO
    , KORL_LOG_LEVEL_VERBOSE };
#define KORL_PLATFORM_LOG(name) void name(\
    unsigned variadicArgumentCount, enum KorlEnumLogLevel logLevel, \
    const wchar_t* cStringFileName, const wchar_t* cStringFunctionName, \
    int lineNumber, const wchar_t* format, ...)
#define KORL_PLATFORM_ASSERT_FAILURE(name) void name(\
    const wchar_t* conditionString, const wchar_t* cStringFileName, int lineNumber)
typedef u16 Korl_Memory_AllocatorHandle;
typedef enum Korl_Memory_AllocatorType
{
    KORL_MEMORY_ALLOCATOR_TYPE_LINEAR
} Korl_Memory_AllocatorType;
#define KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR(name)     Korl_Memory_AllocatorHandle name(Korl_Memory_AllocatorType type, u$ maxBytes)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE(name)   void* name(Korl_Memory_AllocatorHandle handle, u$ bytes, wchar_t* file, int line)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(name) void* name(Korl_Memory_AllocatorHandle handle, void* allocation, u$ bytes, wchar_t* file, int line)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_FREE(name)       void  name(Korl_Memory_AllocatorHandle handle, void* allocation, wchar_t* file, int line)
#define KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY(name)      void  name(Korl_Memory_AllocatorHandle handle)
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
    KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST = 1 << 0
} Korl_Gfx_Batch_Flags;
typedef struct Korl_Gfx_Batch Korl_Gfx_Batch;
#define KORL_GFX_CREATE_CAMERA_ORTHO(name)              Korl_Gfx_Camera name(f32 clipDepth)
#define KORL_GFX_USE_CAMERA(name)                       void name(Korl_Gfx_Camera camera)
#define KORL_GFX_BATCH(name)                            void name(Korl_Gfx_Batch*const batch, Korl_Gfx_Batch_Flags flags)
#define KORL_GFX_CREATE_BATCH_RECTANGLE_TEXTURED(name)  Korl_Gfx_Batch* name(Korl_Memory_AllocatorHandle allocatorHandle, Korl_Math_V2f32 size, const wchar_t* assetNameTexture)
typedef KORL_PLATFORM_LOG                        (fnSig_korlPlatformLog);
typedef KORL_PLATFORM_ASSERT_FAILURE             (fnSig_korlPlatformAssertFailure);
typedef KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR    (fnSig_korl_memory_allocator_create);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE  (fnSig_korl_memory_allocator_allocate);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE(fnSig_korl_memory_allocator_reallocate);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_FREE      (fnSig_korl_memory_allocator_free);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY     (fnSig_korl_memory_allocator_empty);
typedef KORL_GFX_CREATE_CAMERA_ORTHO             (fnSig_korlGfxCreateCameraOrtho);
typedef KORL_GFX_USE_CAMERA                      (fnSig_korlGfxUseCamera);
typedef KORL_GFX_BATCH                           (fnSig_korlGfxBatch);
typedef KORL_GFX_CREATE_BATCH_RECTANGLE_TEXTURED (fnSig_korlGfxCreateBatchRectangleTextured);
typedef struct KorlPlatformApi
{
    fnSig_korlPlatformAssertFailure*           korl_assertConditionFailed;
    fnSig_korlPlatformLog*                     korl_logVariadicArguments;
    fnSig_korlGfxCreateCameraOrtho*            korl_gfx_createCameraOrtho;
    fnSig_korlGfxUseCamera*                    korl_gfx_useCamera;
    fnSig_korlGfxBatch*                        korl_gfx_batch;
    fnSig_korlGfxCreateBatchRectangleTextured* korl_gfx_createBatchRectangleTextured;
} KorlPlatformApi;
