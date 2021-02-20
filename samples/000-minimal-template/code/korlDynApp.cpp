#include "korlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
		return false;
//	ImGui::Text("Hello KORL!");
	g_krb->beginFrame(0.2f, 0, 0.2f);
	defer(g_krb->endFrame());
	/* minimal code to just draw a triangle */
	KgtVertex triVertices[] = 
		{ {.position = {-0.5f, -0.5f,  0.f}}
		, {.position = { 0.5f, -0.5f,  0.f}}
		, {.position = { 0.0f,  0.5f,  0.f}} };
	g_krb->drawTris(
		triVertices, CARRAY_SIZE(triVertices), sizeof(*triVertices), 
		KGT_VERTEX_ATTRIBS_POSITION_ONLY);
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
