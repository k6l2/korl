#include "game.h"
#pragma warning( push )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	#include "imgui/imgui.h"
#pragma warning( pop )
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	g_log = memory.platformLog;
	kassert(sizeof(GameState) <= memory.permanentMemoryBytes);
	g_gs = reinterpret_cast<GameState*>(memory.permanentMemory);
	g_krb = &memory.krb;
	ImGui::SetCurrentContext(
		reinterpret_cast<ImGuiContext*>(memory.imguiContext));
	ImGui::SetAllocatorFunctions(memory.platformImguiAlloc, 
	                             memory.platformImguiFree, 
	                             memory.imguiAllocUserData);
}
GAME_INITIALIZE(gameInitialize)
{
	// GameState memory initialization //
	*g_gs = {};
	// initialize dynamic allocators //
	g_gs->kgaHPermanent = 
		kgaInit(reinterpret_cast<u8*>(memory.permanentMemory) + 
		            sizeof(GameState), 
		        memory.permanentMemoryBytes - sizeof(GameState));
	g_gs->kgaHTransient = kgaInit(memory.transientMemory, 
	                              memory.transientMemoryBytes);
	// Contruct/Initialize the game's AssetManager //
	g_gs->assetManager = kamConstruct(g_gs->kgaHPermanent, 1024,
	                                  g_gs->kgaHTransient);
	// load RawImages from platform files //
	g_gs->kahImgFighter =
		kamAddPng(g_gs->assetManager, memory.platformLoadPng, 
		          "fighter.png");
	const int assetIndexFighter = KASSET("fighter.png");
	// Ask the platform to load us a RawSound asset //
	g_gs->kahSfxShoot = 
		kamAddWav(g_gs->assetManager, memory.platformLoadWav, 
		          "joesteroids-shoot-modified.wav");
	g_gs->kahSfxHit = 
		kamAddWav(g_gs->assetManager, memory.platformLoadWav, 
		          "joesteroids-hit.wav");
	g_gs->kahSfxExplosion = 
		kamAddWav(g_gs->assetManager, memory.platformLoadWav, 
		          "joesteroids-explosion.wav");
	g_gs->kahBgmBattleTheme = 
		kamAddOgg(g_gs->assetManager, memory.platformLoadOgg, 
		          "joesteroids-battle-theme-modified.ogg");
	// Initialize the game's audio mixer //
	g_gs->kAudioMixer = kauConstruct(g_gs->kgaHPermanent, 16, 
	                                 g_gs->assetManager);
	KTapeHandle tapeBgmBattleTheme = 
		kauPlaySound(g_gs->kAudioMixer, g_gs->kahBgmBattleTheme);
	kauSetRepeat(g_gs->kAudioMixer, &tapeBgmBattleTheme, true);
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kauMix(g_gs->kAudioMixer, audioBuffer, sampleBlocksConsumed);
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
			GamePad& gpad = gamePadArray[c];
			if(gpad.buttons.shoulderLeft == ButtonState::PRESSED)
			{
				kauPlaySound(g_gs->kAudioMixer, g_gs->kahSfxHit);
			}
			if(gpad.buttons.shoulderRight >= ButtonState::PRESSED)
			{
				if(gpad.buttons.faceLeft >= ButtonState::PRESSED)
				{
					kauPlaySound(g_gs->kAudioMixer, g_gs->kahSfxExplosion);
				}
				else
				{
					kauPlaySound(g_gs->kAudioMixer, g_gs->kahSfxShoot);
				}
			}
			g_gs->shipWorldPosition.x += 10*gpad.normalizedStickLeft.x;
			g_gs->shipWorldPosition.y += 10*gpad.normalizedStickLeft.y;
			if(!kmath::isNearlyZero(gpad.normalizedStickLeft.x) ||
			   !kmath::isNearlyZero(gpad.normalizedStickLeft.y))
			{
				const f32 stickRadians = 
					kmath::v2Radians(gpad.normalizedStickLeft);
				g_gs->shipWorldOrientation = 
					kmath::quat({0,0,1}, stickRadians - PI32/2);
			}
			gpad.normalizedMotorSpeedLeft  = gpad.normalizedTriggerLeft;
			gpad.normalizedMotorSpeedRight = gpad.normalizedTriggerRight;
			if (gpad.buttons.back  == ButtonState::HELD &&
			    gpad.buttons.start == ButtonState::PRESSED)
			{
				return false;
			}
			if(gpad.buttons.stickClickLeft == ButtonState::PRESSED)
			{
				g_gs->shipWorldPosition = {};
			}
			if(gpad.buttons.stickClickRight == ButtonState::PRESSED)
			{
				g_gs->viewOffset2d = {};
			}
		}
	}
	g_gs->viewOffset2d = g_gs->shipWorldPosition;
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	g_krb->setProjectionOrtho(static_cast<f32>(windowDimensions.x), 
	                          static_cast<f32>(windowDimensions.y), 1.f);
	g_krb->viewTranslate(-g_gs->viewOffset2d);
	g_krb->useTexture(kamGetRawImage(g_gs->assetManager, 
	                                 g_gs->kahImgFighter).krbTextureHandle);
	g_krb->setModelXform(g_gs->shipWorldPosition, g_gs->shipWorldOrientation);
	g_krb->drawQuadTextured({50,50}, {0,0}, {0,1}, {1,1}, {1,0});
	g_krb->setModelXform({0,0}, kmath::IDENTITY_QUATERNION);
	g_krb->drawLine({0,0}, {100,   0}, krb::RED);
	g_krb->drawLine({0,0}, {  0, 100}, krb::GREEN);
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