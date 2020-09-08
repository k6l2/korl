#include "TemplateGameState.h"
internal void templateGameState_onReloadCode(GameMemory& memory)
{
	g_kpl          = &memory.kpl;
	g_krb          = &memory.krb;
	platformLog    = memory.kpl.log;
	platformAssert = memory.kpl.assert;
	/* ImGui support */
	ImGui::SetCurrentContext(
		reinterpret_cast<ImGuiContext*>(memory.imguiContext));
	ImGui::SetAllocatorFunctions(memory.platformImguiAlloc, 
	                             memory.platformImguiFree, 
	                             memory.imguiAllocUserData);
}
internal void templateGameState_initialize(
	KmlTemplateGameState* tgs, GameMemory& memory, size_t totalGameStateSize)
{
	kassert(totalGameStateSize <= memory.permanentMemoryBytes);
	// initialize dynamic allocators //
	tgs->hKgaPermanent = 
		kgaInit(reinterpret_cast<u8*>(memory.permanentMemory) + 
		            totalGameStateSize, 
		        memory.permanentMemoryBytes - totalGameStateSize);
	tgs->hKgaTransient = kgaInit(memory.transientMemory, 
	                             memory.transientMemoryBytes);
	// construct a linear frame allocator //
	{
		const size_t kalFrameSize = kmath::megabytes(5);
		void*const kalFrameStartAddress = 
			kgaAlloc(tgs->hKgaPermanent, kalFrameSize);
		tgs->hKalFrame = kalInit(kalFrameStartAddress, kalFrameSize);
	}
	// Contruct/Initialize the game's AssetManager //
	tgs->assetManager = kamConstruct(tgs->hKgaPermanent, KASSET_COUNT,
	                                 tgs->hKgaTransient, g_kpl, g_krb);
	// Initialize the game's audio mixer //
	tgs->kAudioMixer = kauConstruct(tgs->hKgaPermanent, 16, 
	                                tgs->assetManager);
	// Tell the asset manager to load assets asynchronously! //
	kamPushAllKAssets(tgs->assetManager);
}
internal void templateGameState_renderAudio(
	KmlTemplateGameState* tgs, GameAudioBuffer& audioBuffer, 
	u32 sampleBlocksConsumed)
{
	kauMix(tgs->kAudioMixer, audioBuffer, sampleBlocksConsumed);
}
internal bool templateGameState_updateAndDraw(
	KmlTemplateGameState* tgs, GameKeyboard& gameKeyboard, 
	bool windowIsFocused)
{
	kalReset(tgs->hKalFrame);
	/* Esc, Ctrl+W & Alt+F4 shortcuts to quickly exit the program */
	if(gameKeyboard.escape == ButtonState::PRESSED 
		|| (gameKeyboard.f4 == ButtonState::PRESSED 
			&& gameKeyboard.modifiers.alt)
		|| (gameKeyboard.w == ButtonState::PRESSED 
			&& gameKeyboard.modifiers.control))
	{
		if(windowIsFocused)
			return false;
	}
	/* easy fullscreen shortcuts */
	if(gameKeyboard.f11 == ButtonState::PRESSED 
		|| (gameKeyboard.enter == ButtonState::PRESSED 
			&& gameKeyboard.modifiers.alt))
	{
		g_kpl->setFullscreen(!g_kpl->isFullscreen());
	}
	/* hot-reload all assets which have been reported to be changed by the 
		platform layer (newer file write timestamp) */
	if(kamUnloadChangedAssets(tgs->assetManager))
		kamPushAllKAssets(tgs->assetManager);
	return true;
}
#include "kFlipBook.cpp"
#include "kAudioMixer.cpp"
#pragma warning( push )
	/* warning C4296: '<': expression is always false.  This happens if there 
		are no KAssets in the assets directory */
	#pragma warning( disable : 4296 )
	#include "kAssetManager.cpp"
#pragma warning( pop )
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
#include "kNetClient.cpp"
#include "kNetServer.cpp"
#include "KNetReliableDataBuffer.cpp"