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
    f32  volumeRatio;
    f32  channelVolumeRatios[2];// only valid if spatialization is _off_S
                                //KORL-ISSUE-000-000-125: sfx: long-term goal: support more surround-sound configurations; for now, it's more than fine to just support 2-channel stereo
    u8   category;// index into lookup table for user-defined properties that apply to all tapes with a matching category value
    u8   priority;// korl-sfx has a max # of TapeDecks which can play a Tape; if the user requests to play a Tape, and all TapeDecks are occupied, either: (1) a TapeDeck is playing a lower-priority Tape, in which case we destroy the Tape & occupy the Deck with this Tape, or (2) all TapeDecks are playing higher priority Tapes, in which case the request to play a Tape using this struct fails silently
    bool loop;
    f32  loopStartSeconds;
    struct
    {
        bool            enabled;// if set, this will configure the Tape such that channelVolumeRatios are automatically calculated based on the remaining members
        Korl_Math_V3f32 worldPosition;
        f32             attenuation;
    } spatialization;
    struct
    {
        f32  volumeRatioStart;
        f32  volumeRatioTarget;
        f32  seconds;// the desired time to fade from `volumeRatioStart`=>`volumeRatioTarget`
        bool enabled;// true => `volumeRatio` will be gradually adjusted to `volumeRatioTarget` over the course of `seconds`
        bool stopWhenEqual;// true => the TapeDeck will automatically eject the tape when volumeRatio reaches `volumeRatioTarget`
    } volumeFade;
} Korl_Sfx_TapeDeckControl;
typedef struct Korl_Sfx_TapeCategoryControl
{
    f32 volumeRatio;
} Korl_Sfx_TapeCategoryControl;
korl_global_const Korl_Sfx_TapeDeckControl KORL_SFX_TAPE_DECK_CONTROL_DEFAULT = {.volumeRatio = 1.f, .channelVolumeRatios = {1.f, 1.f}, .category = 0};
/** the # of Decks allocated inside korl-audio determines the # of Tapes that 
 * can be played simultaneously; if the user attempts to call \c korl_sfx_play* 
 * when all Decks are currently playing audio, a \c NULL TapeHandle will be 
 * returned, and no additional audio will be played */
// #define KORL_FUNCTION_korl_sfx_setDeckCount(name)   void                name(u16 deckCount)
#define KORL_FUNCTION_korl_sfx_playResource(name) Korl_Sfx_TapeHandle name(Korl_Resource_Handle resourceHandleAudio, Korl_Sfx_TapeDeckControl tapeDeckControl)
#define KORL_FUNCTION_korl_sfx_setVolume(name)    void                name(f32 volumeRatio)
#define KORL_FUNCTION_korl_sfx_category_set(name) void                name(u8 category, Korl_Sfx_TapeCategoryControl tapeCategoryControl)
#define KORL_FUNCTION_korl_sfx_setListener(name)  void                name(Korl_Math_V3f32 worldPosition, Korl_Math_V3f32 worldNormalUp, Korl_Math_V3f32 worldNormalForward)
#define KORL_FUNCTION_korl_sfx_tape_stop(name)    void                name(Korl_Sfx_TapeHandle* tapeHandle, f32 fadeOutSeconds)
// #define KORL_FUNCTION_korl_sfx_tape_setPause(name)  void                name(Korl_Sfx_TapeHandle* tapeHandle, bool paused)
// #define KORL_FUNCTION_korl_sfx_tape_setVolume(name) void                name(Korl_Sfx_TapeHandle* tapeHandle, f32 volumeRatio)
// #define KORL_FUNCTION_korl_sfx_tape_isValid(name)   bool                name(Korl_Sfx_TapeHandle* tapeHandle)
// #define KORL_FUNCTION_korl_sfx_tape_isPaused(name)  bool                name(Korl_Sfx_TapeHandle* tapeHandle)
