#include "KmlDynApp.h"
#include "kgtDraw.h"
/** @return NAN32 if the ray doesn't intersect with actor */
f32 testRay(const v3f32& rayOrigin, const v3f32& rayNormal, const Actor& actor)
{
	return kgtShapeTestRay(actor.shape, actor.position, actor.orientation, 
	                       rayOrigin, rayNormal);
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(&g_gs->kgtGameState, gameKeyboard, 
	                              windowIsFocused))
		return false;
	bool lockedMouse = false;
	const v3f32 cameraWorldForward = kgtCam3dWorldForward(&g_gs->camera);
	const v3f32 cameraWorldRight   = kgtCam3dWorldRight(&g_gs->camera);
	const v3f32 cameraWorldUp      = kgtCam3dWorldUp(&g_gs->camera);
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
				else if(g_gs->hudState == HudState::MODIFY_SHAPE_ROTATE)
				{
					Actor& actor = g_gs->actors[g_gs->selectedActorId - 1];
					for(u8 e = 0; 
							e < CARRAY_SIZE(g_gs->modifyShapeTempValues); e++)
						actor.orientation.elements[e] = 
							g_gs->modifyShapeTempValues[e];
				}
				g_gs->hudState = HudState::NAVIGATING;
			}
		if(g_gs->hudState == HudState::MODIFY_SHAPE_GRAB
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
					windowDimensions.elements, 
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
					const kQuaternion deltaQuat(deltaAxis, deltaRadians);
					/* new actor orientation = deltaQuat * old orientation */
					const kQuaternion*const actorOrientationCached = 
						reinterpret_cast<kQuaternion*>(
							g_gs->modifyShapeTempValues);
					actor.orientation = deltaQuat * (*actorOrientationCached);
				}
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
					g_gs->modifyShapeTempValues[0] = actor.orientation.qw;
					g_gs->modifyShapeTempValues[1] = actor.orientation.qx;
					g_gs->modifyShapeTempValues[2] = actor.orientation.qy;
					g_gs->modifyShapeTempValues[3] = actor.orientation.qz;
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
		kgtCam3dStep(&g_gs->camera, 
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
			kgtCam3dLook(&g_gs->camera, gameMouse.deltaPosition);
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
				{ .shapeA       = actor.shape
				, .positionA    = actor.position
				, .orientationA = actor.orientation
				, .shapeB       = actor2.shape
				, .positionB    = actor2.position
				, .orientationB = actor2.orientation };
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
	g_krb->beginFrame(0.2f, 0, 0.2f);
	g_krb->setDepthTesting(true);
	g_krb->setBackfaceCulling(!g_gs->wireframe);
	g_krb->setWireframe(g_gs->wireframe);
	kgtCam3dApplyViewProjection(&g_gs->camera, g_krb, windowDimensions);
	/* use the default texture asset */
	USE_IMAGE(g_gs->kgtGameState.assetManager, KAssetIndex::ENUM_SIZE);
	/* draw all the shapes in the scene */
	for(size_t a = 0; a < arrlenu(g_gs->actors); a++)
	{
		Actor& actor = g_gs->actors[a];
		if(g_gs->selectedActorId && g_gs->selectedActorId - 1 == a)
			g_krb->setDefaultColor(krb::YELLOW);
		else
			g_krb->setDefaultColor(krb::WHITE);
		kgtShapeDraw(actor.position, actor.orientation, actor.shape, 
		             g_gs->wireframe, g_krb, g_gs->kgtGameState.hKalFrame);
	}
	/* draw the shape that the user is attempting to add to the scene */
	if(    g_gs->hudState == HudState::ADDING_BOX
	    || g_gs->hudState == HudState::ADDING_SPHERE)
	{
		kgtShapeDraw(g_gs->addShapePosition, kQuaternion::IDENTITY, 
		             g_gs->addShape, g_gs->wireframe, g_krb, 
		             g_gs->kgtGameState.hKalFrame);
	}
	kgtDrawOrigin({10,10,10});
	return true;
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kgtGameStateRenderAudio(&g_gs->kgtGameState, audioBuffer, 
	                        sampleBlocksConsumed);
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
	g_gs->camera.position     = {10,11,12};
	g_gs->camera.radiansYaw   = PI32*3/4;
	g_gs->camera.radiansPitch = -PI32/4;
	/* initialize dynamic array of actors */
	g_gs->actors = arrinit(Actor, g_gs->kgtGameState.hKalPermanent);
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "kgtGameState.cpp"
#include "kgtCamera3d.cpp"
#include "kgtShape.cpp"
#include "kgtDraw.cpp"
