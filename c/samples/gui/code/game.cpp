#include "korl-interface-game.h"
#define _KORL_PLATFORM_API_MACRO_OPERATION(x) fnSig_##x *x;
    #include "korl-interface-platform-api.h"
    #define KORL_DEFINED_INTERFACE_PLATFORM_API// prevent certain KORL modules which define symbols that are required by the game module, but whose codes are confined to the platform layer, from re-defining them since we just declared the API
#undef _KORL_PLATFORM_API_MACRO_OPERATION
korl_internal void _game_getInterfacePlatformApi(KorlPlatformApi korlApi)
{
    #define _KORL_PLATFORM_API_MACRO_OPERATION(x) (x) = korlApi.x;
    #include "korl-interface-platform-api.h"
    #undef _KORL_PLATFORM_API_MACRO_OPERATION
}
typedef struct Memory
{
    Korl_Memory_AllocatorHandle allocatorHeap;
    bool continueRunning;
    bool testWindowOpen;
} Memory;
korl_global_variable Memory* memory;
KORL_GAME_API KORL_GAME_INITIALIZE(korl_game_initialize)
{
    _game_getInterfacePlatformApi(korlApi);
    const Korl_Memory_AllocatorHandle allocatorHeap = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_GENERAL, korl_math_megabytes(8), L"game-heap", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, NULL/*auto-select start address*/);
    memory = KORL_C_CAST(Memory*, korl_allocate(allocatorHeap, sizeof(Memory)));
    memory->allocatorHeap   = allocatorHeap;
    memory->continueRunning = true;
    memory->testWindowOpen  = true;
    korl_gui_setFontAsset(L"data/source-sans/SourceSans3-Semibold.otf");// KORL-ISSUE-000-000-086: gfx: default font path doesn't work, since this subdirectly is unlikely in the game project
    return memory;
}
KORL_GAME_API KORL_GAME_ON_RELOAD(korl_game_onReload)
{
    _game_getInterfacePlatformApi(korlApi);
    memory = KORL_C_CAST(Memory*, context);
}
KORL_GAME_API KORL_GAME_ON_KEYBOARD_EVENT(korl_game_onKeyboardEvent)
{
    if(keyCode == KORL_KEY_ESCAPE && isDown && !isRepeat)
        memory->continueRunning = false;
}
KORL_GAME_API KORL_GAME_UPDATE(korl_game_update)
{
    //@TODO: complete GUI test code
    // korl_gui_widgetTextFormat(L"orphan widget test");
    korl_gui_windowBegin(L"Test Window", &memory->testWindowOpen, KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT);
        korl_gui_widgetTextFormat(L"Greetings!");
    korl_gui_windowEnd();
    korl_gui_windowBegin(L"Test Window Auto-Resize", NULL, KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE | KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR);
    korl_gui_windowEnd();
    return memory->continueRunning;
}
#include "korl-math.c"
#include "korl-checkCast.c"
