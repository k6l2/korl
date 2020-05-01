#include "game.h"
internal void game_renderAudio(GameAudioBuffer& audioBuffer, 
                               GamePad* gamePadArray, u8 numGamePads)
{
	// render theramin audio data to temporary sound buffer //
	local_persist f32 theraminHz = 294.f;
	local_persist f32 theraminSine = 0;
	local_persist const f32 THERAMIN_VOLUME = 5000;
	if(numGamePads > 0)
	{
		for(u8 c = 0; c < numGamePads; c++)
		{
			theraminHz = 294 + gamePadArray[c].normalizedStickLeft.y*256;
		}
	}
	const u32 samplesPerWaveTheramin = audioBuffer.soundSampleHz / theraminHz;
	for(u32 s = 0; s < audioBuffer.lockedSampleCount; s++)
	{
		SoundSample* sample = 
			audioBuffer.memory + (s*audioBuffer.numSoundChannels);
		// Determine what the debug waveforms should look like at this point of
		//	the running sound sample //
		const f32 waveform = sinf(theraminSine);
		theraminSine += 2*PI32*(1.f / samplesPerWaveTheramin);
		while(theraminSine > 2*PI32) theraminSine -= 2*PI32;
		for(u8 c = 0; c < audioBuffer.numSoundChannels; c++)
		{
			*sample++ = static_cast<SoundSample>(THERAMIN_VOLUME * waveform);
		}
	}
}
internal bool game_updateAndDraw(GameGraphicsBuffer& graphicsBuffer, 
                                 GamePad* gamePadArray, u8 numGamePads)
{
	local_persist int offsetX = 0;
	local_persist int offsetY = 0;
	if(numGamePads > 0)
	{
		for(u8 c = 0; c < numGamePads; c++)
		{
			if(gamePadArray[c].buttons.dPadUp == GamePad::ButtonState::HELD)
			{
				offsetY -= 1;
			}
			if(gamePadArray[c].buttons.dPadDown == GamePad::ButtonState::HELD)
			{
				offsetY += 1;
			}
			offsetX += 4*gamePadArray[c].normalizedStickLeft.x;
			offsetY -= 4*gamePadArray[c].normalizedStickLeft.y;
			gamePadArray[c].normalizedMotorSpeedLeft = 
				gamePadArray[c].normalizedTriggerLeft;
			gamePadArray[c].normalizedMotorSpeedRight = 
				gamePadArray[c].normalizedTriggerRight;
#if 0
			if(gamePadArray[c].buttons.back  == GamePad::ButtonState::HELD)
			{
				platformPrintString("back HELD!\n");
			}
			if(gamePadArray[c].buttons.start  == GamePad::ButtonState::PRESSED)
			{
				platformPrintString("start PRESSED!\n");
			}
#endif
			if (gamePadArray[c].buttons.back  == GamePad::ButtonState::HELD &&
			    gamePadArray[c].buttons.start == GamePad::ButtonState::PRESSED)
			{
				return false;
			}
		}
	}
	// render a weird gradient pattern to the offscreen buffer //
	u8* row = reinterpret_cast<u8*>(graphicsBuffer.bitmapMemory);
	for (int y = 0; y < graphicsBuffer.height; y++)
	{
		u32* pixel = reinterpret_cast<u32*>(row);
		for (int x = 0; x < graphicsBuffer.width; x++)
		{
			// pixel format: 0xXxRrGgBb
			*pixel++ = ((u8)(x + offsetX) << 16) | ((u8)(y + offsetY) << 8);
		}
		row += graphicsBuffer.pitch;
	}
	return true;
}