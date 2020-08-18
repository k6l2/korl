#include "KmlGameExample.h"
#pragma warning( push )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	#include "imgui/imgui.h"
#pragma warning( pop )
#include "gen_kassets.h"
#include "exampleGameNet.h"
GAME_ON_RELOAD_CODE(gameOnReloadCode)
{
	g_kpl = &memory.kpl;
	platformLog = memory.kpl.log;
	platformAssert = memory.kpl.assert;
	kassert(sizeof(GameState) <= memory.permanentMemoryBytes);
	g_gs = reinterpret_cast<GameState*>(memory.permanentMemory);
	g_krb = &memory.krb;
	ImGui::SetCurrentContext(
		reinterpret_cast<ImGuiContext*>(memory.imguiContext));
	ImGui::SetAllocatorFunctions(memory.platformImguiAlloc, 
	                             memory.platformImguiFree, 
	                             memory.imguiAllocUserData);
	if(gameMemoryIsInitialized)
	{
		serverOnReload(&g_gs->serverState);
	}
}
GAME_INITIALIZE(gameInitialize)
{
	// GameState memory initialization //
	*g_gs = {};
	// initialize dynamic allocators //
	g_gs->hKgaPermanent = 
		kgaInit(reinterpret_cast<u8*>(memory.permanentMemory) + 
		            sizeof(GameState), 
		        memory.permanentMemoryBytes - sizeof(GameState));
	g_gs->hKgaTransient = kgaInit(memory.transientMemory, 
	                              memory.transientMemoryBytes);
	/* initialize server state */
	serverInitialize(&g_gs->serverState, 1.f/60, 
	                 g_gs->hKgaPermanent, g_gs->hKgaTransient, 
	                 kmath::megabytes(5), kmath::megabytes(5), 30942);
	// construct a linear frame allocator //
	{
		const size_t kalFrameSize = kmath::megabytes(5);
		void*const kalFrameStartAddress = 
			kgaAlloc(g_gs->hKgaPermanent, kalFrameSize);
		g_gs->hKalFrame = kalInit(kalFrameStartAddress, kalFrameSize);
	}
	// Contruct/Initialize the game's AssetManager //
	g_gs->assetManager = kamConstruct(g_gs->hKgaPermanent, KASSET_COUNT,
	                                  g_gs->hKgaTransient, g_kpl, g_krb);
	// Initialize the game's audio mixer //
	g_gs->kAudioMixer = kauConstruct(g_gs->hKgaPermanent, 16, 
	                                 g_gs->assetManager);
	g_gs->tapeBgmBattleTheme = 
		kauPlaySound(g_gs->kAudioMixer, 
		             KAssetIndex::sfx_joesteroids_battle_theme_modified_ogg);
	kauSetVolume(g_gs->kAudioMixer, &g_gs->tapeBgmBattleTheme, 0.25f);
	kauSetRepeat(g_gs->kAudioMixer, &g_gs->tapeBgmBattleTheme, true);
	// Tell the asset manager to load assets asynchronously! //
	kamPushAllKAssets(g_gs->assetManager);
	// Initialize flipbooks //
	kfbInit(&g_gs->kFbShip, g_gs->assetManager, g_krb, 
	        KAssetIndex::gfx_fighter_fbm);
	kfbInit(&g_gs->kFbShipExhaust, g_gs->assetManager, g_krb, 
	        KAssetIndex::gfx_fighter_exhaust_fbm);
	// Initialize dynamic Actor array //
	g_gs->actors = reinterpret_cast<Actor*>(
		arrinit(sizeof(Actor), g_gs->hKgaPermanent, 4));
}
GAME_RENDER_AUDIO(gameRenderAudio)
{
	if(!kamIsLoadingSounds(g_gs->assetManager))
	{
		kauMix(g_gs->kAudioMixer, audioBuffer, sampleBlocksConsumed);
	}
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDraw)
{
	kalReset(g_gs->hKalFrame);
#if INTERNAL_BUILD
	/* TESTING GAME PAD ARRAY */
	if(ImGui::Begin("TESTING GAME PAD PORTS"))
	{
		for(u8 c = 0; c < numGamePads; c++)
		{
			GamePad& gpad = gamePadArray[c];
			switch(gpad.type)
			{
				case GamePadType::UNPLUGGED:
				{
					ImGui::Text("UNPLUGGED");
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
				kNetClientConnect(&g_gs->kNetClient, g_gs->clientAddressBuffer);
			}
			ImGui::InputText("net address", g_gs->clientAddressBuffer, 
			                 CARRAY_SIZE(g_gs->clientAddressBuffer));
		}
		else
		{
			switch(g_gs->kNetClient.connectionState)
			{
				case network::ConnectionState::NOT_CONNECTED:{
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
		}
	}
	ImGui::End();
	/* Networking example Client logic */
	if(g_gs->kNetClient.connectionState != network::ConnectionState::CONNECTED)
	{
		arrsetlen(g_gs->actors, 0);
	}
	kNetClientStep(&g_gs->kNetClient, deltaSeconds, 0.1f*deltaSeconds, 
	               gameWriteClientState, gameClientReadServerState);
#endif// INTERNAL_BUILD
	ImGui::ShowDemoWindow();
	if(gameKeyboard.escape == ButtonState::PRESSED ||
	   (gameKeyboard.f4 == ButtonState::PRESSED && gameKeyboard.modifiers.alt))
	{
		if(windowIsFocused)
			return false;
	}
	if(gameKeyboard.enter == ButtonState::PRESSED && gameKeyboard.modifiers.alt)
	{
		g_kpl->setFullscreen(!g_kpl->isFullscreen());
	}
	if(kamIsLoadingImages(g_gs->assetManager))
	{
		return true;
	}
	/* hot-reload all assets which have been reported to be changed by the 
		platform layer (newer file write timestamp) */
	if(kamUnloadChangedAssets(g_gs->assetManager))
		kamPushAllKAssets(g_gs->assetManager);
	for(u8 c = 0; c < numGamePads; c++)
	{
		GamePad& gpad = gamePadArray[c];
		if(gpad.type == GamePadType::UNPLUGGED)
			continue;
		if(gpad.shoulderLeft2 == ButtonState::PRESSED)
		{
			kauPlaySound(g_gs->kAudioMixer, 
			             KAssetIndex::sfx_joesteroids_hit_wav);
		}
		if(gpad.shoulderRight2 >= ButtonState::PRESSED)
		{
			if(gpad.faceLeft >= ButtonState::PRESSED)
			{
				kauPlaySound(g_gs->kAudioMixer, 
				             KAssetIndex::sfx_joesteroids_explosion_wav);
			}
			else
			{
				kauPlaySound(g_gs->kAudioMixer, 
				             KAssetIndex::sfx_joesteroids_shoot_modified_wav);
			}
		}
		g_gs->shipWorldPosition.x += 10*gpad.stickLeft.x;
		g_gs->shipWorldPosition.y += 10*gpad.stickLeft.y;
		if(!kmath::isNearlyZero(gpad.stickLeft.x) ||
		   !kmath::isNearlyZero(gpad.stickLeft.y))
		{
			const f32 stickRadians = 
				kmath::v2Radians(gpad.stickLeft);
			g_gs->shipWorldOrientation = 
				kQuaternion({0,0,1}, stickRadians - PI32/2);
		}
		const f32 controlVolumeRatio = 
			(gpad.stickRight.y/2) + 0.5f;
		kauSetVolume(g_gs->kAudioMixer, &g_gs->tapeBgmBattleTheme, 
		             controlVolumeRatio);
		gpad.normalizedMotorSpeedLeft  = gpad.triggerLeft;
		gpad.normalizedMotorSpeedRight = gpad.triggerRight;
		if (gpad.back  == ButtonState::HELD &&
		    gpad.start == ButtonState::PRESSED)
		{
			return false;
		}
		if(gpad.stickClickLeft == ButtonState::PRESSED)
		{
			g_gs->shipWorldPosition = {};
		}
#if 1
		if(gpad.faceDown == ButtonState::PRESSED)
		{
			*(int*)0 = 0;
		}
		if(gpad.faceRight > ButtonState::NOT_PRESSED)
		{
			kamUnloadAllAssets(g_gs->assetManager);
		}
#endif //1
	}
	g_krb->beginFrame(0.2f, 0.f, 0.2f);
	const f32 windowAspectRatio = windowDimensions.y == 0 
		? 1 : static_cast<f32>(windowDimensions.x) / windowDimensions.y;
	g_krb->setProjectionOrthoFixedHeight(windowDimensions.x, windowDimensions.y, 
	                                     300, 1.f);
	g_krb->viewTranslate(-g_gs->shipWorldPosition);
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
	g_krb->setModelXform({0,0,0}, kmath::IDENTITY_QUATERNION, {1,1,1});
	g_krb->drawLine({0,0}, {100,   0}, krb::RED);
	g_krb->drawLine({0,0}, {  0, 100}, krb::GREEN);
	g_krb->drawCircle(50, 10, {1,0,0,0.5f}, {1,0,0,1}, 16);
	return true;
}
GAME_ON_PRE_UNLOAD(gameOnPreUnload)
{
	serverOnPreUnload(&g_gs->serverState);
	kNetClientOnPreUnload(&g_gs->kNetClient);
}
#include "kFlipBook.cpp"
#include "kAudioMixer.cpp"
#include "kAssetManager.cpp"
#include "kGeneralAllocator.cpp"
#include "kAllocatorLinear.cpp"
#pragma warning( push )
	// warning C4127: conditional expression is constant
	#pragma warning( disable : 4127 )
	// warning C4820: bytes padding added after data member
	#pragma warning( disable : 4820 )
	// warning C4365: 'argument': conversion
	#pragma warning( disable : 4365 )
	// warning C4577: 'noexcept' used with no exception handling mode 
	//                specified...
	#pragma warning( disable : 4577 )
	// warning C4774: 'sscanf' : format string expected in argument 2 is not a 
	//                string literal
	#pragma warning( disable : 4774 )
	#include "imgui/imgui_demo.cpp"
	#include "imgui/imgui_draw.cpp"
	#include "imgui/imgui_widgets.cpp"
	#include "imgui/imgui.cpp"
#pragma warning( pop )
#define STB_DS_IMPLEMENTATION
internal void* kStbDsRealloc(void* allocatedAddress, size_t newAllocationSize, 
                             void* context)
{
	kassert(context);
	KgaHandle hKga = reinterpret_cast<KgaHandle>(context);
	void*const result = kgaRealloc(hKga, allocatedAddress, newAllocationSize);
	kassert(result);
	return result;
}
internal void kStbDsFree(void* allocatedAddress, void* context)
{
	kassert(context);
	KgaHandle hKga = reinterpret_cast<KgaHandle>(context);
	kgaFree(hKga, allocatedAddress);
}
#pragma warning( push )
	// warning C4365: 'argument': conversion
	#pragma warning( disable : 4365 )
	// warning C4456: declaration of 'i' hides previous local declaration
	#pragma warning( disable : 4456 )
	#include "stb/stb_ds.h"
#pragma warning( pop )
#include "kmath.cpp"
#include "kutil.cpp"
#include "serverExample.cpp"
#include "kNetClient.cpp"
#include "kNetServer.cpp"
#include "exampleGameNet.cpp"