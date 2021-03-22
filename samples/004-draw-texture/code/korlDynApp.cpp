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
			g_gs->camPosition2d = 
				kmath::rotate(g_gs->camPosition2d, -deltaSeconds);
		if(gameKeyboard.f >= ButtonState::PRESSED)
			g_gs->camPosition2d = 
				kmath::rotate(g_gs->camPosition2d, deltaSeconds);
	}
	/* begin drawing the sample */
	g_krb->beginFrame(
		v3f32{0.2f, 0.f, 0.2f}.elements, windowDimensions.elements);
	defer(g_krb->endFrame());
	/* setup a 3D projection to draw a textured cube */
	g_krb->setBackfaceCulling(true);
	g_krb->setDepthTesting(true);
	g_krb->setProjectionFov(90.f, 1.f, 100.f);
	const v3f32 camPosition = 
		v3f32{g_gs->camPosition2d.x, g_gs->camPosition2d.y, 7};
	g_krb->lookAt(
		camPosition.elements, v3f32::ZERO.elements, WORLD_UP.elements);
#if !SEPARATE_ASSET_MODULES_COMPLETE
	if(windowIsFocused)
		if(KORL_BUTTON_ON(gameKeyboard.r))
			kgt_assetManager_free(g_kam, KgtAssetIndex::gfx_crate_png);
#if 0
	/* display a little color square of the upper-left most pixel of 
		KgtAssetIndex::gfx_crate_png */
	RawImage imgCrate = kgt_assetPng_get(g_kam, KgtAssetIndex::gfx_crate_png);
	const u8 cratePixelR = korlRawImageGetRed(imgCrate, v2u32{0,0});
	const u8 cratePixelG = korlRawImageGetGreen(imgCrate, v2u32{0,0});
	const u8 cratePixelB = korlRawImageGetBlue(imgCrate, v2u32{0,0});
	ImGui::ColorButton(
		"", 
		ImVec4(cratePixelR/255.f
		      , cratePixelG/255.f
		      , cratePixelB/255.f, 1.f));
#endif//0
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
		g_krb->useTexture(
			kgt_assetTexture_get(g_kam, KgtAssetIndex::gfx_crate_tex));
		g_krb->drawTris(
			meshBox, 36, sizeof(meshBox[0]), KGT_VERTEX_ATTRIBS_NO_COLOR);
	}
	/* draw a simple textured quad on the screen */
	{
		g_krb->setProjectionOrtho(1);
		const v2f32 position = 
			{ static_cast<f32>(windowDimensions.x)* 0.5f
			, static_cast<f32>(windowDimensions.y)*-0.5f };
		const v2f32 ratioAnchor = {1,1};
		const f32 counterClockwiseRadians = 0.f;
		const v2f32 scale = {1,1};
		g_krb->setModelXform2d(
			position, q32{v3f32::Z, counterClockwiseRadians}, scale);
		g_krb->useTexture(
			kgt_assetTexture_get(g_kam, KgtAssetIndex::gfx_crate_tex));
		const RawImage rawImg = 
			kgt_assetPng_get(g_kam, KgtAssetIndex::gfx_crate_png);
		const v2u32 imageSize = {rawImg.sizeX, rawImg.sizeY};
		const v2f32 quadSize = 
			{ static_cast<f32>(imageSize.x)
			, static_cast<f32>(imageSize.y) };
		local_const RgbaF32 KGT_DRAW_QUAD_WHITE[]  = 
			{ krb::WHITE, krb::WHITE, krb::WHITE, krb::WHITE };
		g_krb->drawQuadTextured(
			quadSize.elements, ratioAnchor.elements, KGT_DRAW_QUAD_WHITE, 
			v2f32::ZERO.elements, v2f32{1,1}.elements);
	}
#else
	kgtDrawAxes({10,10,10});
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
		KGT_USE_IMAGE(KgtAssetIndex::gfx_crate_tex);
		KGT_DRAW_TRIS_DYNAMIC(meshBox, 36, KGT_VERTEX_ATTRIBS_NO_COLOR);
	}
	/* draw a simple textured quad on the screen */
	g_krb->setProjectionOrtho(1);
	kgtDrawTexture2d(KgtAssetIndex::gfx_crate_tex, 
		{static_cast<f32>(windowDimensions.x)* 0.5f, 
		 static_cast<f32>(windowDimensions.y)*-0.5f}, {1,1}, 0.f, {4,4});
#endif// SEPARATE_ASSET_MODULES_COMPLETE
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
#if !SEPARATE_ASSET_MODULES_COMPLETE
	kgt_assetManager_load(g_kam, KgtAssetIndex::gfx_crate_png);
#endif//!SEPARATE_ASSET_MODULES_COMPLETE
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "kgtGameState.cpp"
