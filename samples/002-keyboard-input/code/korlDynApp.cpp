#include "korlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
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
		g_gs->samplePlayer.orient = 
			{v3f32::Z, kmath::v2Radians(controlDirection) - PI32/2};
	/* draw the sample */
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	g_krb->setProjectionOrthoFixedHeight(
		windowDimensions.x, windowDimensions.y, 100, 1.f);
	kgtDrawOrigin({10,10,10});
	/* if you can draw a triangle, you can draw ANYTHING~ */
	{
		g_krb->setModelXform2d(
			g_gs->samplePlayer.position, g_gs->samplePlayer.orient, {1,1});
		const local_persist KgtVertex meshTri[] = 
			{ {{ 0,  5,0}, {}, krb::RED  }
			, {{ 3, -5,0}, {}, krb::GREEN}
			, {{-3, -5,0}, {}, krb::BLUE } };
		DRAW_TRIS(meshTri, KGT_VERTEX_ATTRIBS_NO_TEXTURE);
	}
	return true;
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
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
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kgtGameStateRenderAudio(audioBuffer, sampleBlocksConsumed);
}
#include "kgtGameState.cpp"
