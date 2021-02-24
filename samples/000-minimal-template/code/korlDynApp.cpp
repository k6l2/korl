#include "korlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
		return false;
//	ImGui::Text("Hello KORL!");
	g_kpl->mouseSetRelativeMode(KORL_BUTTON_ON(gameMouse.left));
	if(KORL_BUTTON_ON(gameMouse.left))
	{
		g_gs->viewPosition.x += 
			0.001f*static_cast<f32>(gameMouse.deltaPosition.x);
		g_gs->viewPosition.y -= 
			0.001f*static_cast<f32>(gameMouse.deltaPosition.y);
	}
	g_krb->beginFrame(v3f32{0.2f, 0, 0.2f}.elements, windowDimensions.elements);
	defer(g_krb->endFrame());
	g_krb->setProjectionOrthoFixedHeight(1, 1);
	g_krb->setViewXform2d(g_gs->viewPosition);
	g_krb->setDefaultColor(krb::BLACK);
	/* draw a triangle with color vertex attributes */
	{
		local_const KgtVertex TRI_VERTICES[] = 
			{ {.position = {-0.5f, -0.5f,  0.f}, .color = {1.f, 0.f, 0.f, 0.5f}}
			, {.position = { 0.5f, -0.5f,  0.f}, .color = {0.f, 1.f, 0.f, 0.5f}}
			, {.position = { 0.0f,  0.5f,  0.f}, .color = {0.f, 0.f, 1.f, 0.5f}} };
		local_persist f32 radians = 0;
		local_persist f32 seconds = 0;
		radians += deltaSeconds*2*PI32;
		seconds += deltaSeconds;
		const f32 scale = sinf(seconds)*0.5f + 0.5f;
		g_krb->setModelXform2d(
			{0.5,0}, q32(v3f32::Z, radians, true), {scale, scale});
		g_krb->drawTris(
			TRI_VERTICES, CARRAY_SIZE(TRI_VERTICES), sizeof(*TRI_VERTICES), 
			KGT_VERTEX_ATTRIBS_NO_TEXTURE);
	}
	g_krb->setModelXform(v3f32::ZERO, q32::IDENTITY, {1,1,1});
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
	/* minimal code to draw a quad */
	{
		KGT_USE_IMAGE(KgtAssetIndex::birb_png);
		g_krb->drawQuadTextured(
			v2f32{1.f, 1.f}.elements, v2f32{0.5f, 0.5f}.elements, 
			KGT_DRAW_QUAD_WHITE, v2f32::ZERO.elements, v2f32{1,1}.elements);
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
