#include "korl-interface-game.h"// includes "korl-interface-platform.h"
#include "utility/korl-stringPool.h"
#include "utility/korl-logConsole.h"
#include "utility/korl-camera-freeFly.h"
#include "utility/korl-utility-gfx.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-utility-stb-ds.h"
typedef struct Memory
{
    Korl_Memory_AllocatorHandle allocatorHeap;
    Korl_Memory_AllocatorHandle allocatorStack;
    bool                        quit;
    Korl_StringPool             stringPool;// used by logConsole
    Korl_LogConsole             logConsole;
    Korl_Camera_FreeFly         camera;
    f32                         seconds;
} Memory;
korl_global_variable Memory* memory;
KORL_EXPORT KORL_GAME_INITIALIZE(korl_game_initialize)
{
    korl_platform_getApi(korlApi);
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(8);
    const Korl_Memory_AllocatorHandle allocatorHeap = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"game", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    memory = KORL_C_CAST(Memory*, korl_allocate(allocatorHeap, sizeof(Memory)));
    memory->allocatorHeap  = allocatorHeap;
    memory->allocatorStack = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"game-stack", KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME, &heapCreateInfo);
    memory->stringPool     = korl_stringPool_create(allocatorHeap);
    memory->logConsole     = korl_logConsole_create(&memory->stringPool);
    memory->camera         = {.position = KORL_MATH_V3F32_ONE * 200, .yawPitch = {-KORL_PI32/4, -KORL_PI32/4}};
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
    switch(keyCode)
    {
    case KORL_KEY_E    : {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_FORWARD   , isDown); break;}
    case KORL_KEY_D    : {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_BACKWARD  , isDown); break;}
    case KORL_KEY_S    : {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_LEFT      , isDown); break;}
    case KORL_KEY_F    : {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_RIGHT     , isDown); break;}
    case KORL_KEY_A    : {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_DOWN      , isDown); break;}
    case KORL_KEY_SPACE: {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_UP        , isDown); break;}
    case KORL_KEY_I    : {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_UP   , isDown); break;}
    case KORL_KEY_K    : {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_DOWN , isDown); break;}
    case KORL_KEY_J    : {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_LEFT , isDown); break;}
    case KORL_KEY_L    : {korl_camera_freeFly_setInput(&memory->camera, KORL_CAMERA_FREEFLY_INPUT_FLAG_LOOK_RIGHT, isDown); break;}
    default: break;
    }
    if(isDown && !isRepeat)
        switch(keyCode)
        {
        case KORL_KEY_ESCAPE:{memory->quit = true; break;}
        case KORL_KEY_GRAVE :{korl_logConsole_toggle(&memory->logConsole); break;}
        default: break;
        }
}
KORL_EXPORT KORL_GAME_UPDATE(korl_game_update)
{
    memory->seconds += deltaSeconds;
    korl_logConsole_update(&memory->logConsole, deltaSeconds, korl_log_getBuffer, {windowSizeX, windowSizeY}, memory->allocatorStack);
    /* camera... */
    korl_camera_freeFly_step(&memory->camera, deltaSeconds);
    /* lights... */
    korl_gfx_setClearColor(1,1,1);
    //KORL-FEATURE-000-000-059: gfx: add "Light" asset/resource; we should be able to define lights in some kind of file (JSON, etc.)
    const Korl_Gfx_Light lights[] = {
        {.type      = KORL_GFX_LIGHT_TYPE_DIRECTIONAL
        ,.direction = {0.5f, 0.1f, -1}
        ,.color     = {.ambient  = KORL_MATH_V3F32_ONE * 0.01f
                      ,.diffuse  = KORL_MATH_V3F32_ONE * 0.2f
                      ,.specular = KORL_MATH_V3F32_ONE * 0.5f}}
        ,
        {.type        = KORL_GFX_LIGHT_TYPE_POINT
        ,.position    = korl_math_quaternion_fromAxisRadians(KORL_MATH_V3F32_Z, memory->seconds, true) * (Korl_Math_V3f32{-1,1,1} * 100)
        ,.color       = {.ambient  = KORL_MATH_V3F32_ONE * 0.05f
                        ,.diffuse  = KORL_MATH_V3F32_ONE * 0.5f
                        ,.specular = KORL_MATH_V3F32_ONE}
        ,.attenuation = {.constant  = 1
                        ,.linear    = 9e-5f
                        ,.quadratic = 3e-5f}}
        ,
        {.type          = KORL_GFX_LIGHT_TYPE_SPOT
        ,.position      = memory->camera.position
        ,.direction     = korl_camera_freeFly_forward(&memory->camera)
        ,.color         = {.ambient  = KORL_MATH_V3F32_ONE * 0.05f
                          ,.diffuse  = KORL_MATH_V3F32_ONE * 0.5f
                          ,.specular = KORL_MATH_V3F32_ONE}
        ,.attenuation   = {.constant  = 1
                          ,.linear    = 4.5e-5f
                          ,.quadratic = 1.5e-5f}
        ,.cutOffCosines = {.inner = korl_math_cosine(0.1f * KORL_PI32)
                          ,.outer = korl_math_cosine(0.2f * KORL_PI32)}}
        };
    korl_gfx_light_use(lights, korl_arraySize(lights));
    /* action! */
    Korl_Gfx_Drawable scene3d;
    korl_gfx_drawable_scene3d_initialize(&scene3d, korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"data/cube.glb"), KORL_ASSETCACHE_GET_FLAG_LAZY));
    scene3d._model.scale    = KORL_MATH_V3F32_ONE * 50;
    //KORL-FEATURE-000-000-060: gfx: add "Material" asset/resource; we should be able to define materials in some kind of file (GLB, JSON, etc.)
    scene3d.subType.scene3d.materialSlots[0] = {.material = {.properties = {.factorColorBase     = KORL_MATH_V4F32_ONE
                                                                           ,.factorColorEmissive = {0, 0.8f, 0.05f}
                                                                           ,.factorColorSpecular = KORL_MATH_V4F32_ONE
                                                                           ,.shininess           = 32}
                                                            ,.maps = {.resourceHandleTextureBase     = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"data/crate-base.png"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                     ,.resourceHandleTextureSpecular = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"data/crate-specular.png"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                     ,.resourceHandleTextureEmissive = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"data/crate-emissive.png"), KORL_ASSETCACHE_GET_FLAG_LAZY)}
                                                            ,.shaders = {.resourceHandleShaderVertex   = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/korl-lit.vert.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)
                                                                        ,.resourceHandleShaderFragment = korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"build/shaders/crate.frag.spv"), KORL_ASSETCACHE_GET_FLAG_LAZY)}}
                                               ,.used = true};
    for(i32 y = -5; y < 5; y++)
        for(i32 x = -5; x < 5; x++)
        {
            scene3d._model.position = {KORL_C_CAST(f32, x) * 150.f, KORL_C_CAST(f32, y) * 150.f, 0};
            korl_gfx_draw(&scene3d);
        }
    for(u$ i = 0; i < korl_arraySize(lights); i++)
    {
        const Korl_Gfx_Light*const light = lights + i;
        if(light->type == KORL_GFX_LIGHT_TYPE_DIRECTIONAL)
            continue;
        scene3d.subType.scene3d.materialSlots[0].used = false;
        scene3d._model.scale    = KORL_MATH_V3F32_ONE * 10;
        scene3d._model.position = light->position;
        scene3d._model.rotation = KORL_MATH_QUATERNION_IDENTITY;
        korl_gfx_draw(&scene3d);
    }
    /* debug */
    Korl_Gfx_Batch*const batchAxis = korl_gfx_createBatchAxisLines(memory->allocatorStack);
    korl_gfx_batchSetScale(batchAxis, KORL_MATH_V3F32_ONE * 100);
    korl_gfx_batch(batchAxis, KORL_GFX_BATCH_FLAGS_NONE);
    /**/
    return !memory->quit;
}
#include "utility/korl-utility-stb-ds.c"
#include "utility/korl-stringPool.c"
#include "utility/korl-checkCast.c"
#include "utility/korl-utility-math.c"
#include "utility/korl-logConsole.c"
#include "utility/korl-camera-freeFly.c"
#include "utility/korl-utility-gfx.c"
#include "utility/korl-utility-string.c"
#include "utility/korl-utility-memory.c"
