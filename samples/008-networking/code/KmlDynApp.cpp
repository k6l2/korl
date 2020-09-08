#include "KmlDynApp.h"
#include "exampleGameNet.h"
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	templateGameState_onReloadCode(memory);
	g_gs = reinterpret_cast<GameState*>(memory.permanentMemory);
	if(gameMemoryIsInitialized)
	{
		serverOnReload(&g_gs->serverState);
	}
}
GAME_INITIALIZE(gameInitialize)
{
	*g_gs = {};// clear all GameState memory before initializing the template
	templateGameState_initialize(&g_gs->templateGameState, memory, 
	                             sizeof(GameState));
	/* initialize server state */
	serverInitialize(&g_gs->serverState, 1.f/60, 
	                 g_gs->templateGameState.hKgaPermanent, 
	                 g_gs->templateGameState.hKgaTransient, 
	                 kmath::megabytes(5), kmath::megabytes(5), 
	                 GAME_NET_SERVER_LISTEN_PORT);
	// Initialize flipbooks //
	kfbInit(&g_gs->kFbShip, g_gs->templateGameState.assetManager, g_krb, 
	        KAssetIndex::gfx_fighter_fbm);
	kfbInit(&g_gs->kFbShipExhaust, g_gs->templateGameState.assetManager, g_krb, 
	        KAssetIndex::gfx_fighter_exhaust_fbm);
	// Initialize dynamic Actor array //
	g_gs->actors = arrinit(Actor, g_gs->templateGameState.hKgaPermanent);
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	templateGameState_renderAudio(&g_gs->templateGameState, audioBuffer, 
	                              sampleBlocksConsumed);
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	if(!templateGameState_updateAndDraw(&g_gs->templateGameState, gameKeyboard, 
	                                    windowIsFocused))
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
		if(kNetClientIsDisconnected(&g_gs->kNetClient))
		{
			if(ImGui::Button("Connect to Server"))
			{
				kNetClientConnect(&g_gs->kNetClient, g_gs->clientAddressBuffer, 
				                  GAME_NET_SERVER_LISTEN_PORT);
			}
			ImGui::InputText("net address", g_gs->clientAddressBuffer, 
			                 CARRAY_SIZE(g_gs->clientAddressBuffer));
		}
		else
		{
			switch(g_gs->kNetClient.connectionState)
			{
				case network::ConnectionState::DISCONNECTING:{
					ImGui::Text("Disconnecting...%c", 
					            "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
				}break;
				case network::ConnectionState::ACCEPTING:{
					ImGui::Text("Connecting...%c", 
					            "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
				}break;
				case network::ConnectionState::CONNECTED:{
					if(ImGui::Button("Disconnect"))
					{
						kNetClientBeginDisconnect(&g_gs->kNetClient);
					}
					if(ImGui::Button("Drop"))
					{
						kNetClientDropConnection(&g_gs->kNetClient);
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
				kNetClientQueueReliableMessage(&g_gs->kNetClient, 
					g_gs->reliableMessageBuffer, reliableMessageBytes);
			}
		}
	}
	ImGui::End();
	/* Networking example Client logic */
	if(kNetClientIsDisconnected(&g_gs->kNetClient))
	{
		arrsetlen(g_gs->actors, 0);
	}
	kNetClientStep(&g_gs->kNetClient, deltaSeconds, 0.1f*deltaSeconds, 
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
		if(gpad.faceRight > ButtonState::NOT_PRESSED)
		{
			kamUnloadAllAssets(g_gs->templateGameState.assetManager);
		}
#endif //@TODO: @simplification; delete this code
		/* send the client's controller input for this frame as a reliable 
			message */
		if(!kNetClientIsDisconnected(&g_gs->kNetClient))
		{
			const u16 reliableMessageBytes = 
				reliableMessageClientControlInputPack(
					controlInput, g_gs->reliableMessageBuffer, 
					g_gs->reliableMessageBuffer + 
						CARRAY_SIZE(g_gs->reliableMessageBuffer));
			kNetClientQueueReliableMessage(&g_gs->kNetClient, 
				g_gs->reliableMessageBuffer, reliableMessageBytes);
		}
	}
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	g_krb->setProjectionOrthoFixedHeight(windowDimensions.x, windowDimensions.y, 
	                                     300, 1.f);
	for(size_t a = 0; a < arrlenu(g_gs->actors); a++)
	{
		Actor& actor = g_gs->actors[a];
		if(actor.clientId == g_gs->kNetClient.id)
		{
			g_krb->viewTranslate(-actor.shipWorldPosition);
			break;
		}
	}
	kfbStep(&g_gs->kFbShip       , deltaSeconds);
	kfbStep(&g_gs->kFbShipExhaust, deltaSeconds);
	for(size_t a = 0; a < arrlenu(g_gs->actors); a++)
	{
		Actor& actor = g_gs->actors[a];
		g_krb->setModelXform2d(actor.shipWorldPosition, 
		                       actor.shipWorldOrientation, {1,1});
		kfbDraw(&g_gs->kFbShip       , krb::WHITE);
		kfbDraw(&g_gs->kFbShipExhaust, krb::WHITE);
	}
	/* draw a simple 2D origin */
	{
		g_krb->setModelXform2d({0,0}, kQuaternion::IDENTITY, {10,10});
		const local_persist VertexNoTexture meshOrigin[] = 
			{ {{0,0,0}, krb::RED  }, {{1,0,0}, krb::RED  }
			, {{0,0,0}, krb::GREEN}, {{0,1,0}, krb::GREEN} };
		DRAW_LINES(meshOrigin, VERTEX_NO_TEXTURE_ATTRIBS);
	}
	return true;
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
	serverOnPreUnload(&g_gs->serverState);
}
#include "TemplateGameState.cpp"
#include "serverExample.cpp"
#include "exampleGameNet.cpp"