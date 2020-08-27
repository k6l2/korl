#include "KmlGame.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
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
	/* draw the sample */
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	g_krb->setBackfaceCulling(true);
	g_krb->setDepthTesting(true);
	g_krb->setProjectionFov(90.f, windowDimensions.elements, 1.f, 100.f);
	const v3f32 camPosition = 
		v3f32{g_gs->camPosition2d.x, g_gs->camPosition2d.y, 7};
	g_krb->lookAt(camPosition.elements, v3f32::ZERO.elements, 
	              WORLD_UP.elements);
	/* draw a simple 2D origin */
	{
		g_krb->setModelXform(v3f32::ZERO, kQuaternion::IDENTITY, {10,10,10});
		const local_persist Vertex meshOrigin[] = 
			{ {v3f32::ZERO, krb::RED  }, {{1,0,0}, krb::RED  }
			, {v3f32::ZERO, krb::GREEN}, {{0,1,0}, krb::GREEN}
			, {v3f32::ZERO, krb::BLUE }, {{0,0,1}, krb::BLUE } };
		DRAW_LINES(meshOrigin, VERTEX_ATTRIBS_NO_TEXTURE);
	}
	/* draw textures onto geometry
		- the `kasset` build tool automatically generates KAssetIndex entries 
			for all files (excluding ones that match regex patterns in the 
			`assets/assets.ignore` file)
		- the template game state contains code which automatically loads assets 
			asynchronously at runtime
		- the template game state also automatically hot-reloads assets when 
			they are changed on disk! */
	{
		const local_persist Vertex CRATE_MESH[] = 
			// top (+Z) low-right
			{ {{ 1, 1, 1}, krb::WHITE, {1,1}}
			, {{-1, 1, 1}, krb::WHITE, {1,0}}
			, {{ 1,-1, 1}, krb::WHITE, {0,1}}
			// top (+Z) up-left
			, {{-1,-1, 1}, krb::WHITE, {0,0}}
			, {{ 1,-1, 1}, krb::WHITE, {0,1}}
			, {{-1, 1, 1}, krb::WHITE, {1,0}}
			// front (+X) low-right
			, {{ 1, 1, 1}, krb::WHITE, {1,0}}
			, {{ 1,-1,-1}, krb::WHITE, {0,1}}
			, {{ 1, 1,-1}, krb::WHITE, {1,1}}
			// front (+X) up-left
			, {{ 1,-1,-1}, krb::WHITE, {0,1}}
			, {{ 1, 1, 1}, krb::WHITE, {1,0}}
			, {{ 1,-1, 1}, krb::WHITE, {0,0}}
			// left (+Y) low-right
			, {{ 1, 1,-1}, krb::WHITE, {0,1}}
			, {{-1, 1,-1}, krb::WHITE, {1,1}}
			, {{-1, 1, 1}, krb::WHITE, {1,0}}
			// left (+Y) up-left
			, {{-1, 1, 1}, krb::WHITE, {1,0}}
			, {{ 1, 1, 1}, krb::WHITE, {0,0}}
			, {{ 1, 1,-1}, krb::WHITE, {0,1}}
			// back (-X) low-right
			, {{-1, 1,-1}, krb::WHITE, {0,1}}
			, {{-1,-1,-1}, krb::WHITE, {1,1}}
			, {{-1,-1, 1}, krb::WHITE, {1,0}}
			// back (-X) up-left
			, {{-1,-1, 1}, krb::WHITE, {1,0}}
			, {{-1, 1, 1}, krb::WHITE, {0,0}}
			, {{-1, 1,-1}, krb::WHITE, {0,1}}
			// right (-Y) low-right
			, {{-1,-1,-1}, krb::WHITE, {0,1}}
			, {{ 1,-1,-1}, krb::WHITE, {1,1}}
			, {{ 1,-1, 1}, krb::WHITE, {1,0}}
			// right (-Y) up-left
			, {{ 1,-1, 1}, krb::WHITE, {1,0}}
			, {{-1,-1, 1}, krb::WHITE, {0,0}}
			, {{-1,-1,-1}, krb::WHITE, {0,1}}
			// bottom (-Z) low-right
			, {{-1,-1,-1}, krb::WHITE, {0,1}}
			, {{-1, 1,-1}, krb::WHITE, {1,1}}
			, {{ 1, 1,-1}, krb::WHITE, {1,0}}
			// bottom (-Z) up-left
			, {{ 1, 1,-1}, krb::WHITE, {1,0}}
			, {{ 1,-1,-1}, krb::WHITE, {0,0}}
			, {{-1,-1,-1}, krb::WHITE, {0,1}}
			};
		g_krb->setModelXform({0,0,0}, kQuaternion::IDENTITY, {4,4,4});
		g_krb->useTexture(kamGetTexture(g_gs->templateGameState.assetManager, 
		                                KAssetIndex::gfx_crate_png));
		DRAW_TRIS(CRATE_MESH, VERTEX_ATTRIBS);
	}
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