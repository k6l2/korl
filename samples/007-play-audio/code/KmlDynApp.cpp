#include "KmlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
		return false;
	if(ImGui::Begin("Jukebox", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Music (OGG-Vorbis)");
		if(ImGui::SliderFloat("volume ratio", &g_gs->musicVolumeRatio, 0, 1))
		{
			kauSetVolume(g_gs->templateGameState.kAudioMixer, 
			             &g_gs->musicTapeHandle, g_gs->musicVolumeRatio);
		}
		if(ImGui::Checkbox("loop?", &g_gs->musicLoop))
		{
			kauSetRepeat(g_gs->templateGameState.kAudioMixer, 
			             &g_gs->musicTapeHandle, g_gs->musicLoop);
		}
		if(kauIsTapeHandleValid(g_gs->templateGameState.kAudioMixer, 
		                        &g_gs->musicTapeHandle))
		{
			if(kauGetPause(g_gs->templateGameState.kAudioMixer, 
			               &g_gs->musicTapeHandle))
			{
				if(ImGui::Button("Resume"))
				{
					kauSetPause(g_gs->templateGameState.kAudioMixer, 
					            &g_gs->musicTapeHandle, false);
				}
			}
			else
			{
				if(ImGui::Button("Pause"))
				{
					kauSetPause(g_gs->templateGameState.kAudioMixer, 
					            &g_gs->musicTapeHandle, true);
				}
			}
			ImGui::SameLine();
			if(ImGui::Button("Stop"))
			{
				kauStopSound(g_gs->templateGameState.kAudioMixer, 
				             &g_gs->musicTapeHandle);
			}
		}
		else
		{
			if(ImGui::Button("Play"))
			{
				g_gs->musicTapeHandle = 
					kauPlaySound(
						g_gs->templateGameState.kAudioMixer, 
						KAssetIndex::sfx_joesteroids_battle_theme_modified_ogg);
				kauSetRepeat(g_gs->templateGameState.kAudioMixer, 
				             &g_gs->musicTapeHandle, g_gs->musicLoop);
				kauSetVolume(g_gs->templateGameState.kAudioMixer, 
				             &g_gs->musicTapeHandle, g_gs->musicVolumeRatio);
			}
		}
		ImGui::Separator();
		ImGui::Text("Sound Effects (WAVE)");
		ImGui::SliderFloat("sfx volume ratio", &g_gs->sfxVolumeRatio, 0, 1);
		if(ImGui::Button("Play Explosion"))
		{
			KTapeHandle kth = 
				kauPlaySound(g_gs->templateGameState.kAudioMixer,
				             KAssetIndex::sfx_joesteroids_explosion_wav);
			kauSetVolume(g_gs->templateGameState.kAudioMixer, &kth, 
			             g_gs->sfxVolumeRatio);
		}
		if(ImGui::Button("Play Hit"))
		{
			KTapeHandle kth = 
				kauPlaySound(g_gs->templateGameState.kAudioMixer,
				             KAssetIndex::sfx_joesteroids_hit_wav);
			kauSetVolume(g_gs->templateGameState.kAudioMixer, &kth, 
			             g_gs->sfxVolumeRatio);
		}
		if(ImGui::Button("Play Shoot"))
		{
			KTapeHandle kth = 
				kauPlaySound(g_gs->templateGameState.kAudioMixer,
				             KAssetIndex::sfx_joesteroids_shoot_modified_wav);
			kauSetVolume(g_gs->templateGameState.kAudioMixer, &kth, 
			             g_gs->sfxVolumeRatio);
		}
	}
	ImGui::End();
	g_krb->beginFrame(0.2f, 0, 0.2f);
	return true;
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	templateGameState_renderAudio(&g_gs->templateGameState, audioBuffer, 
	                              sampleBlocksConsumed);
}
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	templateGameState_onReloadCode(memory);
	g_gs = reinterpret_cast<GameState*>(memory.permanentMemory);
}
GAME_INITIALIZE(gameInitialize)
{
	*g_gs = {};// clear all GameState memory before initializing the template
	templateGameState_initialize(&g_gs->templateGameState, memory, 
	                             sizeof(GameState));
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "TemplateGameState.cpp"