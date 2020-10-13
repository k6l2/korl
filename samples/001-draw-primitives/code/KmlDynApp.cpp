#include "KmlDynApp.h"
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
		g_krb->setModelXform2d({0,0}, kQuaternion::IDENTITY, {10,10});
		local_persist const Vertex meshOrigin[] = 
			{ {{0,0,0}, {}, krb::RED  }, {{1,0,0}, {}, krb::RED  }
			, {{0,0,0}, {}, krb::GREEN}, {{0,1,0}, {}, krb::GREEN} };
		DRAW_LINES(g_krb, meshOrigin, VERTEX_ATTRIBS_NO_TEXTURE);
	}
	/* if you can draw a triangle, you can draw ANYTHING~ */
	{
		const kQuaternion quatModel = kQuaternion({0,0,1}, g_gs->seconds);
		const f32 distanceFromOrigin = 
			kmath::lerp(10, 40, kmath::sine_0_1(3*g_gs->seconds));
		const v2f32 positionModel = kQuaternion({0,0,-1}, g_gs->seconds)
			.transform(v2f32{distanceFromOrigin,0});
		g_krb->setModelXform2d(positionModel, quatModel, {1,1});
		local_persist const Vertex meshTri[] = 
			{ {{  0, 30,0}, {}, {1,0,0,0.5f}}
			, {{ 30,-20,0}, {}, {0,1,0,0.5f}}
			, {{-30,-20,0}, {}, {0,0,1,0.5f}} };
		DRAW_TRIS(g_krb, meshTri, VERTEX_ATTRIBS_NO_TEXTURE);
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
