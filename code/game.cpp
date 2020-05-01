#include "game.h"
internal void game_renderAudio(GameMemory& memory, 
                               GameAudioBuffer& audioBuffer, 
                               GamePad* gamePadArray, u8 numGamePads)
{
	GameState* gameState = reinterpret_cast<GameState*>(memory.permanentMemory);
	// render theramin audio data to temporary sound buffer //
	if(numGamePads > 0)
	{
		for(u8 c = 0; c < numGamePads; c++)
		{
			gameState->theraminHz = 
				294 + gamePadArray[c].normalizedStickLeft.y*256;
		}
	}
	const u32 samplesPerWaveTheramin = 
		audioBuffer.soundSampleHz / gameState->theraminHz;
	for(u32 s = 0; s < audioBuffer.lockedSampleCount; s++)
	{
		SoundSample* sample = 
			audioBuffer.memory + (s*audioBuffer.numSoundChannels);
		// Determine what the debug waveforms should look like at this point of
		//	the running sound sample //
		const f32 waveform = sinf(gameState->theraminSine);
		gameState->theraminSine += 2*PI32*(1.f / samplesPerWaveTheramin);
		if(gameState->theraminSine > 2*PI32)
		{
			gameState->theraminSine -= 2*PI32;
		}
		for(u8 c = 0; c < audioBuffer.numSoundChannels; c++)
		{
			*sample++ = 
				static_cast<SoundSample>(gameState->theraminVolume * waveform);
		}
	}
}
internal bool game_updateAndDraw(GameMemory& memory, 
                                 GameGraphicsBuffer& graphicsBuffer, 
                                 GamePad* gamePadArray, u8 numGamePads)
{
	kassert(sizeof(GameState) <= memory.permanentMemoryBytes);
	GameState* gameState = reinterpret_cast<GameState*>(memory.permanentMemory);
	if(!memory.initialized)
	{
		*gameState = {};
		memory.initialized = true;
	}
	if(numGamePads > 0)
	{
		for(u8 c = 0; c < numGamePads; c++)
		{
			if(gamePadArray[c].buttons.dPadUp == GamePad::ButtonState::HELD)
			{
				gameState->offsetY -= 1;
			}
			if(gamePadArray[c].buttons.dPadDown == GamePad::ButtonState::HELD)
			{
				gameState->offsetY += 1;
			}
			gameState->offsetX += 4*gamePadArray[c].normalizedStickLeft.x;
			gameState->offsetY -= 4*gamePadArray[c].normalizedStickLeft.y;
			gamePadArray[c].normalizedMotorSpeedLeft = 
				gamePadArray[c].normalizedTriggerLeft;
			gamePadArray[c].normalizedMotorSpeedRight = 
				gamePadArray[c].normalizedTriggerRight;
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
			*pixel++ = ((u8)(x + gameState->offsetX) << 16) | 
			           ((u8)(y + gameState->offsetY) << 8);
		}
		row += graphicsBuffer.pitch;
	}
	return true;
}