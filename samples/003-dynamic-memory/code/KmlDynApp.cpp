#include "KmlDynApp.h"
#include <algorithm>
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(g_kgs, gameKeyboard, windowIsFocused))
		return false;
	g_gs->seconds += deltaSeconds;
	/* display GUI window containing sample instructions */
	ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("[space] - add actor");
	ImGui::Text("[backspace] - remove actor");
	ImGui::End();
	/* handle keyboard input & update the sample player's state */
	v2f32 controlDirection = {};
	if(windowIsFocused)
	{
		if(gameKeyboard.space == ButtonState::PRESSED)
		{
			/* dynamically add a new actor into the actor array using STB_DS */
			arrput(g_gs->dynamicArrayActorPositions, v3f32{});
			if(arrlenu(g_gs->dynamicArrayActorPositions) > 1)
			/* re-arrange the actors in a circle if there's more than 1 of 
				them */
			{
				const f32 radiansPerActor = 
					2*PI32 / arrlenu(g_gs->dynamicArrayActorPositions);
				for(size_t a = 0; 
					a < arrlenu(g_gs->dynamicArrayActorPositions); a++)
				{
					g_gs->dynamicArrayActorPositions[a] = 
						q32(WORLD_UP, a*radiansPerActor)
							.transform(v3f32{10,0,0});
				}
			}
		}
		if(gameKeyboard.backspace == ButtonState::PRESSED)
		{
			/* dynamically remove actors from array using STB_DS */
			if(arrlen(g_gs->dynamicArrayActorPositions) > 0)
				arrpop(g_gs->dynamicArrayActorPositions);
		}
	}
	/* draw the sample */
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	g_krb->setBackfaceCulling(true);
	g_krb->setDepthTesting(true);
	g_krb->setProjectionFov(90.f, windowDimensions.elements, 1.f, 100.f);
	const v2f32 camPos2d = kmath::rotate({15,0}, 0.25f*g_gs->seconds);
	const v3f32 camPosition = v3f32{camPos2d.x,camPos2d.y,0.5};
	g_krb->lookAt(camPosition.elements, v3f32::ZERO.elements, 
	              WORLD_UP.elements);
	/* draw a simple 2D origin */
	kgtDrawOrigin({10,10,10});
	const local_persist KgtVertex tetrahedronVerts[] = 
		{ {{ 1, 1, 1}, {}, {1,0,0,0.75}}
		, {{-1,-1, 1}, {}, {0,1,0,0.75}}
		, {{-1, 1,-1}, {}, {0,0,1,0.75}}
		, {{ 1,-1,-1}, {}, {1,1,0,0.75}} };
	const local_persist KgtVertex meshTri[] = 
		{ tetrahedronVerts[0], tetrahedronVerts[2], tetrahedronVerts[1]
		, tetrahedronVerts[0], tetrahedronVerts[1], tetrahedronVerts[3]
		, tetrahedronVerts[0], tetrahedronVerts[3], tetrahedronVerts[2]
		, tetrahedronVerts[1], tetrahedronVerts[2], tetrahedronVerts[3] };
	/* render translucent actors from back to front */
	if(arrlenu(g_gs->dynamicArrayActorPositions) > 0)
	{
		const size_t actorTempArrayLength = 
			arrlenu(g_gs->dynamicArrayActorPositions);
		/* we can sort a separate array of actors using a linear frame allocator 
			for temp storage, allowing us to preserve the original actor array's 
			order for demonstration purposes */
		v3f32* actorTempArray = ALLOC_FRAME_ARRAY(v3f32, actorTempArrayLength);
		for(size_t a = 0; a < actorTempArrayLength; a++)
			actorTempArray[a] = g_gs->dynamicArrayActorPositions[a];
		const auto actorSort = 
			[&camPosition](const v3f32& a, const v3f32& b)->bool
			{
				return (a - camPosition).magnitudeSquared() > 
				       (b - camPosition).magnitudeSquared();
			};
		std::sort(actorTempArray, actorTempArray + actorTempArrayLength, 
		          actorSort);
		for(size_t a = 0; a < actorTempArrayLength; a++)
		{
			g_krb->setModelXform(actorTempArray[a], q32::IDENTITY, {1,1,1});
			DRAW_TRIS(meshTri, KGT_VERTEX_ATTRIBS_NO_TEXTURE);
		}
	}
	return true;
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	kgtGameStateOnReloadCode(memory);
	g_gs = reinterpret_cast<GameState*>(memory.permanentMemory);
}
GAME_INITIALIZE(gameInitialize)
{
	*g_gs = {};// clear all GameState memory before initializing the template
	kgtGameStateInitialize(&g_gs->kgtGameState, memory, sizeof(GameState));
	/* initialize a dynamic array of actor positions using STB_DS */
	g_gs->dynamicArrayActorPositions = 
		arrinit(v3f32, g_gs->kgtGameState.hKalPermanent);
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kgtGameStateRenderAudio(g_kgs, audioBuffer, sampleBlocksConsumed);
}
#include "kgtGameState.cpp"
