/**
 * # Code Samples
 * 
 *   ## Example of implementation of the game interface (minimum code):
 *   
 *     ```
 *     #include "korl-interface-game.h"
 *     KORL_GAME_API KORL_GAME_INITIALIZE(korl_game_initialize)
 *     {
 *         return NULL;// replace this with whatever global memory state the game module needs
 *     }
 *     KORL_GAME_API KORL_GAME_ON_RELOAD(korl_game_onReload)
 *     {
 *     }
 *     KORL_GAME_API KORL_GAME_UPDATE(korl_game_update)
 *     {
 *         return true;// true => continue running the application
 *     }
 *     ```
 *   
 *   ## Example code of declaration of KORL platform API & obtaining function pointers:
 *   
 *     For more details, as well as a complete list of all KORL platform APIs, see 
 *     `korl-interface-platform-api.h`.
 *     
 *     ```
 *     #define _KORL_PLATFORM_API_MACRO_OPERATION(x) fnSig_##x *x;
 *         #include "korl-interface-platform-api.h"
 *         #define KORL_DEFINED_INTERFACE_PLATFORM_API// prevent certain KORL modules which define symbols that are required by the game module, but whose codes are confined to the platform layer, from re-defining them since we just declared the API
 *     #undef _KORL_PLATFORM_API_MACRO_OPERATION
 *     korl_internal void _game_getInterfacePlatformApi(KorlPlatformApi korlApi)
 *     {
 *         #define _KORL_PLATFORM_API_MACRO_OPERATION(x) (x) = korlApi.x;
 *         #include "korl-interface-platform-api.h"
 *         #undef _KORL_PLATFORM_API_MACRO_OPERATION
 *     }
 *     KORL_GAME_API KORL_GAME_INITIALIZE(korl_game_initialize)
 *     {
 *         _game_getInterfacePlatformApi(korlApi);
 *         return NULL;// replace this with whatever global memory state the game module needs
 *     }
 *     KORL_GAME_API KORL_GAME_ON_RELOAD(korl_game_onReload)
 *     {
 *         _game_getInterfacePlatformApi(korlApi);
 *     }
 *     ```
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
#define KORL_GAME_API extern "C" __declspec(dllexport)
/**
 * \return a pointer to an implementation-dependent context; the game module 
 *         itself defines what this data is; the platform layer should have no 
 *         knowledge of this data, as there is no reason to as far as I can tell
 */
#define KORL_GAME_INITIALIZE(name)        void* name(KorlPlatformApi korlApi)
#define KORL_GAME_ON_RELOAD(name)         void  name(void* context, KorlPlatformApi korlApi)
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
