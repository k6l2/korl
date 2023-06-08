#include "korl-interface-game.h"
#include "utility/korl-utility-stb-ds.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-stringPool.h"
#include "utility/korl-logConsole.h"
typedef struct Memory
{
    Korl_Memory_AllocatorHandle allocatorHeap;
    Korl_Memory_AllocatorHandle allocatorStack;
    bool continueRunning;
    Korl_StringPool stringPool;
    Korl_LogConsole logConsole;
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
    memory->logConsole.enable = true;
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
KORL_EXPORT KORL_GAME_UPDATE(korl_game_update)
{
    korl_logConsole_update(&memory->logConsole, deltaSeconds, korl_log_getBuffer, {windowSizeX, windowSizeY}, memory->allocatorStack);
    return memory->continueRunning;
}
#include "utility/korl-utility-stb-ds.c"
#include "utility/korl-stringPool.c"
#include "utility/korl-checkCast.c"
#include "utility/korl-logConsole.c"
#include "utility/korl-utility-math.c"
#include "utility/korl-utility-string.c"
#include "utility/korl-utility-memory.c"
