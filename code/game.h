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
internal void platformPrintString(char* string);
internal void game_renderAudio(GameAudioBuffer& audioBuffer, 
                               GamePad* gamePadArray, u8 numGamePads);
/** 
 * @return false if the platform should close the game application
 */
internal bool game_updateAndDraw(GameGraphicsBuffer& graphicsBuffer, 
                                 GamePad* gamePadArray, u8 numGamePads);