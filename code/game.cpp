#include "game.h"
#include "platform-game-interfaces.h"
#include "z85.h"
#include "z85_png_fighter.h"
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
#if 0
struct GameGraphicsState
{
	KrbTextureHandle textureHandles[1024];
	bool textureHandleUsed[1024];
	u16 nextUnusedTextureHandle;
	u16 usedTextureHandleCount
};
#endif //0
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	kassert(sizeof(GameState) <= memory.permanentMemoryBytes);
	GameState* gameState = reinterpret_cast<GameState*>(memory.permanentMemory);
#if 0
	kassert(sizeof(GameGraphicsState) <= memory.transientMemoryBytes);
	GameGraphicsState* gameGraphicsState = 
		reinterpret_cast<GameGraphicsState*>(memory.transientMemory);
#endif //0
	if(!memory.initialized)
	{
		*gameState = {};
		gameState->gAllocTransient = kgaInit(memory.transientMemory, 
		                                     memory.transientMemoryBytes);
#if 0
		*gameGraphicsState = {};
		memory.krbAllocTextureHandles(gameGraphicsState->textureHandles,
		                             sizeof(gameGraphicsState->textureHandles));
#endif //0
		const size_t tempImageDataBytes = 
			z85::decodedFileSizeBytes(sizeof(z85_png_fighter) - 1);
		i8*const tempImageDataBuffer = reinterpret_cast<i8*>(
			kgaAlloc(gameState->gAllocTransient, 
			         tempImageDataBytes));
		gameState->kthFighter = 
			memory.krbLoadImageZ85(z85_png_fighter, sizeof(z85_png_fighter) - 1,
			                       tempImageDataBuffer);
		kgaFree(gameState->gAllocTransient, tempImageDataBuffer);
		kassert(kgaUsedBytes(gameState->gAllocTransient) == 0);
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
				gameState->viewOffset2d.y += 1.f;
			}
			if(gamePadArray[c].buttons.dPadDown == ButtonState::HELD)
			{
				gameState->viewOffset2d.y -= 1.f;
			}
			if(gamePadArray[c].buttons.dPadLeft == ButtonState::HELD)
			{
				gameState->viewOffset2d.x -= 1.f;
			}
			if(gamePadArray[c].buttons.dPadRight == ButtonState::HELD)
			{
				gameState->viewOffset2d.x += 1.f;
			}
			gameState->viewOffset2d.x += 
				10*gamePadArray[c].normalizedStickLeft.x;
			gameState->viewOffset2d.y += 
				10*gamePadArray[c].normalizedStickLeft.y;
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
	memory.krbSetProjectionOrtho(static_cast<f32>(windowDimensions.x), 
	                             static_cast<f32>(windowDimensions.y), 1.f);
	memory.krbViewTranslate(-gameState->viewOffset2d);
	memory.krbDrawLine({0,0}, {100,0}, krb::RED);
	memory.krbDrawLine({0,0}, {0,100}, krb::GREEN);
	memory.krbUseTexture(gameState->kthFighter);
	// memory.krbDrawTri({100,100}, {200,100}, {100,200});
	// memory.krbDrawTri({200,100}, {100,200}, {200,200});
	memory.krbDrawTriTextured({100,100}, {200,100}, {100,200},
	                          {0,1}, {1,1}, {0,0});
	memory.krbDrawTriTextured({200,100}, {100,200}, {200,200},
	                          {1,1}, {0,0}, {1,0});
#if 0
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
#endif
	return true;
}
#include "generalAllocator.cpp"
#include "z85.cpp"