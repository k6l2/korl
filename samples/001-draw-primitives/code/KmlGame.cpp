#include "KmlGame.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
		return false;
	g_gs->seconds += deltaSeconds;// animate the sample scene
	/* Initialize backend renderer for drawing.   This must occur before ANY 
		drawing operations take place!  */
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	/* Setup an orthographic projection transform such that the y-axis always 
		represents 100 `units` in world-space, regardless of window size.  
		The `view` transform is right-handed & automatically centered on the 
		origin by default. */
	g_krb->setProjectionOrthoFixedHeight(windowDimensions.x, windowDimensions.y, 
	                                     100, 1.f);
	/* draw a simple 2D origin */
	{
		g_krb->setModelXform({0,0,0}, kmath::IDENTITY_QUATERNION, {10,10,10});
		const local_persist VertexNoTexture meshOrigin[] = 
			{ {{0,0,0}, krb::RED  }, {{1,0,0}, krb::RED  }
			, {{0,0,0}, krb::GREEN}, {{0,1,0}, krb::GREEN} };
		DRAW_LINES(meshOrigin, VERTEX_NO_TEXTURE_ATTRIBS);
	}
	/* if you can draw a triangle, you can draw ANYTHING~ */
	{
		const kQuaternion quatModel = kQuaternion({0,0,1}, g_gs->seconds);
		const f32 distanceFromOrigin = 
			kmath::lerp(10, 40, SINEF_0_1(3*g_gs->seconds));
		const v3f32 positionModel = kQuaternion({0,0,-1}, g_gs->seconds)
			.transform({distanceFromOrigin,0,0});
		g_krb->setModelXform(positionModel, quatModel, {1,1,1});
		const local_persist VertexNoTexture meshTri[] = 
			{ {{  0, 30,0}, {1,0,0,0.5f}}
			, {{ 30,-20,0}, {0,1,0,0.5f}}
			, {{-30,-20,0}, {0,0,1,0.5f}} };
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