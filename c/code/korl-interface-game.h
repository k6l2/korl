#pragma once
#include "korl-interface-game-memory.h"// contains the game-specific definition of GameMemory struct
#define GAME_INITIALIZE(name) void name(GameMemory* memory)
#define GAME_ON_KEYBOARD_EVENT(name) void name(Korl_KeyboardCode keyCode, bool isDown)
/** 
 * @return false if the platform should close the game application
 */
#define GAME_UPDATE(name) \
    bool name(f32 deltaSeconds, u32 windowSizeX, u32 windowSizeY, bool windowIsFocused)
typedef GAME_INITIALIZE(fnSig_gameInitialize);
typedef GAME_UPDATE(fnSig_gameUpdate);
typedef GAME_ON_KEYBOARD_EVENT(fnSig_gameOnKeyboardEvent);
#ifdef __cplusplus
extern "C" {
#endif
GAME_INITIALIZE(korl_game_initialize);
GAME_UPDATE(korl_game_update);
GAME_ON_KEYBOARD_EVENT(korl_game_onKeyboardEvent);
#ifdef __cplusplus
}//extern "C"
#endif
