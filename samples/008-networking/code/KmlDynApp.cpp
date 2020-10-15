#include "KmlDynApp.h"
#include "exampleGameNet.h"
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	kgtGameStateOnReloadCode(memory);
	g_gs = reinterpret_cast<GameState*>(memory.permanentMemory);
	if(gameMemoryIsInitialized)
	{
		serverOnReload(&g_gs->serverState);
	}
}
GAME_INITIALIZE(gameInitialize)
{
	*g_gs = {};// clear all GameState memory before initializing the template
	kgtGameStateInitialize(&g_gs->kgtGameState, memory, sizeof(GameState));
	/* initialize server state */
	serverInitialize(&g_gs->serverState, 1.f/60, 
	                 g_gs->kgtGameState.hKalPermanent, 
	                 g_gs->kgtGameState.hKalTransient, 
	                 kmath::megabytes(5), kmath::megabytes(5), 
	                 GAME_NET_SERVER_LISTEN_PORT);
	// Initialize flipbooks //
	kgtFlipBookInit(&g_gs->kFbShip, KgtAssetIndex::gfx_fighter_fbm);
	kgtFlipBookInit(
		&g_gs->kFbShipExhaust, KgtAssetIndex::gfx_fighter_exhaust_fbm);
	// Initialize dynamic Actor array //
	g_gs->actors = arrinit(Actor, g_gs->kgtGameState.hKalPermanent);
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	kgtGameStateRenderAudio(
		&g_gs->kgtGameState, audioBuffer, sampleBlocksConsumed);
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!kgtGameStateUpdateAndDraw(
			&g_gs->kgtGameState, gameKeyboard, windowIsFocused))
		return false;
	/* TESTING SERVER EXAMPLE */
	if(ImGui::Begin("TESTING SERVER EXAMPLE"))
	{
		switch(serverOpState(&g_gs->serverState))
		{
			case ServerOperatingState::RUNNING:
			{
				if(ImGui::Button("Stop Server"))
				{
					serverStop(&g_gs->serverState);
				}
			}break;
			case ServerOperatingState::STOPPING:
			{
				ImGui::Text("Stopping server...");
			}break;
			case ServerOperatingState::STOPPED:
			{
				if(ImGui::Button("Start Server"))
				{
					serverStart(&g_gs->serverState);
				}
			}break;
		}
		ImGui::Separator();
		if(kgtNetClientIsDisconnected(&g_gs->kNetClient))
		{
			if(ImGui::Button("Connect to Server"))
			{
				kgtNetClientConnect(
					&g_gs->kNetClient, g_gs->clientAddressBuffer, 
					GAME_NET_SERVER_LISTEN_PORT);
			}
			ImGui::InputText("net address", g_gs->clientAddressBuffer, 
			                 CARRAY_SIZE(g_gs->clientAddressBuffer));
		}
		else
		{
			switch(g_gs->kNetClient.connectionState)
			{
				case kgtNet::ConnectionState::DISCONNECTING:{
					ImGui::Text("Disconnecting...%c", 
					            "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
				}break;
				case kgtNet::ConnectionState::ACCEPTING:{
					ImGui::Text("Connecting...%c", 
					            "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
				}break;
				case kgtNet::ConnectionState::CONNECTED:{
					if(ImGui::Button("Disconnect"))
					{
						kgtNetClientBeginDisconnect(&g_gs->kNetClient);
					}
					if(ImGui::Button("Drop"))
					{
						kgtNetClientDropConnection(&g_gs->kNetClient);
					}
				}break;
			}
			const i32 pingMilliseconds = static_cast<i32>(
				g_gs->kNetClient.serverReportedRoundTripTime*1000);
			ImGui::Text("ping: %ims", pingMilliseconds);
			ImGui::InputText("", g_gs->clientReliableTextBuffer, 
			                 CARRAY_SIZE(g_gs->clientReliableTextBuffer));
			ImGui::SameLine();
			if(ImGui::Button("SEND"))
			{
				const u16 reliableMessageBytes = 
					reliableMessageAnsiTextPack(
						g_gs->clientReliableTextBuffer, 
						g_gs->reliableMessageBuffer, 
						g_gs->reliableMessageBuffer + 
							CARRAY_SIZE(g_gs->reliableMessageBuffer));
				kgtNetClientQueueReliableMessage(&g_gs->kNetClient, 
					g_gs->reliableMessageBuffer, reliableMessageBytes);
			}
		}
	}
	ImGui::End();
	/* Networking example Client logic */
	if(kgtNetClientIsDisconnected(&g_gs->kNetClient))
	{
		arrsetlen(g_gs->actors, 0);
	}
	kgtNetClientStep(&g_gs->kNetClient, deltaSeconds, 0.1f*deltaSeconds, 
	                 gameWriteClientState, gameClientReadServerState, 
	                 gameClientReadReliableMessage);
	for(u8 c = 0; c < numGamePads; c++)
	{
		GamePad& gpad = gamePadArray[c];
		if(gpad.type == GamePadType::UNPLUGGED)
			continue;
		ClientControlInput controlInput = {};
		controlInput.controlMoveVector = gpad.stickLeft;
		gpad.normalizedMotorSpeedLeft  = gpad.triggerLeft;
		gpad.normalizedMotorSpeedRight = gpad.triggerRight;
		if (gpad.back  == ButtonState::HELD &&
		    gpad.start == ButtonState::PRESSED)
		{
			return false;
		}
		if(gpad.stickClickLeft == ButtonState::PRESSED)
		{
			controlInput.controlResetPosition = true;
		}
#if 1
		//@TODO: @cleanup; delete this code
		if(gpad.faceRight > ButtonState::NOT_PRESSED)
		{
			kgtAssetManagerUnloadAllAssets(g_gs->kgtGameState.assetManager);
		}
#endif 
		/* send the client's controller input for this frame as a reliable 
			message */
		if(!kgtNetClientIsDisconnected(&g_gs->kNetClient))
		{
			const u16 reliableMessageBytes = 
				reliableMessageClientControlInputPack(
					controlInput, g_gs->reliableMessageBuffer, 
					g_gs->reliableMessageBuffer + 
						CARRAY_SIZE(g_gs->reliableMessageBuffer));
			kgtNetClientQueueReliableMessage(&g_gs->kNetClient, 
				g_gs->reliableMessageBuffer, reliableMessageBytes);
		}
	}
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	g_krb->setProjectionOrthoFixedHeight(
		windowDimensions.x, windowDimensions.y, 300, 1.f);
	for(size_t a = 0; a < arrlenu(g_gs->actors); a++)
	{
		Actor& actor = g_gs->actors[a];
		if(actor.clientId == g_gs->kNetClient.id)
		{
			g_krb->viewTranslate(-actor.shipWorldPosition);
			break;
		}
	}
	kgtFlipBookStep(&g_gs->kFbShip       , deltaSeconds);
	kgtFlipBookStep(&g_gs->kFbShipExhaust, deltaSeconds);
	for(size_t a = 0; a < arrlenu(g_gs->actors); a++)
	{
		Actor& actor = g_gs->actors[a];
		g_krb->setModelXform2d(
			actor.shipWorldPosition, actor.shipWorldOrientation, {1,1});
		kgtFlipBookDraw(&g_gs->kFbShip       , krb::WHITE);
		kgtFlipBookDraw(&g_gs->kFbShipExhaust, krb::WHITE);
	}
	/* draw a simple 2D origin */
	kgtDrawOrigin({10,10,10});
	return true;
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
	serverOnPreUnload(&g_gs->serverState);
}
#include "kgtGameState.cpp"
#include "serverExample.cpp"
#include "exampleGameNet.cpp"
