#include "game.h"
#include "z85.h"
#include "z85_png_fighter.h"
#pragma warning( push )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	#include "imgui/imgui.h"
#pragma warning( pop )
#include <cstdio>
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	g_log = memory.platformLog;
	kassert(sizeof(GameState) <= memory.permanentMemoryBytes);
	g_gameState = reinterpret_cast<GameState*>(memory.permanentMemory);
	ImGui::SetCurrentContext(
		reinterpret_cast<ImGuiContext*>(memory.imguiContext));
	ImGui::SetAllocatorFunctions(memory.platformImguiAlloc, 
	                             memory.platformImguiFree, 
	                             memory.imguiAllocUserData);
}
GAME_INITIALIZE(gameInitialize)
{
	gameOnReloadCode(memory);
	*g_gameState = {};
	g_gameState->gAllocTransient = kgaInit(memory.transientMemory, 
	                                       memory.transientMemoryBytes);
	// upload a texture to the GPU //
	const size_t tempImageDataBytes = 
		z85::decodedFileSizeBytes(sizeof(z85_png_fighter) - 1);
	i8*const tempImageDataBuffer = reinterpret_cast<i8*>(
		kgaAlloc(g_gameState->gAllocTransient, 
		         tempImageDataBytes));
	g_gameState->kthFighter = 
		memory.krbLoadImageZ85(z85_png_fighter, sizeof(z85_png_fighter) - 1,
		                       tempImageDataBuffer);
	kgaFree(g_gameState->gAllocTransient, tempImageDataBuffer);
	kassert(kgaUsedBytes(g_gameState->gAllocTransient) == 0);
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	///TODO: write data to the GameAudioBuffer using an AudioMixer API
#if 0
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
#endif// 0
}
void poop()
{
	local_persist bool YES_STACK_OVERFLOW_PLS = true;
	if(YES_STACK_OVERFLOW_PLS) poop(); else printf("loop");
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	ImGui::ShowDemoWindow();
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
#if 0
			g_gameState->theraminHz = 
				294 + gamePadArray[c].normalizedStickLeft.y*256;
#endif// 0
			if(gamePadArray[c].buttons.dPadUp == ButtonState::HELD)
			{
				g_gameState->viewOffset2d.y += 1.f;
			}
			if(gamePadArray[c].buttons.dPadDown == ButtonState::HELD)
			{
				g_gameState->viewOffset2d.y -= 1.f;
			}
			if(gamePadArray[c].buttons.dPadLeft == ButtonState::HELD)
			{
				g_gameState->viewOffset2d.x -= 1.f;
			}
			if(gamePadArray[c].buttons.dPadRight == ButtonState::HELD)
			{
				g_gameState->viewOffset2d.x += 1.f;
			}
			if(gamePadArray[c].buttons.faceDown == ButtonState::PRESSED)
			{
				*(int*)0 = 0;// ;)
			}
			if(gamePadArray[c].buttons.faceRight == ButtonState::PRESSED)
			{
				poop();// ;o
			}
			g_gameState->shipWorldPosition.x += 
				10*gamePadArray[c].normalizedStickLeft.x;
			g_gameState->shipWorldPosition.y += 
				10*gamePadArray[c].normalizedStickLeft.y;
			g_gameState->viewOffset2d.x += 
				10*gamePadArray[c].normalizedStickRight.x;
			g_gameState->viewOffset2d.y += 
				10*gamePadArray[c].normalizedStickRight.y;
			gamePadArray[c].normalizedMotorSpeedLeft = 
				gamePadArray[c].normalizedTriggerLeft;
			gamePadArray[c].normalizedMotorSpeedRight = 
				gamePadArray[c].normalizedTriggerRight;
			if (gamePadArray[c].buttons.back  == ButtonState::HELD &&
			    gamePadArray[c].buttons.start == ButtonState::PRESSED)
			{
				return false;
			}
			if(gamePadArray[c].buttons.stickClickLeft == ButtonState::PRESSED)
			{
				g_gameState->shipWorldPosition = {};
			}
			if(gamePadArray[c].buttons.stickClickRight == ButtonState::PRESSED)
			{
				g_gameState->viewOffset2d = {};
			}
			bgClearGreen = fabsf(gamePadArray[c].normalizedStickLeft.y);
		}
	}
	memory.krbBeginFrame(0.2f, bgClearGreen, 0.2f);
	memory.krbSetProjectionOrtho(static_cast<f32>(windowDimensions.x), 
	                             static_cast<f32>(windowDimensions.y), 1.f);
	// g_gameState->viewOffset2d = g_gameState->shipWorldPosition;
	memory.krbViewTranslate(-g_gameState->viewOffset2d);
	memory.krbUseTexture(g_gameState->kthFighter);
	// memory.krbDrawTri({100,100}, {200,100}, {100,200});
	// memory.krbDrawTri({200,100}, {100,200}, {200,200});
	memory.krbSetModelXform(g_gameState->shipWorldPosition);
	memory.krbDrawTriTextured({-50,-50}, {50,-50}, {-50,50},
	                          {0,1}, {1,1}, {0,0});
	memory.krbDrawTriTextured({50,-50}, {-50,50}, {50,50},
	                          {1,1}, {0,0}, {1,0});
	memory.krbSetModelXform({0,0});
	memory.krbDrawLine({0,0}, {100,0}, krb::RED);
	memory.krbDrawLine({0,0}, {0,100}, krb::GREEN);
	return true;
}
#include "generalAllocator.cpp"
#include "z85.cpp"
#pragma warning( push )
	// warning C4127: conditional expression is constant
	#pragma warning( disable : 4127 )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	// warning C4365: 'argument': conversion
	#pragma warning( disable : 4365 )
	// warning C4577: 'noexcept' used with no exception handling mode 
	//                specified...
	#pragma warning( disable : 4577 )
	// warning C4774: 'sscanf' : format string expected in argument 2 is not a 
	//                string literal
	#pragma warning( disable : 4774 )
	#include "imgui/imgui_demo.cpp"
	#include "imgui/imgui_draw.cpp"
	#include "imgui/imgui_widgets.cpp"
	#include "imgui/imgui.cpp"
#pragma warning( pop )