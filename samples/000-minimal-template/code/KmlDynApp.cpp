#include "KmlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(g_kgs, gameKeyboard, windowIsFocused))
		return false;
	ImGui::Text("Hello KML!");
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
