#include "KmlGame.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
		return false;
	/* display GUI window containing sample instructions */
	ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("[E,S,D,F] - move");
	ImGui::Text("[R] - reset");
	ImGui::End();
	/* handle keyboard input & update the sample player's state */
	v2f32 controlDirection = {};
	if(windowIsFocused)
	{
		if(gameKeyboard.e >= ButtonState::HELD)
		{
			controlDirection.y += 1;
		}
		if(gameKeyboard.d >= ButtonState::HELD)
		{
			controlDirection.y -= 1;
		}
		if(gameKeyboard.s >= ButtonState::HELD)
		{
			controlDirection.x -= 1;
		}
		if(gameKeyboard.f >= ButtonState::HELD)
		{
			controlDirection.x += 1;
		}
		if(gameKeyboard.r == ButtonState::PRESSED)
			g_gs->samplePlayer = {};
	}
	const f32 controlMag = controlDirection.normalize();
	g_gs->samplePlayer.position += 100.f * deltaSeconds * controlDirection;
	if(!kmath::isNearlyZero(controlMag))
		g_gs->samplePlayer.orientation = 
			kQuaternion({0,0,1}, kmath::v2Radians(controlDirection) - PI32/2);
	/* draw the sample */
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	g_krb->setProjectionOrthoFixedHeight(windowDimensions.x, windowDimensions.y, 
	                                     100, 1.f);
	/* draw a simple 2D origin */
	{
		g_krb->setModelXform2d({0,0}, kQuaternion::IDENTITY, {10,10});
		const local_persist VertexNoTexture meshOrigin[] = 
			{ {{0,0,0}, krb::RED  }, {{1,0,0}, krb::RED  }
			, {{0,0,0}, krb::GREEN}, {{0,1,0}, krb::GREEN} };
		DRAW_LINES(meshOrigin, VERTEX_NO_TEXTURE_ATTRIBS);
	}
	/* if you can draw a triangle, you can draw ANYTHING~ */
	{
		g_krb->setModelXform2d(g_gs->samplePlayer.position, 
		                       g_gs->samplePlayer.orientation, {1,1});
		const local_persist VertexNoTexture meshTri[] = 
			{ {{  0, 5,0}, krb::RED  }
			, {{ 3, -5,0}, krb::GREEN}
			, {{-3, -5,0}, krb::BLUE } };
		DRAW_TRIS(meshTri, VERTEX_NO_TEXTURE_ATTRIBS);
	}
	return true;
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
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
GAME_RENDER_AUDIO(gameRenderAudio)
{
	templateGameState_renderAudio(&g_gs->templateGameState, audioBuffer, 
	                              sampleBlocksConsumed);
}
#include "TemplateGameState.cpp"