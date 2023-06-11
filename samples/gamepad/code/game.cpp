#include "korl-interface-game.h"
#include "utility/korl-utility-stb-ds.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-stringPool.h"
#include "utility/korl-logConsole.h"
#include "utility/korl-utility-gfx.h"
typedef struct Memory
{
    Korl_Memory_AllocatorHandle allocatorHeap;
    Korl_Memory_AllocatorHandle allocatorStack;
    bool                        continueRunning;
    Korl_StringPool             stringPool;
    Korl_LogConsole             logConsole;
    struct
    {
        bool buttons[KORL_GAMEPAD_BUTTON_ENUM_COUNT];
        f32  axes[KORL_GAMEPAD_AXIS_ENUM_COUNT];
    } gamepadState;
} Memory;
korl_global_variable Memory* memory;
KORL_EXPORT KORL_GAME_INITIALIZE(korl_game_initialize)
{
    korl_platform_getApi(korlApi);
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(8);
    const Korl_Memory_AllocatorHandle allocatorHeap = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"game", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    memory = KORL_C_CAST(Memory*, korl_allocate(allocatorHeap, sizeof(Memory)));
    memory->allocatorHeap   = allocatorHeap;
    memory->allocatorStack  = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"game-stack", KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME, &heapCreateInfo);
    memory->continueRunning = true;
    memory->stringPool      = korl_stringPool_create(allocatorHeap);
    memory->logConsole      = korl_logConsole_create(&memory->stringPool);
    korl_gui_setFontAsset(L"data/source-sans/SourceSans3-Semibold.otf");// KORL-ISSUE-000-000-086: gfx: default font path doesn't work, since this subdirectly is unlikely in the game project
    return memory;
}
KORL_EXPORT KORL_GAME_ON_RELOAD(korl_game_onReload)
{
    korl_platform_getApi(korlApi);
    memory = KORL_C_CAST(Memory*, context);
}
KORL_EXPORT KORL_GAME_ON_KEYBOARD_EVENT(korl_game_onKeyboardEvent)
{
    if(isDown && !isRepeat)
        switch(keyCode)
        {
        case KORL_KEY_ESCAPE: memory->continueRunning = false;             break;
        case KORL_KEY_GRAVE : korl_logConsole_toggle(&memory->logConsole); break;
        default: break;
        }
}
KORL_EXPORT KORL_GAME_ON_GAMEPAD_EVENT(korl_game_onGamepadEvent)
{
    switch(gamepadEvent.type)
    {
    case KORL_GAMEPAD_EVENT_TYPE_BUTTON:{
        memory->gamepadState.buttons[gamepadEvent.subType.button.index] = gamepadEvent.subType.button.pressed;
        break;}
    case KORL_GAMEPAD_EVENT_TYPE_AXIS:{
        memory->gamepadState.axes[gamepadEvent.subType.axis.index] = gamepadEvent.subType.axis.value;
        break;}
    }
}
KORL_EXPORT KORL_GAME_UPDATE(korl_game_update)
{
    korl_logConsole_update(&memory->logConsole, deltaSeconds, korl_log_getBuffer, {windowSizeX, windowSizeY}, memory->allocatorStack);
    /* draw the gamepad state */
    korl_gfx_useCamera(korl_gfx_camera_createOrthoFixedHeight(450, 1));
    korl_shared_const f32 CIRCLE_RADIUS = 32;
    Korl_Gfx_Batch*const circle = korl_gfx_createBatchCircle(memory->allocatorStack, CIRCLE_RADIUS, 32, KORL_COLOR4U8_BLACK);
    korl_shared_const Korl_Math_V2f32 POSITION_DISPLAY_ROOT           = {0, -25};
    korl_shared_const Korl_Math_V2f32 POSITION_BUTTON_CLUSTER_LEFT    = POSITION_DISPLAY_ROOT + Korl_Math_V2f32{-200,    0};
    korl_shared_const Korl_Math_V2f32 POSITION_BUTTON_CLUSTER_RIGHT   = POSITION_DISPLAY_ROOT + Korl_Math_V2f32{ 200,    0};
    korl_shared_const Korl_Math_V2f32 POSITION_BUTTON_CLUSTER_TOP     = POSITION_DISPLAY_ROOT + Korl_Math_V2f32{   0,  100};
    korl_shared_const Korl_Math_V2f32 POSITION_BUTTON_CLUSTER_CENTER  = POSITION_DISPLAY_ROOT + Korl_Math_V2f32{   0,  -25};
    korl_shared_const Korl_Math_V2f32 POSITION_STICK_LEFT             = POSITION_DISPLAY_ROOT + Korl_Math_V2f32{-100, -125};
    korl_shared_const Korl_Math_V2f32 POSITION_STICK_RIGHT            = POSITION_DISPLAY_ROOT + Korl_Math_V2f32{ 100, -125};
    korl_shared_const Korl_Math_V2f32 BUTTON_CLUSTER_OFFSET_TOP       = {100, 50};
    korl_shared_const f32             BUTTON_CLUSTER_RADIUS           = 50;
    korl_shared_const f32             RADIANS_BUTTON_CLUSTER_LEFT[]   = {KORL_PI32/2, -KORL_PI32/2, KORL_PI32, 0};
    korl_shared_const f32             RADIANS_BUTTON_CLUSTER_RIGHT[]  = {-KORL_PI32/2, 0, KORL_PI32, KORL_PI32/2};
    korl_shared_const f32             RADIANS_BUTTON_CLUSTER_CENTER[] = {0, KORL_PI32};
    korl_shared_const Korl_Math_V2f32 OFFSETS_BUTTON_CLUSTER_TOP[]    = {{-BUTTON_CLUSTER_OFFSET_TOP.x, 0}, {BUTTON_CLUSTER_OFFSET_TOP.x, 0}
                                                                        ,{-BUTTON_CLUSTER_OFFSET_TOP.x, BUTTON_CLUSTER_OFFSET_TOP.y}, {BUTTON_CLUSTER_OFFSET_TOP.x, BUTTON_CLUSTER_OFFSET_TOP.y}};
    for(u32 i = 0; i < 4; i++)
    {
        korl_gfx_batchSetPosition2dV2f32(circle, POSITION_BUTTON_CLUSTER_LEFT + korl_math_quaternion_fromAxisRadians(KORL_MATH_V3F32_Z, RADIANS_BUTTON_CLUSTER_LEFT[i], true) * Korl_Math_V2f32{BUTTON_CLUSTER_RADIUS, 0});
        korl_gfx_batchCircleSetColor(circle, memory->gamepadState.buttons[KORL_GAMEPAD_BUTTON_CLUSTER_LEFT_UP + i] ? KORL_COLOR4U8_WHITE : KORL_COLOR4U8_BLACK);
        korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    }
    for(u32 i = 0; i < 4; i++)
    {
        korl_gfx_batchSetPosition2dV2f32(circle, POSITION_BUTTON_CLUSTER_RIGHT + korl_math_quaternion_fromAxisRadians(KORL_MATH_V3F32_Z, RADIANS_BUTTON_CLUSTER_RIGHT[i], true) * Korl_Math_V2f32{BUTTON_CLUSTER_RADIUS, 0});
        korl_gfx_batchCircleSetColor(circle, memory->gamepadState.buttons[KORL_GAMEPAD_BUTTON_CLUSTER_RIGHT_DOWN + i] ? KORL_COLOR4U8_WHITE : KORL_COLOR4U8_BLACK);
        korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    }
    for(u32 i = 0; i < 2; i++)
    {
        korl_gfx_batchSetPosition2dV2f32(circle, POSITION_BUTTON_CLUSTER_TOP + OFFSETS_BUTTON_CLUSTER_TOP[i]);
        korl_gfx_batchCircleSetColor(circle, memory->gamepadState.buttons[KORL_GAMEPAD_BUTTON_CLUSTER_TOP_LEFT + i] ? KORL_COLOR4U8_WHITE : KORL_COLOR4U8_BLACK);
        korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    }
    korl_gfx_batchSetPosition2dV2f32(circle, POSITION_BUTTON_CLUSTER_CENTER);
    korl_gfx_batchCircleSetColor(circle, memory->gamepadState.buttons[KORL_GAMEPAD_BUTTON_CLUSTER_CENTER_MIDDLE] ? KORL_COLOR4U8_WHITE : KORL_COLOR4U8_BLACK);
    korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    for(u32 i = 0; i < 2; i++)
    {
        korl_gfx_batchSetPosition2dV2f32(circle, POSITION_BUTTON_CLUSTER_CENTER + korl_math_quaternion_fromAxisRadians(KORL_MATH_V3F32_Z, RADIANS_BUTTON_CLUSTER_CENTER[i], true) * Korl_Math_V2f32{BUTTON_CLUSTER_RADIUS, 0});
        korl_gfx_batchCircleSetColor(circle, memory->gamepadState.buttons[KORL_GAMEPAD_BUTTON_CLUSTER_CENTER_RIGHT + i] ? KORL_COLOR4U8_WHITE : KORL_COLOR4U8_BLACK);
        korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    }
    {/* draw stick range backgrounds */
        korl_gfx_batchSetScale(circle, KORL_MATH_V3F32_ONE * 2);
        korl_gfx_batchCircleSetColor(circle, {10,10,10,255});
        korl_gfx_batchSetPosition2dV2f32(circle, POSITION_STICK_LEFT);
        korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_gfx_batchSetPosition2dV2f32(circle, POSITION_STICK_RIGHT);
        korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_gfx_batchSetScale(circle, KORL_MATH_V3F32_ONE);
        korl_gfx_batchCircleSetColor(circle, {20,20,20,255});
        korl_gfx_batchSetPosition2dV2f32(circle, POSITION_STICK_LEFT);
        korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_gfx_batchSetPosition2dV2f32(circle, POSITION_STICK_RIGHT);
        korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    }
    korl_gfx_batchSetPosition2dV2f32(circle, POSITION_STICK_LEFT + Korl_Math_V2f32{memory->gamepadState.axes[KORL_GAMEPAD_AXIS_STICK_LEFT_X], memory->gamepadState.axes[KORL_GAMEPAD_AXIS_STICK_LEFT_Y]} * CIRCLE_RADIUS);
    korl_gfx_batchCircleSetColor(circle, memory->gamepadState.buttons[KORL_GAMEPAD_BUTTON_STICK_LEFT] ? KORL_COLOR4U8_WHITE : KORL_COLOR4U8_BLACK);
    korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    korl_gfx_batchSetPosition2dV2f32(circle, POSITION_STICK_RIGHT + Korl_Math_V2f32{memory->gamepadState.axes[KORL_GAMEPAD_AXIS_STICK_RIGHT_X], memory->gamepadState.axes[KORL_GAMEPAD_AXIS_STICK_RIGHT_Y]} * CIRCLE_RADIUS);
    korl_gfx_batchCircleSetColor(circle, memory->gamepadState.buttons[KORL_GAMEPAD_BUTTON_STICK_RIGHT] ? KORL_COLOR4U8_WHITE : KORL_COLOR4U8_BLACK);
    korl_gfx_batch(circle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    Korl_Gfx_Batch*const rectangle = korl_gfx_createBatchRectangleColored(memory->allocatorStack, 2*CIRCLE_RADIUS*KORL_MATH_V2F32_ONE, {0.5f, 0}, KORL_COLOR4U8_WHITE);
    for(u32 i = 0; i < 2; i++)
    {
        korl_gfx_batchSetPosition2dV2f32(rectangle, POSITION_BUTTON_CLUSTER_TOP + OFFSETS_BUTTON_CLUSTER_TOP[2 + i]);
        korl_gfx_batchRectangleSetColor(rectangle, KORL_COLOR4U8_BLACK);
        korl_gfx_batchSetScale(rectangle, KORL_MATH_V3F32_ONE);
        korl_gfx_batch(rectangle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
        korl_gfx_batchRectangleSetColor(rectangle, KORL_COLOR4U8_WHITE);
        korl_gfx_batchSetScale(rectangle, {1, memory->gamepadState.axes[KORL_GAMEPAD_AXIS_TRIGGER_LEFT + i], 1});
        korl_gfx_batch(rectangle, KORL_GFX_BATCH_FLAG_DISABLE_DEPTH_TEST);
    }
    /**/
    return memory->continueRunning;
}
#include "utility/korl-utility-stb-ds.c"
#include "utility/korl-stringPool.c"
#include "utility/korl-checkCast.c"
#include "utility/korl-logConsole.c"
#include "utility/korl-utility-math.c"
#include "utility/korl-utility-string.c"
#include "utility/korl-utility-memory.c"
#include "utility/korl-utility-gfx.c"
