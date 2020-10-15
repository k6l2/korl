#include "KmlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(g_kgs, gameKeyboard, windowIsFocused))
		return false;
	KgtAudioMixer*const am = g_kgs->audioMixer;
	if(ImGui::Begin("Jukebox", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Music (OGG-Vorbis)");
		if(ImGui::SliderFloat("volume ratio", &g_gs->musicVolumeRatio, 0, 1))
		{
			kgtAudioMixerSetVolume(am, &g_gs->htMusic, g_gs->musicVolumeRatio);
		}
		if(ImGui::Checkbox("loop?", &g_gs->musicLoop))
		{
			kgtAudioMixerSetRepeat(am, &g_gs->htMusic, g_gs->musicLoop);
		}
		if(kgtAudioMixerIsTapeHandleValid(am, &g_gs->htMusic))
		{
			if(kgtAudioMixerGetPause(am, &g_gs->htMusic))
			{
				if(ImGui::Button("Resume"))
				{
					kgtAudioMixerSetPause(am, &g_gs->htMusic, false);
				}
			}
			else
			{
				if(ImGui::Button("Pause"))
				{
					kgtAudioMixerSetPause(am, &g_gs->htMusic, true);
				}
			}
			ImGui::SameLine();
			if(ImGui::Button("Stop"))
			{
				kgtAudioMixerStopSound(am, &g_gs->htMusic);
			}
		}
		else
		{
			if(ImGui::Button("Play"))
			{
				g_gs->htMusic = kgtAudioMixerPlaySound(
					am, 
					KgtAssetIndex::sfx_joesteroids_battle_theme_modified_ogg);
				kgtAudioMixerSetRepeat(am, &g_gs->htMusic, g_gs->musicLoop);
				kgtAudioMixerSetVolume(
					am, &g_gs->htMusic, g_gs->musicVolumeRatio);
			}
		}
		ImGui::Separator();
		ImGui::Text("Sound Effects (WAVE)");
		ImGui::SliderFloat("sfx volume ratio", &g_gs->sfxVolumeRatio, 0, 1);
		if(ImGui::Button("Play Explosion"))
		{
			KgtTapeHandle kth = 
				kgtAudioMixerPlaySound(
					am, KgtAssetIndex::sfx_joesteroids_explosion_wav);
			kgtAudioMixerSetVolume(am, &kth, g_gs->sfxVolumeRatio);
		}
		if(ImGui::Button("Play Hit"))
		{
			KgtTapeHandle kth = 
				kgtAudioMixerPlaySound(
					am, KgtAssetIndex::sfx_joesteroids_hit_wav);
			kgtAudioMixerSetVolume(am, &kth, g_gs->sfxVolumeRatio);
		}
		if(ImGui::Button("Play Shoot"))
		{
			KgtTapeHandle kth = 
				kgtAudioMixerPlaySound(
					am, KgtAssetIndex::sfx_joesteroids_shoot_modified_wav);
			kgtAudioMixerSetVolume(am, &kth, g_gs->sfxVolumeRatio);
		}
	}
	ImGui::End();
	g_krb->beginFrame(0.2f, 0, 0.2f);
	return true;
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kgtGameStateRenderAudio(g_kgs, audioBuffer, sampleBlocksConsumed);
}
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	kgtGameStateOnReloadCode(memory);
	g_gs = reinterpret_cast<GameState*>(memory.permanentMemory);
}
GAME_INITIALIZE(gameInitialize)
{
	*g_gs = {};// clear all GameState memory before initializing the template
	kgtGameStateInitialize(&g_gs->kgtGameState, memory, sizeof(GameState));
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "kgtGameState.cpp"
