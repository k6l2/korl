#include "korlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
		return false;
//	ImGui::Text("Hello KORL!");
	g_krb->beginFrame(v3f32{0.2f, 0, 0.2f}.elements, windowDimensions.elements);
	defer(g_krb->endFrame());
	/* minimal code to just draw a triangle */
	{
		local_const f32 VERTEX_POSITIONS[] = 
			{ -0.5f, -0.5f,  0.f
			,  0.5f, -0.5f,  0.f
			,  0.0f,  0.5f,  0.f };
		local_const u32 VERTEX_STRIDE = 3*sizeof(f32);
		local_const KrbVertexAttributeOffsets VERTEX_ATTRIB_OFFSETS = 
			{ .position_3f32 = 0
			, .color_4f32    = VERTEX_STRIDE
			, .texCoord_2f32 = VERTEX_STRIDE };
		g_krb->drawTris(
			VERTEX_POSITIONS, 3, VERTEX_STRIDE, VERTEX_ATTRIB_OFFSETS);
	}
	/* draw a triangle with color vertex attributes */
	{
		local_const KgtVertex TRI_VERTICES[] = 
			{ {.position = { 0.0f, -0.5f,  0.f}, .color = {1.f, 0.f, 0.f, 0.5f}}
			, {.position = { 1.0f, -0.5f,  0.f}, .color = {0.f, 1.f, 0.f, 0.5f}}
			, {.position = { 0.5f,  0.5f,  0.f}, .color = {0.f, 0.f, 1.f, 0.5f}} };
		g_krb->drawTris(
			TRI_VERTICES, CARRAY_SIZE(TRI_VERTICES), sizeof(*TRI_VERTICES), 
			KGT_VERTEX_ATTRIBS_NO_TEXTURE);
	}
	/* minimal code to draw a quad */
	{
		local_const Color4f32 QUAD_COLOR[4] = 
			{krb::BLACK, krb::BLACK, krb::BLACK, krb::BLACK};
		g_krb->drawQuad(
			v2f32{1.f, 1.f}.elements, v2f32{0.5f, 0.f}.elements, QUAD_COLOR);
	}
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
