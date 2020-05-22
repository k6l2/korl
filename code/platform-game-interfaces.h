#pragma once
#include "global-defines.h"
#include "krb-interface.h"
// Data structures which must be the same for the Platform & Game layers ///////
/* PLATFORM INTERFACE *********************************************************/
enum class PlatformLogCategory : u8
{
	K_INFO,
	K_WARNING,
	K_ERROR
};
struct RawImage
{
	u32 sizeX;
	u32 sizeY;
	// pixel data layout: 0xRrGgBbAa
	u8* pixelData;
};
struct RawSound
{
	u32 sampleHz;
	u32 sampleBlockCount;
	u16 bitsPerSample;
	u8 channelCount;
	u8 channelCount_PADDING[5];
	// A sample block is a contiguous collection of all samples from all 
	//	channels.  Sample data is stored in blocks.
	SoundSample* sampleData;
};
#define PLATFORM_LOG(name) void name(const char* sourceFileName, \
                                     u32 sourceFileLineNumber, \
                                     PlatformLogCategory logCategory, \
                                     const char* formattedString, ...)
#define PLATFORM_IMGUI_ALLOC(name) void* name(size_t sz, void* user_data)
#define PLATFORM_IMGUI_FREE(name) void  name(void* ptr, void* user_data)
#define PLATFORM_DECODE_Z85_PNG(name) RawImage name(const u8* z85PngData, \
                                                    size_t z85ImageNumBytes)
///TODO: pass a general allocator to PLATFORM_DECODE_Z85_PNG to destroy the
///      PLATFORM_FREE_RAW_IMAGE API since it would become completely useless
#define PLATFORM_FREE_RAW_IMAGE(name) void name(RawImage& rawImage)
/**
 * @return If there is a failure loading the file, an invalid RawSound 
 *         containing sampleData==nullptr is returned.
 */
using KgaHandle = void*;
#define PLATFORM_LOAD_WAV(name) RawSound name(const char* fileName, \
                                              KgaHandle kgaHandle)
typedef PLATFORM_LOG(fnSig_platformLog);
typedef PLATFORM_IMGUI_ALLOC(fnSig_platformImguiAlloc);
typedef PLATFORM_IMGUI_FREE(fnSig_platformImguiFree);
typedef PLATFORM_DECODE_Z85_PNG(fnSig_platformDecodeZ85Png);
typedef PLATFORM_FREE_RAW_IMAGE(fnSig_platformFreeRawImage);
typedef PLATFORM_LOAD_WAV(fnSig_platformLoadWav);
// INTERNAL DEBUG INTERFACE STUFF //////////////////////////////////////////////
#if INTERNAL_BUILD
struct PlatformDebugReadFileResult
{
	u32 dataBytes;
	u32 dataBytes_PADDING;
	void* data;
};
/** @return a valid result (non-zero data & dataBytes) if successful */
#define PLATFORM_READ_ENTIRE_FILE(name) \
	PlatformDebugReadFileResult name(char* fileName)
#define PLATFORM_FREE_FILE_MEMORY(name) void name(void* fileMemory)
#define PLATFORM_WRITE_ENTIRE_FILE(name) bool name(char* fileName, \
                                                   void* data, u32 dataBytes)
typedef PLATFORM_READ_ENTIRE_FILE(fnSig_PlatformReadEntireFile);
typedef PLATFORM_FREE_FILE_MEMORY(fnSig_PlatformFreeFileMemory);
typedef PLATFORM_WRITE_ENTIRE_FILE(fnSig_PlatformWriteEntireFile);
#endif
/***************************************************** END PLATFORM INTERFACE */
struct GameMemory
{
	void* permanentMemory;
	u64   permanentMemoryBytes;
	void* transientMemory;
	u64   transientMemoryBytes;
	///TODO: maybe instead of passing shit tons of function pointers here, we 
	///      can do something similar to ImGui and have a `platformContext` &
	///      `krbContext` which is passed as a parameter to all these functions?
	fnSig_platformLog* platformLog;
	fnSig_platformDecodeZ85Png* platformDecodeZ85Png;
	fnSig_platformFreeRawImage* platformFreeRawImage;
	fnSig_platformLoadWav* platformLoadWav;
#if INTERNAL_BUILD
	fnSig_PlatformReadEntireFile* platformReadEntireFile;
	fnSig_PlatformFreeFileMemory* platformFreeFileMemory;
	fnSig_PlatformWriteEntireFile* platformWriteEntireFile;
#endif
	fnSig_krbBeginFrame* krbBeginFrame;
	fnSig_krbSetProjectionOrtho* krbSetProjectionOrtho;
	fnSig_krbDrawLine* krbDrawLine;
	fnSig_krbDrawTri* krbDrawTri;
	fnSig_krbDrawTriTextured* krbDrawTriTextured;
	fnSig_krbViewTranslate* krbViewTranslate;
	fnSig_krbSetModelXform* krbSetModelXform;
	fnSig_krbLoadImage* krbLoadImage;
	fnSig_krbUseTexture* krbUseTexture;
	void* imguiContext;
	fnSig_platformImguiAlloc* platformImguiAlloc;
	fnSig_platformImguiFree* platformImguiFree;
	void* imguiAllocUserData;
};
struct GameAudioBuffer
{
	// A sample block is a contiguous collection of all samples from all 
	//	channels.  `memory` is stored in blocks.
	///TODO: rename this to `sampleBlocks`
	SoundSample* memory;
	///TODO: rename this to `lockedSampleBlockCount`?
	u32 lockedSampleCount;
	u32 soundSampleHz;
	u8 numSoundChannels;
#if INTERNAL_BUILD
	u8 numSoundChannels_PADDING[7];
#endif
};
enum class ButtonState : u8
{
	NOT_PRESSED,
	PRESSED,
	HELD
};
struct GameKeyboard
{
	struct Modifiers
	{
		bool alt;
		bool shift;
		bool control;
	} modifiers;
	union
	{
		ButtonState vKeys[94];
		struct 
		{
			ButtonState a;
			ButtonState b;
			ButtonState c;
			ButtonState d;
			ButtonState e;
			ButtonState f;
			ButtonState g;
			ButtonState h;
			ButtonState i;
			ButtonState j;
			ButtonState k;
			ButtonState l;
			ButtonState m;
			ButtonState n;
			ButtonState o;
			ButtonState p;
			ButtonState q;
			ButtonState r;
			ButtonState s;
			ButtonState t;
			ButtonState u;
			ButtonState v;
			ButtonState w;
			ButtonState x;
			ButtonState y;
			ButtonState z;
			ButtonState comma;
			ButtonState period;
			ButtonState slashForward;
			ButtonState slashBack;
			ButtonState curlyBraceLeft;
			ButtonState curlyBraceRight;
			ButtonState semicolon;
			ButtonState quote;
			ButtonState grave;
			ButtonState tenkeyless_0;
			ButtonState tenkeyless_1;
			ButtonState tenkeyless_2;
			ButtonState tenkeyless_3;
			ButtonState tenkeyless_4;
			ButtonState tenkeyless_5;
			ButtonState tenkeyless_6;
			ButtonState tenkeyless_7;
			ButtonState tenkeyless_8;
			ButtonState tenkeyless_9;
			ButtonState tenkeyless_minus;
			ButtonState equals;
			ButtonState backspace;
			ButtonState escape;
			ButtonState enter;
			ButtonState space;
			ButtonState tab;
			ButtonState shiftLeft;
			ButtonState shiftRight;
			ButtonState controlLeft;
			ButtonState controlRight;
			ButtonState alt;
			ButtonState f1;
			ButtonState f2;
			ButtonState f3;
			ButtonState f4;
			ButtonState f5;
			ButtonState f6;
			ButtonState f7;
			ButtonState f8;
			ButtonState f9;
			ButtonState f10;
			ButtonState f11;
			ButtonState f12;
			ButtonState arrowUp;
			ButtonState arrowDown;
			ButtonState arrowLeft;
			ButtonState arrowRight;
			ButtonState insert;
			ButtonState deleteKey;
			ButtonState home;
			ButtonState end;
			ButtonState pageUp;
			ButtonState pageDown;
			ButtonState numpad_0;
			ButtonState numpad_1;
			ButtonState numpad_2;
			ButtonState numpad_3;
			ButtonState numpad_4;
			ButtonState numpad_5;
			ButtonState numpad_6;
			ButtonState numpad_7;
			ButtonState numpad_8;
			ButtonState numpad_9;
			ButtonState numpad_period;
			ButtonState numpad_divide;
			ButtonState numpad_multiply;
			ButtonState numpad_minus;
			ButtonState numpad_add;
#if INTERNAL_BUILD
			/** KEEP THIS THE LAST VAR IN THE STRUCT ! ! ! 
			 * This is used to make sure the size of the vKeys array matches the
			 * # of buttons in this anonymous struct.
			*/
			ButtonState DUMMY_LAST_BUTTON_STATE;
#endif
		};
	};
};
struct GamePad
{
	v2f32 normalizedStickLeft;
	v2f32 normalizedStickRight;
	struct Buttons
	{
		ButtonState faceUp;
		ButtonState faceDown;
		ButtonState faceLeft;
		ButtonState faceRight;
		ButtonState start;
		ButtonState back;
		ButtonState dPadUp;
		ButtonState dPadDown;
		ButtonState dPadLeft;
		ButtonState dPadRight;
		ButtonState shoulderLeft;
		ButtonState shoulderRight;
		ButtonState stickClickLeft;
		ButtonState stickClickRight;
	} buttons;
#if INTERNAL_BUILD
	i16 buttons_PADDING;
#endif
	float normalizedTriggerLeft;
	float normalizedTriggerRight;
	float normalizedMotorSpeedLeft;
	float normalizedMotorSpeedRight;
};
/* GAME INTERFACE *************************************************************/
/**
 * Guaranteed to be called once when the application starts.
 */
#define GAME_INITIALIZE(name) void name(GameMemory& memory)
/**
 * OPTIONAL.  Guaranteed to be called every time the game code is reloaded.
 * Guaranteed to be called BEFORE GAME_INITIALIZE.
 */
#define GAME_ON_RELOAD_CODE(name) void name(GameMemory& memory)
#define GAME_RENDER_AUDIO(name) void name(GameMemory& memory, \
                                          GameAudioBuffer& audioBuffer)
/** 
 * @return false if the platform should close the game application
 */
#define GAME_UPDATE_AND_DRAW(name) bool name(GameMemory& memory, \
                                             v2u32 windowDimensions, \
                                             GameKeyboard& gameKeyboard, \
                                             GamePad* gamePadArray, \
                                             u8 numGamePads)
typedef GAME_INITIALIZE(fnSig_gameInitialize);
typedef GAME_ON_RELOAD_CODE(fnSig_gameOnReloadCode);
typedef GAME_RENDER_AUDIO(fnSig_gameRenderAudio);
typedef GAME_UPDATE_AND_DRAW(fnSig_gameUpdateAndDraw);
extern "C" GAME_INITIALIZE(gameInitialize);
extern "C" GAME_ON_RELOAD_CODE(gameOnReloadCode);
extern "C" GAME_RENDER_AUDIO(gameRenderAudio);
extern "C" GAME_UPDATE_AND_DRAW(gameUpdateAndDraw);
/********************************************************* END GAME INTERFACE */