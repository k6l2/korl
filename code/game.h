#pragma once
#include "global-defines.h"
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
struct GamePad
{
	v2f32 normalizedStickLeft;
	v2f32 normalizedStickRight;
	enum class ButtonState : u8
	{
		NOT_PRESSED,
		PRESSED,
		HELD
	};
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
};
struct GameState
{
	int offsetX = 0;
	int offsetY = 0;
	f32 theraminHz = 294.f;
	f32 theraminSine = 0;
	f32 theraminVolume = 5000;
};
/* PLATFORM INTERFACE *********************************************************/
internal void platformPrintDebugString(char* string);
#if INTERNAL_BUILD
struct PlatformDebugReadFileResult
{
	void* data;
	u32 dataBytes;
};
/** @return a valid result (non-zero data & dataBytes) if successful */
internal PlatformDebugReadFileResult platformReadEntireFile(char* fileName);
internal void platformFreeFileMemory(void* fileMemory);
internal bool platformWriteEntireFile(char* fileName, 
                                      void* data, u32 dataBytes);
#endif
/***************************************************** END PLATFORM INTERFACE */
internal void game_renderAudio(GameMemory& memory, 
                               GameAudioBuffer& audioBuffer, 
                               GamePad* gamePadArray, u8 numGamePads);
/** 
 * @return false if the platform should close the game application
 */
internal bool game_updateAndDraw(GameMemory& memory, 
                                 GameGraphicsBuffer& graphicsBuffer, 
                                 GamePad* gamePadArray, u8 numGamePads);