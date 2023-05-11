#include <stdlib.h>// needed for __FILEW__, etc...
#include "korl-interface-game.h"
#define _KORL_PLATFORM_API_MACRO_OPERATION(x) fnSig_##x *x;
    #include "korl-interface-platform-api.h"
    //KORL-ISSUE-000-000-120: interface-platform: remove KORL_DEFINED_INTERFACE_PLATFORM_API; this it just messy imo; if there is clear separation of code that should only exist in the platform layer, then we should probably just physically separate it out into separate source file(s)
    #define KORL_DEFINED_INTERFACE_PLATFORM_API// prevent certain KORL modules which define symbols that are required by the game module, but whose codes are confined to the platform layer, from re-defining them since we just declared the API
#undef _KORL_PLATFORM_API_MACRO_OPERATION
korl_internal void _game_getInterfacePlatformApi(KorlPlatformApi korlApi)
{
    #define _KORL_PLATFORM_API_MACRO_OPERATION(x) (x) = korlApi.x;
    #include "korl-interface-platform-api.h"
    #undef _KORL_PLATFORM_API_MACRO_OPERATION
}
#include "korl-string.h"
#include "korl-stringPool.h"
#include "korl-logConsole.h"
#include "korl-gfx.h"
enum InputFlags
    {INPUT_FLAG_FORWARD
    ,INPUT_FLAG_BACKWARD
    ,INPUT_FLAG_LEFT
    ,INPUT_FLAG_RIGHT
    ,INPUT_FLAG_UP
    ,INPUT_FLAG_DOWN
    ,INPUT_FLAG_LOOK_UP
    ,INPUT_FLAG_LOOK_DOWN
    ,INPUT_FLAG_LOOK_LEFT
    ,INPUT_FLAG_LOOK_RIGHT
};
typedef struct Camera
{
    Korl_Math_V3f32 position;
    Korl_Math_V2f32 yawPitch;// x => yaw, y => pitch
} Camera;
typedef struct Memory
{
    Korl_Memory_AllocatorHandle allocatorHeap;
    Korl_Memory_AllocatorHandle allocatorStack;
    bool                        quit;
    Korl_StringPool             stringPool;// used by logConsole
    Korl_LogConsole             logConsole;
    Camera                      camera;
    u32                         inputFlags;
    f32                         seconds;
} Memory;
korl_global_variable Memory* memory;
KORL_EXPORT KORL_GAME_INITIALIZE(korl_game_initialize)
{
    _game_getInterfacePlatformApi(korlApi);
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
    _game_getInterfacePlatformApi(korlApi);
    memory = KORL_C_CAST(Memory*, context);
}
KORL_EXPORT KORL_GAME_ON_KEYBOARD_EVENT(korl_game_onKeyboardEvent)
{
    if(isDown)
    {
        if(!isRepeat)
            switch(keyCode)
            {
            case KORL_KEY_E     :{memory->inputFlags |= (1 << INPUT_FLAG_FORWARD); break;}
            case KORL_KEY_D     :{memory->inputFlags |= (1 << INPUT_FLAG_BACKWARD); break;}
            case KORL_KEY_S     :{memory->inputFlags |= (1 << INPUT_FLAG_LEFT); break;}
            case KORL_KEY_F     :{memory->inputFlags |= (1 << INPUT_FLAG_RIGHT); break;}
            case KORL_KEY_A     :{memory->inputFlags |= (1 << INPUT_FLAG_DOWN); break;}
            case KORL_KEY_SPACE :{memory->inputFlags |= (1 << INPUT_FLAG_UP); break;}
            case KORL_KEY_I     :{memory->inputFlags |= (1 << INPUT_FLAG_LOOK_UP); break;}
            case KORL_KEY_K     :{memory->inputFlags |= (1 << INPUT_FLAG_LOOK_DOWN); break;}
            case KORL_KEY_J     :{memory->inputFlags |= (1 << INPUT_FLAG_LOOK_LEFT); break;}
            case KORL_KEY_L     :{memory->inputFlags |= (1 << INPUT_FLAG_LOOK_RIGHT); break;}
            case KORL_KEY_F4    :{korl_command_invoke(KORL_RAW_CONST_UTF8("load"), memory->allocatorStack); break;}
            case KORL_KEY_ESCAPE:{memory->quit = true; break;}
            case KORL_KEY_GRAVE :{korl_logConsole_toggle(&memory->logConsole); break;}
            default: break;
            }
    }
    else
        switch(keyCode)
        {
        case KORL_KEY_E    :{memory->inputFlags &= ~(1 << INPUT_FLAG_FORWARD); break;}
        case KORL_KEY_D    :{memory->inputFlags &= ~(1 << INPUT_FLAG_BACKWARD); break;}
        case KORL_KEY_S    :{memory->inputFlags &= ~(1 << INPUT_FLAG_LEFT); break;}
        case KORL_KEY_F    :{memory->inputFlags &= ~(1 << INPUT_FLAG_RIGHT); break;}
        case KORL_KEY_A    :{memory->inputFlags &= ~(1 << INPUT_FLAG_DOWN); break;}
        case KORL_KEY_SPACE:{memory->inputFlags &= ~(1 << INPUT_FLAG_UP); break;}
        case KORL_KEY_I    :{memory->inputFlags &= ~(1 << INPUT_FLAG_LOOK_UP); break;}
        case KORL_KEY_K    :{memory->inputFlags &= ~(1 << INPUT_FLAG_LOOK_DOWN); break;}
        case KORL_KEY_J    :{memory->inputFlags &= ~(1 << INPUT_FLAG_LOOK_LEFT); break;}
        case KORL_KEY_L    :{memory->inputFlags &= ~(1 << INPUT_FLAG_LOOK_RIGHT); break;}
        default: break;
        }
}
KORL_EXPORT KORL_GAME_UPDATE(korl_game_update)
{
    memory->seconds += deltaSeconds;
    korl_logConsole_update(&memory->logConsole, deltaSeconds, korl_log_getBuffer, {windowSizeX, windowSizeY}, memory->allocatorStack);
    /* camera... */
    // @TODO: pull out the 3D camera movement code into a utility module, as I am likely to re-use this stuff in multiple projects
    korl_shared_const Korl_Math_V3f32 DEFAULT_FORWARD = KORL_MATH_V3F32_MINUS_Y;// blender model space
    korl_shared_const Korl_Math_V3f32 DEFAULT_RIGHT   = KORL_MATH_V3F32_MINUS_X;// blender model space
    korl_shared_const Korl_Math_V3f32 DEFAULT_UP      = KORL_MATH_V3F32_Z;      // blender model space
    {
        Korl_Math_V2f32 cameraLook = KORL_MATH_V2F32_ZERO;
        if(memory->inputFlags & (1 << INPUT_FLAG_LOOK_UP))    cameraLook.y += 1;
        if(memory->inputFlags & (1 << INPUT_FLAG_LOOK_DOWN))  cameraLook.y -= 1;
        if(memory->inputFlags & (1 << INPUT_FLAG_LOOK_LEFT))  cameraLook.x += 1;
        if(memory->inputFlags & (1 << INPUT_FLAG_LOOK_RIGHT)) cameraLook.x -= 1;
        const f32 cameraLookMagnitude = korl_math_v2f32_magnitude(&cameraLook);
        if(!korl_math_isNearlyZero(cameraLookMagnitude))
        {
            korl_shared_const f32 CAMERA_LOOK_SPEED = 2;
            cameraLook = korl_math_v2f32_normalKnownMagnitude(cameraLook, cameraLookMagnitude);
            memory->camera.yawPitch += deltaSeconds * CAMERA_LOOK_SPEED * cameraLook;
            KORL_MATH_ASSIGN_CLAMP(memory->camera.yawPitch.y, -KORL_PI32/2, KORL_PI32/2);
        }
        Korl_Math_V3f32 cameraMove = KORL_MATH_V3F32_ZERO;
        if(memory->inputFlags & (1 << INPUT_FLAG_FORWARD))  cameraMove += DEFAULT_FORWARD;
        if(memory->inputFlags & (1 << INPUT_FLAG_BACKWARD)) cameraMove -= DEFAULT_FORWARD;
        if(memory->inputFlags & (1 << INPUT_FLAG_RIGHT))    cameraMove += DEFAULT_RIGHT;
        if(memory->inputFlags & (1 << INPUT_FLAG_LEFT))     cameraMove -= DEFAULT_RIGHT;
        if(memory->inputFlags & (1 << INPUT_FLAG_UP))       cameraMove += DEFAULT_UP;
        if(memory->inputFlags & (1 << INPUT_FLAG_DOWN))     cameraMove -= DEFAULT_UP;
        const f32 cameraMoveMagnitude = korl_math_v3f32_magnitude(&cameraMove);
        if(!korl_math_isNearlyZero(cameraMoveMagnitude))
        {
            korl_shared_const f32 CAMERA_SPEED = 1000;
            cameraMove = korl_math_v3f32_normalKnownMagnitude(cameraMove, cameraMoveMagnitude);
            const Korl_Math_Quaternion quaternionCameraPitch = korl_math_quaternion_fromAxisRadians(DEFAULT_RIGHT, memory->camera.yawPitch.y, true);
            const Korl_Math_Quaternion quaternionCameraYaw   = korl_math_quaternion_fromAxisRadians(DEFAULT_UP   , memory->camera.yawPitch.x, true);
            const Korl_Math_V3f32      cameraForward         = quaternionCameraYaw * quaternionCameraPitch * cameraMove;
            memory->camera.position += deltaSeconds * CAMERA_SPEED * cameraForward;
        }
    }
    const Korl_Math_Quaternion quaternionCameraPitch = korl_math_quaternion_fromAxisRadians(DEFAULT_RIGHT, memory->camera.yawPitch.y, true);
    const Korl_Math_Quaternion quaternionCameraYaw   = korl_math_quaternion_fromAxisRadians(DEFAULT_UP   , memory->camera.yawPitch.x, true);
    const Korl_Math_V3f32      cameraForward         = quaternionCameraYaw * quaternionCameraPitch * DEFAULT_FORWARD;
    const Korl_Math_V3f32      cameraUp              = quaternionCameraYaw * quaternionCameraPitch * DEFAULT_UP;
    korl_gfx_useCamera(korl_gfx_createCameraFov(90, 10, 1e16f, memory->camera.position, memory->camera.position + cameraForward, cameraUp));
    /* lights... */
    korl_gfx_setClearColor(1,1,1);
    //@TODO: we should be able to define lights in some kind of file (JSON, etc.)
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
                        ,.linear    = 0.00009f
                        ,.quadratic = 0.00003f}}
        ,
        {.type          = KORL_GFX_LIGHT_TYPE_SPOT
        ,.position      = memory->camera.position
        ,.direction     = cameraForward
        ,.color         = {.ambient  = KORL_MATH_V3F32_ONE * 0.05f
                          ,.diffuse  = KORL_MATH_V3F32_ONE * 0.5f
                          ,.specular = KORL_MATH_V3F32_ONE}
        ,.attenuation   = {.constant  = 1
                          ,.linear    = 0.000045f
                          ,.quadratic = 0.000015f}
        ,.cutOffCosines = {.inner = korl_math_cosine(0.1f * KORL_PI32)
                          ,.outer = korl_math_cosine(0.2f * KORL_PI32)}}
        };
    korl_gfx_light_use(lights, korl_arraySize(lights));
    /* action! */
    Korl_Gfx_Drawable scene3d;
    korl_gfx_drawable_scene3d_initialize(&scene3d, korl_resource_fromFile(KORL_RAW_CONST_UTF16(L"data/cube.glb"), KORL_ASSETCACHE_GET_FLAG_LAZY));
    scene3d._model.scale    = KORL_MATH_V3F32_ONE * 50;
    //@TODO: we should be able to define materials in some kind of file (GLB, JSON, etc.)
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
#include "korl-math.c"
#include "korl-checkCast.c"
#include "korl-string.c"
#include "korl-stringPool.c"
#include "korl-logConsole.c"
#define STBDS_UNIT_TESTS // for the sake of detecting any other C++ warnings; we aren't going to actually run any of these tests
#include "korl-stb-ds.c"
#include "korl-gfx.c"