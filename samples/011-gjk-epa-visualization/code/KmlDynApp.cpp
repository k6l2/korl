#include "KmlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
		return false;
	ImGui::Text("Hello KML!");
	if(windowIsFocused)
	{
		if(gameKeyboard.f1 == ButtonState::PRESSED)
		{
			arrsetlen(g_gs->actors, 2);
#if 1 // two overlapping boxes
			g_gs->actors[0].shape.type = ShapeType::BOX;
			g_gs->actors[1].shape.type = ShapeType::BOX;
			g_gs->actors[0].shape.box.lengths = {1,1,1};
			g_gs->actors[1].shape.box.lengths = {1,1,1};
			g_gs->actors[0].position = {-3,-3,3};
			g_gs->actors[1].position = {-3,-3,3};
			g_gs->actors[0].orientation = kQuaternion::IDENTITY;
			g_gs->actors[1].orientation = kQuaternion::IDENTITY;
#endif// two overlapping boxes
#if 0 // two barely overlapping spheres causing large GJK iteration count
			g_gs->actors[0].shape.type = ShapeType::SPHERE;
			g_gs->actors[1].shape.type = ShapeType::SPHERE;
			g_gs->actors[0].shape.sphere.radius = 1;
			g_gs->actors[1].shape.sphere.radius = 1;
			g_gs->actors[0].position = {8.92687702f, -0.164159507f, 0.522272706f};
			g_gs->actors[1].position = {8.23775101f, -0.965597868f, 2.22015452f};
			g_gs->actors[0].orient = q32::IDENTITY;
			g_gs->actors[1].orient = q32::IDENTITY;
#endif // two barely overlapping spheres causing large GJK iteration count
#if 0 // two overlapping spheres could be difficult to converge w/ EPA 
			g_gs->actors[0].shape.type = ShapeType::SPHERE;
			g_gs->actors[1].shape.type = ShapeType::SPHERE;
			g_gs->actors[0].shape.sphere.radius = 1;
			g_gs->actors[1].shape.sphere.radius = 1;
			g_gs->actors[0].position = {-3,-3,3};
			g_gs->actors[1].position = {-3,-3,3};
			g_gs->actors[0].orientation = kQuaternion::IDENTITY;
			g_gs->actors[1].orientation = kQuaternion::IDENTITY;
#endif// two overlapping spheres could be difficult to converge w/ EPA
#if 0// sphere-box causing GJK large iteration count
			g_gs->actors[0].shape.type = ShapeType::SPHERE;
			g_gs->actors[1].shape.type = ShapeType::BOX;
			g_gs->actors[0].shape.sphere.radius = 1;
			g_gs->actors[1].shape.box.lengths = {10.f, 2.57500005f, 1.f};
			g_gs->actors[0].position = {-1.79410827f, -0.718385637f, 0.663178146f};
			g_gs->actors[1].position = {-3.32584429f, -2.50412655f, 0.531933784f};
			g_gs->actors[0].orientation = kQuaternion::IDENTITY;
			g_gs->actors[1].orientation = {0.946916521f, -0.111779444f, -0.157558724f, -0.256962419f};
#endif// sphere-box causing GJK large iteration count
#if 0//sphere-box causing crash in EPA!!!
			g_gs->actors[0].shape.type = ShapeType::SPHERE;
			g_gs->actors[1].shape.type = ShapeType::BOX;
			g_gs->actors[0].shape.sphere.radius = 1;
			g_gs->actors[1].shape.box.lengths = {1,1,1};
			g_gs->actors[0].position = {2.17163181f, 1.81948531f, -0.776723206f};
			g_gs->actors[1].position = {0.847760916f, 2.80684662f, -0.114495963f};
			g_gs->actors[0].orientation = kQuaternion::IDENTITY;
			g_gs->actors[1].orientation = {0.976274312f, 0.0385377184f, -0.205251694f, -0.0572286993f};
#endif//sphere-box causing crash in EPA!!!
#if 0//two barely intersecting spheres
			g_gs->actors[0].shape.type = ShapeType::SPHERE;
			g_gs->actors[1].shape.type = ShapeType::SPHERE;
			g_gs->actors[0].shape.sphere.radius = 1;
			g_gs->actors[1].shape.sphere.radius = 1;
			g_gs->actors[0].position = {-0.214954376f,4.43508482f,-0.0842480659f};
			g_gs->actors[1].position = {-1.82670593f,5.56357288f,-0.432786942f};
			g_gs->actors[0].orientation = kQuaternion::IDENTITY;
			g_gs->actors[1].orientation = kQuaternion::IDENTITY;
#endif// two barely intersecting spheres
		}
		if(gameKeyboard.f2 == ButtonState::PRESSED)
		{
			Actor& actorA = g_gs->actors[0];
			Actor& actorB = g_gs->actors[1];
			ShapeGjkSupportData shapeGjkSupportData = 
				{ .shapeA       = actorA.shape
				, .positionA    = actorA.position
				, .orientationA = actorA.orientation
				, .shapeB       = actorB.shape
				, .positionB    = actorB.position
				, .orientationB = actorB.orientation };
			const v3f32 initialSupportDirection = 
				-(actorA.position - actorB.position);
			kmath::epa_cleanup(&g_gs->epaState);
			kmath::gjk_initialize(
				&g_gs->gjkState, shapeGjkSupport, &shapeGjkSupportData, 
				&initialSupportDirection);
			KLOG(INFO, "distance between actorA/B=%f", 
			     (actorA.position - actorB.position).magnitude());
		}
		if(gameKeyboard.f3 == ButtonState::PRESSED)
		{
			Actor& actorA = g_gs->actors[0];
			Actor& actorB = g_gs->actors[1];
			ShapeGjkSupportData shapeGjkSupportData = 
				{ .shapeA       = actorA.shape
				, .positionA    = actorA.position
				, .orientationA = actorA.orientation
				, .shapeB       = actorB.shape
				, .positionB    = actorB.position
				, .orientationB = actorB.orientation };
			if(g_gs->gjkState.lastIterationResult != 
				kmath::GjkIterationResult::SUCCESS)
			{
				kmath::gjk_iterate(
					&g_gs->gjkState, shapeGjkSupport, &shapeGjkSupportData);
				if(g_gs->gjkState.lastIterationResult == 
					kmath::GjkIterationResult::SUCCESS)
				{
					kmath::epa_initialize(
						&g_gs->epaState, g_gs->gjkState.o_simplex, 
						g_gs->templateGameState.hKalPermanent);
				}
			}
			else
			{
				kmath::epa_iterate(
					&g_gs->epaState, shapeGjkSupport, &shapeGjkSupportData, 
					g_gs->templateGameState.hKalFrame);
			}
		}
	}
	g_krb->beginFrame(0.2f, 0, 0.2f);
	/* GJK debug draw stuff */
	{
		Actor& actorA = g_gs->actors[0];
		Actor& actorB = g_gs->actors[1];
		ShapeGjkSupportData shapeGjkSupportData = 
			{ .shapeA       = actorA.shape
			, .positionA    = actorA.position
			, .orientationA = actorA.orientation
			, .shapeB       = actorB.shape
			, .positionB    = actorB.position
			, .orientationB = actorB.orientation };
		/* draw debug minkowski difference */
		{
			kmath::generateUniformSpherePoints(
				g_gs->minkowskiDifferencePointCloudCount, 
				g_gs->minkowskiDifferencePointCloud, 
				sizeof(g_gs->minkowskiDifferencePointCloud),
				sizeof(g_gs->minkowskiDifferencePointCloud[0]),
				offsetof(Vertex, position));
			for(u16 p = 0; p < g_gs->minkowskiDifferencePointCloudCount; p++)
			{
				g_gs->minkowskiDifferencePointCloud[p].position = 
					shapeGjkSupport(
						g_gs->minkowskiDifferencePointCloud[p].position, 
						&shapeGjkSupportData);
			}
			g_krb->setModelXform(v3f32::ZERO, kQuaternion::IDENTITY, {1,1,1});
			g_krb->setDefaultColor(krb::WHITE);
			g_krb->drawPoints(
				g_gs->minkowskiDifferencePointCloud, 
				g_gs->minkowskiDifferencePointCloudCount, 
				sizeof(g_gs->minkowskiDifferencePointCloud[0]), 
				VERTEX_ATTRIBS_POSITION_ONLY);
		}
		if(g_gs->gjkState.lastIterationResult == 
				kmath::GjkIterationResult::SUCCESS
			&& g_gs->epaState.lastIterationResult == 
				kmath::EpaIterationResult::SUCCESS)
		/* draw the minimum translation vector calculated via EPA! */
		{
			const v3f32 minTranslationVec = 
				g_gs->epaState.resultDistance * g_gs->epaState.resultNormal;
			g_krb->setModelXform(v3f32::ZERO, kQuaternion::IDENTITY, {1,1,1});
			const Vertex mesh[] = 
				{ {v3f32::ZERO      , {}, krb::CYAN}
				, {minTranslationVec, {}, krb::CYAN} };
			DRAW_LINES(mesh, VERTEX_ATTRIBS_NO_TEXTURE);
		}
		if(g_gs->gjkState.lastIterationResult == 
			kmath::GjkIterationResult::SUCCESS)
		/* draw the EPA's polytope */
		{
			const u16 epaPolytopeVertexCount = 
				kmath::epa_buildPolytopeTriangles(
					&g_gs->epaState, nullptr, 0, 0, 0, 0);
			Vertex* epaPolytopeTris = 
				ALLOC_FRAME_ARRAY(g_gs, Vertex, epaPolytopeVertexCount);
			kmath::epa_buildPolytopeTriangles(
				&g_gs->epaState, epaPolytopeTris, 
				epaPolytopeVertexCount*sizeof(Vertex), sizeof(Vertex), 
				offsetof(Vertex, position), offsetof(Vertex, color));
			g_krb->drawTris(
				epaPolytopeTris, epaPolytopeVertexCount, sizeof(Vertex), 
				VERTEX_ATTRIBS_NO_TEXTURE);
			/* draw the polytope outline since the scene is unlit */
			const u16 epaPolytopeLineVertexCount = 
				kmath::epa_buildPolytopeEdges(
					&g_gs->epaState, nullptr, 0, 0, 0, 
					g_gs->templateGameState.hKalFrame);
			Vertex* epaPolytopeLines = 
				ALLOC_FRAME_ARRAY(g_gs, Vertex, epaPolytopeLineVertexCount);
			kmath::epa_buildPolytopeEdges(
				&g_gs->epaState, epaPolytopeLines, 
				epaPolytopeLineVertexCount*sizeof(Vertex), sizeof(Vertex), 
				offsetof(Vertex, position), g_gs->templateGameState.hKalFrame);
			g_krb->setDefaultColor(krb::BLACK);
			g_krb->drawLines(
				epaPolytopeLines, epaPolytopeLineVertexCount, sizeof(Vertex), 
				VERTEX_ATTRIBS_POSITION_ONLY);
		}
		/* draw the GJK simplex */
		else
		{
			Vertex vertices[12];
			const u8 simplexLineVertexCount = 
				kmath::gjk_buildSimplexLines(
					&g_gs->gjkState, vertices, sizeof(vertices), 
					sizeof(vertices[0]), offsetof(Vertex, position));
			switch(g_gs->gjkState.lastIterationResult)
			{
				case kmath::GjkIterationResult::INCOMPLETE:
					g_krb->setDefaultColor(krb::WHITE);
					break;
				case kmath::GjkIterationResult::FAILURE:
					g_krb->setDefaultColor(krb::RED);
					break;
				case kmath::GjkIterationResult::SUCCESS:
					g_krb->setDefaultColor(krb::BLACK);
					break;
			}
			if(simplexLineVertexCount)
			{
				g_krb->setModelXform(
					v3f32::ZERO, kQuaternion::IDENTITY, {1,1,1});
				g_krb->drawLines(
					vertices, simplexLineVertexCount, sizeof(vertices[0]), 
					VERTEX_ATTRIBS_POSITION_ONLY);
			}
		}
		if(g_gs->gjkState.lastIterationResult != 
			kmath::GjkIterationResult::SUCCESS)
		/* draw the GJK search direction from the last added simplex vertex */
		{
			const v3f32 lastSimplexPosition = 
				g_gs->gjkState.o_simplex[g_gs->gjkState.simplexSize - 1];
			const v3f32 searchDirection = 
				kmath::normal(g_gs->gjkState.searchDirection);
			g_krb->setModelXform(v3f32::ZERO, kQuaternion::IDENTITY, {1,1,1});
			const Vertex mesh[] = 
				{ {lastSimplexPosition                  , {}, krb::YELLOW}
				, {lastSimplexPosition + searchDirection, {}, krb::YELLOW} };
			DRAW_LINES(mesh, VERTEX_ATTRIBS_NO_TEXTURE);
		}
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
