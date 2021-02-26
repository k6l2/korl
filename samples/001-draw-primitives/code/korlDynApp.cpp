#include "korlDynApp.h"
/* In order to draw primitives, we must define a structure to pack the vertex 
	data of our choice, as well as the memory layout instructions for KRB.  This 
	utility header provides definitions of such constructs for your 
	convenience! This header is automatically include in kgtGameState.h */
//#include "kgtVertex.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
		return false;
	g_gs->seconds += deltaSeconds;// animate the sample scene
	/* Initialize backend renderer for drawing.   This must occur before ANY 
		drawing operations take place!  */
	g_krb->beginFrame(
		v3f32{0.2f, 0.f, 0.2f}.elements, windowDimensions.elements);
	defer(g_krb->endFrame());
	/* Setup an orthographic projection transform such that the y-axis always 
		represents 100 `units` in world-space, regardless of window size.  
		The `view` transform is right-handed & automatically centered on the 
		origin by default. */
	g_krb->setProjectionOrthoFixedHeight(100, 1.f);
	kgtDrawAxes({10,10,10});
	/* if you can draw a triangle, you can draw ANYTHING~ */
	{
		const q32 quatModel = {{0,0,1}, g_gs->seconds};
		const f32 distanceFromOrigin = 
			kmath::lerp(10, 40, kmath::sine_0_1(3*g_gs->seconds));
		const v2f32 positionModel = q32{{0,0,-1}, g_gs->seconds}
			.transform(v2f32{distanceFromOrigin,0});
		g_krb->setModelXform2d(positionModel, quatModel, {1,1});
		local_persist const KgtVertex meshTri[] = 
			{ {{  0, 30,0}, {}, {1,0,0,0.5f}}
			, {{ 30,-20,0}, {}, {0,1,0,0.5f}}
			, {{-30,-20,0}, {}, {0,0,1,0.5f}} };
		KGT_DRAW_TRIS(meshTri, KGT_VERTEX_ATTRIBS_NO_TEXTURE);
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
