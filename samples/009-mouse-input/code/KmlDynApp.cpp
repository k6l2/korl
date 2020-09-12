#include "KmlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
		return false;
	v3f32 mouseEyeRayPosition  = {};
	v3f32 mouseEyeRayDirection = {};
	f32 resultEyeRayCollidePlaneXY = NAN32;
	v3f32 eyeRayCollidePlaneXY = {};
	bool lockedMouse = false;
	const v3f32 cameraWorldForward = 
		(kQuaternion(v3f32::Z*-1, g_gs->cameraRadiansYaw) * 
		 kQuaternion(v3f32::Y*-1, g_gs->cameraRadiansPitch))
		    .transform(v3f32::X);
	if(windowIsFocused)
	{
		if(gameMouse.windowPosition.x >= 0 
			&& gameMouse.windowPosition.x < 
				static_cast<i64>(windowDimensions.x)
			&& gameMouse.windowPosition.y >= 0 
			&& gameMouse.windowPosition.y < 
				static_cast<i64>(windowDimensions.y))
		{
			if(!g_krb->screenToWorld(
					gameMouse.windowPosition.elements, 
					windowDimensions.elements, 
					mouseEyeRayPosition.elements, 
					mouseEyeRayDirection.elements))
			{
				KLOG(ERROR, "Failed to get mouse eye ray!");
			}
			resultEyeRayCollidePlaneXY = 
				kmath::collideRayPlane(
					mouseEyeRayPosition, mouseEyeRayDirection, 
					v3f32::Z, 0, false);
			if(!isnan(resultEyeRayCollidePlaneXY))
			{
				eyeRayCollidePlaneXY = mouseEyeRayPosition + 
					resultEyeRayCollidePlaneXY*mouseEyeRayDirection;
			}
		}
		lockedMouse = gameMouse.right > ButtonState::NOT_PRESSED;
		if(gameMouse.middle == ButtonState::PRESSED)
			g_gs->orthographicView = !g_gs->orthographicView;
		if(gameMouse.left == ButtonState::PRESSED && !lockedMouse)
		{
			if(!isnan(resultEyeRayCollidePlaneXY))
			{
				g_gs->clickLocation = eyeRayCollidePlaneXY;
				g_gs->lastEyeRay[0] = mouseEyeRayPosition;
				g_gs->lastEyeRay[1] = mouseEyeRayPosition + 
					1000*mouseEyeRayDirection;
			}
		}
		local_persist const f32 CAMERA_SPEED = 25;
		if(lockedMouse)
		/* move the camera yaw & pitch based on relative mouse inputs */
		{
			g_gs->cameraRadiansYaw += 0.01f*gameMouse.deltaPosition.x;
			g_gs->cameraRadiansYaw  = fmodf(g_gs->cameraRadiansYaw, 2*PI32);
			g_gs->cameraRadiansPitch -= 0.01f*gameMouse.deltaPosition.y;
			local_persist const f32 MAX_PITCH_MAGNITUDE = PI32/2 - 0.001f;
			if(g_gs->cameraRadiansPitch < -MAX_PITCH_MAGNITUDE)
				g_gs->cameraRadiansPitch = -MAX_PITCH_MAGNITUDE;
			if(g_gs->cameraRadiansPitch > MAX_PITCH_MAGNITUDE)
				g_gs->cameraRadiansPitch = MAX_PITCH_MAGNITUDE;
			if(gameMouse.forward > ButtonState::NOT_PRESSED)
				g_gs->cameraPosition += 
					deltaSeconds * CAMERA_SPEED * cameraWorldForward;
			if(gameMouse.back > ButtonState::NOT_PRESSED)
				g_gs->cameraPosition -= 
					deltaSeconds * CAMERA_SPEED * cameraWorldForward;
		}
		else
		{
			if(gameMouse.forward > ButtonState::NOT_PRESSED)
				g_gs->cameraPosition += deltaSeconds * CAMERA_SPEED * v3f32::Z;
			if(gameMouse.back > ButtonState::NOT_PRESSED)
				g_gs->cameraPosition -= deltaSeconds * CAMERA_SPEED * v3f32::Z;
		}
	}
	if(ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if(!lockedMouse)
			ImGui::Text("[mouse left  ] - raycast mouse onto XY plane");
		ImGui::Text("[mouse middle] - toggle orthographic view");
		ImGui::Text("[mouse right ] - hold to control camera");
		if(lockedMouse)
		{
			ImGui::Text("[mouse move] - camera yaw/pitch");
		}
		ImGui::Separator();
		ImGui::Text("Mouse Window Position={%i,%i}", 
		            gameMouse.windowPosition.x, gameMouse.windowPosition.y);
		ImGui::Text("Mouse Eye World Position={%f,%f,%f}", 
		            mouseEyeRayPosition.x, mouseEyeRayPosition.y, 
		            mouseEyeRayPosition.z);
		ImGui::Text("Mouse Eye World Direction={%f,%f,%f}", 
		            mouseEyeRayDirection.x, mouseEyeRayDirection.y, 
		            mouseEyeRayDirection.z);
		ImGui::Text("Camera Yaw=%f degrees", g_gs->cameraRadiansYaw*180/PI32);
		ImGui::Text("Camera Pitch=%f degrees", 
		            g_gs->cameraRadiansPitch*180/PI32);
	}
	ImGui::End();
	g_kpl->mouseSetRelativeMode(lockedMouse);
	g_krb->beginFrame(0.2f, 0, 0.2f);
	g_krb->setDepthTesting(true);
	if(g_gs->orthographicView)
		g_krb->setProjectionOrthoFixedHeight(
			windowDimensions.x, windowDimensions.y, 100, 1000);
	else
		g_krb->setProjectionFov(50.f, windowDimensions.elements, 1, 1000);
	g_krb->lookAt(g_gs->cameraPosition.elements, 
	              (g_gs->cameraPosition + cameraWorldForward).elements, 
	              v3f32::Z.elements);
	/* draw something at the click location */
	{
		g_krb->setModelXform(
			g_gs->clickLocation, kQuaternion::IDENTITY, {1,1,1});
		g_krb->drawCircle(5, 0, {.25f,.75f,.75f,1}, krb::TRANSPARENT, 32);
	}
	if(!lockedMouse && !isnan(resultEyeRayCollidePlaneXY))
	/* draw a crosshair where the mouse intersects with the XY world plane */
	{
		g_krb->setModelXform(
			eyeRayCollidePlaneXY, kQuaternion::IDENTITY, {5,5,5});
		local_persist const VertexNoTexture MESH[] = 
			{ {{-1, 0,0}, krb::WHITE}, {{1,0,0}, krb::WHITE}
			, {{ 0,-1,0}, krb::WHITE}, {{0,1,0}, krb::WHITE} };
		DRAW_LINES(MESH, VERTEX_ATTRIBS_NO_TEXTURE);
	}
	/* draw origin */
	{
		g_krb->setModelXform(v3f32::ZERO, kQuaternion::IDENTITY, {10,10,10});
		local_persist const VertexNoTexture MESH[] = 
			{ {{0,0,0}, krb::RED  }, {{1,0,0}, krb::RED  }
			, {{0,0,0}, krb::GREEN}, {{0,1,0}, krb::GREEN}
			, {{0,0,0}, krb::BLUE }, {{0,0,1}, krb::BLUE } };
		DRAW_LINES(MESH, VERTEX_ATTRIBS_NO_TEXTURE);
	}
#if DEBUG_DELETE_LATER
	/* @NoCommit: debug draw the mouse ray */
	{
		g_krb->setModelXform(v3f32::ZERO, kQuaternion::IDENTITY, {1,1,1});
		const VertexNoTexture mesh[] = 
			{ {g_gs->lastEyeRay[0], krb::YELLOW}
			, {g_gs->lastEyeRay[1], krb::YELLOW}};
		DRAW_LINES(mesh, VERTEX_ATTRIBS_NO_TEXTURE);
	}
	/* @NoCommit: draw a crosshair at the debug draw the mouse ray */
	{
		g_krb->setDepthTesting(false);
		g_krb->setModelXform(
			g_gs->clickLocation, kQuaternion::IDENTITY, {5,5,5});
		local_persist const VertexNoTexture MESH[] = 
			{ {{-1, 0,0}, krb::WHITE}, {{1,0,0}, krb::WHITE}
			, {{ 0,-1,0}, krb::WHITE}, {{0,1,0}, krb::WHITE} };
		DRAW_LINES(MESH, VERTEX_ATTRIBS_NO_TEXTURE);
	}
#endif//DEBUG_DELETE_LATER
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