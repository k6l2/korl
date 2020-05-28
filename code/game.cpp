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
	// load RawImages from platform files //
	g_gameState->kahImgFighter =
		kamAddPng(g_gameState->assetManager, memory.platformLoadPng,
		          "assets/fighter.png");
	g_gameState->kthFighter = 
		memory.krb.loadImage(kamGetRawImage(g_gameState->assetManager, 
		                                    g_gameState->kahImgFighter));
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
	g_gameState->kAudioMixer = kauConstruct(g_gameState->kgaHPermanent, 16, 
	                                        g_gameState->assetManager);
	KTapeHandle tapeBgmBattleTheme = 
		kauPlaySound(g_gameState->kAudioMixer, g_gameState->kahBgmBattleTheme);
	kauSetRepeat(g_gameState->kAudioMixer, &tapeBgmBattleTheme, true);
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kauMix(g_gameState->kAudioMixer, audioBuffer, sampleBlocksConsumed);
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	ImGui::ShowDemoWindow();
	if (gameKeyboard.escape == ButtonState::PRESSED ||
		(gameKeyboard.f4 == ButtonState::PRESSED && gameKeyboard.modifiers.alt))
	{
		return false;
	}
	if(numGamePads > 0)
	{
		for(u8 c = 0; c < numGamePads; c++)
		{
			if(gamePadArray[c].buttons.shoulderLeft == ButtonState::PRESSED)
			{
				kauPlaySound(g_gameState->kAudioMixer,
				             g_gameState->kahSfxHit);
			}
			if(gamePadArray[c].buttons.shoulderRight >= ButtonState::PRESSED)
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
			if(!kmath::isNearlyZero(gamePadArray[c].normalizedStickLeft.x) ||
			   !kmath::isNearlyZero(gamePadArray[c].normalizedStickLeft.y))
			{
				const f32 stickRadians = 
					kmath::v2Radians(gamePadArray[c].normalizedStickLeft);
				g_gameState->shipWorldOrientation = 
					kmath::quat({0,0,1}, stickRadians - PI32/2);
			}
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
		}
	}
	g_gameState->viewOffset2d = g_gameState->shipWorldPosition;
	memory.krb.beginFrame(0.2f, 0.f, 0.2f);
	memory.krb.setProjectionOrtho(static_cast<f32>(windowDimensions.x), 
	                              static_cast<f32>(windowDimensions.y), 1.f);
	memory.krb.viewTranslate(-g_gameState->viewOffset2d);
	memory.krb.useTexture(g_gameState->kthFighter);
	memory.krb.setModelXform(g_gameState->shipWorldPosition, 
	                         g_gameState->shipWorldOrientation);
	memory.krb.drawQuadTextured({50,50}, {0,0}, {0,1}, {1,1}, {1,0});
	memory.krb.setModelXform({0,0}, kmath::IDENTITY_QUATERNION);
	memory.krb.drawLine({0,0}, {100,   0}, krb::RED);
	memory.krb.drawLine({0,0}, {  0, 100}, krb::GREEN);
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