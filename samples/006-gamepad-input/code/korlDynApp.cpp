#include "korlDynApp.h"
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(gameKeyboard, windowIsFocused))
		return false;
	/* simple HUD visualization of which controllers are plugged in */
	if(ImGui::Begin("Gamepads", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		for(u8 c = 0; c < numGamePads; c++)
		{
			GamePad& gpad = gamePadArray[c];
			switch(gpad.type)
			{
				case GamePadType::UNPLUGGED:
				{
					ImGui::Text("---UNPLUGGED---");
				} break;
				case GamePadType::XINPUT:
				{
					ImGui::Text("XINPUT");
				} break;
				case GamePadType::DINPUT_GENERIC:
				{
					ImGui::Text("DINPUT_GENERIC");
				} break;
			}
		}
	}
	ImGui::End();
	/* some local state to display a simple HUD of the gamepad inputs */
	v2f32 hudLeftStick = {};
	RgbaF32 hudFillColorStickClickLeft = {1,1,1,0.5f};
	RgbaF32 hudFillColorFaceUp         = krb::TRANSPARENT;
	RgbaF32 hudFillColorFaceDown       = krb::TRANSPARENT;
	RgbaF32 hudFillColorFaceLeft       = krb::TRANSPARENT;
	RgbaF32 hudFillColorFaceRight      = krb::TRANSPARENT;
	if(windowIsFocused)
	/* process gamepad input of the first gamepad in the array that is plugged 
		in, if one exists */
	{
		for(u8 g = 0; g < numGamePads; g++)
		{
			if(gamePadArray[g].type == GamePadType::UNPLUGGED)
				continue;
			hudLeftStick = gamePadArray[g].stickLeft;
			if(gamePadArray[g].faceUp >= ButtonState::PRESSED)
				hudFillColorFaceUp = krb::WHITE;
			if(gamePadArray[g].faceDown >= ButtonState::PRESSED)
				hudFillColorFaceDown = krb::WHITE;
			if(gamePadArray[g].faceLeft >= ButtonState::PRESSED)
				hudFillColorFaceLeft = krb::WHITE;
			if(gamePadArray[g].faceRight >= ButtonState::PRESSED)
				hudFillColorFaceRight = krb::WHITE;
			if(gamePadArray[g].stickClickLeft >= ButtonState::PRESSED)
				hudFillColorStickClickLeft = krb::WHITE;
			break;
		}
	}
	/* everything below here is just drawing the hud representation of the 
		gamepad */
	g_krb->beginFrame(v3f32{0.2f, 0, 0.2f}.elements, windowDimensions.elements);
	defer(g_krb->endFrame());
	g_krb->setProjectionOrthoFixedHeight(150, 1);
	local_persist const v2f32 HUD_LEFT_STICK_OFFSET = {-75,0};
	g_krb->setModelXform2d(HUD_LEFT_STICK_OFFSET, q32::IDENTITY, {1,1});
	g_krb->drawCircle(25, 0, krb::TRANSPARENT, krb::WHITE, 32);
	g_krb->setModelXform2d(
		HUD_LEFT_STICK_OFFSET + 25*hudLeftStick, q32::IDENTITY, {1,1});
	g_krb->drawCircle(20, 0, hudFillColorStickClickLeft, krb::TRANSPARENT, 32);
	local_persist const v2f32 HUD_FACE_BUTTON_OFFSET = {75,0};
	local_persist const f32 HUD_FACE_BUTTON_SPREAD = 20;
	g_krb->setModelXform2d(
		HUD_FACE_BUTTON_OFFSET + v2f32{0,HUD_FACE_BUTTON_SPREAD}, 
		q32::IDENTITY, {1,1});
	g_krb->drawCircle(5, 0, hudFillColorFaceUp, krb::WHITE, 32);
	g_krb->setModelXform2d(
		HUD_FACE_BUTTON_OFFSET + v2f32{0,-HUD_FACE_BUTTON_SPREAD}, 
		q32::IDENTITY, {1,1});
	g_krb->drawCircle(5, 0, hudFillColorFaceDown, krb::WHITE, 32);
	g_krb->setModelXform2d(
		HUD_FACE_BUTTON_OFFSET + v2f32{-HUD_FACE_BUTTON_SPREAD,0}, 
		q32::IDENTITY, {1,1});
	g_krb->drawCircle(5, 0, hudFillColorFaceLeft, krb::WHITE, 32);
	g_krb->setModelXform2d(
		HUD_FACE_BUTTON_OFFSET + v2f32{HUD_FACE_BUTTON_SPREAD,0}, 
		q32::IDENTITY, {1,1});
	g_krb->drawCircle(5, 0, hudFillColorFaceRight, krb::WHITE, 32);
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
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
}
#include "kgtGameState.cpp"
