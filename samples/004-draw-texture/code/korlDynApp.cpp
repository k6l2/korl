#include "korlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
		return false;
	/* display GUI window containing sample instructions */
	ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("[s] - rotate camera clockwise");
	ImGui::Text("[f] - rotate camera counter-clockwise");
	ImGui::End();
	/* process input */
	if(windowIsFocused)
	{
		if(gameKeyboard.s >= ButtonState::PRESSED)
		{
			g_gs->camPosition2d = 
				kmath::rotate(g_gs->camPosition2d, -deltaSeconds);
		}
		if(gameKeyboard.f >= ButtonState::PRESSED)
		{
			g_gs->camPosition2d = 
				kmath::rotate(g_gs->camPosition2d, deltaSeconds);
		}
	}
	/* begin drawing the sample */
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	/* setup a 3D projection to draw a textured cube */
	g_krb->setBackfaceCulling(true);
	g_krb->setDepthTesting(true);
	g_krb->setProjectionFov(90.f, windowDimensions.elements, 1.f, 100.f);
	const v3f32 camPosition = 
		v3f32{g_gs->camPosition2d.x, g_gs->camPosition2d.y, 7};
	g_krb->lookAt(camPosition.elements, v3f32::ZERO.elements, 
	              WORLD_UP.elements);
	kgtDrawOrigin({10,10,10});
	/* draw a textured cube 
		- the `kasset` build tool automatically generates KAssetIndex entries 
			for all files (excluding ones that match regex patterns in the 
			`assets/assets.ignore` file)
		- the template game state contains code which automatically loads assets 
			asynchronously at runtime
		- the template game state also automatically hot-reloads assets when 
			they are changed on disk! */
	{
		const size_t meshBoxBytes = 36*sizeof(KgtVertex);
		KgtVertex* meshBox = KGT_ALLOC_FRAME_ARRAY(KgtVertex, 36);
		kmath::generateMeshBox(
			{2,2,2}, meshBox, meshBoxBytes, sizeof(meshBox[0]), 
			offsetof(KgtVertex, position), offsetof(KgtVertex, textureNormal));
		g_krb->setModelXform({0,0,0}, q32::IDENTITY, {4,4,4});
		USE_IMAGE(KgtAssetIndex::gfx_crate_tex);
		DRAW_TRIS_DYNAMIC(meshBox, 36, KGT_VERTEX_ATTRIBS_NO_COLOR);
	}
	/* draw a simple textured quad on the screen */
	g_krb->setProjectionOrtho(windowDimensions.x, windowDimensions.y, 1);
	kgtDrawTexture2d(KgtAssetIndex::gfx_crate_tex, 
		{static_cast<f32>(windowDimensions.x)* 0.5f, 
		 static_cast<f32>(windowDimensions.y)*-0.5f}, {1,1}, 0.f, {4,4});
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
