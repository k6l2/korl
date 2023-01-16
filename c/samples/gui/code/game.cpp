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
    u$ testTextWidgets;
    struct Console
    {
        bool enable;
        f32 fadeInRatio;
        u$ lastLoggedCharacters;
    } console;
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
    if(isDown && !isRepeat)
        switch(keyCode)
        {
        case KORL_KEY_ESCAPE:{
            memory->continueRunning = false;
            break;}
        case KORL_KEY_GRAVE:{
            memory->console.enable = !memory->console.enable;
            break;}
        default: break;
        }
}
KORL_GAME_API KORL_GAME_UPDATE(korl_game_update)
{
    ///@TODO: finish log console widget
    memory->console.fadeInRatio = korl_math_exDecay(memory->console.fadeInRatio, memory->console.enable ? 1.f : 0.f, 40.f, deltaSeconds);
    if(memory->console.enable || !korl_math_isNearlyEqualEpsilon(memory->console.fadeInRatio, 0, .01f))
    {
        u$       loggedBytes      = 0;
        acu16    logBuffer        = korl_log_getBuffer(&loggedBytes);
        const u$ loggedCharacters = loggedBytes/sizeof(*logBuffer.data);
        const u$ newCharacters    = KORL_MATH_MIN(loggedCharacters - memory->console.lastLoggedCharacters, logBuffer.size);
        logBuffer.data = logBuffer.data + logBuffer.size - newCharacters;
        logBuffer.size = newCharacters;
        korl_gui_setNextWidgetSize({KORL_C_CAST(f32, windowSizeX)
                                   ,KORL_C_CAST(f32, windowSizeY)*0.5f*memory->console.fadeInRatio});
        korl_gui_setNextWidgetParentOffset({0, KORL_C_CAST(f32, windowSizeY)});
        korl_gui_windowBegin(L"console", NULL, KORL_GUI_WINDOW_STYLE_FLAG_NONE);
            korl_gui_widgetText(L"console text", logBuffer, 1'000/*max line count*/, NULL/*codepointTest*/, NULL/*codepointTestData*/, KORL_GUI_WIDGET_TEXT_FLAG_LOG);
        korl_gui_windowEnd();
        memory->console.lastLoggedCharacters = loggedCharacters;
    }
    else
        memory->console.lastLoggedCharacters = 0;
    korl_gui_widgetButtonFormat(L"just a test button that does nothing!");
    for(u$ i = 0; i < 5; i++)
    {
        korl_gui_setLoopIndex(i);
        korl_gui_widgetTextFormat(L"orphan widget test");
    }
    korl_gui_setLoopIndex(0);
    korl_gui_windowBegin(L"Test Window", &memory->testWindowOpen, KORL_GUI_WINDOW_STYLE_FLAGS_DEFAULT);
        korl_gui_widgetTextFormat(L"add/remove widgets:");
        korl_gui_realignY();
        if(korl_gui_widgetButtonFormat(L"+"))
            memory->testTextWidgets++;
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
    return memory->continueRunning;
}
#include "korl-math.c"
#include "korl-checkCast.c"
