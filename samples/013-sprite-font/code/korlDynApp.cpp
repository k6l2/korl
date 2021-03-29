#include "korlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
		return false;
	if(ImGui::Begin("Sample Instructions"))
	{
		ImGui::Text("Try typing some lower-case letters in here:");
		ImGui::InputText("", g_gs->textBuffer, CARRAY_SIZE(g_gs->textBuffer));
	}
	ImGui::End();
	g_krb->beginFrame(v3f32{0.2f, 0, 0.2f}.elements, windowDimensions.elements);
	g_krb->setProjectionOrthoFixedHeight(256, 1);
	kgtSpriteFontDraw(KgtAssetIndex::gfx_sample_sprite_font_sfm, 
		g_gs->textBuffer, v2f32::ZERO, {0.5f, 0}, {1,1}, 
		krb::WHITE, krb::TRANSPARENT, g_krb, g_kam);
	g_krb->endFrame();
	return true;
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kgtGameStateRenderAudio(audioBuffer, sampleBlocksConsumed);
}
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	g_gs = reinterpret_cast<GameState*>(memory.permanentMemory);
	kgtGameStateOnReloadCode(&g_gs->kgtGameState, memory);
}
GAME_INITIALIZE(gameInitialize)
{
	*g_gs = {};// clear all GameState memory before initializing the template
	kgtGameStateInitialize(memory, sizeof(GameState));
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "kgtGameState.cpp"
