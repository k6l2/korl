#include "korlDynApp.h"
/** @return NAN32 if the ray doesn't intersect with actor */
f32 testRay(const v3f32& rayOrigin, const v3f32& rayNormal, const Actor& actor)
{
	return kgtShapeTestRay(
		actor.shape, actor.position, actor.orient, rayOrigin, rayNormal);
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
		return false;
	bool lockedMouse = false;
	const v3f32 cameraWorldForward = kgtCamera3dWorldForward(&g_gs->camera);
	const v3f32 cameraWorldRight   = kgtCamera3dWorldRight(&g_gs->camera);
	const v3f32 cameraWorldUp      = kgtCamera3dWorldUp(&g_gs->camera);
	/* handle user input */
	if(windowIsFocused)
	{
		const bool mouseOverAnyGui = ImGui::IsAnyItemHovered() 
			|| ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
		v3f32 worldEyeRayPosition;
		v3f32 worldEyeRayDirection;
		const v2i32 mouseWindowPosition = mouseOverAnyGui
			? v2i32
				{ kmath::safeTruncateI32(windowDimensions.x) / 2
				, kmath::safeTruncateI32(windowDimensions.y) / 2}
			: gameMouse.windowPosition;
		if(!g_krb->screenToWorld(
				mouseWindowPosition.elements, 
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
				else if(g_gs->hudState == HudState::MODIFY_SHAPE_ROTATE)
				{
					Actor& actor = g_gs->actors[g_gs->selectedActorId - 1];
					for(u8 e = 0; 
							e < CARRAY_SIZE(g_gs->modifyShapeTempValues); e++)
						actor.orient.elements[e] = 
							g_gs->modifyShapeTempValues[e];
				}
				g_gs->hudState = HudState::NAVIGATING;
			}
		if(    g_gs->hudState == HudState::MODIFY_SHAPE_GRAB
			|| g_gs->hudState == HudState::MODIFY_SHAPE_ROTATE)
		{
			/* calculate the plane which we will use to modify the shape */
			const v3f32 modShapeBackPlanePosition = g_gs->camera.position + 
				cameraWorldForward * g_gs->modifyShapePlaneDistanceFromCamera;
			const f32 modShapeBackPlaneDistance = 
				modShapeBackPlanePosition.dot(cameraWorldForward);
			/* calculate the eye ray for the start window position */
			v3f32 worldEyeRayPositionStart;
			v3f32 worldEyeRayDirectionStart;
			if(!g_krb->screenToWorld(
					g_gs->modifyShapeWindowPositionStart.elements, 
					worldEyeRayPositionStart.elements, 
					worldEyeRayDirectionStart.elements))
			{
				KLOG(ERROR, "Failed to get mouse world eye ray start!");
			}
			/* calculate raycast onto the mod shape backplane for the start 
				window position */
			const f32 collide_eyeRay_modShapeBackPlaneStart = 
				kmath::collideRayPlane(
					worldEyeRayPositionStart, worldEyeRayDirectionStart, 
					cameraWorldForward, modShapeBackPlaneDistance, false);
			const v3f32 modShapePlaneEyeRayStart = worldEyeRayPositionStart + 
				collide_eyeRay_modShapeBackPlaneStart*worldEyeRayDirectionStart;
			/* calculate raycast onto the mod shape backplane for the current 
				window position */
			const f32 collide_eyeRay_modShapeBackPlane = 
				kmath::collideRayPlane(
					worldEyeRayPosition, worldEyeRayDirection, 
					cameraWorldForward, modShapeBackPlaneDistance, false);
			const v3f32 modShapePlaneEyeRay = worldEyeRayPosition + 
				collide_eyeRay_modShapeBackPlane*worldEyeRayDirection;
			if(!isnan(collide_eyeRay_modShapeBackPlane) 
				&& !isinf(collide_eyeRay_modShapeBackPlane))
			{
				Actor& actor = g_gs->actors[g_gs->selectedActorId - 1];
				/* new actor position = old actor position + 
					(raycastCurrent - raycastStart) */
				if(g_gs->hudState == HudState::MODIFY_SHAPE_GRAB)
				{
					const v3f32*const actorPositionCached = 
						reinterpret_cast<v3f32*>(g_gs->modifyShapeTempValues);
					actor.position = *actorPositionCached + 
						(modShapePlaneEyeRay - modShapePlaneEyeRayStart);
				}
				else if(g_gs->hudState == HudState::MODIFY_SHAPE_ROTATE)
				{
					/* calculate the deltaQuat */
					const v3f32 shapeModPlaneDeltaStart = 
						modShapePlaneEyeRayStart - actor.position;
					const v3f32 shapeModPlaneDeltaCurrent = 
						modShapePlaneEyeRay - actor.position;
					const v3f32 deltaAxis = shapeModPlaneDeltaStart
						.cross(shapeModPlaneDeltaCurrent);
					const f32 deltaRadians = kmath::radiansBetween(
						shapeModPlaneDeltaStart, shapeModPlaneDeltaCurrent);
					const q32 deltaQuat(deltaAxis, deltaRadians);
					/* new actor orientation = deltaQuat * old orientation */
					const q32*const actorOrientationCached = 
						reinterpret_cast<q32*>(g_gs->modifyShapeTempValues);
					actor.orient = deltaQuat * (*actorOrientationCached);
				}
			}
			if(gameMouse.left == ButtonState::PRESSED)
				g_gs->hudState = HudState::NAVIGATING;
		}
		else if(g_gs->hudState == HudState::ADDING_BOX 
		    ||  g_gs->hudState == HudState::ADDING_SPHERE)
		{
			g_gs->addShapePosition = worldEyeRayPosition + 
				18*worldEyeRayDirection;
			if(!mouseOverAnyGui && gameMouse.left == ButtonState::PRESSED)
			{
				Actor newActor;
				newActor.position = g_gs->addShapePosition;
				newActor.shape    = g_gs->addShape;
				arrpush(g_gs->actors, newActor);
			}
		}
		else if(g_gs->hudState == HudState::NAVIGATING)
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
						actor.position - g_gs->camera.position;
					g_gs->modifyShapePlaneDistanceFromCamera = 
						eyeToActor.projectOnto(cameraWorldForward).magnitude();
					g_gs->modifyShapeWindowPositionStart = 
						gameMouse.windowPosition;
				}
				else if(gameKeyboard.r == ButtonState::PRESSED)
				{
					g_gs->hudState = HudState::MODIFY_SHAPE_ROTATE;
					Actor& actor = g_gs->actors[g_gs->selectedActorId - 1];
					g_gs->modifyShapeTempValues[0] = actor.orient.qw;
					g_gs->modifyShapeTempValues[1] = actor.orient.qx;
					g_gs->modifyShapeTempValues[2] = actor.orient.qy;
					g_gs->modifyShapeTempValues[3] = actor.orient.qz;
					const v3f32 eyeToActor = 
						actor.position - g_gs->camera.position;
					g_gs->modifyShapePlaneDistanceFromCamera = 
						eyeToActor.projectOnto(cameraWorldForward).magnitude();
					g_gs->modifyShapeWindowPositionStart = 
						gameMouse.windowPosition;
				}
			}
		}
		lockedMouse = gameMouse.right > ButtonState::NOT_PRESSED;
		g_kpl->mouseSetRelativeMode(lockedMouse);
		if(gameMouse.middle == ButtonState::PRESSED)
			g_gs->camera.orthographicView = !g_gs->camera.orthographicView;
		kgtCamera3dStep(&g_gs->camera, 
			gameKeyboard.e > ButtonState::NOT_PRESSED, 
			gameKeyboard.d > ButtonState::NOT_PRESSED, 
			gameKeyboard.f > ButtonState::NOT_PRESSED, 
			gameKeyboard.s > ButtonState::NOT_PRESSED, 
			gameKeyboard.space > ButtonState::NOT_PRESSED, 
			gameKeyboard.a > ButtonState::NOT_PRESSED 
				&& !gameKeyboard.modifiers.shift,
			deltaSeconds);
		if(lockedMouse)
		{
			kgtCamera3dLook(&g_gs->camera, gameMouse.deltaPosition);
		}
		if(gameKeyboard.modifiers.shift 
			&& gameKeyboard.a > ButtonState::NOT_PRESSED)
		{
			if(g_gs->hudState == HudState::NAVIGATING)
				g_gs->hudState = HudState::ADDING_SHAPE;
		}
		if(gameKeyboard.w == ButtonState::PRESSED)
			g_gs->wireframe = !g_gs->wireframe;
	}
	/* display HUD GUI for sample controls */
	if(g_gs->hudState != HudState::NAVIGATING)
	{
		if(ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if(g_gs->hudState == HudState::ADDING_BOX 
					|| g_gs->hudState == HudState::ADDING_SPHERE)
				ImGui::Text("[mouse left] - add shape to the scene");
			else if(g_gs->hudState == HudState::MODIFY_SHAPE_GRAB
					|| g_gs->hudState == HudState::MODIFY_SHAPE_ROTATE)
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
				ImGui::Text("[R           ] - rotate shape");
			}
			else
			{
				ImGui::Text("[X           ] - *** DISABLED ***");
				ImGui::Text("[G           ] - *** DISABLED ***");
				ImGui::Text("[R           ] - *** DISABLED ***");
			}
			ImGui::Separator();
			ImGui::Checkbox("resolve shape collisions", 
			                &g_gs->resolveShapeCollisions);
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
				g_gs->addShape.type = KgtShapeType::BOX;
				g_gs->addShape.box.lengths = {1,1,1};
			}
			if(ImGui::Button("Sphere"))
			{
				g_gs->hudState = HudState::ADDING_SPHERE;
				g_gs->addShape.type = KgtShapeType::SPHERE;
				g_gs->addShape.sphere.radius = 1.f;
			}
		}
		ImGui::End();
	}
	else if(g_gs->hudState == HudState::ADDING_BOX)
	{
		if(ImGui::Begin("Add Box", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SliderFloat3(
				"lengths", g_gs->addShape.box.lengths.elements, 0.1f, 10.f);
		}
		ImGui::End();
	}
	else if(g_gs->hudState == HudState::ADDING_SPHERE)
	{
		if(ImGui::Begin(
			"Add Sphere", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SliderFloat(
				"radius", &g_gs->addShape.sphere.radius, 0.1f, 10.f);
		}
		ImGui::End();
	}
	/* perform collision detection + resolution on all shapes in the scene */
	for(size_t a = 0; a < arrlenu(g_gs->actors); a++)
	{
		if(!g_gs->resolveShapeCollisions)
			continue;
		Actor& actor = g_gs->actors[a];
		for(size_t a2 = 0; a2 < arrlenu(g_gs->actors); a2++)
		{
			if(a == a2)
				continue;
			Actor& actor2 = g_gs->actors[a2];
			v3f32 gjkSimplex[4];
			KgtShapeGjkSupportData shapeGjkSupportData = 
				{ .shapeA    = actor.shape
				, .positionA = actor.position
				, .orientA   = actor.orient
				, .shapeB    = actor2.shape
				, .positionB = actor2.position
				, .orientB   = actor2.orient };
			const v3f32 initialSupportDirection = 
				-(actor.position - actor2.position);
			v3f32 minTranslationVec;
			f32   minTranslationDist;
			if(   kmath::gjk(kgtShapeGjkSupport, &shapeGjkSupportData, 
			                 gjkSimplex, &initialSupportDirection)
			   && kmath::epa(&minTranslationVec, &minTranslationDist, 
			                 kgtShapeGjkSupport, &shapeGjkSupportData, 
			                 gjkSimplex, g_gs->kgtGameState.hKalFrame))
			{
				actor .position += 0.5f*minTranslationDist*minTranslationVec;
				actor2.position -= 0.5f*minTranslationDist*minTranslationVec;
			}
		}
	}
	/* render the scene */
	g_krb->beginFrame(v3f32{0.2f, 0, 0.2f}.elements, windowDimensions.elements);
	defer(g_krb->endFrame());
	g_krb->setDepthTesting(true);
	g_krb->setBackfaceCulling(!g_gs->wireframe);
	g_krb->setWireframe(g_gs->wireframe);
	kgtCamera3dApplyViewProjection(&g_gs->camera);
	/* use the default texture asset */
	KGT_USE_IMAGE(KgtAssetIndex::ENUM_SIZE);
	/* draw all the shapes in the scene */
	for(size_t a = 0; a < arrlenu(g_gs->actors); a++)
	{
		Actor& actor = g_gs->actors[a];
		if(g_gs->selectedActorId && g_gs->selectedActorId - 1 == a)
			g_krb->setDefaultColor(krb::YELLOW);
		else
			g_krb->setDefaultColor(krb::WHITE);
		kgtShapeDraw(
			actor.shape, actor.position, actor.orient, g_gs->wireframe, 
			g_gs->kgtGameState.hKalFrame);
	}
	/* draw the shape that the user is attempting to add to the scene */
	if(    g_gs->hudState == HudState::ADDING_BOX
	    || g_gs->hudState == HudState::ADDING_SPHERE)
	{
		kgtShapeDraw(
			g_gs->addShape, g_gs->addShapePosition, q32::IDENTITY, 
			g_gs->wireframe, g_gs->kgtGameState.hKalFrame);
	}
	kgtDrawAxes({10,10,10});
	kgtDrawOrigin(windowDimensions, cameraWorldForward, g_gs->camera.position);
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
	g_gs->camera.position = {10,11,12};
	kgtCamera3dLookAt(&g_gs->camera, v3f32::ZERO);
	/* initialize dynamic array of actors */
	g_gs->actors = arrinit(Actor, g_gs->kgtGameState.hKalPermanent);
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "kgtGameState.cpp"
#include "kgtCamera3d.cpp"
#include "kgtShape.cpp"
