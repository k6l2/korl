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
typedef struct Memory
{
    Korl_Memory_AllocatorHandle allocatorHeap;
    bool continueRunning;
    bool testWindowOpen;
    u$ testTextWidgets;
    Korl_StringPool stringPool;
    Korl_LogConsole logConsole;
} Memory;
korl_global_variable Memory* memory;
KORL_EXPORT KORL_FUNCTION_korl_command_callback(korl_game_commandTest)
{
    korl_log(VERBOSE, "shoop DA WOOP");
}
KORL_EXPORT KORL_GAME_INITIALIZE(korl_game_initialize)
{
    _game_getInterfacePlatformApi(korlApi);
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(8);
    const Korl_Memory_AllocatorHandle allocatorHeap = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"game-heap", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    memory = KORL_C_CAST(Memory*, korl_allocate(allocatorHeap, sizeof(Memory)));
    memory->allocatorHeap   = allocatorHeap;
    memory->continueRunning = true;
    memory->testWindowOpen  = true;
    memory->stringPool      = korl_stringPool_create(allocatorHeap);
    memory->logConsole      = korl_logConsole_create(&memory->stringPool);
    korl_gui_setFontAsset(L"data/source-sans/SourceSans3-Semibold.otf");// KORL-ISSUE-000-000-086: gfx: default font path doesn't work, since this subdirectly is unlikely in the game project
    korl_command_register(KORL_RAW_CONST_UTF8("test"), korl_game_commandTest);
    return memory;
}
KORL_EXPORT KORL_GAME_ON_RELOAD(korl_game_onReload)
{
    _game_getInterfacePlatformApi(korlApi);
    memory = KORL_C_CAST(Memory*, context);
}
KORL_EXPORT KORL_GAME_ON_KEYBOARD_EVENT(korl_game_onKeyboardEvent)
{
    if(isDown && !isRepeat)
        switch(keyCode)
        {
        case KORL_KEY_ESCAPE:{
            memory->continueRunning = false;
            break;}
        case KORL_KEY_GRAVE:{
            korl_logConsole_toggle(&memory->logConsole);
            break;}
        default: break;
        }
}
KORL_EXPORT KORL_GAME_UPDATE(korl_game_update)
{
    korl_logConsole_update(&memory->logConsole, deltaSeconds, korl_log_getBuffer, {windowSizeX, windowSizeY});
    // korl_gui_widgetButtonFormat(L"just a test button that does nothing!");
    for(u$ i = 0; i < 1; i++)
    {
        korl_gui_setLoopIndex(i);
        korl_gui_widgetTextFormat(L"orphan widget test");
    }
    korl_gui_setLoopIndex(0);
    korl_gui_windowBegin(L"Test Window", &memory->testWindowOpen, KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT);
        korl_gui_widgetTextFormat(L"add/remove widgets:");
        korl_gui_realignY();
        if(korl_gui_widgetButtonFormat(L"+"))
        {
            memory->testTextWidgets++;
            korl_log(VERBOSE, "testTextWidgets++ == %llu", memory->testTextWidgets);
        }
        korl_gui_realignY();
        if(korl_gui_widgetButtonFormat(L"-") && memory->testTextWidgets)
            memory->testTextWidgets--;
        for(u$ i = 0; i < memory->testTextWidgets; i++)
        {
            korl_gui_setLoopIndex(i);
            korl_gui_widgetTextFormat(L"[%llu] hello :)", i);
        }
        korl_gui_setLoopIndex(0);
    korl_gui_windowEnd();
    korl_gui_windowBegin(L"Test Window Auto-Resize", NULL, KORL_GUI_WINDOW_STYLE_FLAG_AUTO_RESIZE | KORL_GUI_WINDOW_STYLE_FLAG_TITLEBAR);
        if(!memory->testWindowOpen && korl_gui_widgetButtonFormat(L"show test window"))
            memory->testWindowOpen = true;
    korl_gui_windowEnd();
    bool isNoTitlebarWindowOpen = memory->testWindowOpen;
    korl_gui_windowBegin(L"Test Window NO-TITLEBAR", &isNoTitlebarWindowOpen, KORL_GUI_WINDOW_STYLE_FLAG_NONE);
    korl_gui_windowEnd();
    korl_gui_windowBegin(L"Test Bare Window", NULL, KORL_GUI_WINDOW_STYLE_FLAG_NONE);
    korl_gui_windowEnd();
    korl_gui_widgetTextFormat(L"orphan widget test2");
    return memory->continueRunning;
}
#include "korl-math.c"
#include "korl-checkCast.c"
#include "korl-string.c"
#include "korl-stringPool.c"
#include "korl-logConsole.c"
#define STB_DS_IMPLEMENTATION
#define STBDS_UNIT_TESTS // for the sake of detecting any other C++ warnings; we aren't going to actually run any of these tests
#define STBDS_ASSERT(x) korl_assert(x)
#include "stb/stb_ds.h"
