#pragma once
#define internal        static
#define local_persist   static
#define global_variable static
#if INTERNAL_BUILD || SLOW_BUILD
	#define kassert(expression) if(!(expression)) { *(int*)0 = 0; }
#else
	#define kassert(expression) {}
#endif
// crt math operations
#include <math.h>
#include <stdint.h>
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using SoundSample = i16;
const f32 PI32 = 3.14159f;
struct v2f32
{
	f32 x;
	f32 y;
};
namespace kmath
{
	// Thanks, Micha Wiedenmann
	// Derived from: https://stackoverflow.com/q/19837576
	internal inline bool isNearlyEqual(f32 fA, f32 fB)
	{
		local_persist const f32 EPSILON = 1e-5f;
		return fabsf(fA - fB) <= EPSILON * fabsf(fA);
	}
	internal inline bool isNearlyZero(f32 f)
	{
		local_persist const f32 EPSILON = 1e-5f;
		return isNearlyEqual(f, 0.f);
	}
	/** @return # of bytes which represent `k` kilobytes */
	internal inline u64 kilobytes(u64 k)
	{
		return k*1024;
	}
	/** @return # of bytes which represent `m` megabytes */
	internal inline u64 megabytes(u64 m)
	{
		return kilobytes(m)*1024;
	}
	/** @return # of bytes which represent `g` gigabytes */
	internal inline u64 gigabytes(u64 g)
	{
		return megabytes(g)*1024;
	}
	/** @return # of bytes which represent `t` terabytes */
	internal inline u64 terabytes(u64 t)
	{
		return gigabytes(t)*1024;
	}
	internal inline u32 safeTruncateU64(u64 value)
	{
		kassert(value < 0xFFFFFFFF);
		return static_cast<u32>(value);
	}
}
// Data structures which must be the same for the Platform & Game layers ///////
/* PLATFORM INTERFACE *********************************************************/
#define PLATFORM_PRINT_DEBUG_STRING(name) void name(char* string)
typedef PLATFORM_PRINT_DEBUG_STRING(fnSig_PlatformPrintDebugString);
#if INTERNAL_BUILD
struct PlatformDebugReadFileResult
{
	void* data;
	u32 dataBytes;
};
#define PLATFORM_READ_ENTIRE_FILE(name) \
	PlatformDebugReadFileResult name(char* fileName)
typedef PLATFORM_READ_ENTIRE_FILE(fnSig_PlatformReadEntireFile);
/** @return a valid result (non-zero data & dataBytes) if successful */
#define PLATFORM_FREE_FILE_MEMORY(name) void name(void* fileMemory)
typedef PLATFORM_FREE_FILE_MEMORY(fnSig_PlatformFreeFileMemory);
#define PLATFORM_WRITE_ENTIRE_FILE(name) bool name(char* fileName, \
                                                   void* data, u32 dataBytes)
typedef PLATFORM_WRITE_ENTIRE_FILE(fnSig_PlatformWriteEntireFile);
#endif
/***************************************************** END PLATFORM INTERFACE */
struct GameGraphicsBuffer
{
	void* bitmapMemory;
	u32 width;
	u32 height;
	u32 pitch;
	u8 bytesPerPixel;
};
struct GameAudioBuffer
{
	SoundSample* memory;
	u32 lockedSampleCount;
	u32 soundSampleHz;
	u8 numSoundChannels;
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
	float normalizedTriggerLeft;
	float normalizedTriggerRight;
	float normalizedMotorSpeedLeft;
	float normalizedMotorSpeedRight;
};
struct GameMemory
{
	bool initialized;
	void* permanentMemory;
	u64   permanentMemoryBytes;
	void* transientMemory;
	u64   transientMemoryBytes;
	fnSig_PlatformPrintDebugString* platformPrintDebugString;
#if INTERNAL_BUILD
	fnSig_PlatformReadEntireFile* platformReadEntireFile;
	fnSig_PlatformFreeFileMemory* platformFreeFileMemory;
	fnSig_PlatformWriteEntireFile* platformWriteEntireFile;
#endif
};
/* GAME INTERFACE *************************************************************/
#define GAME_RENDER_AUDIO(name) void name(GameMemory& memory, \
                                          GameAudioBuffer& audioBuffer)
typedef GAME_RENDER_AUDIO(fnSig_GameRenderAudio);
GAME_RENDER_AUDIO(gameRenderAudioStub)
{
}
extern "C" GAME_RENDER_AUDIO(gameRenderAudio);
#define GAME_UPDATE_AND_DRAW(name) bool name(GameMemory& memory, \
                                             GameGraphicsBuffer& graphicsBuffer, \
                                             GameKeyboard& gameKeyboard, \
                                             GamePad* gamePadArray, \
                                             u8 numGamePads)
typedef GAME_UPDATE_AND_DRAW(fnSig_GameUpdateAndDraw);
GAME_UPDATE_AND_DRAW(gameUpdateAndDrawStub)
{
	return false;
}
/** 
 * @return false if the platform should close the game application
 */
extern "C" GAME_UPDATE_AND_DRAW(gameUpdateAndDraw);
/********************************************************* END GAME INTERFACE */