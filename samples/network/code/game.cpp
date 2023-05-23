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
} Memory;
korl_global_variable Memory* memory;
KORL_EXPORT KORL_FUNCTION_korl_command_callback(command_resolve)
{
    const Korl_Network_Address address = korl_network_resolveAddress(parameters[1]);
}
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
    korl_gui_setFontAsset(L"data/source-sans/SourceSans3-Semibold.otf");// KORL-ISSUE-000-000-086: gfx: default font path doesn't work, since this subdirectly is unlikely in the game project
    korl_command_register(KORL_RAW_CONST_UTF8("resolve"), command_resolve);
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
        case KORL_KEY_ESCAPE:{memory->quit = true; break;}
        case KORL_KEY_GRAVE :{korl_logConsole_toggle(&memory->logConsole); break;}
        default: break;
        }
}
KORL_EXPORT KORL_GAME_UPDATE(korl_game_update)
{
    korl_logConsole_update(&memory->logConsole, deltaSeconds, korl_log_getBuffer, {windowSizeX, windowSizeY}, memory->allocatorStack);
    /* debug */
    Korl_Gfx_Batch*const batchAxis = korl_gfx_createBatchAxisLines(memory->allocatorStack);
    korl_gfx_batchSetScale(batchAxis, KORL_MATH_V3F32_ONE * 100);
    korl_gfx_batch(batchAxis, KORL_GFX_BATCH_FLAGS_NONE);
    /**/
    return !memory->quit;
}
#define STBDS_UNIT_TESTS // for the sake of detecting any other C++ warnings; we aren't going to actually run any of these tests
#include "utility/korl-utility-stb-ds.c"
#include "utility/korl-stringPool.c"
#include "utility/korl-checkCast.c"
#include "utility/korl-utility-math.c"
#include "utility/korl-logConsole.c"
#include "utility/korl-camera-freeFly.c"
#include "utility/korl-utility-gfx.c"
#include "utility/korl-utility-string.c"
#include "utility/korl-utility-memory.c"
