#include "game.h"
GAME_RENDER_AUDIO(gameRenderAudio)
{
	GameState* gameState = reinterpret_cast<GameState*>(memory.permanentMemory);
	// render theramin audio data to temporary sound buffer //
	const u32 samplesPerWaveTheramin = static_cast<u32>(
		audioBuffer.soundSampleHz / gameState->theraminHz);
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
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	kassert(sizeof(GameState) <= memory.permanentMemoryBytes);
	GameState* gameState = reinterpret_cast<GameState*>(memory.permanentMemory);
	if(!memory.initialized)
	{
		*gameState = {};
#if INTERNAL_BUILD && 0
		PlatformDebugReadFileResult readFileResult = 
			memory.platformReadEntireFile(__FILE__);
		memory.platformPrintDebugString(
			reinterpret_cast<char*>(readFileResult.data));
		memory.platformWriteEntireFile("game_copy.cpp", 
		                               readFileResult.data, 
								       readFileResult.dataBytes);
		memory.platformFreeFileMemory(readFileResult.data);
#endif
		memory.initialized = true;
	}
	if (gameKeyboard.escape == ButtonState::PRESSED ||
		(gameKeyboard.f4 == ButtonState::PRESSED && gameKeyboard.modifiers.alt))
	{
		return false;
	}
	f32 bgClearGreen = 0;
	if(numGamePads > 0)
	{
		for(u8 c = 0; c < numGamePads; c++)
		{
			gameState->theraminHz = 
				294 + gamePadArray[c].normalizedStickLeft.y*256;
			if(gamePadArray[c].buttons.dPadUp == ButtonState::HELD)
			{
				gameState->offsetY -= 1;
			}
			if(gamePadArray[c].buttons.dPadDown == ButtonState::HELD)
			{
				gameState->offsetY += 1;
			}
			if(gamePadArray[c].buttons.dPadLeft == ButtonState::HELD)
			{
				gameState->offsetX -= 1;
			}
			if(gamePadArray[c].buttons.dPadRight == ButtonState::HELD)
			{
				gameState->offsetX += 1;
			}
			gameState->offsetX += 
				static_cast<i32>(100*gamePadArray[c].normalizedStickLeft.x);
			gameState->offsetY -= 
				static_cast<i32>(100*gamePadArray[c].normalizedStickLeft.y);
			gamePadArray[c].normalizedMotorSpeedLeft = 
				gamePadArray[c].normalizedTriggerLeft;
			gamePadArray[c].normalizedMotorSpeedRight = 
				gamePadArray[c].normalizedTriggerRight;
			if (gamePadArray[c].buttons.back  == ButtonState::HELD &&
			    gamePadArray[c].buttons.start == ButtonState::PRESSED)
			{
				return false;
			}
			bgClearGreen = fabsf(gamePadArray[c].normalizedStickLeft.y);
		}
	}
	memory.krbBeginFrame(0.2f, bgClearGreen, 0.2f);
	// render a weird gradient pattern to the offscreen buffer //
	u8* row = reinterpret_cast<u8*>(graphicsBuffer.bitmapMemory);
	for (u32 y = 0; y < graphicsBuffer.height; y++)
	{
		u32* pixel = reinterpret_cast<u32*>(row);
		for (u32 x = 0; x < graphicsBuffer.width; x++)
		{
			// pixel format: 0xXxRrGgBb
			*pixel++ = static_cast<u32>(((u8)(x + gameState->offsetX) << 8) | 
			                            ((u8)(y + gameState->offsetY) << 8));
		}
		row += graphicsBuffer.pitch;
	}
	return true;
}