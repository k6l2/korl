#pragma once
#include "korl-interface-game-memory.h"// contains the game-specific definition of GameMemory struct
#define GAME_ON_RELOAD(name) void name(GameMemory* memory, KorlPlatformApi korlApi)
#define GAME_INITIALIZE(name) void name(void)
#define GAME_ON_KEYBOARD_EVENT(name) void name(Korl_KeyboardCode keyCode, bool isDown, bool isRepeat)
#define GAME_ON_MOUSE_EVENT(name) void name(Korl_MouseEvent mouseEvent)
/** 
 * @return false if the platform should close the game application
 */
#define GAME_UPDATE(name) \
    bool name(f32 deltaSeconds, u32 windowSizeX, u32 windowSizeY, bool windowIsFocused)
typedef GAME_INITIALIZE(fnSig_gameInitialize);
typedef GAME_UPDATE(fnSig_gameUpdate);
typedef GAME_ON_KEYBOARD_EVENT(fnSig_gameOnKeyboardEvent);
typedef GAME_ON_MOUSE_EVENT(fnSig_gameOnMouseEvent);
#ifdef __cplusplus
extern "C" {
#endif
GAME_ON_RELOAD(korl_onReload);
GAME_INITIALIZE(korl_game_initialize);
GAME_UPDATE(korl_game_update);
GAME_ON_KEYBOARD_EVENT(korl_game_onKeyboardEvent);
GAME_ON_MOUSE_EVENT(korl_game_onMouseEvent);
#ifdef __cplusplus
}//extern "C"
#endif
