#include "KmlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
		return false;
	if(gameMouse.middle == ButtonState::PRESSED)
	{
		g_gs->orthographicView = !g_gs->orthographicView;
	}
	v3f32 mouseEyeRayPosition  = {};
	v3f32 mouseEyeRayDirection = {};
	f32 resultEyeRayCollidePlaneXY = NAN32;
	if(gameMouse.windowPosition.x >= 0 
		&& gameMouse.windowPosition.x < static_cast<i64>(windowDimensions.x)
		&& gameMouse.windowPosition.y >= 0 
		&& gameMouse.windowPosition.y < static_cast<i64>(windowDimensions.y))
	{
		if(!g_krb->screenToWorld(
				gameMouse.windowPosition.elements, windowDimensions.elements, 
				mouseEyeRayPosition.elements, mouseEyeRayDirection.elements))
		{
			KLOG(ERROR, "Failed to get mouse eye ray!");
		}
		resultEyeRayCollidePlaneXY = 
			kmath::collideRayPlane(mouseEyeRayPosition, mouseEyeRayDirection,
			                       v3f32::Z, 0, false);
	}
	ImGui::Text("Mouse Window Position={%i,%i}", 
	            gameMouse.windowPosition.x, gameMouse.windowPosition.y);
	ImGui::Text("Mouse Eye World Position={%f,%f,%f}", mouseEyeRayPosition.x, 
	            mouseEyeRayPosition.y, mouseEyeRayPosition.z);
	ImGui::Text("Mouse Eye World Direction={%f,%f,%f}", mouseEyeRayDirection.x, 
	            mouseEyeRayDirection.y, mouseEyeRayDirection.z);
	g_krb->beginFrame(0.2f, 0, 0.2f);
	if(g_gs->orthographicView)
		g_krb->setProjectionOrthoFixedHeight(
			windowDimensions.x, windowDimensions.y, 100, 1000);
	else
		g_krb->setProjectionFov(90.f, windowDimensions.elements, 1, 1000);
	g_krb->lookAt(v3f32{10,11,12}.elements, v3f32::ZERO.elements, 
	              v3f32::Z.elements);
	if(!isnan(resultEyeRayCollidePlaneXY))
	/* draw a crosshair where the mouse intersects with the XY world plane */
	{
		const v3f32 eyeRayCollidePlaneXY = mouseEyeRayPosition + 
			resultEyeRayCollidePlaneXY*mouseEyeRayDirection;
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
	g_gs->orthographicView = true;
	templateGameState_initialize(&g_gs->templateGameState, memory, 
	                             sizeof(GameState));
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "TemplateGameState.cpp"