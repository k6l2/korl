#include "KmlDynApp.h"
void drawBox(const v3f32& worldPosition, const kQuaternion& orientation, 
             const Shape& shape)
{
	g_krb->setModelXform(worldPosition, orientation, {1,1,1});
	kmath::GeneratedMeshVertex generatedBox[36];
	kmath::generateMeshBox(
		shape.box.lengths, generatedBox, sizeof(generatedBox));
	const KrbVertexAttributeOffsets vertexAttribOffsets = g_gs->wireframe
		? VERTEX_ATTRIBS_GENERATED_MESH_NO_TEX
		: VERTEX_ATTRIBS_GENERATED_MESH;
	DRAW_TRIS(generatedBox, vertexAttribOffsets);
}
void drawSphere(const v3f32& worldPosition, const kQuaternion& orientation, 
                const Shape& shape)
{
	const v3f32 sphereScale = 
		{ shape.sphere.radius, shape.sphere.radius, shape.sphere.radius };
	g_krb->setModelXform(worldPosition, orientation, sphereScale);
	const KrbVertexAttributeOffsets vertexAttribOffsets = g_gs->wireframe
		? VERTEX_ATTRIBS_GENERATED_MESH_NO_TEX
		: VERTEX_ATTRIBS_GENERATED_MESH;
	g_krb->drawTris(
		g_gs->generatedSphereMesh, g_gs->generatedSphereMeshVertexCount, 
	    sizeof(g_gs->generatedSphereMesh[0]), vertexAttribOffsets);
}
void drawShape(const v3f32& worldPosition, const kQuaternion& orientation, 
               const Shape& shape)
{
	switch(shape.type)
	{
		case ShapeType::BOX:
			drawBox(worldPosition, orientation, shape);
			break;
		case ShapeType::SPHERE:
			drawSphere(worldPosition, orientation, shape);
			break;
	}
}
/** @return NAN32 if the ray doesn't intersect with actor */
f32 testRay(const v3f32& rayOrigin, const v3f32& rayNormal, const Actor& actor)
{
	switch(actor.shape.type)
	{
		case ShapeType::BOX:
			return kmath::collideRayBox(
				rayOrigin, rayNormal, actor.position, actor.orientation, 
				actor.shape.box.lengths);
		case ShapeType::SPHERE:
			return kmath::collideRaySphere(
				rayOrigin, rayNormal, actor.position, 
				actor.shape.sphere.radius);
	}
	kassert(!"This code should never execute!");
	return NAN32;
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
		return false;
	bool lockedMouse = false;
	const v3f32 cameraWorldForward = 
		(kQuaternion(v3f32::Z*-1, g_gs->cameraRadiansYaw) * 
		 kQuaternion(v3f32::Y*-1, g_gs->cameraRadiansPitch))
		    .transform(v3f32::X);
	const v3f32 cameraWorldRight = 
		(kQuaternion(v3f32::Z*-1, g_gs->cameraRadiansYaw) * 
		 kQuaternion(v3f32::Y*-1, g_gs->cameraRadiansPitch))
		    .transform(v3f32::Y*-1);
	const v3f32 cameraWorldUp = 
		(kQuaternion(v3f32::Z*-1, g_gs->cameraRadiansYaw) * 
		 kQuaternion(v3f32::Y*-1, g_gs->cameraRadiansPitch))
		    .transform(v3f32::Z);
#if DEBUG_DELETE_LATER
	g_gs->eyeRayActorHitPosition = {NAN32, NAN32, NAN32};
#endif// DEBUG_DELETE_LATER
	/* handle user input */
	if(windowIsFocused)
	{
		const bool mouseOverAnyGui = ImGui::IsAnyItemHovered() 
			|| ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
		v3f32 worldEyeRayPosition;
		v3f32 worldEyeRayDirection;
		const v2i32 mouseWindowPosition = mouseOverAnyGui
			? windowDimensions / 2
			: gameMouse.windowPosition;
		if(!g_krb->screenToWorld(
				mouseWindowPosition.elements, windowDimensions.elements, 
				worldEyeRayPosition.elements, worldEyeRayDirection.elements))
		{
			KLOG(ERROR, "Failed to get mouse world eye ray!");
		}
		if(g_gs->hudState != HudState::NAVIGATING)
			if(gameKeyboard.grave > ButtonState::NOT_PRESSED)
			{
				if(g_gs->hudState == HudState::MODIFY_SHAPE_GRAB)
				{
					Actor& actor = g_gs->actors[g_gs->selectedActorId - 1];
					actor.position.x = g_gs->modifyShapeTempValues[0];
					actor.position.y = g_gs->modifyShapeTempValues[1];
					actor.position.z = g_gs->modifyShapeTempValues[2];
				}
				g_gs->hudState = HudState::NAVIGATING;
			}
		if(g_gs->hudState == HudState::MODIFY_SHAPE_GRAB)
		{
			const v3f32 grabBackPlanePosition = g_gs->cameraPosition + 
				cameraWorldForward * 
				g_gs->modifyShapeGrabPlaneDistanceFromCamera;
			const f32 grabBackPlaneDistance = 
				grabBackPlanePosition.dot(cameraWorldForward);
			const f32 collide_eyeRay_grabBackPlane = 
				kmath::collideRayPlane(
					worldEyeRayPosition, worldEyeRayDirection, 
					cameraWorldForward, grabBackPlaneDistance, false);
			if(!isnan(collide_eyeRay_grabBackPlane) 
				&& !isinf(collide_eyeRay_grabBackPlane))
			{
				Actor& actor = g_gs->actors[g_gs->selectedActorId - 1];
				actor.position = worldEyeRayPosition + 
					collide_eyeRay_grabBackPlane*worldEyeRayDirection;
			}
			if(gameMouse.left == ButtonState::PRESSED)
				g_gs->hudState = HudState::NAVIGATING;
		}
		else if(    g_gs->hudState == HudState::ADDING_BOX 
		         || g_gs->hudState == HudState::ADDING_SPHERE)
		{
			g_gs->addShapePosition = worldEyeRayPosition + 
				18*worldEyeRayDirection;
			if(!mouseOverAnyGui && gameMouse.left == ButtonState::PRESSED)
			{
				Actor newActor;
				newActor.position = g_gs->addShapePosition;
				newActor.shape    = g_gs->addShape;
#if DEBUG_DELETE_LATER
				const kQuaternion modelQuat(
					g_gs->testRadianAxis, g_gs->testRadians);
				newActor.orientation = modelQuat;
#endif// DEBUG_DELETE_LATER
				arrpush(g_gs->actors, newActor);
			}
		}
		lockedMouse = gameMouse.right > ButtonState::NOT_PRESSED;
		g_kpl->mouseSetRelativeMode(lockedMouse);
		if(gameMouse.middle == ButtonState::PRESSED)
			g_gs->orthographicView = !g_gs->orthographicView;
		local_persist const f32 CAMERA_SPEED = 25;
		/* move the camera world position */
		if(gameKeyboard.e > ButtonState::NOT_PRESSED)
			g_gs->cameraPosition += 
				deltaSeconds * CAMERA_SPEED * cameraWorldForward;
		if(gameKeyboard.d > ButtonState::NOT_PRESSED)
			g_gs->cameraPosition -= 
				deltaSeconds * CAMERA_SPEED * cameraWorldForward;
		if(gameKeyboard.f > ButtonState::NOT_PRESSED)
			g_gs->cameraPosition += 
				deltaSeconds * CAMERA_SPEED * cameraWorldRight;
		if(gameKeyboard.s > ButtonState::NOT_PRESSED)
			g_gs->cameraPosition -= 
				deltaSeconds * CAMERA_SPEED * cameraWorldRight;
		if(gameKeyboard.space > ButtonState::NOT_PRESSED)
			g_gs->cameraPosition += 
				deltaSeconds * CAMERA_SPEED * cameraWorldUp;
		if(gameKeyboard.a > ButtonState::NOT_PRESSED 
			&& !gameKeyboard.modifiers.shift)
				g_gs->cameraPosition -= 
					deltaSeconds * CAMERA_SPEED * cameraWorldUp;
		if(lockedMouse)
		{
			local_persist const f32 CAMERA_LOOK_SENSITIVITY = 0.0025f;
			local_persist const f32 MAX_PITCH_MAGNITUDE = PI32/2 - 0.001f;
			/* move the camera yaw & pitch based on relative mouse inputs */
			g_gs->cameraRadiansYaw += 
				CAMERA_LOOK_SENSITIVITY*gameMouse.deltaPosition.x;
			g_gs->cameraRadiansYaw  = fmodf(g_gs->cameraRadiansYaw, 2*PI32);
			g_gs->cameraRadiansPitch -= 
				CAMERA_LOOK_SENSITIVITY*gameMouse.deltaPosition.y;
			if(g_gs->cameraRadiansPitch < -MAX_PITCH_MAGNITUDE)
				g_gs->cameraRadiansPitch = -MAX_PITCH_MAGNITUDE;
			if(g_gs->cameraRadiansPitch > MAX_PITCH_MAGNITUDE)
				g_gs->cameraRadiansPitch = MAX_PITCH_MAGNITUDE;
		}
		if(gameKeyboard.modifiers.shift 
			&& gameKeyboard.a > ButtonState::NOT_PRESSED)
		{
			if(g_gs->hudState == HudState::NAVIGATING)
				g_gs->hudState = HudState::ADDING_SHAPE;
		}
		if(gameKeyboard.w == ButtonState::PRESSED)
			g_gs->wireframe = !g_gs->wireframe;
		if(g_gs->hudState == HudState::NAVIGATING)
		{
			f32 nearestEyeRayHit = INFINITY32;
			u32 nearestActorId = 0;
			for(size_t a = 0; a < arrlenu(g_gs->actors); a++)
			{
				Actor& actor = g_gs->actors[a];
				f32 eyeRayHit = 
					testRay(worldEyeRayPosition, worldEyeRayDirection, actor);
				if(isnan(eyeRayHit))
					continue;
				if(eyeRayHit < nearestEyeRayHit)
				{
					nearestEyeRayHit = eyeRayHit;
					nearestActorId = kmath::safeTruncateU32(a + 1);
				}
			}
			if(gameMouse.left == ButtonState::PRESSED)
				g_gs->selectedActorId = 0;
			if(nearestEyeRayHit < INFINITY32)
			{
				if(gameMouse.left == ButtonState::PRESSED)
					g_gs->selectedActorId = nearestActorId;
#if DEBUG_DELETE_LATER
				g_gs->eyeRayActorHitPosition = worldEyeRayPosition + 
					nearestEyeRayHit*worldEyeRayDirection;
#endif// DEBUG_DELETE_LATER
			}
			if(g_gs->selectedActorId)
			{
				if(gameKeyboard.x > ButtonState::NOT_PRESSED)
				{
					arrdel(g_gs->actors, g_gs->selectedActorId - 1);
					g_gs->selectedActorId = 0;
				}
				else if(gameKeyboard.g == ButtonState::PRESSED)
				{
					g_gs->hudState = HudState::MODIFY_SHAPE_GRAB;
					Actor& actor = g_gs->actors[g_gs->selectedActorId - 1];
					g_gs->modifyShapeTempValues[0] = actor.position.x;
					g_gs->modifyShapeTempValues[1] = actor.position.y;
					g_gs->modifyShapeTempValues[2] = actor.position.z;
					const v3f32 eyeToActor = 
						actor.position - worldEyeRayPosition;
					g_gs->modifyShapeGrabPlaneDistanceFromCamera = 
						eyeToActor.projectOnto(cameraWorldForward).magnitude();
#if DEBUG_DELETE_LATER
					KLOG(INFO, "%f", g_gs->modifyShapeGrabPlaneDistanceFromCamera);
					g_gs->modifyShapeWindowPositionStart = 
						gameMouse.windowPosition;
						//(actor.position - worldEyeRayPosition).magnitude();
#endif// DEBUG_DELETE_LATER
				}
			}
		}
	}
	/* display HUD GUI for sample controls */
	if(g_gs->hudState != HudState::NAVIGATING)
	{
		if(ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if(g_gs->hudState == HudState::ADDING_BOX 
				|| g_gs->hudState == HudState::ADDING_SPHERE)
				ImGui::Text("[mouse left] - add shape to the scene");
			else if(g_gs->hudState == HudState::MODIFY_SHAPE_GRAB)
				ImGui::Text("[mouse left] - confirm action");
			ImGui::Text("[GRAVE     ] - cancel action");
		}
		ImGui::End();
	}
	if(g_gs->hudState == HudState::NAVIGATING)
	{
		if(ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("[mouse left  ] - select shape");
			ImGui::Text("[mouse middle] - toggle orthographic view");
			ImGui::Text("[mouse right ] - hold to control camera yaw/pitch");
			if(lockedMouse)
			{
				ImGui::Text("[mouse move  ] - camera yaw/pitch");
			}
			else
			{
				ImGui::Text("[mouse move  ] - *** DISABLED ***");
			}
			ImGui::Text("[E | D       ] - move camera forward | back");
			ImGui::Text("[S | F       ] - move camera left | right");
			ImGui::Text("[SPACE | A   ] - move camera up | down");
			ImGui::Text("[SHIFT + A   ] - add shape");
			ImGui::Text("[W           ] - toggle wireframe");
			if(g_gs->selectedActorId)
			{
				ImGui::Text("[X           ] - delete shape");
				ImGui::Text("[G           ] - grab shape");
			}
			else
			{
				ImGui::Text("[X           ] - *** DISABLED ***");
				ImGui::Text("[G           ] - *** DISABLED ***");
			}
		}
		ImGui::End();
	}
	else if(g_gs->hudState == HudState::ADDING_SHAPE)
	{
		if(ImGui::Begin("Add", nullptr, 
		                ImGuiWindowFlags_AlwaysAutoResize))
		{
			if(ImGui::Button("Box"))
			{
				g_gs->hudState = HudState::ADDING_BOX;
				g_gs->addShape.type = ShapeType::BOX;
				g_gs->addShape.box.lengths = {1,1,1};
			}
			if(ImGui::Button("Sphere"))
			{
				g_gs->hudState = HudState::ADDING_SPHERE;
				g_gs->addShape.type = ShapeType::SPHERE;
				g_gs->addShape.sphere.radius = 1.f;
			}
		}
		ImGui::End();
	}
	else if(g_gs->hudState == HudState::ADDING_BOX)
	{
		if(ImGui::Begin("Add Box", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SliderFloat3("lengths", g_gs->addShape.box.lengths.elements, 
			                    0.1f, 10.f);
		}
		ImGui::End();
	}
	else if(g_gs->hudState == HudState::ADDING_SPHERE)
	{
		if(ImGui::Begin("Add Sphere", nullptr, 
		                ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SliderFloat("radius", &g_gs->addShape.sphere.radius, 
			                   0.1f, 10.f);
		}
		ImGui::End();
	}
#if DEBUG_DELETE_LATER
	ImGui::SliderFloat3("testPosition", g_gs->testPosition.elements, -20, 20);
	ImGui::SliderFloat3("testRadianAxis", g_gs->testRadianAxis.elements, -1, 1);
	ImGui::SliderFloat("testRadians", &g_gs->testRadians, 0, 4*PI32);
#endif// DEBUG_DELETE_LATER
	/* render the scene */
	g_krb->beginFrame(0.2f, 0, 0.2f);
	g_krb->setDepthTesting(true);
	g_krb->setBackfaceCulling(!g_gs->wireframe);
	g_krb->setWireframe(g_gs->wireframe);
	if(g_gs->orthographicView)
		g_krb->setProjectionOrthoFixedHeight(
			windowDimensions.x, windowDimensions.y, 100, 1000);
	else
		g_krb->setProjectionFov(50.f, windowDimensions.elements, 1, 1000);
	g_krb->lookAt(g_gs->cameraPosition.elements, 
	              (g_gs->cameraPosition + cameraWorldForward).elements, 
	              v3f32::Z.elements);
	/* draw all the shapes in the scene */
	for(size_t a = 0; a < arrlenu(g_gs->actors); a++)
	{
		Actor& actor = g_gs->actors[a];
		if(g_gs->selectedActorId && g_gs->selectedActorId - 1 == a)
			g_krb->setDefaultColor(krb::YELLOW);
		else
			g_krb->setDefaultColor(krb::WHITE);
		drawShape(actor.position, actor.orientation, actor.shape);
	}
	/* draw the shape that the user is attempting to add to the scene */
	if(    g_gs->hudState == HudState::ADDING_BOX
	    || g_gs->hudState == HudState::ADDING_SPHERE)
	{
#if DEBUG_DELETE_LATER
		const kQuaternion modelQuat(g_gs->testRadianAxis, g_gs->testRadians);
		drawShape(g_gs->addShapePosition, modelQuat, 
#else
		drawShape(g_gs->addShapePosition, kQuaternion::IDENTITY, 
#endif //DEBUG_DELETE_LATER
		          g_gs->addShape);
	}
#if DEBUG_DELETE_LATER
	if(!isnan(g_gs->eyeRayActorHitPosition.x))
	/* draw a crosshair on the 3D position where we hit an actor */
	{
		g_krb->setModelXform(g_gs->eyeRayActorHitPosition, 
		                     kQuaternion::IDENTITY, {10,10,10});
		g_krb->setModelXformBillboard(true, true, true);
		local_persist const VertexNoTexture MESH[] = 
			{ {{-1,-1,0}, krb::WHITE}, {{1, 1,0}, krb::WHITE}
			, {{-1, 1,0}, krb::WHITE}, {{1,-1,0}, krb::WHITE} };
		DRAW_LINES(MESH, VERTEX_ATTRIBS_NO_TEXTURE);
	}
	{
		const kQuaternion modelQuat(g_gs->testRadianAxis, g_gs->testRadians);
		m4x4f32 modelMatrix;
		kmath::makeM4f32(modelQuat, g_gs->testPosition, &modelMatrix);
		g_krb->setModelMatrix(modelMatrix.elements);
		local_persist const VertexNoTexture MESH[] = 
			{ {{0,0,0}, krb::RED  }, {{1,0,0}, krb::RED  }
			, {{0,0,0}, krb::GREEN}, {{0,1,0}, krb::GREEN}
			, {{0,0,0}, krb::BLUE }, {{0,0,1}, krb::BLUE } };
		DRAW_LINES(MESH, VERTEX_ATTRIBS_NO_TEXTURE);
	}
#endif// DEBUG_DELETE_LATER
	/* draw origin */
	{
		g_krb->setModelXform(v3f32::ZERO, kQuaternion::IDENTITY, {10,10,10});
		local_persist const VertexNoTexture MESH[] = 
			{ {{0,0,0}, krb::RED  }, {{1,0,0}, krb::RED  }
			, {{0,0,0}, krb::GREEN}, {{0,1,0}, krb::GREEN}
			, {{0,0,0}, krb::BLUE }, {{0,0,1}, krb::BLUE } };
		DRAW_LINES(MESH, VERTEX_ATTRIBS_NO_TEXTURE);
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
	/* generate a single sphere mesh with reasonable resolution that we can 
		reuse with different scales */
	g_gs->generatedSphereMeshVertexCount = 
		kmath::generateMeshCircleSphereVertexCount(6, 6);
	g_gs->generatedSphereMesh = reinterpret_cast<kmath::GeneratedMeshVertex*>(
		kgaAlloc(g_gs->templateGameState.hKgaPermanent,
		         sizeof(kmath::GeneratedMeshVertex) * 
		              g_gs->generatedSphereMeshVertexCount));
	kmath::generateMeshCircleSphere(
		1.f, 6, 6, g_gs->generatedSphereMesh, 
		g_gs->generatedSphereMeshVertexCount * 
			sizeof(kmath::GeneratedMeshVertex));
	/* initialize dynamic array of actors */
	g_gs->actors = arrinit(Actor, g_gs->templateGameState.hKgaPermanent);
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "TemplateGameState.cpp"