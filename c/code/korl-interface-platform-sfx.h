#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform-resource.h"
typedef struct Korl_Sfx_TapeHandle
{
    Korl_Resource_Handle resource;
    u16                  salt;
    u8                   deckIndex;
} Korl_Sfx_TapeHandle;
typedef struct Korl_Sfx_TapeDeckControl
{
    f32 volumeRatio;
    u8  category;// index into lookup table for user-defined properties that apply to all tapes with a matching category value
    // bool loop;// @TODO
} Korl_Sfx_TapeDeckControl;
typedef struct Korl_Sfx_TapeCategoryControl
{
    f32 volumeRatio;
} Korl_Sfx_TapeCategoryControl;
korl_global_const Korl_Sfx_TapeDeckControl KORL_SFX_TAPE_DECK_CONTROL_DEFAULT = {.volumeRatio = 1.f};
/** the # of Decks allocated inside korl-audio determines the # of Tapes that 
 * can be played simultaneously; if the user attempts to call \c korl_sfx_play* 
 * when all Decks are currently playing audio, a \c NULL TapeHandle will be 
 * returned, and no additional audio will be played */
// #define KORL_FUNCTION_korl_sfx_setDeckCount(name)   void                name(u16 deckCount)
#define KORL_FUNCTION_korl_sfx_playResource(name)   Korl_Sfx_TapeHandle name(Korl_Resource_Handle resourceHandleAudio, Korl_Sfx_TapeDeckControl tapeDeckControl)
#define KORL_FUNCTION_korl_sfx_setVolume(name)      void                name(f32 volumeRatio)
#define KORL_FUNCTION_korl_sfx_category_set(name)   void                name(u8 category, Korl_Sfx_TapeCategoryControl tapeCategoryControl)
// #define KORL_FUNCTION_korl_sfx_tape_stop(name)      void                name(Korl_Sfx_TapeHandle* tapeHandle)
// #define KORL_FUNCTION_korl_sfx_tape_setPause(name)  void                name(Korl_Sfx_TapeHandle* tapeHandle, bool paused)
// #define KORL_FUNCTION_korl_sfx_tape_setVolume(name) void                name(Korl_Sfx_TapeHandle* tapeHandle, f32 volumeRatio)
// #define KORL_FUNCTION_korl_sfx_tape_isValid(name)   bool                name(Korl_Sfx_TapeHandle* tapeHandle)
// #define KORL_FUNCTION_korl_sfx_tape_isPaused(name)  bool                name(Korl_Sfx_TapeHandle* tapeHandle)
