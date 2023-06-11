/**
 * # Code Samples
 * 
 *   ## Example of implementation of the game interface (minimum code):
 *     
 *     For more details, as well as a complete list of all KORL platform APIs, see 
 *     `korl-interface-platform-api.h`.
 *     
 *     ```
 *     #include "korl-interface-game.h"
 *     KORL_EXPORT KORL_GAME_INITIALIZE(korl_game_initialize)
 *     {
 *         korl_platform_getApi(korlApi);
 *         return NULL;// replace this with whatever global memory state the game module needs
 *     }
 *     KORL_EXPORT KORL_GAME_ON_RELOAD(korl_game_onReload)
 *     {
 *         korl_platform_getApi(korlApi);
 *     }
 *     KORL_EXPORT KORL_GAME_UPDATE(korl_game_update)
 *     {
 *         return true;// true => continue running the application
 *     }
 *     ```
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
/**
 * \return a pointer to an implementation-dependent context; the game module 
 *         itself defines what this data is; the platform layer should have no 
 *         knowledge of this data, as there is no reason to as far as I can tell
 */
#define KORL_GAME_INITIALIZE(name)        void* name(KorlPlatformApi korlApi)
#define KORL_GAME_ON_RELOAD(name)         void  name(void* context, KorlPlatformApi korlApi)
//@TODO: API rectification: create a Korl_KeyEvent
#define KORL_GAME_ON_KEYBOARD_EVENT(name) void  name(Korl_KeyboardCode keyCode, bool isDown, bool isRepeat)                   // OPTIONAL symbol in the game code module
#define KORL_GAME_ON_MOUSE_EVENT(name)    void  name(Korl_MouseEvent mouseEvent)                                              // OPTIONAL symbol in the game code module
#define KORL_GAME_ON_GAMEPAD_EVENT(name)  void  name(Korl_GamepadEvent gamepadEvent)                                          // OPTIONAL symbol in the game code module
/** 
 * \return false if the platform should close the game application
 */
#define KORL_GAME_UPDATE(name)            bool name(f32 deltaSeconds, u32 windowSizeX, u32 windowSizeY, bool windowIsFocused)
#define KORL_GAME_ON_ASSET_RELOADED(name) void name(acu16 rawUtf16AssetName, Korl_AssetCache_AssetData assetData)             // OPTIONAL symbol in the game code module
typedef KORL_GAME_INITIALIZE       (fnSig_korl_game_initialize);
typedef KORL_GAME_ON_RELOAD        (fnSig_korl_game_onReload);
typedef KORL_GAME_ON_KEYBOARD_EVENT(fnSig_korl_game_onKeyboardEvent);
typedef KORL_GAME_ON_MOUSE_EVENT   (fnSig_korl_game_onMouseEvent);
typedef KORL_GAME_ON_GAMEPAD_EVENT (fnSig_korl_game_onGamepadEvent);
typedef KORL_GAME_UPDATE           (fnSig_korl_game_update);
typedef KORL_GAME_ON_ASSET_RELOADED(fnSig_korl_game_onAssetReloaded);
