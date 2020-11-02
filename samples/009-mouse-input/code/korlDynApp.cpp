#include "korlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
		return false;
	v3f32 mouseEyeRayPosition  = {};
	v3f32 mouseEyeRayDirection = {};
	f32 resultEyeRayCollidePlaneXY = NAN32;
	v3f32 eyeRayCollidePlaneXY = {};
	bool lockedMouse = false;
	const v3f32 cameraWorldForward = kgtCamera3dWorldForward(&g_gs->camera);
	if(windowIsFocused)
	/* process mouse input only if the window is in focus */
	{
		/* perform a raycast from the mouse position onto the XY plane */
		if(   gameMouse.windowPosition.x >= 0 
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
		if(gameMouse.left == ButtonState::PRESSED && !lockedMouse)
		{
			if(!isnan(resultEyeRayCollidePlaneXY))
			{
				g_gs->clickLocation = eyeRayCollidePlaneXY;
			}
		}
		if(gameMouse.middle == ButtonState::PRESSED)
			g_gs->camera.orthographicView = !g_gs->camera.orthographicView;
		if(lockedMouse)
			kgtCamera3dLook(&g_gs->camera, gameMouse.deltaPosition);
		kgtCamera3dStep(&g_gs->camera, 
			lockedMouse ? gameMouse.forward > ButtonState::NOT_PRESSED : false, 
			lockedMouse ? gameMouse.back    > ButtonState::NOT_PRESSED : false, 
			false, false, 
			lockedMouse ? false : gameMouse.forward > ButtonState::NOT_PRESSED, 
			lockedMouse ? false : gameMouse.back    > ButtonState::NOT_PRESSED, 
			deltaSeconds);
		g_gs->clickCircleSize = kmath::clamp(
			g_gs->clickCircleSize + 0.01f*gameMouse.deltaWheel, 1.f, 10.f);
	}
	/* display HUD GUI for sample controls */
	if(ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if(lockedMouse)
			ImGui::Text("[mouse left   ] - *** DISABLED ***");
		else
			ImGui::Text("[mouse left   ] - raycast mouse onto XY plane");
		ImGui::Text("[mouse middle ] - toggle orthographic view");
		ImGui::Text("[mouse right  ] - hold to control camera yaw/pitch");
		if(lockedMouse)
		{
			ImGui::Text("[mouse move   ] - camera yaw/pitch");
			ImGui::Text("[mouse back   ] - camera back");
			ImGui::Text("[mouse forward] - camera forward");
		}
		else
		{
			ImGui::Text("[mouse move   ] - *** DISABLED ***");
			ImGui::Text("[mouse back   ] - camera down");
			ImGui::Text("[mouse forward] - camera up");
		}
		ImGui::Text("[mouse wheel  ] - mouse cursor size");
	}
	ImGui::End();
	/* mouse "relative mode" effectively disable the mouse cursor and use the 
		mouse as a purely relative axis device (only delta inputs are valid) */
	g_kpl->mouseSetRelativeMode(lockedMouse);
	/* render the scene */
	g_krb->beginFrame(0.2f, 0, 0.2f);
	g_krb->setDepthTesting(true);
	kgtCamera3dApplyViewProjection(&g_gs->camera, windowDimensions);
	/* draw something at the click location */
	{
		g_krb->setModelXform(g_gs->clickLocation, q32::IDENTITY, {1,1,1});
		g_krb->drawCircle(
			g_gs->clickCircleSize, 0, {.25f,.75f,.75f,1}, krb::TRANSPARENT, 32);
	}
	if(!lockedMouse && !isnan(resultEyeRayCollidePlaneXY))
	/* draw a crosshair where the mouse intersects with the XY world plane */
	{
		g_krb->setModelXform(
			eyeRayCollidePlaneXY, q32::IDENTITY, 
			{ g_gs->clickCircleSize 
			, g_gs->clickCircleSize 
			, g_gs->clickCircleSize} );
		local_persist const KgtVertex MESH[] = 
			{ {{-1, 0,0}, {}, krb::WHITE}, {{1,0,0}, {}, krb::WHITE}
			, {{ 0,-1,0}, {}, krb::WHITE}, {{0,1,0}, {}, krb::WHITE} };
		DRAW_LINES(MESH, KGT_VERTEX_ATTRIBS_NO_TEXTURE);
	}
	kgtDrawOrigin({10,10,10});
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
	g_gs->camera.position = {10,10,10};
	kgtCamera3dLookAt(&g_gs->camera, v3f32::ZERO);
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "kgtGameState.cpp"
#include "kgtCamera3d.cpp"
