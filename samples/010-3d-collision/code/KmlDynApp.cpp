#include "KmlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
		return false;
	v3f32 mouseEyeRayPosition  = {};
	v3f32 mouseEyeRayDirection = {};
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
	/* handle user input */
	if(windowIsFocused)
	{
		lockedMouse = gameMouse.right > ButtonState::NOT_PRESSED;
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
		if(gameKeyboard.controlLeft > ButtonState::NOT_PRESSED)
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
	}
	/* display HUD GUI for sample controls */
	if(ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("[mouse middle  ] - toggle orthographic view");
		ImGui::Text("[mouse right   ] - hold to control camera yaw/pitch");
		if(lockedMouse)
		{
			ImGui::Text("[mouse move    ] - camera yaw/pitch");
		}
		else
		{
			ImGui::Text("[mouse move    ] - *** DISABLED ***");
		}
		ImGui::Text("[E / D           ] - move camera forward/back");
		ImGui::Text("[S / F           ] - move camera left/right");
		ImGui::Text("[L-SHIFT / L-CTRL] - move camera up/down");
	}
	ImGui::End();
#if DEBUG_DELETE_LATER
	//ImGui::SliderFloat3("boxLengths", g_gs->boxLengths.elements, 0.1f, 10.f);
	ImGui::SliderFloat("radius", &g_gs->radius, 0.1f, 10.f);
	ImGui::SliderInt("latitudes", (int*)&g_gs->latitudes, 2, 100);
	ImGui::SliderInt("longitudes", (int*)&g_gs->longitudes, 3, 100);
#endif//DEBUG_DELETE_LATER
	g_kpl->mouseSetRelativeMode(lockedMouse);
	/* render the scene */
	g_krb->beginFrame(0.2f, 0, 0.2f);
	g_krb->setDepthTesting(true);
	g_krb->setBackfaceCulling(true);
	if(g_gs->orthographicView)
		g_krb->setProjectionOrthoFixedHeight(
			windowDimensions.x, windowDimensions.y, 100, 1000);
	else
		g_krb->setProjectionFov(50.f, windowDimensions.elements, 1, 1000);
	g_krb->lookAt(g_gs->cameraPosition.elements, 
	              (g_gs->cameraPosition + cameraWorldForward).elements, 
	              v3f32::Z.elements);
#if DEBUG_DELETE_LATER
	/* draw test box */
	{
		g_krb->setModelXform(v3f32::ZERO, kQuaternion::IDENTITY, {1,1,1});
#if 0
		kmath::GeneratedMeshVertex generatedBox[36];
		kmath::generateMeshBox(g_gs->boxLengths, generatedBox, 
		                       sizeof(generatedBox));
		DRAW_TRIS(generatedBox, VERTEX_ATTRIBS_GENERATED_MESH);
#else
		const size_t sphereVertexCount = 
			kmath::generateMeshCircleSphereVertexCount(
				g_gs->latitudes, g_gs->longitudes);
		kmath::GeneratedMeshVertex* generatedSphere = 
			ALLOC_FRAME_ARRAY(kmath::GeneratedMeshVertex, sphereVertexCount);
		kmath::generateMeshCircleSphere(
			g_gs->radius, g_gs->latitudes, g_gs->longitudes, generatedSphere, 
			sphereVertexCount*sizeof(kmath::GeneratedMeshVertex));
		g_krb->drawTris(generatedSphere, sphereVertexCount, 
		    sizeof(generatedSphere[0]), VERTEX_ATTRIBS_GENERATED_MESH);
#endif
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
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "TemplateGameState.cpp"