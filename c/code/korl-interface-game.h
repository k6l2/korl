#pragma once
#include "korl-interface-game-memory.h"// contains the game-specific definition of GameMemory struct
#define KORL_GAME_API extern "C" __declspec(dllexport)
#define KORL_GAME_ON_RELOAD(name)         void name(GameMemory* memory, KorlPlatformApi korlApi)
#define KORL_GAME_INITIALIZE(name)        void name(void)
#define KORL_GAME_ON_KEYBOARD_EVENT(name) void name(Korl_KeyboardCode keyCode, bool isDown, bool isRepeat)
#define KORL_GAME_ON_MOUSE_EVENT(name)    void name(Korl_MouseEvent mouseEvent)
/** 
 * @return false if the platform should close the game application
 */
#define KORL_GAME_UPDATE(name)            bool name(f32 deltaSeconds, u32 windowSizeX, u32 windowSizeY, bool windowIsFocused)
#define KORL_GAME_ON_ASSET_RELOADED(name) void name(const wchar_t* rawUtf16AssetName, Korl_AssetCache_AssetData assetData)
typedef KORL_GAME_ON_RELOAD        (fnSig_korl_game_onReload);
typedef KORL_GAME_INITIALIZE       (fnSig_korl_game_initialize);
typedef KORL_GAME_ON_KEYBOARD_EVENT(fnSig_korl_game_onKeyboardEvent);
typedef KORL_GAME_ON_MOUSE_EVENT   (fnSig_korl_game_onMouseEvent);
typedef KORL_GAME_UPDATE           (fnSig_korl_game_update);
typedef KORL_GAME_ON_ASSET_RELOADED(fnSig_korl_game_onAssetReloaded);
