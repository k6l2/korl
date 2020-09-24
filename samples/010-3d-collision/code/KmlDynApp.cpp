#include "KmlDynApp.h"
v3f32 supportShape(const Shape& shape, const kQuaternion& orientation, 
                   const v3f32& supportDirection)
{
	switch(shape.type)
	{
		case ShapeType::BOX:
			return kmath::supportBox(
				shape.box.lengths, orientation, supportDirection);
		case ShapeType::SPHERE:
			return kmath::supportSphere(
				shape.sphere.radius, supportDirection);
	}
	KLOG(ERROR, "shape.type {%i} is invalid!", i32(shape.type));
	return {};
}
void drawBox(const v3f32& worldPosition, const kQuaternion& orientation, 
             const Shape& shape)
{
	g_krb->setModelXform(worldPosition, orientation, {1,1,1});
	Vertex generatedBox[36];
	kmath::generateMeshBox(
		shape.box.lengths, generatedBox, sizeof(generatedBox), 
		sizeof(generatedBox[0]), offsetof(Vertex,position), 
		offsetof(Vertex,textureNormal));
	const KrbVertexAttributeOffsets vertexAttribOffsets = g_gs->wireframe
		? VERTEX_ATTRIBS_VERTEX_POSITION_ONLY
		: VERTEX_ATTRIBS_VERTEX_NO_COLOR;
	DRAW_TRIS(generatedBox, vertexAttribOffsets);
}
void drawSphere(const v3f32& worldPosition, const kQuaternion& orientation, 
                const Shape& shape)
{
	const v3f32 sphereScale = 
		{ shape.sphere.radius, shape.sphere.radius, shape.sphere.radius };
	g_krb->setModelXform(worldPosition, orientation, sphereScale);
	const KrbVertexAttributeOffsets vertexAttribOffsets = g_gs->wireframe
		? VERTEX_ATTRIBS_VERTEX_POSITION_ONLY
		: VERTEX_ATTRIBS_VERTEX_NO_COLOR;
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
	KLOG(ERROR, "actor.shape.type {%i} is invalid!", i32(actor.shape.type));
	return NAN32;
}
struct ShapeGjkSupportData
{
	const Shape& shapeA;
	const v3f32& positionA;
	const kQuaternion& orientationA;
	const Shape& shapeB;
	const v3f32& positionB;
	const kQuaternion& orientationB;
};
internal GJK_SUPPORT_FUNCTION(shapeGjkSupport)
{
	const ShapeGjkSupportData& data = 
		*reinterpret_cast<ShapeGjkSupportData*>(userData);
	const v3f32 supportA = data.positionA + 
		supportShape(data.shapeA, data.orientationA,  supportDirection);
	const v3f32 supportB = data.positionB + 
		supportShape(data.shapeB, data.orientationB, -supportDirection);
	return supportA - supportB;
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
#if DEBUG_DELETE_LATER
		if(gameKeyboard.f1 == ButtonState::PRESSED)
		{
			arrsetlen(g_gs->actors, 2);
			g_gs->actors[0].shape.type = ShapeType::SPHERE;
			g_gs->actors[1].shape.type = ShapeType::SPHERE;
			g_gs->actors[0].shape.sphere.radius = 1;
			g_gs->actors[1].shape.sphere.radius = 1;
			g_gs->actors[0].position = {-0.214954376f,4.43508482f,-0.0842480659f};
			g_gs->actors[1].position = {-1.82670593f,5.56357288f,-0.432786942f};
			g_gs->actors[0].orientation = kQuaternion::IDENTITY;
			g_gs->actors[1].orientation = kQuaternion::IDENTITY;
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
			kmath::gjk_iterate(
				&g_gs->gjkState, shapeGjkSupport, &shapeGjkSupportData);
		}
#endif// DEBUG_DELETE_LATER
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
			const v3f32 modShapeBackPlanePosition = g_gs->cameraPosition + 
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
#if DEBUG_DELETE_LATER
				const kQuaternion modelQuat(
					g_gs->testRadianAxis, g_gs->testRadians);
				newActor.orientation = modelQuat;
#endif// DEBUG_DELETE_LATER
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
						actor.position - g_gs->cameraPosition;
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
						actor.position - g_gs->cameraPosition;
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
			g_gs->orthographicView = !g_gs->orthographicView;
		local_persist const f32 CAMERA_SPEED_MAX = 25;
		local_persist const f32 CAMERA_ACCELERATION = 10;
		local_persist const f32 CAMERA_DECELERATION = 50;
		v3f32 camControl = v3f32::ZERO;
		/* move the camera world position */
		if(gameKeyboard.e > ButtonState::NOT_PRESSED)
			camControl += cameraWorldForward;
		if(gameKeyboard.d > ButtonState::NOT_PRESSED)
			camControl -= cameraWorldForward;
		if(gameKeyboard.f > ButtonState::NOT_PRESSED)
			camControl += cameraWorldRight;
		if(gameKeyboard.s > ButtonState::NOT_PRESSED)
			camControl -= cameraWorldRight;
		if(gameKeyboard.space > ButtonState::NOT_PRESSED)
			camControl += cameraWorldUp;
		if(gameKeyboard.a > ButtonState::NOT_PRESSED 
			&& !gameKeyboard.modifiers.shift)
				camControl -= cameraWorldUp;
		f32 camControlMag = camControl.normalize();
		if(kmath::isNearlyZero(camControlMag))
		{
			v3f32 camDecelDir = kmath::normal(-g_gs->cameraVelocity);
			g_gs->cameraVelocity += 
				deltaSeconds*CAMERA_DECELERATION*camDecelDir;
			if(g_gs->cameraVelocity.dot(camDecelDir) > 0)
				g_gs->cameraVelocity = v3f32::ZERO;
		}
		else
			g_gs->cameraVelocity += deltaSeconds*CAMERA_ACCELERATION*camControl;
		f32 camSpeed = g_gs->cameraVelocity.normalize();
		if(camSpeed > CAMERA_SPEED_MAX)
			camSpeed = CAMERA_SPEED_MAX;
		g_gs->cameraVelocity *= camSpeed;
		g_gs->cameraPosition += deltaSeconds*g_gs->cameraVelocity;
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
	{
		i32 sliderInt = g_gs->minkowskiDifferencePointCloudCount;
		ImGui::SliderInt("pointCloudCount", &sliderInt, 1, 
		                 CARRAY_SIZE(g_gs->minkowskiDifferencePointCloud) - 1);
		g_gs->minkowskiDifferencePointCloudCount = 
			kmath::safeTruncateU16(sliderInt);
	}
	if(ImGui::Button("assert false"))
	{
		kassert(false);
	}
	if(ImGui::Button("crash program"))
	{
		*((int*)0) = 0;
	}
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
		g_krb->setProjectionFov(50.f, windowDimensions.elements, 0.001f, 100);
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
#if 1
		for(size_t a2 = 0; a2 < arrlenu(g_gs->actors); a2++)
		{
			if(a == a2)
				continue;
			Actor& actor2 = g_gs->actors[a2];
			v3f32 gjkSimplex[4];
			ShapeGjkSupportData shapeGjkSupportData = 
				{ .shapeA       = actor.shape
				, .positionA    = actor.position
				, .orientationA = actor.orientation
				, .shapeB       = actor2.shape
				, .positionB    = actor2.position
				, .orientationB = actor2.orientation };
			if(kmath::gjk(shapeGjkSupport, &shapeGjkSupportData, gjkSimplex))
				g_krb->setDefaultColor(krb::RED);
		}
#endif// 0
		drawShape(actor.position, actor.orientation, actor.shape);
#if DEBUG_DELETE_LATER && 0
		/* calculate the world-space support function of the shape */
		const v3f32 supportDirection = actor.position;
		const v3f32 supportResult = actor.position + 
			(actor.shape.type == ShapeType::BOX
			? kmath::supportBox(
				actor.shape.box.lengths, actor.orientation, supportDirection)
			: kmath::supportSphere(
				actor.shape.sphere.radius, supportDirection));
		/* draw a crosshair on the 3D position of the support direction */
		{
			g_krb->setModelXform(
				supportResult, kQuaternion::IDENTITY, {10,10,10});
			g_krb->setModelXformBillboard(true, true, true);
			local_persist const VertexNoTexture MESH[] = 
				{ {{-1,-1,0}, krb::WHITE}, {{1, 1,0}, krb::WHITE}
				, {{-1, 1,0}, krb::WHITE}, {{1,-1,0}, krb::WHITE} };
			DRAW_LINES(MESH, VERTEX_ATTRIBS_NO_TEXTURE);
		}
#endif //DEBUG_DELETE_LATER
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
	if(arrlenu(g_gs->actors) == 2)
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
				VERTEX_ATTRIBS_VERTEX_POSITION_ONLY);
		}
		/* draw the GJK simplex */
		{
			VertexNoTexture vertices[12];
			const u8 simplexLineVertexCount = 
				kmath::gjk_buildSimplexLines(
					&g_gs->gjkState, vertices, sizeof(vertices), 
					sizeof(vertices[0]), offsetof(VertexNoTexture, position));
			switch(g_gs->gjkState.lastIterationResult)
			{
				case kmath::GjkIterationResult::INCOMPLETE:
					g_krb->setDefaultColor(krb::WHITE);
					break;
				case kmath::GjkIterationResult::FAILURE:
					g_krb->setDefaultColor(krb::RED);
					break;
				case kmath::GjkIterationResult::SUCCESS:
					g_krb->setDefaultColor(krb::GREEN);
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
		/* draw the GJK search direction from the last added simplex vertex */
		{
			const v3f32 lastSimplexPosition = 
				g_gs->gjkState.o_simplex[g_gs->gjkState.simplexSize - 1];
			const v3f32 searchDirection = 
				kmath::normal(g_gs->gjkState.searchDirection);
			g_krb->setModelXform(v3f32::ZERO, kQuaternion::IDENTITY, {1,1,1});
			const VertexNoTexture mesh[] = 
				{ {lastSimplexPosition                  , krb::YELLOW}
				, {lastSimplexPosition + searchDirection, krb::YELLOW} };
			DRAW_LINES(mesh, VERTEX_ATTRIBS_NO_TEXTURE);
		}
	}
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
		kmath::generateMeshCircleSphereVertexCount(16, 16);
	const size_t generatedSphereMeshBytes = 
		g_gs->generatedSphereMeshVertexCount * sizeof(Vertex);
	g_gs->generatedSphereMesh = reinterpret_cast<Vertex*>(
		kgaAlloc(g_gs->templateGameState.hKgaPermanent,
		         generatedSphereMeshBytes));
	kmath::generateMeshCircleSphere(
		1.f, 16, 16, g_gs->generatedSphereMesh, generatedSphereMeshBytes, 
		sizeof(Vertex), offsetof(Vertex,position), 
		offsetof(Vertex,textureNormal));
	/* initialize dynamic array of actors */
	g_gs->actors = arrinit(Actor, g_gs->templateGameState.hKgaPermanent);
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "TemplateGameState.cpp"
