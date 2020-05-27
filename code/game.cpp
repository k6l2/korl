#include "game.h"
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
	// GameState memory initialization //
	*g_gameState = {};
	// initialize dynamic allocators //
	g_gameState->kgaHPermanent = 
		kgaInit(reinterpret_cast<u8*>(memory.permanentMemory) + 
		            sizeof(GameState), 
		        memory.permanentMemoryBytes - sizeof(GameState));
	g_gameState->kgaHTransient = kgaInit(memory.transientMemory, 
	                                     memory.transientMemoryBytes);
	// Contruct/Initialize the game's AssetManager //
	g_gameState->assetManager = kamConstruct(g_gameState->kgaHPermanent, 1024,
	                                         g_gameState->kgaHTransient);
#if 0
	// upload a texture to the GPU //
	RawImage rawImage = 
		memory.platformDecodeZ85Png(z85_png_fighter, 
		                            sizeof(z85_png_fighter) - 1);
	g_gameState->kthFighter = memory.krbLoadImage(rawImage);
	memory.platformFreeRawImage(rawImage);
#endif // 0
	// Ask the platform to load us a RawSound asset //
	g_gameState->kahSfxShoot = 
		kamAddWav(g_gameState->assetManager, memory.platformLoadWav, 
		          "assets/joesteroids-shoot-modified.wav");
	g_gameState->kahSfxHit = 
		kamAddWav(g_gameState->assetManager, memory.platformLoadWav, 
		          "assets/joesteroids-hit.wav");
	g_gameState->kahSfxExplosion = 
		kamAddWav(g_gameState->assetManager, memory.platformLoadWav, 
		          "assets/joesteroids-explosion.wav");
	g_gameState->kahBgmBattleTheme = 
		kamAddOgg(g_gameState->assetManager, memory.platformLoadOgg, 
		          "assets/joesteroids-battle-theme-modified.ogg");
	// Initialize the game's audio mixer //
	g_gameState->kAudioMixer = kauConstruct(g_gameState->kgaHPermanent, 32, 
	                                        g_gameState->assetManager);
	KTapeHandle tapeBgmBattleTheme = 
		kauPlaySound(g_gameState->kAudioMixer, g_gameState->kahBgmBattleTheme);
	kauSetRepeat(g_gameState->kAudioMixer, &tapeBgmBattleTheme, true);
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kauMix(g_gameState->kAudioMixer, audioBuffer);
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
			if(gamePadArray[c].buttons.shoulderLeft == ButtonState::PRESSED)
			{
				kauPlaySound(g_gameState->kAudioMixer,
				             g_gameState->kahSfxHit);
			}
			if(gamePadArray[c].buttons.shoulderRight == ButtonState::PRESSED)
			{
				if(gamePadArray[c].buttons.faceLeft >= ButtonState::PRESSED)
				{
					kauPlaySound(g_gameState->kAudioMixer,
					             g_gameState->kahSfxExplosion);
				}
				else
				{
					kauPlaySound(g_gameState->kAudioMixer,
					             g_gameState->kahSfxShoot);
				}
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
	//memory.krbUseTexture(g_gameState->kthFighter);
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
#include "kAudioMixer.cpp"
#include "kAssetManager.cpp"
#include "generalAllocator.cpp"
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